#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "konsens/client.hpp"
#include "konsens/message.hpp"
#include "konsens/state_machine.hpp"
#include "konsens/storage.hpp"
#include "konsens/wire.hpp"

namespace konsens {

template <storage S, state_machine M>
class node;

static_assert(std::is_nothrow_move_constructible_v<paxos_message>);

enum class paxos_errc { hardware_fault = 1, inconsistent_storage, state_differs_from_log, result_differs };

class paxos_error_category final : public std::error_category {
public:
    const char* name() const noexcept override { return "konsens.paxos"; }

    std::string message(const int code) const override
    {
        switch (static_cast<paxos_errc>(code)) {
        case paxos_errc::hardware_fault:
            return "Two nodes disagree on an entry that the protocol keeps equal; the node stops voting";
        case paxos_errc::inconsistent_storage:
            return "The values in the storage of the node break a rule that the protocol keeps; the node stops voting";
        case paxos_errc::state_differs_from_log:
            return "The applied index of the state storage is above the log, or its chain CRC differs from the log; "
                   "the "
                   "node stops voting";
        case paxos_errc::result_differs:
            return "A chosen result fails its CRC on this node, or a value that the node wrote reads back different; "
                   "the node throws";
        }
        return "Unknown paxos error";
    }
};

inline const std::error_category& paxos_category() noexcept
{
    static const paxos_error_category category;
    return category;
}

inline std::error_code make_error_code(const paxos_errc code) noexcept
{
    return {static_cast<int>(code), paxos_category()};
}

class node_fault : public std::system_error {
public:
    using std::system_error::system_error;
};

struct hardware_fault {
    std::uint64_t index;
    node_id pid;
    std::uint32_t crc;
    node_id other_pid;
    std::uint32_t other_crc;
};

inline bool error_is_a_damage(const std::error_code e) noexcept
{
    return e == make_error_code(paxos_errc::hardware_fault) || e == make_error_code(paxos_errc::inconsistent_storage) ||
           e == make_error_code(storage_errc::crc_mismatch) || e == make_error_code(storage_errc::malformed_record) ||
           e == make_error_code(storage_errc::missing_entry) || e == make_error_code(storage_errc::missing_meta_record);
}

constexpr bool meta_values_are_consistent(const meta_record& m) noexcept
{
    return !(m.log_len > 0 && m.accepted_round == ballot{}) && m.accepted_round <= m.promise &&
           m.decided_idx <= m.log_len;
}

struct peer_seen {
    ballot n;
    std::uint64_t accepted_idx;
};

struct peer_step_back {
    node_id pid;
    peer_seen seen;
    peer_seen received;
};

enum class role : std::uint8_t { follower, leader };

enum class phase : std::uint8_t { prepare, accept, recover, none };

struct promise_meta_data {
    ballot n_accepted;
    std::uint64_t accepted_idx;
    std::uint64_t decided_idx;
    node_id pid;
    std::uint32_t decided_chain_crc;
};

struct not_promised {};

struct promised_higher {};

using promise_state = std::variant<not_promised, promise_meta_data, promised_higher>;

struct taken_log {
    std::uint64_t sync_idx;
    std::vector<entry> suffix;
    std::uint64_t decided_idx;
    std::uint32_t decided_chain_crc;

    std::uint64_t log_len() const { return sync_idx + suffix.size(); }
};

struct leader_state {
    ballot n_leader;
    std::vector<node_id> nodes;
    std::vector<promise_state> promises_meta;
    std::vector<sequence_number> follower_seq_nums;
    std::vector<std::uint64_t> accepted_indexes;
    promise_meta_data max_promise_meta;
    std::optional<konsens::log_sync> max_promise_sync;
    bool accepted_stop_sign;
    std::optional<taken_log> taken{};
    std::uint64_t first_changed_idx{std::numeric_limits<std::uint64_t>::max()};

    static leader_state with(ballot n_leader, std::span<const node_id> nodes)
    {
        return {n_leader,
                std::vector<node_id>(nodes.begin(), nodes.end()),
                std::vector<promise_state>(nodes.size(), not_promised{}),
                std::vector<sequence_number>(nodes.size(), sequence_number{}),
                std::vector<std::uint64_t>(nodes.size(), 0),
                promise_meta_data{},
                std::nullopt,
                false};
    }

    std::size_t pid_to_idx(node_id pid) const
    {
        const auto it = std::ranges::find(nodes, pid);
        if (it == nodes.end()) [[unlikely]] {
            throw std::logic_error("konsens: leader_state has no node with this pid");
        }
        return static_cast<std::size_t>(std::ranges::distance(nodes.begin(), it));
    }

    std::size_t majority() const { return nodes.size() / 2 + 1; }

    void increment_seq_num_session(node_id pid)
    {
        auto& s = follower_seq_nums[pid_to_idx(pid)];
        s.session += 1;
        s.counter = 0;
    }

    sequence_number next_seq_num(node_id pid)
    {
        auto& s = follower_seq_nums[pid_to_idx(pid)];
        s.counter += 1;
        return s;
    }

    bool set_promise(const promise_meta_data& meta, const std::optional<konsens::log_sync>& sync, bool check_max_prom)
    {
        const auto& mm = max_promise_meta;
        const bool greater = meta.n_accepted > mm.n_accepted ||
                             (meta.n_accepted == mm.n_accepted && meta.accepted_idx > mm.accepted_idx);
        if (check_max_prom && greater) {
            max_promise_meta = meta;
            max_promise_sync = sync;
        }
        promises_meta[pid_to_idx(meta.pid)] = meta;
        const auto promised = std::ranges::count_if(
            promises_meta, [](const promise_state& p) { return std::holds_alternative<promise_meta_data>(p); });
        return static_cast<std::size_t>(promised) >= majority();
    }

    void reset_promise(node_id pid) { promises_meta[pid_to_idx(pid)] = not_promised{}; }

    void lost_promise(node_id pid) { promises_meta[pid_to_idx(pid)] = promised_higher{}; }

    std::optional<konsens::log_sync> take_max_promise_sync() { return std::exchange(max_promise_sync, std::nullopt); }

    const promise_meta_data& get_promise_meta(node_id pid) const
    {
        const auto* m = std::get_if<promise_meta_data>(&promises_meta[pid_to_idx(pid)]);
        if (m == nullptr) [[unlikely]] {
            throw std::logic_error("konsens: no promise meta data for a promised follower");
        }
        return *m;
    }

    auto get_promised_followers() const
    {
        return std::views::iota(std::size_t{0}, nodes.size()) | std::views::filter([this](std::size_t i) {
                   return std::holds_alternative<promise_meta_data>(promises_meta[i]) && nodes[i] != n_leader.pid;
               }) |
               std::views::transform([this](std::size_t i) { return nodes[i]; });
    }

    auto get_preparable_peers(node_id own_pid) const
    {
        return std::views::iota(std::size_t{0}, nodes.size()) | std::views::filter([this, own_pid](std::size_t i) {
                   return std::holds_alternative<not_promised>(promises_meta[i]) && nodes[i] != own_pid;
               }) |
               std::views::transform([this](std::size_t i) { return nodes[i]; });
    }

    void set_accepted_idx(node_id pid, std::uint64_t idx) { accepted_indexes[pid_to_idx(pid)] = idx; }

    bool is_chosen(std::uint64_t idx) const
    {
        const auto n = std::ranges::count_if(accepted_indexes, [idx](std::uint64_t a) { return a >= idx; });
        return static_cast<std::size_t>(n) >= majority();
    }

    std::uint64_t highest_chosen_idx() const
    {
        auto sorted = accepted_indexes;
        const auto at = std::next(sorted.begin(), static_cast<std::ptrdiff_t>(majority() - 1));
        std::ranges::nth_element(sorted, at, std::ranges::greater{});
        return *at;
    }
};

struct pending_read {
    node_id node;
    std::uint64_t read;
};

struct read_round {
    ballot n;
    std::uint64_t id;
    std::vector<pending_read> reads;
    std::uint64_t read_index;
    std::vector<node_id> acks;
};

struct indexed_read {
    std::uint64_t read;
    std::uint64_t read_index;
    konsens::ballot ballot;
};

inline constexpr std::size_t max_build_entries = 256;

enum class start_kind : std::uint8_t { first, clean, build, retired };

struct sequence_paxos_config {
    std::uint32_t configuration_id;
    node_id pid;
    std::vector<node_id> peers;
    std::vector<node_id> build_from{};
    start_kind start{};
    std::uint64_t verified_idx{};
    bool sync_by_the_caller{};
};

struct build_state {
    std::vector<node_id> sources;
    std::size_t source;
    bool answered;
};

struct decided_update {
    std::uint64_t decided_idx;
    bool flushed;
};

constexpr decided_update update_decided(std::uint64_t decided_idx, std::uint64_t accepted_idx,
                                        std::uint64_t new_decided_idx)
{
    if (new_decided_idx <= decided_idx) {
        return {decided_idx, false};
    }
    if (new_decided_idx > accepted_idx) {
        return {std::min(new_decided_idx, accepted_idx), true};
    }
    return {new_decided_idx, false};
}

struct high_water_mark {
    ballot promise;
    ballot accepted_round;
    std::uint64_t log_len;
    std::uint64_t decided_idx;
};

constexpr high_water_mark mark_of(const meta_record& m) noexcept
{
    return {m.promise, m.accepted_round, m.log_len, m.decided_idx};
}

constexpr bool is_below_mark(const high_water_mark& m, const high_water_mark& h) noexcept
{
    return m.promise < h.promise || m.decided_idx < h.decided_idx ||
           std::pair{m.accepted_round, m.log_len} < std::pair{h.accepted_round, h.log_len};
}

constexpr high_water_mark raise_mark(const high_water_mark& h, const high_water_mark& m) noexcept
{
    const bool pair_raised = std::pair{h.accepted_round, h.log_len} < std::pair{m.accepted_round, m.log_len};
    return {std::max(h.promise, m.promise), pair_raised ? m.accepted_round : h.accepted_round,
            pair_raised ? m.log_len : h.log_len, std::max(h.decided_idx, m.decided_idx)};
}

inline std::expected<cluster_config, std::error_code> next_configuration_of(const entry& e)
{
    if (!entry_is_a_stop_sign(e)) {
        return std::unexpected(make_error_code(storage_errc::malformed_record));
    }
    return decode_cluster_config(e.payload);
}

template <storage S>
class sequence_paxos {
public:
    using result = std::expected<void, std::error_code>;

    static std::expected<sequence_paxos, std::error_code> with(const sequence_paxos_config& config, S internal_storage,
                                                               const std::uint64_t nonce)
    {
        validate_cluster(config.pid, config.peers);
        if (config.configuration_id == 0) [[unlikely]] {
            throw std::logic_error("konsens: configuration_id must not be 0");
        }
        sequence_paxos sp(config, std::move(internal_storage));
        const auto m = sp.read_meta_between_steps();
        if (!m && error_is_a_damage(m.error())) [[unlikely]] {
            sp.nonce_ = nonce;
            sp.error_ = m.error();
            return sp;
        }
        if (!m) [[unlikely]] {
            return std::unexpected(m.error());
        }
        if (m->promise != ballot{} && m->promise.config_id != config.configuration_id) [[unlikely]] {
            throw std::logic_error("konsens: the configuration_id differs from the configuration of the store");
        }
        sp.mark_ = mark_of(*m);
        sp.synced_len_ = m->log_len;
        sp.leader_state_ = leader_state::with(m->promise, sp.nodes_);
        sp.nonce_ = nonce;
        const auto t = sp.internal_storage_.begin_read_only();
        if (!t) [[unlikely]] {
            return std::unexpected(t.error());
        }
        sp.verified_idx_ = find_the_verified_idx(*t, m->decided_idx, m->log_len).value_or(m->decided_idx);
        const auto first = t->get_config_first_idx();
        if (!first && error_is_a_damage(first.error())) [[unlikely]] {
            sp.error_ = first.error();
            return sp;
        }
        if (!first) [[unlikely]] {
            return std::unexpected(first.error());
        }
        if ((!*first && config.configuration_id > 1) != (config.start == start_kind::build)) [[unlikely]] {
            throw std::logic_error("konsens: a build start needs a store of a later configuration without its first index");
        }
        if (config.start == start_kind::build) {
            if (config.build_from.empty()) [[unlikely]] {
                throw std::logic_error("konsens: a node of a later configuration needs build_from for its build");
            }
            sp.build_ = build_state{config.build_from, 0, false};
            sp.send_build_request(m->log_len);
            return sp;
        }
        sp.config_first_idx_ = first->value_or(0);
        if (auto r = check_the_start_of_the_configuration(*t, config.configuration_id, sp.config_first_idx_, *m);
            !r && error_is_a_damage(r.error())) [[unlikely]] {
            sp.error_ = r.error();
            return sp;
        } else if (!r) [[unlikely]] {
            return std::unexpected(r.error());
        }
        switch (config.start) {
        case start_kind::first:
            if (m->promise != ballot{}) [[unlikely]] {
                throw std::logic_error("konsens: a first start needs a store without a promise");
            }
            break;
        case start_kind::clean:
            sp.state_ = {role::follower, phase::recover};
            sp.verified_idx_ = std::max(sp.verified_idx_, std::min(config.verified_idx, m->log_len));
            sp.send_preparereq_to_all_peers(m->promise);
            break;
        case start_kind::retired:
            sp.retired_ = true;
            break;
        case start_kind::build:
            break;
        }
        return sp;
    }

    result handle(paxos_message m)
    {
        incoming_.push_back(std::move(m));
        const auto& kept = incoming_.back();
        return std::visit([this, &kept](const auto& msg) -> result { return this->handle(kept.from, kept.to, msg); },
                          kept.msg);
    }

    [[nodiscard]] result end_step()
    {
        throw_if_a_write_did_not_finish();
        if (step_ended_) {
            return {};
        }
        if (error_) [[unlikely]] {
            roll_back_the_transaction();
            drop_incoming_and_outgoing();
            return error_unless_retired();
        }
        if (auto r = begin_step(); !r) [[unlikely]] {
            drop_incoming_and_outgoing();
            return r;
        }
        if (state_ == std::pair{role::leader, phase::accept}) {
            cut_the_buffer_after_a_stop_sign();
            if (auto r = accept_entries_leader(buffered_proposals_); !r) [[unlikely]] {
                drop_incoming_and_outgoing();
                return r;
            }
            buffered_proposals_.clear();
        }
        step_ended_ = true;
        return {};
    }

    [[nodiscard]] result commit()
    {
        if (auto r = commit_phase_one(); !r) [[unlikely]] {
            return r;
        }
        commit_phase_two();
        return {};
    }

    [[nodiscard]] result commit_phase_one()
    {
        const auto ended = end_step();
        step_ended_ = false;
        if (!ended) [[unlikely]] {
            return ended;
        }
        if (error_) [[unlikely]] {
            roll_back_the_transaction();
            drop_incoming_and_outgoing();
            return error_unless_retired();
        }
        if (!txn_) {
            return {};
        }
        const auto m = read_checked_meta(*txn_);
        if (!m) [[unlikely]] {
            const auto stopped = stop_voting(m.error());
            drop_incoming_and_outgoing();
            return stopped;
        }
        incoming_.clear();
        step_open_ = false;
        if (txn_->has_writes() && a_sync_is_out()) {
            the_transaction_holds_ended_steps_ = true;
            return {};
        }
        auto txn = std::move(*txn_);
        txn_.reset();
        open_log_.reset();
        the_transaction_holds_ended_steps_ = false;
        if (!txn.has_writes()) {
            mark_ = raise_mark(mark_, mark_of(*m));
            return {};
        }
        pending_commit_.emplace(std::move(txn), *m);
        return {};
    }

    void commit_phase_two()
    {
        if (!pending_commit_) {
            return;
        }
        auto u = std::move(*pending_commit_);
        pending_commit_.reset();
        write_unfinished_ = true;
        const bool log_writes = u.txn.has_log_writes();
        const bool synced = log_writes && !sync_by_the_caller_;
        if (auto c = synced ? std::move(u.txn).commit() : std::move(u.txn).commit_without_a_sync(); !c) [[unlikely]] {
            throw write_error(c.error(), "konsens: the storage failed to commit the step");
        }
        write_unfinished_ = false;
        a_commit_waits_for_its_sync_ = !synced;
        mark_ = raise_mark(mark_, mark_of(u.meta));
        if (sync_by_the_caller_ && log_writes) {
            last_sync_number_ += 1;
            unsynced_steps_.push_back({last_sync_number_, u.meta.log_len});
        }
    }

    void commit_the_open_transaction()
    {
        throw_if_a_write_did_not_finish();
        throw_if_a_step_is_open();
        if (!txn_) {
            return;
        }
        const auto m = read_checked_meta(*txn_);
        if (!m) [[unlikely]] {
            throw write_error(m.error(), "konsens: the storage failed to read the meta for the commit of the open transaction");
        }
        pending_commit_.emplace(std::move(*txn_), *m);
        txn_.reset();
        open_log_.reset();
        the_transaction_holds_ended_steps_ = false;
        commit_phase_two();
    }

    bool a_sync_is_out() const { return last_sync_number_ > synced_sync_number_; }
    bool the_transaction_holds_ended_steps() const { return the_transaction_holds_ended_steps_; }
    std::uint64_t get_last_sync_number() const { return last_sync_number_; }
    std::uint64_t get_synced_sync_number() const { return synced_sync_number_; }
    std::uint64_t get_synced_len() const { return synced_len_; }

    result the_log_is_synced(const std::uint64_t sync_number)
    {
        throw_if_a_write_did_not_finish();
        synced_sync_number_ = sync_number;
        const auto covered = std::ranges::find_if(
            unsynced_steps_, [sync_number](const unsynced_step& u) { return u.sync_number > sync_number; });
        if (covered != unsynced_steps_.begin()) {
            synced_len_ = std::max(synced_len_, std::prev(covered)->log_len);
        }
        unsynced_steps_.erase(unsynced_steps_.begin(), covered);
        if (error_ || retired_ || build_ || state_ != std::pair{role::leader, phase::accept}) {
            return {};
        }
        if (auto r = begin_step(); !r) [[unlikely]] {
            return r;
        }
        const auto m = read_meta();
        if (!m) [[unlikely]] {
            return stop_voting(m.error());
        }
        if (leader_state_.n_leader != m->promise) {
            return {};
        }
        const auto own = std::max(bound_by_the_synced_len(bound_by_the_verified_idx(m->log_len)),
                                  leader_state_.accepted_indexes.at(leader_state_.pid_to_idx(pid_)));
        leader_state_.set_accepted_idx(pid_, own);
        const auto chosen = leader_state_.highest_chosen_idx();
        if (chosen <= m->decided_idx) {
            return {};
        }
        const auto chain = read_prefix_chain_crc(chosen);
        if (!chain) [[unlikely]] {
            return stop_voting(chain.error());
        }
        return decide_if_chosen(chosen, *chain, *m);
    }

    void the_sync_failed()
    {
        internal_storage_.the_sync_failed();
        write_unfinished_ = true;
        roll_back_the_transaction();
        drop_incoming_and_outgoing();
    }

    int wal_file_descriptor() const { return internal_storage_.wal_file_descriptor(); }

    void sync_the_last_commit()
    {
        if (!a_commit_waits_for_its_sync_) {
            return;
        }
        auto t = internal_storage_.begin();
        if (!t) [[unlikely]] {
            throw write_error(t.error(), "konsens: the storage failed to begin the sync of the last commit");
        }
        const auto m = t->get_meta();
        if (!m) [[unlikely]] {
            throw write_error(m.error(), "konsens: the storage failed to read the meta for the sync of the last commit");
        }
        if (auto w = t->set_decided_index(m->decided_idx); !w) [[unlikely]] {
            throw write_error(w.error(), "konsens: the storage failed to write the meta for the sync of the last commit");
        }
        if (auto c = std::move(*t).commit(); !c) [[unlikely]] {
            throw write_error(c.error(), "konsens: the storage failed to sync the last commit");
        }
        a_commit_waits_for_its_sync_ = false;
    }

    std::vector<paxos_message>
    take_the_messages_before_the_sync(const std::span<const node_id> peers_with_held_messages)
    {
        std::vector<paxos_message> early;
        std::vector<paxos_message> held;
        std::vector<node_id> held_peers(peers_with_held_messages.begin(), peers_with_held_messages.end());
        for (auto& m : outgoing_) {
            const bool may_leave = std::holds_alternative<accept_decide>(m.msg) || std::holds_alternative<decide>(m.msg);
            if (may_leave && !std::ranges::contains(held_peers, m.to)) {
                early.push_back(std::move(m));
            } else {
                held_peers.push_back(m.to);
                held.push_back(std::move(m));
            }
        }
        outgoing_ = std::move(held);
        return early;
    }

private:
    template <storage, state_machine>
    friend class node;

    template <typename T>
        requires requires(const T& msg) { paxos_msg{std::in_place_type<T>, msg}; }
    result handle(node_id from, node_id to, const T& msg)
    {
        if (retired_ && (error_ || !std::is_same_v<T, build_request>)) {
            return {};
        }
        if (error_) [[unlikely]] {
            return std::unexpected(*error_);
        }
        if constexpr (std::is_same_v<T, build_request>) {
            if (to != pid_ || from == pid_ || build_) {
                return {};
            }
            if (auto r = begin_step(); !r) [[unlikely]] {
                return r;
            }
            return handle_build_request(msg, from);
        }
        if (build_) {
            if constexpr (std::is_same_v<T, build_response> || std::is_same_v<T, build_refused>) {
                if (to != pid_ || from != build_->sources[build_->source] || msg.nonce != nonce_) {
                    return {};
                }
                if (auto r = begin_step(); !r) [[unlikely]] {
                    return r;
                }
                build_->answered = true;
                if constexpr (std::is_same_v<T, build_response>) {
                    return handle_build_response(msg);
                } else {
                    take_the_next_build_source();
                    return {};
                }
            }
            return {};
        }
        if (to != pid_ || from == pid_ || !std::ranges::contains(peers_, from)) [[unlikely]] {
            return {};
        }
        if constexpr (carries_a_ballot_of_the_sender<T>) {
            if (!raise_peer_seen(from, msg)) [[unlikely]] {
                return {};
            }
        }
        if (auto r = begin_step(); !r) [[unlikely]] {
            return r;
        }
        if constexpr (std::is_same_v<T, prepare_req>) {
            return handle_preparereq(msg, from);
        } else if constexpr (std::is_same_v<T, prepare>) {
            return handle_prepare(msg, from);
        } else if constexpr (std::is_same_v<T, konsens::promise>) {
            if (state_ == std::pair{role::leader, phase::prepare}) {
                return handle_promise_prepare(msg, from);
            }
            if (state_ == std::pair{role::leader, phase::accept}) {
                return handle_promise_accept(msg, from);
            }
            return {};
        } else if constexpr (std::is_same_v<T, accept_sync>) {
            return handle_acceptsync(msg, from);
        } else if constexpr (std::is_same_v<T, accept_decide>) {
            return handle_acceptdecide(msg);
        } else if constexpr (std::is_same_v<T, accepted>) {
            return handle_accepted(msg, from);
        } else if constexpr (std::is_same_v<T, decide>) {
            return handle_decide(msg);
        } else if constexpr (std::is_same_v<T, not_accepted>) {
            handle_notaccepted(msg, from);
            return {};
        } else if constexpr (std::is_same_v<T, read_index_request>) {
            return handle_read_index_request(msg, from);
        } else if constexpr (std::is_same_v<T, heartbeat>) {
            return handle_heartbeat(msg, from);
        } else if constexpr (std::is_same_v<T, heartbeat_ack>) {
            return handle_heartbeat_ack(msg, from);
        } else if constexpr (std::is_same_v<T, read_index_reply>) {
            return handle_read_index_reply(msg);
        } else if constexpr (std::is_same_v<T, answer> || std::is_same_v<T, try_again> || std::is_same_v<T, refused> ||
                             std::is_same_v<T, work> || std::is_same_v<T, build_request> ||
                             std::is_same_v<T, build_response> || std::is_same_v<T, build_refused>) {
            return {};
        } else {
            static_assert(std::is_same_v<T, proposal_forward>);
            return handle_forwarded_proposal(msg.entries);
        }
    }

public:
    result handle_leader(ballot n)
    {
        if (error_) [[unlikely]] {
            return std::unexpected(*error_);
        }
        if (auto r = begin_step(); !r) [[unlikely]] {
            return r;
        }
        const auto m = read_meta();
        if (!m) [[unlikely]] {
            return stop_voting(m.error());
        }
        if (n <= leader_state_.n_leader || n <= m->promise) {
            return {};
        }
        if (n.pid != pid_) {
            state_.first = role::follower;
            return {};
        }
        const auto decided_chain = read_prefix_chain_crc(m->decided_idx);
        if (!decided_chain) [[unlikely]] {
            return stop_voting(decided_chain.error());
        }
        write_or_throw(txn_->set_promise(n));
        leader_state_ = leader_state::with(n, nodes_);
        leader_state_.set_promise(
            {m->accepted_round, bound_by_the_verified_idx(m->log_len), m->decided_idx, pid_, *decided_chain},
            std::nullopt, true);
        state_ = {role::leader, phase::prepare};
        for (const auto peer : peers_) {
            send_prepare(peer, *m);
        }
        return {};
    }

    result append(entry e)
    {
        if (error_) [[unlikely]] {
            return std::unexpected(*error_);
        }
        if (auto r = refuse_if_building(); !r) [[unlikely]] {
            return r;
        }
        if (state_ == std::pair{role::leader, phase::prepare} || state_ == std::pair{role::leader, phase::accept}) {
            if (holds_a_stop_sign()) [[unlikely]] {
                return std::unexpected(make_error_code(client_errc::reconfiguring));
            }
            buffered_proposals_.push_back(std::move(e));
            return {};
        }
        if (auto r = begin_step(); !r) [[unlikely]] {
            return r;
        }
        const auto m = read_meta();
        if (!m) [[unlikely]] {
            return stop_voting(m.error());
        }
        std::vector<entry> entries;
        entries.push_back(std::move(e));
        forward_proposals(std::move(entries), *m);
        return {};
    }

    result resend_message_timeout()
    {
        if (retired_) {
            return {};
        }
        if (error_) [[unlikely]] {
            return std::unexpected(*error_);
        }
        if (build_) {
            return ask_again_for_the_build();
        }
        if (auto r = begin_step(); !r) [[unlikely]] {
            return r;
        }
        const auto m = read_meta();
        if (!m) [[unlikely]] {
            return stop_voting(m.error());
        }
        if (state_.first == role::leader) {
            if (state_.second == phase::prepare || state_.second == phase::accept) {
                for (const auto peer : leader_state_.get_preparable_peers(pid_)) {
                    send_prepare(peer, *m);
                }
            }
            if (state_.second == phase::accept) {
                const auto decided_chain = read_prefix_chain_crc(m->decided_idx);
                if (!decided_chain) [[unlikely]] {
                    return stop_voting(decided_chain.error());
                }
                for (const auto f : leader_state_.get_promised_followers()) {
                    send_decide(f, m->decided_idx, *decided_chain);
                }
            }
            return {};
        }
        if (state_.second == phase::prepare) {
            if (answered_prepare_) {
                auto p = promise_for(*answered_prepare_);
                if (!p) [[unlikely]] {
                    return stop_voting(p.error());
                }
                outgoing_.push_back({pid_, answered_prepare_->n.pid, std::move(*p)});
            } else {
                state_ = {role::follower, phase::recover};
                send_preparereq_to_all_peers(m->promise);
            }
        } else if (state_.second == phase::recover) {
            send_preparereq_to_all_peers(m->promise);
        }
        return {};
    }

    bool holds_a_stop_sign() const
    {
        return state_.first == role::leader &&
               (leader_state_.accepted_stop_sign || std::ranges::any_of(buffered_proposals_, entry_is_a_stop_sign));
    }

    result error_unless_retired() const
    {
        if (retired_) {
            return {};
        }
        return std::unexpected(*error_);
    }

    result refuse_if_retired() const
    {
        if (retired_) [[unlikely]] {
            return std::unexpected(make_error_code(client_errc::retired));
        }
        return {};
    }

    result refuse_if_building() const
    {
        if (build_) [[unlikely]] {
            return std::unexpected(make_error_code(client_errc::building));
        }
        return {};
    }

    std::vector<paxos_message> take_outgoing_messages()
    {
        throw_if_a_write_did_not_finish();
        throw_if_a_step_is_open();
        return std::exchange(outgoing_, {});
    }

    result start_read(const std::uint64_t read)
    {
        if (error_) [[unlikely]] {
            return std::unexpected(*error_);
        }
        if (state_.first == role::leader) {
            if (auto r = begin_step(); !r) [[unlikely]] {
                return r;
            }
            read_pending_.push_back({pid_, read});
            return start_read_round();
        }
        const auto m = read_meta_between_steps();
        if (!m) [[unlikely]] {
            return stop_voting(m.error());
        }
        const node_id leader = m->promise.pid;
        if (leader == 0 || leader == pid_) [[unlikely]] {
            return std::unexpected(make_error_code(client_errc::retry));
        }
        if (auto r = begin_step(); !r) [[unlikely]] {
            return r;
        }
        outgoing_.push_back({pid_, leader, read_index_request{read}});
        return {};
    }

    void resend_heartbeats()
    {
        if (error_ || state_ != std::pair{role::leader, phase::accept} || read_round_.reads.empty() ||
            read_round_.n != leader_state_.n_leader) {
            return;
        }
        for (const auto peer : peers_) {
            if (!std::ranges::contains(read_round_.acks, peer)) {
                outgoing_.push_back({pid_, peer, heartbeat{read_round_.n, read_round_.id}});
            }
        }
    }

    std::vector<indexed_read> take_indexed_reads() { return std::exchange(indexed_reads_, {}); }

    std::expected<std::vector<entry>, std::error_code> read_decided_suffix(std::uint64_t from_idx) const
    {
        throw_if_a_step_is_open();
        const auto t = internal_storage_.begin_read_only();
        if (!t) [[unlikely]] {
            return std::unexpected(t.error());
        }
        const auto m = read_checked_meta(*t);
        if (!m) [[unlikely]] {
            return std::unexpected(m.error());
        }
        return read_range(*t, std::min(from_idx, m->decided_idx), m->decided_idx);
    }

    std::pair<role, phase> get_state() const { return state_; }
    std::expected<ballot, std::error_code> get_promise() const
    {
        return values_between_steps().transform([](const high_water_mark& m) { return m.promise; });
    }
    std::expected<ballot, std::error_code> get_accepted_round() const
    {
        return values_between_steps().transform([](const high_water_mark& m) { return m.accepted_round; });
    }
    std::expected<std::uint64_t, std::error_code> get_decided_idx() const
    {
        return values_between_steps().transform([](const high_water_mark& m) { return m.decided_idx; });
    }
    std::expected<std::uint64_t, std::error_code> get_accepted_idx() const
    {
        return values_between_steps().transform([](const high_water_mark& m) { return m.log_len; });
    }
    std::expected<std::uint32_t, std::error_code> get_prefix_chain_crc(const std::uint64_t length) const
    {
        throw_if_a_step_is_open();
        const auto t = internal_storage_.begin_read_only();
        if (!t) [[unlikely]] {
            return std::unexpected(t.error());
        }
        return read_prefix_chain_crc_in(*t, length);
    }

    result check_applied_chain_crc(const std::uint64_t applied_idx, const std::uint32_t applied_chain_crc)
    {
        throw_if_a_step_is_open();
        const auto t = internal_storage_.begin_read_only();
        if (!t) [[unlikely]] {
            return stop_voting(t.error());
        }
        const auto m = read_checked_meta(*t);
        if (!m) [[unlikely]] {
            return stop_voting(m.error());
        }
        if (applied_idx > m->log_len) [[unlikely]] {
            return stop_voting(make_error_code(paxos_errc::state_differs_from_log));
        }
        const auto own = read_prefix_chain_crc_in(*t, applied_idx);
        if (!own) [[unlikely]] {
            return stop_voting(own.error());
        }
        if (*own != applied_chain_crc) [[unlikely]] {
            hardware_fault_ = konsens::hardware_fault{applied_idx, pid_, *own, pid_, applied_chain_crc};
            return stop_voting(make_error_code(paxos_errc::state_differs_from_log));
        }
        return {};
    }
    const leader_state& get_leader_state() const { return leader_state_; }
    const konsens::read_round& get_read_round() const { return read_round_; }
    const S& get_internal_storage() const
    {
        throw_if_a_step_is_open();
        return internal_storage_;
    }

    std::expected<bool, std::error_code> log_or_buffered_proposals_contain(const std::uint64_t from_idx, const entry& e)
    {
        if (std::ranges::contains(buffered_proposals_, e)) {
            return true;
        }
        if (auto r = begin_step(); !r) [[unlikely]] {
            return std::unexpected(r.error());
        }
        const auto m = read_meta();
        if (!m) [[unlikely]] {
            return stop_voting(m.error());
        }
        const auto log = read_the_open_log(from_idx, m->log_len);
        if (!log) [[unlikely]] {
            return stop_voting(log.error());
        }
        return std::ranges::contains(log->entries, e);
    }
    node_id get_pid() const { return pid_; }
    std::uint64_t get_verified_idx() const { return verified_idx_; }
    bool votes() const { return !error_ && !build_ && !retired_; }
    bool retired() const { return retired_; }
    bool building() const { return build_.has_value(); }
    std::uint64_t get_config_first_idx() const { return config_first_idx_; }

    result scan_the_whole_log()
    {
        const auto chains = read_verified_chain_crcs();
        if (!chains) [[unlikely]] {
            return std::unexpected(chains.error());
        }
        if (chains->size() < mark_.log_len) [[unlikely]] {
            return stop_voting(make_error_code(storage_errc::crc_mismatch));
        }
        return {};
    }

    std::optional<std::error_code> get_error() const { return error_; }
    std::optional<konsens::hardware_fault> get_hardware_fault() const { return hardware_fault_; }
    std::vector<peer_step_back> take_peer_step_backs() { return std::exchange(peer_step_backs_, {}); }

    result reconnected(const node_id pid)
    {
        if (retired_) {
            return {};
        }
        if (error_) [[unlikely]] {
            return std::unexpected(*error_);
        }
        if (pid != pid_) {
            peers_seen_.at(node_index_of(pid)) = {};
        }
        if (pid == pid_ || retired_) {
            return {};
        }
        if (auto r = begin_step(); !r) [[unlikely]] {
            return r;
        }
        const auto m = read_meta();
        if (!m) [[unlikely]] {
            return stop_voting(m.error());
        }
        reconnected(pid, *m);
        return {};
    }

private:
    sequence_paxos(const sequence_paxos_config& config, S internal_storage)
        : configuration_id_(config.configuration_id), pid_(config.pid), peers_(config.peers),
          internal_storage_(std::move(internal_storage)), sync_by_the_caller_(config.sync_by_the_caller)
    {
        nodes_ = peers_;
        peers_seen_.assign(peers_.size() + 1, peer_seen{});
        nodes_.push_back(pid_);
        std::ranges::sort(nodes_);
    }

    template <typename T>
    static std::expected<std::uint64_t, std::error_code> find_the_verified_idx(const T& t, const std::uint64_t from,
                                                                             const std::uint64_t log_len)
    {
        const auto first = std::min(from, log_len);
        const auto entries = read_range(t, first, log_len);
        if (!entries) [[unlikely]] {
            return std::unexpected(entries.error());
        }
        const auto done = std::ranges::find_if(*entries, entry_is_a_request);
        return first + static_cast<std::uint64_t>(std::distance(entries->begin(), done));
    }

    template <typename T>
    static result check_the_start_of_the_configuration(const T& t, const std::uint32_t configuration_id,
                                                       const std::uint64_t config_first_idx, const meta_record& m)
    {
        if (configuration_id == 1 && config_first_idx == 0) {
            return {};
        }
        if (configuration_id == 1 || config_first_idx == 0 || m.decided_idx < config_first_idx) [[unlikely]] {
            throw std::logic_error("konsens: the store holds no decided StopSign that starts this configuration");
        }
        const auto last = read_range(t, config_first_idx - 1, config_first_idx);
        if (!last) [[unlikely]] {
            return std::unexpected(last.error());
        }
        const auto next = next_configuration_of(last->front());
        if (!next || next->configuration_id != configuration_id) [[unlikely]] {
            throw std::logic_error("konsens: the store holds no decided StopSign that starts this configuration");
        }
        return {};
    }

    result extend_the_verified_idx(const std::uint64_t from)
    {
        const auto m = read_meta();
        if (!m) [[unlikely]] {
            return stop_voting(m.error());
        }
        const auto first = std::min(from, m->log_len);
        const auto log = read_the_open_log(first, m->log_len);
        if (!log) [[unlikely]] {
            return stop_voting(log.error());
        }
        const auto done = std::ranges::find_if(log->entries, entry_is_a_request);
        verified_idx_ = first + static_cast<std::uint64_t>(std::distance(log->entries.begin(), done));
        return {};
    }

    std::uint64_t bound_by_the_verified_idx(const std::uint64_t accepted_idx) const
    {
        return std::min(accepted_idx, verified_idx_);
    }

    std::uint64_t bound_by_the_synced_len(const std::uint64_t accepted_idx) const
    {
        return sync_by_the_caller_ ? std::min(accepted_idx, synced_len_) : accepted_idx;
    }

    void lower_the_synced_len(const std::uint64_t first_changed_idx)
    {
        synced_len_ = std::min(synced_len_, first_changed_idx);
        for (auto& u : unsynced_steps_) {
            u.log_len = std::min(u.log_len, first_changed_idx);
        }
    }

    void lower_the_synced_len_if_the_round_changes(const ballot before, const ballot after)
    {
        if (before != after) {
            lower_the_synced_len(0);
        }
    }

    std::expected<accepted, std::error_code> accepted_up_to_the_verified_idx(const ballot n) const
    {
        const auto m = read_meta();
        if (!m) [[unlikely]] {
            return std::unexpected(m.error());
        }
        const auto bound = bound_by_the_verified_idx(m->log_len);
        const auto chain = read_prefix_chain_crc(bound);
        if (!chain) [[unlikely]] {
            return std::unexpected(chain.error());
        }
        return accepted{n, bound, *chain};
    }

    struct open_log_view {
        std::shared_ptr<const std::vector<entry>> owner;
        std::span<const entry> entries;
    };

    std::expected<open_log_view, std::error_code> read_the_open_log(const std::uint64_t from, const std::uint64_t log_len)
    {
        const auto first = std::min(from, log_len);
        const auto writes = txn_->count_of_entry_writes();
        if (!open_log_ || open_log_->entry_writes != writes || first < open_log_->from ||
            open_log_->from + open_log_->entries->size() != log_len) {
            auto entries = read_range(*txn_, first, log_len);
            if (!entries) [[unlikely]] {
                return std::unexpected(entries.error());
            }
            open_log_.emplace(first, writes, std::make_shared<const std::vector<entry>>(std::move(*entries)));
        }
        const std::span<const entry> all{*open_log_->entries};
        return open_log_view{open_log_->entries, all.subspan(first - open_log_->from)};
    }

    template <typename T>
    static std::expected<std::vector<entry>, std::error_code> read_range(const T& t, const std::uint64_t from,
                                                                         const std::uint64_t to)
    {
        auto entries = t.get_entries(from, to);
        if (entries && entries->size() != to - from) [[unlikely]] {
            return std::unexpected(make_error_code(storage_errc::missing_entry));
        }
        return entries;
    }

    template <typename T>
    std::expected<meta_record, std::error_code> read_checked_meta(const T& t) const
    {
        const auto m = t.get_meta();
        if (!m) [[unlikely]] {
            return m;
        }
        const auto last = t.get_last_entry();
        if (!last) [[unlikely]] {
            return std::unexpected(last.error());
        }
        const auto log_len = last->transform([](const last_entry& l) { return l.index + 1; }).value_or(0);
        const auto chain_crc = last->transform([](const last_entry& l) { return l.chain_crc; }).value_or(0);
        if (log_len != m->log_len || chain_crc != m->last_chain_crc) [[unlikely]] {
            return std::unexpected(make_error_code(storage_errc::missing_entry));
        }
        if (!meta_values_are_consistent(*m) || is_below_mark(mark_of(*m), mark_)) [[unlikely]] {
            return std::unexpected(make_error_code(paxos_errc::inconsistent_storage));
        }
        return m;
    }

    std::expected<meta_record, std::error_code> read_meta_between_steps() const
    {
        if (txn_) {
            return read_checked_meta(*txn_);
        }
        const auto t = internal_storage_.begin_read_only();
        if (!t) [[unlikely]] {
            return std::unexpected(t.error());
        }
        return read_checked_meta(*t);
    }

    std::expected<meta_record, std::error_code> read_meta() const { return txn_->get_meta(); }

    std::expected<high_water_mark, std::error_code> values_between_steps() const
    {
        if (txn_) {
            return txn_->get_meta().transform(mark_of);
        }
        if (!error_ && !write_unfinished_) {
            return mark_;
        }
        return read_meta_between_steps().transform(mark_of);
    }

    result begin_step()
    {
        throw_if_a_write_did_not_finish();
        if (txn_) {
            step_open_ = true;
            return {};
        }
        auto t = internal_storage_.begin();
        if (!t) [[unlikely]] {
            return stop_voting(t.error());
        }
        txn_.emplace(std::move(*t));
        open_log_.reset();
        step_open_ = true;
        const auto m = read_checked_meta(*txn_);
        if (!m) [[unlikely]] {
            return stop_voting(m.error());
        }
        return {};
    }

    void write_or_throw(const std::expected<void, std::error_code>& written)
    {
        if (!written) [[unlikely]] {
            roll_back_the_transaction();
            write_unfinished_ = true;
            throw write_error(written.error(), "konsens: the storage failed to write in the step");
        }
    }

    std::unexpected<std::error_code> stop_voting(std::error_code e)
    {
        error_ = e;
        roll_back_the_transaction();
        return std::unexpected(e);
    }

    void roll_back_the_transaction()
    {
        txn_.reset();
        open_log_.reset();
        step_open_ = false;
        the_transaction_holds_ended_steps_ = false;
    }

    void throw_if_a_step_is_open() const
    {
        if (step_open_) [[unlikely]] {
            throw std::logic_error("konsens: a read of the storage or a take of messages while a step is open");
        }
    }

    void throw_if_a_write_did_not_finish() const
    {
        if (write_unfinished_) [[unlikely]] {
            throw std::logic_error("konsens: a write of the storage threw, and the node takes no further step");
        }
    }

    void drop_incoming_and_outgoing()
    {
        outgoing_.clear();
        incoming_.clear();
    }

    std::unexpected<std::error_code> stop_voting(const konsens::hardware_fault& fault)
    {
        hardware_fault_ = fault;
        return stop_voting(make_error_code(paxos_errc::hardware_fault));
    }

    std::expected<std::uint32_t, std::error_code> read_prefix_chain_crc(const std::uint64_t length) const
    {
        return read_prefix_chain_crc_in(*txn_, length);
    }

    template <typename T>
    static std::expected<std::uint32_t, std::error_code> read_prefix_chain_crc_in(const T& t,
                                                                                  const std::uint64_t length)
    {
        if (length == 0) {
            return 0;
        }
        const auto c = t.get_chain_crc(length - 1);
        if (!c) [[unlikely]] {
            return std::unexpected(c.error());
        }
        if (!*c) [[unlikely]] {
            return std::unexpected(make_error_code(storage_errc::missing_entry));
        }
        return **c;
    }

    result check_decided_chain_crc(const std::uint64_t decided_idx, const std::uint32_t decided_chain_crc,
                                   const node_id from)
    {
        const auto own = read_prefix_chain_crc(decided_idx);
        if (!own) [[unlikely]] {
            return stop_voting(own.error());
        }
        if (*own != decided_chain_crc) [[unlikely]] {
            return stop_voting(konsens::hardware_fault{decided_idx, pid_, *own, from, decided_chain_crc});
        }
        return {};
    }

    std::expected<std::uint32_t, std::error_code> read_the_taken_chain_crc(const taken_log& t,
                                                                           const std::uint64_t length) const
    {
        const auto own = read_prefix_chain_crc(std::min(length, t.sync_idx));
        if (!own || length <= t.sync_idx) {
            return own;
        }
        return compute_chain_crc(*own, t.sync_idx, std::span(t.suffix).first(length - t.sync_idx));
    }

    std::expected<std::uint64_t, std::error_code> read_highest_verified_decided_idx(const taken_log& t,
                                                                                    const std::uint64_t own) const
    {
        auto highest = own;
        for (const auto& p : leader_state_.promises_meta) {
            const auto* meta = std::get_if<promise_meta_data>(&p);
            if (meta == nullptr || meta->decided_idx <= highest || meta->decided_idx > t.log_len()) {
                continue;
            }
            const auto chain = read_the_taken_chain_crc(t, meta->decided_idx);
            if (!chain) [[unlikely]] {
                return std::unexpected(chain.error());
            }
            if (*chain == meta->decided_chain_crc) {
                highest = meta->decided_idx;
            }
        }
        return highest;
    }

    template <typename T>
    static constexpr bool carries_a_ballot_of_the_sender =
        std::is_same_v<T, prepare> || std::is_same_v<T, konsens::promise> || std::is_same_v<T, accept_sync> ||
        std::is_same_v<T, accept_decide> || std::is_same_v<T, accepted> || std::is_same_v<T, decide>;

    template <typename T>
    bool raise_peer_seen(const node_id from, const T& msg)
    {
        auto& seen = peers_seen_.at(node_index_of(from));
        peer_seen received{msg.n, 0};
        if constexpr (std::is_same_v<T, accepted>) {
            received.accepted_idx = msg.accepted_idx;
        }
        constexpr bool counts_the_index = std::is_same_v<T, accepted>;
        if (received.n < seen.n ||
            (counts_the_index && received.n == seen.n && received.accepted_idx < seen.accepted_idx)) [[unlikely]] {
            peer_step_backs_.push_back({from, seen, received});
            return false;
        }
        if (received.n > seen.n) {
            seen = received;
        } else {
            seen.accepted_idx = std::max(seen.accepted_idx, received.accepted_idx);
        }
        return true;
    }

    std::size_t node_index_of(const node_id pid) const
    {
        return static_cast<std::size_t>(std::ranges::distance(nodes_.begin(), std::ranges::find(nodes_, pid)));
    }

    std::expected<konsens::log_sync, std::error_code> create_log_sync(const std::uint64_t common_prefix_idx,
                                                                      const meta_record& m) const
    {
        auto suffix = read_range(*txn_, std::min(common_prefix_idx, m.log_len), m.log_len);
        if (!suffix) [[unlikely]] {
            return std::unexpected(suffix.error());
        }
        return konsens::log_sync{std::move(*suffix), common_prefix_idx};
    }

    std::expected<konsens::promise, std::error_code> promise_for(const prepare& prep) const
    {
        const auto m = read_meta();
        if (!m) [[unlikely]] {
            return std::unexpected(m.error());
        }
        auto verified = *m;
        verified.log_len = bound_by_the_verified_idx(m->log_len);
        const auto last_chain = read_prefix_chain_crc(verified.log_len);
        if (!last_chain) [[unlikely]] {
            return std::unexpected(last_chain.error());
        }
        std::optional<konsens::log_sync> sync;
        if (m->accepted_round > prep.n_accepted) {
            auto s = create_log_sync(prep.decided_idx, verified);
            if (!s) [[unlikely]] {
                return std::unexpected(s.error());
            }
            sync = std::move(*s);
        } else if (m->accepted_round == prep.n_accepted && verified.log_len > prep.accepted_idx) {
            auto s = create_log_sync(prep.accepted_idx, verified);
            if (!s) [[unlikely]] {
                return std::unexpected(s.error());
            }
            sync = std::move(*s);
        }
        const auto decided_chain = read_prefix_chain_crc(m->decided_idx);
        if (!decided_chain) [[unlikely]] {
            return std::unexpected(decided_chain.error());
        }
        return konsens::promise{prep.n,          m->accepted_round, m->decided_idx, verified.log_len,
                                std::move(sync), *decided_chain,    *last_chain};
    }

    void send_preparereq_to_all_peers(const ballot promise)
    {
        for (const auto peer : peers_) {
            outgoing_.push_back({pid_, peer, prepare_req{promise}});
        }
    }

    void send_prepare(const node_id to, const meta_record& m)
    {
        outgoing_.push_back({pid_, to,
                             prepare{leader_state_.n_leader, m.decided_idx, m.accepted_round,
                                     bound_by_the_verified_idx(m.log_len)}});
    }

    void send_decide(const node_id to, const std::uint64_t decided_idx, const std::uint32_t decided_chain_crc)
    {
        outgoing_.push_back(
            {pid_, to, decide{leader_state_.n_leader, leader_state_.next_seq_num(to), decided_idx, decided_chain_crc}});
    }

    void reconnected(const node_id pid, const meta_record& m)
    {
        if (pid == pid_) {
            return;
        }
        if (pid == m.promise.pid) {
            state_ = {role::follower, phase::recover};
        }
        outgoing_.push_back({pid_, pid, prepare_req{m.promise}});
    }

    bool check_valid_ballot(const ballot message_ballot, const meta_record& m)
    {
        if (m.promise == message_ballot) {
            return true;
        }
        if (m.promise > message_ballot) {
            outgoing_.push_back({pid_, message_ballot.pid, not_accepted{m.promise}});
            return false;
        }
        reconnected(message_ballot.pid, m);
        return false;
    }

    message_status handle_sequence_num(const sequence_number seq_num, const node_id from, const meta_record& m)
    {
        const auto status = current_seq_num_.check_msg_status(seq_num);
        if (status == message_status::expected) {
            current_seq_num_ = seq_num;
        } else if (status == message_status::dropped_preceding) {
            reconnected(from, m);
        }
        return status;
    }

    void forward_proposals(std::vector<entry> entries, const meta_record& m)
    {
        const node_id leader = m.promise.pid;
        if (leader > 0 && leader != pid_) {
            outgoing_.push_back({pid_, leader, proposal_forward{std::move(entries)}});
        } else {
            buffered_proposals_.insert(buffered_proposals_.end(), std::make_move_iterator(entries.begin()),
                                       std::make_move_iterator(entries.end()));
        }
    }

    void cut_the_buffer_after_a_stop_sign()
    {
        if (leader_state_.accepted_stop_sign) [[unlikely]] {
            buffered_proposals_.clear();
            return;
        }
        if (const auto s = std::ranges::find_if(buffered_proposals_, entry_is_a_stop_sign);
            s != buffered_proposals_.end()) [[unlikely]] {
            buffered_proposals_.erase(std::next(s), buffered_proposals_.end());
        }
    }

    result accept_entries_leader(const std::span<const entry> entries)
    {
        if (entries.empty()) {
            return {};
        }
        const auto s = std::ranges::find_if(entries, entry_is_a_stop_sign);
        if (leader_state_.accepted_stop_sign || (s != entries.end() && std::next(s) != entries.end())) [[unlikely]] {
            throw std::logic_error("konsens: the leader appends an entry after a StopSign");
        }
        write_or_throw(txn_->append_entries(entries));
        leader_state_.accepted_stop_sign = s != entries.end();
        if (auto r = extend_the_verified_idx(verified_idx_); !r) [[unlikely]] {
            return r;
        }
        const auto m = read_meta();
        if (!m) [[unlikely]] {
            return stop_voting(m.error());
        }
        const auto decided_chain = read_prefix_chain_crc(m->decided_idx);
        if (!decided_chain) [[unlikely]] {
            return stop_voting(decided_chain.error());
        }
        leader_state_.set_accepted_idx(pid_, bound_by_the_synced_len(bound_by_the_verified_idx(m->log_len)));
        for (const auto f : leader_state_.get_promised_followers()) {
            outgoing_.push_back({pid_, f,
                                 accept_decide{leader_state_.n_leader,
                                               leader_state_.next_seq_num(f),
                                               m->decided_idx,
                                               {entries.begin(), entries.end()},
                                               *decided_chain}});
        }
        return {};
    }

    result handle_preparereq(const prepare_req& prepreq, const node_id from)
    {
        if (state_.first != role::leader || prepreq.n > leader_state_.n_leader) {
            return {};
        }
        const auto m = read_meta();
        if (!m) [[unlikely]] {
            return stop_voting(m.error());
        }
        leader_state_.reset_promise(from);
        send_prepare(from, *m);
        return {};
    }

    result handle_prepare(const prepare& prep, const node_id from)
    {
        const auto m = read_meta();
        if (!m) [[unlikely]] {
            return stop_voting(m.error());
        }
        if (!(m->promise < prep.n || (m->promise == prep.n && state_.first == role::follower))) {
            return {};
        }
        write_or_throw(txn_->set_promise(prep.n));
        auto p = promise_for(prep);
        if (!p) [[unlikely]] {
            return stop_voting(p.error());
        }
        state_ = {role::follower, phase::prepare};
        leader_state_.taken.reset();
        current_seq_num_ = {};
        answered_prepare_ = prep;
        outgoing_.push_back({pid_, from, std::move(*p)});
        return {};
    }

    result handle_promise_prepare(const konsens::promise& prom, const node_id from)
    {
        if (prom.n != leader_state_.n_leader) {
            return {};
        }
        const promise_meta_data meta{prom.n_accepted, prom.accepted_idx, prom.decided_idx, from,
                                     prom.decided_chain_crc};
        if (leader_state_.taken) {
            leader_state_.set_promise(meta, std::nullopt, false);
            return {};
        }
        if (leader_state_.set_promise(meta, prom.log_sync, true)) {
            return handle_majority_promises();
        }
        return {};
    }

    result handle_promise_accept(const konsens::promise& prom, const node_id from)
    {
        if (prom.n != leader_state_.n_leader) {
            return {};
        }
        const promise_meta_data meta{prom.n_accepted, prom.accepted_idx, prom.decided_idx, from,
                                     prom.decided_chain_crc};
        leader_state_.set_promise(meta, std::nullopt, false);
        return send_accsync(from);
    }

    result handle_majority_promises()
    {
        const auto m = read_meta();
        if (!m) [[unlikely]] {
            return stop_voting(m.error());
        }
        auto sync = leader_state_.take_max_promise_sync();
        taken_log t{sync ? sync->sync_idx : bound_by_the_verified_idx(m->log_len),
                    sync ? std::move(sync->suffix) : std::vector<entry>{}, 0, 0};
        if (t.sync_idx < m->applied_idx) [[unlikely]] {
            throw std::logic_error("konsens: the taken log ends below the applied index of the leader");
        }
        const auto decided_idx = read_highest_verified_decided_idx(t, m->decided_idx);
        if (!decided_idx) [[unlikely]] {
            return stop_voting(decided_idx.error());
        }
        const auto decided_chain = read_the_taken_chain_crc(t, *decided_idx);
        if (!decided_chain) [[unlikely]] {
            return stop_voting(decided_chain.error());
        }
        t.decided_idx = *decided_idx;
        t.decided_chain_crc = *decided_chain;
        const auto above = read_the_taken_entries(t, t.decided_idx, t.log_len());
        if (!above) [[unlikely]] {
            return stop_voting(above.error());
        }
        if (std::ranges::any_of(*above, entry_is_a_request)) {
            leader_state_.taken = std::move(t);
            return {};
        }
        return accept_the_taken_log(t.sync_idx, t.suffix, t.decided_idx, leader_state_.first_changed_idx);
    }

    std::expected<std::vector<entry>, std::error_code>
    read_the_taken_entries(const taken_log& t, const std::uint64_t from, const std::uint64_t to) const
    {
        if (from > to || to > t.log_len()) [[unlikely]] {
            throw std::logic_error("konsens: a read of the taken log outside of its range");
        }
        auto own = read_range(*txn_, std::min(from, t.sync_idx), std::min(to, t.sync_idx));
        if (!own) [[unlikely]] {
            return own;
        }
        const auto first = std::max(from, t.sync_idx) - t.sync_idx;
        const auto last = std::max(to, t.sync_idx) - t.sync_idx;
        own->insert(own->end(), std::next(t.suffix.begin(), static_cast<std::ptrdiff_t>(first)),
                    std::next(t.suffix.begin(), static_cast<std::ptrdiff_t>(last)));
        return own;
    }

    result accept_the_selected_entries(const std::vector<entry>& selected)
    {
        if (state_ != std::pair{role::leader, phase::prepare} || !leader_state_.taken) [[unlikely]] {
            throw std::logic_error("konsens: the leader accepts selected entries only while it selects them");
        }
        const auto t = *std::exchange(leader_state_.taken, std::nullopt);
        const auto d = t.decided_idx;
        const auto above = read_the_taken_entries(t, d, t.log_len());
        if (!above) [[unlikely]] {
            return stop_voting(above.error());
        }
        if (selected.size() > above->size()) [[unlikely]] {
            throw std::logic_error("konsens: more selected entries than taken entries");
        }
        const auto same = std::ranges::mismatch(selected, *above);
        const auto changed = static_cast<std::uint64_t>(std::distance(selected.begin(), same.in1));
        const auto fc = changed == above->size() ? leader_state_.first_changed_idx : d + changed + 1;
        const auto kept = std::min(t.sync_idx, fc - 1);
        auto suffix = read_the_taken_entries(t, std::min(kept, d), d);
        if (!suffix) [[unlikely]] {
            return stop_voting(suffix.error());
        }
        const auto from = std::max(kept, d) - d;
        suffix->insert(suffix->end(), std::next(selected.begin(), static_cast<std::ptrdiff_t>(from)), selected.end());
        return accept_the_taken_log(kept, *suffix, d, fc);
    }

    result accept_the_taken_log(const std::uint64_t sync_idx, const std::vector<entry>& suffix,
                                const std::uint64_t decided_idx, const std::uint64_t first_changed_idx)
    {
        const auto m = read_meta();
        if (!m) [[unlikely]] {
            return stop_voting(m.error());
        }
        write_or_throw(txn_->append_on_prefix(sync_idx, suffix));
        lower_the_synced_len(sync_idx);
        leader_state_.first_changed_idx = first_changed_idx;
        write_or_throw(txn_->set_accepted_round(leader_state_.n_leader));
        lower_the_synced_len_if_the_round_changes(m->accepted_round, leader_state_.n_leader);
        write_or_throw(txn_->set_decided_index(decided_idx));
        if (auto r = extend_the_verified_idx(std::min(verified_idx_, sync_idx)); !r) [[unlikely]] {
            return r;
        }
        const auto after = read_meta();
        if (!after) [[unlikely]] {
            return stop_voting(after.error());
        }
        const auto last =
            read_range(*txn_, after->log_len - std::min(after->log_len, std::uint64_t{1}), after->log_len);
        if (!last) [[unlikely]] {
            return stop_voting(last.error());
        }
        leader_state_.accepted_stop_sign =
            after->log_len > config_first_idx_ && std::ranges::any_of(*last, entry_is_a_stop_sign);
        state_ = {role::leader, phase::accept};
        leader_state_.set_accepted_idx(pid_, bound_by_the_synced_len(bound_by_the_verified_idx(after->log_len)));
        for (const auto f : leader_state_.get_promised_followers()) {
            if (auto r = send_accsync(f); !r) [[unlikely]] {
                return r;
            }
        }
        return start_read_round();
    }

    result send_accsync(const node_id to)
    {
        const auto m = read_meta();
        if (!m) [[unlikely]] {
            return stop_voting(m.error());
        }
        const auto& mm = leader_state_.max_promise_meta;
        const auto& fm = leader_state_.get_promise_meta(to);
        const auto valid = std::min(fm.n_accepted == leader_state_.n_leader ? fm.accepted_idx
                                    : fm.n_accepted == mm.n_accepted        ? std::min(mm.accepted_idx, fm.accepted_idx)
                                                                            : fm.decided_idx,
                                    leader_state_.first_changed_idx - 1);
        auto sync = create_log_sync(valid, *m);
        if (!sync) [[unlikely]] {
            return stop_voting(sync.error());
        }
        const auto decided_chain = read_prefix_chain_crc(m->decided_idx);
        if (!decided_chain) [[unlikely]] {
            return stop_voting(decided_chain.error());
        }
        leader_state_.increment_seq_num_session(to);
        const auto seq_num = leader_state_.next_seq_num(to);
        outgoing_.push_back(
            {pid_, to, accept_sync{leader_state_.n_leader, seq_num, m->decided_idx, std::move(*sync), *decided_chain}});
        return {};
    }

    result handle_acceptsync(const accept_sync& accsync, const node_id from)
    {
        const auto m = read_meta();
        if (!m) [[unlikely]] {
            return stop_voting(m.error());
        }
        if (!check_valid_ballot(accsync.n, *m) || state_ != std::pair{role::follower, phase::prepare}) {
            return {};
        }
        const auto sync_idx = accsync.log_sync.sync_idx;
        if (sync_idx > m->log_len || (accsync.n == applied_sync_.n && accsync.seq_num.session <= applied_sync_.session))
            [[unlikely]] {
            return {};
        }
        if (m->accepted_round != accsync.n || sync_idx + accsync.log_sync.suffix.size() > m->log_len) {
            write_or_throw(txn_->append_on_prefix(sync_idx, accsync.log_sync.suffix));
            lower_the_synced_len(sync_idx);
            if (auto r = extend_the_verified_idx(std::min(verified_idx_, sync_idx)); !r) [[unlikely]] {
                return r;
            }
        }
        if (accsync.decided_idx > m->decided_idx) {
            if (auto r = check_decided_chain_crc(accsync.decided_idx, accsync.decided_chain_crc, from); !r)
                [[unlikely]] {
                return r;
            }
            write_or_throw(txn_->set_decided_index(accsync.decided_idx));
        }
        write_or_throw(txn_->set_accepted_round(accsync.n));
        lower_the_synced_len_if_the_round_changes(m->accepted_round, accsync.n);
        const auto after = read_meta();
        if (!after) [[unlikely]] {
            return stop_voting(after.error());
        }
        answered_prepare_.reset();
        if (!buffered_proposals_.empty()) {
            forward_proposals(std::exchange(buffered_proposals_, {}), *after);
        }
        state_ = {role::follower, phase::accept};
        current_seq_num_ = accsync.seq_num;
        applied_sync_ = {accsync.n, accsync.seq_num.session};
        auto acc = accepted_up_to_the_verified_idx(accsync.n);
        if (!acc) [[unlikely]] {
            return stop_voting(acc.error());
        }
        outgoing_.push_back({pid_, from, std::move(*acc)});
        return {};
    }

    result handle_acceptdecide(const accept_decide& acc_dec)
    {
        const auto m = read_meta();
        if (!m) [[unlikely]] {
            return stop_voting(m.error());
        }
        if (!check_valid_ballot(acc_dec.n, *m) || state_ != std::pair{role::follower, phase::accept} ||
            handle_sequence_num(acc_dec.seq_num, acc_dec.n.pid, *m) != message_status::expected) {
            return {};
        }
        const auto count = acc_dec.entries.size();
        const auto u = update_decided(m->decided_idx, m->log_len + count, acc_dec.decided_idx);
        if (count > 0) {
            write_or_throw(txn_->append_entries(acc_dec.entries));
            if (auto r = extend_the_verified_idx(verified_idx_); !r) [[unlikely]] {
                return r;
            }
        }
        if (u.decided_idx == acc_dec.decided_idx && u.decided_idx != m->decided_idx) {
            if (auto r = check_decided_chain_crc(u.decided_idx, acc_dec.decided_chain_crc, acc_dec.n.pid); !r)
                [[unlikely]] {
                return r;
            }
            write_or_throw(txn_->set_decided_index(u.decided_idx));
        }
        if (count > 0 || u.flushed) {
            auto acc = accepted_up_to_the_verified_idx(acc_dec.n);
            if (!acc) [[unlikely]] {
                return stop_voting(acc.error());
            }
            outgoing_.push_back({pid_, acc_dec.n.pid, std::move(*acc)});
        }
        return {};
    }

    result handle_decide(const decide& dec)
    {
        const auto m = read_meta();
        if (!m) [[unlikely]] {
            return stop_voting(m.error());
        }
        if (!check_valid_ballot(dec.n, *m) || state_.second != phase::accept ||
            handle_sequence_num(dec.seq_num, dec.n.pid, *m) != message_status::expected) {
            return {};
        }
        const auto u = update_decided(m->decided_idx, m->log_len, dec.decided_idx);
        if (u.decided_idx == dec.decided_idx && u.decided_idx != m->decided_idx) {
            if (auto r = check_decided_chain_crc(u.decided_idx, dec.decided_chain_crc, dec.n.pid); !r) [[unlikely]] {
                return r;
            }
            write_or_throw(txn_->set_decided_index(u.decided_idx));
        }
        if (u.flushed) {
            outgoing_.push_back({pid_, dec.n.pid, accepted{dec.n, m->log_len, m->last_chain_crc}});
        }
        return {};
    }

    result handle_accepted(const accepted& acc, const node_id from)
    {
        if (acc.n != leader_state_.n_leader || state_ != std::pair{role::leader, phase::accept}) {
            return {};
        }
        const auto m = read_meta();
        if (!m) [[unlikely]] {
            return stop_voting(m.error());
        }
        if (acc.accepted_idx > m->log_len) [[unlikely]] {
            return {};
        }
        const auto own_chain = read_prefix_chain_crc(acc.accepted_idx);
        if (!own_chain) [[unlikely]] {
            return stop_voting(own_chain.error());
        }
        if (*own_chain != acc.accepted_chain_crc) [[unlikely]] {
            return {};
        }
        leader_state_.set_accepted_idx(from, acc.accepted_idx);
        return decide_if_chosen(acc.accepted_idx, *own_chain, *m);
    }

    result decide_if_chosen(const std::uint64_t accepted_idx, const std::uint32_t chain, const meta_record& m)
    {
        if (accepted_idx <= m.decided_idx || !leader_state_.is_chosen(accepted_idx)) {
            return {};
        }
        write_or_throw(txn_->set_decided_index(accepted_idx));
        for (const auto f : leader_state_.get_promised_followers()) {
            send_decide(f, accepted_idx, chain);
        }
        return {};
    }

    result pass_the_checked_done_record(const std::uint64_t index)
    {
        if (auto r = extend_the_verified_idx(index); !r) [[unlikely]] {
            return r;
        }
        const auto m = read_meta();
        if (!m) [[unlikely]] {
            return stop_voting(m.error());
        }
        if (state_ == std::pair{role::leader, phase::accept} && leader_state_.n_leader == m->promise) {
            const auto own = std::max(bound_by_the_synced_len(bound_by_the_verified_idx(m->log_len)),
                                      leader_state_.accepted_indexes.at(leader_state_.pid_to_idx(pid_)));
            leader_state_.set_accepted_idx(pid_, own);
            const auto chain = read_prefix_chain_crc(own);
            if (!chain) [[unlikely]] {
                return stop_voting(chain.error());
            }
            return decide_if_chosen(own, *chain, *m);
        }
        if (state_ == std::pair{role::follower, phase::accept} && m->promise.pid != 0 && m->promise.pid != pid_) {
            auto acc = accepted_up_to_the_verified_idx(m->promise);
            if (!acc) [[unlikely]] {
                return stop_voting(acc.error());
            }
            outgoing_.push_back({pid_, m->promise.pid, std::move(*acc)});
        }
        return {};
    }

    std::expected<std::vector<std::uint32_t>, std::error_code> read_verified_chain_crcs() const
    {
        const auto t = internal_storage_.begin_read_only();
        if (!t) [[unlikely]] {
            return std::unexpected(t.error());
        }
        std::vector<std::uint32_t> chains;
        for (std::uint64_t index = 0;; ++index) {
            const auto stored = t->get_chain_crc(index);
            if (!stored || !*stored) {
                return chains;
            }
            const auto e = t->get_entries(index, index + 1);
            if (!e || e->size() != 1) {
                return chains;
            }
            const auto previous = chains.empty() ? std::uint32_t{0} : chains.back();
            if (compute_chain_crc(previous, index, e->front()) != **stored) {
                return chains;
            }
            chains.push_back(**stored);
        }
    }

    void send_build_request(const std::uint64_t from_idx)
    {
        outgoing_.push_back({pid_, build_->sources[build_->source], build_request{nonce_, from_idx}});
    }

    void take_the_next_build_source() { build_->source = (build_->source + 1) % build_->sources.size(); }

    result ask_again_for_the_build()
    {
        if (auto r = begin_step(); !r) [[unlikely]] {
            return r;
        }
        const auto m = read_meta();
        if (!m) [[unlikely]] {
            return stop_voting(m.error());
        }
        if (!build_->answered) {
            take_the_next_build_source();
        }
        build_->answered = false;
        send_build_request(m->log_len);
        return {};
    }

    result handle_build_request(const build_request& req, const node_id from)
    {
        const auto refuse = [&]() -> result {
            outgoing_.push_back({pid_, from, build_refused{req.nonce}});
            return {};
        };
        const auto m = read_meta();
        if (!m) [[unlikely]] {
            return stop_voting(m.error());
        }
        if (m->decided_idx <= config_first_idx_ || req.from_idx > m->decided_idx) {
            return refuse();
        }
        const auto last = read_range(*txn_, m->decided_idx - 1, m->decided_idx);
        if (!last) [[unlikely]] {
            return stop_voting(last.error());
        }
        const auto next = next_configuration_of(last->front());
        if (!next) {
            return refuse();
        }
        if (!std::ranges::contains(next->nodes, from)) [[unlikely]] {
            return {};
        }
        const auto to = std::min(m->decided_idx, req.from_idx + max_build_entries);
        auto entries = read_range(*txn_, req.from_idx, to);
        if (!entries) [[unlikely]] {
            return stop_voting(entries.error());
        }
        const auto chain = read_prefix_chain_crc(to);
        if (!chain) [[unlikely]] {
            return stop_voting(chain.error());
        }
        const auto decided_chain = read_prefix_chain_crc(m->decided_idx);
        if (!decided_chain) [[unlikely]] {
            return stop_voting(decided_chain.error());
        }
        outgoing_.push_back(
            {pid_, from,
             build_response{req.nonce, req.from_idx, std::move(*entries), *chain, m->decided_idx, *decided_chain}});
        return {};
    }

    result handle_build_response(const build_response& resp)
    {
        const auto m = read_meta();
        if (!m) [[unlikely]] {
            return stop_voting(m.error());
        }
        if (resp.from_idx != m->log_len) {
            return {};
        }
        const auto length = resp.from_idx + resp.entries.size();
        if (resp.entries.empty() || length > resp.decided_idx) [[unlikely]] {
            take_the_next_build_source();
            return {};
        }
        const auto before = read_prefix_chain_crc(resp.from_idx);
        if (!before) [[unlikely]] {
            return stop_voting(before.error());
        }
        const auto chain = compute_chain_crc(*before, resp.from_idx, resp.entries);
        if (chain != resp.last_chain_crc || (length == resp.decided_idx && chain != resp.decided_chain_crc))
            [[unlikely]] {
            take_the_next_build_source();
            return {};
        }
        const ballot start{configuration_id_, 0, 0, 0};
        if (m->promise != start) {
            write_or_throw(txn_->set_promise(start));
            write_or_throw(txn_->set_accepted_round(start));
            lower_the_synced_len_if_the_round_changes(m->accepted_round, start);
        }
        write_or_throw(txn_->append_entries(resp.entries));
        write_or_throw(txn_->set_decided_index(length));
        if (length < resp.decided_idx) {
            send_build_request(length);
            return {};
        }
        const auto& last = resp.entries.back();
        const auto next = next_configuration_of(last);
        if (!next || next->configuration_id != configuration_id_ || !std::ranges::contains(next->nodes, pid_) ||
            !std::ranges::all_of(peers_, [&next](const node_id p) { return std::ranges::contains(next->nodes, p); }) ||
            next->nodes.size() != nodes_.size()) [[unlikely]] {
            throw std::logic_error("konsens: the decided StopSign of the build names another configuration");
        }
        write_or_throw(txn_->set_config_first_idx(length));
        config_first_idx_ = length;
        verified_idx_ = length;
        leader_state_ = leader_state::with(start, nodes_);
        build_.reset();
        return {};
    }

    void handle_notaccepted(const not_accepted& not_acc, const node_id from)
    {
        if (state_.first == role::leader && leader_state_.n_leader < not_acc.n) {
            leader_state_.lost_promise(from);
        }
    }

    result handle_forwarded_proposal(const std::span<const entry> entries)
    {
        if (state_.first == role::leader) {
            buffered_proposals_.insert(buffered_proposals_.end(), entries.begin(), entries.end());
            return {};
        }
        const auto m = read_meta();
        if (!m) [[unlikely]] {
            return stop_voting(m.error());
        }
        forward_proposals({entries.begin(), entries.end()}, *m);
        return {};
    }

    result start_read_round()
    {
        if (holds_a_stop_sign()) [[unlikely]] {
            read_pending_.clear();
            return {};
        }
        if (state_ != std::pair{role::leader, phase::accept} || read_pending_.empty() ||
            (!read_round_.reads.empty() && read_round_.n == leader_state_.n_leader)) {
            return {};
        }
        const auto m = read_meta();
        if (!m) [[unlikely]] {
            return stop_voting(m.error());
        }
        read_round_ = {
            leader_state_.n_leader, read_round_.id + 1, std::exchange(read_pending_, {}), m->log_len, {pid_}};
        for (const auto peer : peers_) {
            outgoing_.push_back({pid_, peer, heartbeat{read_round_.n, read_round_.id}});
        }
        return {};
    }

    result handle_read_index_request(const read_index_request& req, const node_id from)
    {
        if (state_.first != role::leader) {
            return {};
        }
        read_pending_.push_back({from, req.read});
        return start_read_round();
    }

    result handle_heartbeat(const heartbeat& hb, const node_id from)
    {
        const auto m = read_meta();
        if (!m) [[unlikely]] {
            return stop_voting(m.error());
        }
        if (m->promise == hb.n) {
            outgoing_.push_back({pid_, from, heartbeat_ack{hb.n, hb.id}});
        }
        return {};
    }

    result handle_heartbeat_ack(const heartbeat_ack& ack, const node_id from)
    {
        auto& rd = read_round_;
        if (state_ != std::pair{role::leader, phase::accept} || rd.reads.empty() || rd.n != ack.n || rd.id != ack.id ||
            leader_state_.n_leader != ack.n) {
            return {};
        }
        if (!std::ranges::contains(rd.acks, from)) {
            rd.acks.push_back(from);
        }
        if (rd.acks.size() < leader_state_.majority()) {
            return {};
        }
        const auto m = read_meta();
        if (!m) [[unlikely]] {
            return stop_voting(m.error());
        }
        for (const auto& r : rd.reads) {
            if (r.node == pid_) {
                indexed_reads_.push_back({r.read, rd.read_index, m->promise});
            }
        }
        for (const auto peer : peers_) {
            std::vector<std::uint64_t> reads;
            for (const auto& r : rd.reads) {
                if (r.node == peer) {
                    reads.push_back(r.read);
                }
            }
            if (!reads.empty()) {
                outgoing_.push_back({pid_, peer, read_index_reply{std::move(reads), rd.read_index}});
            }
        }
        rd.reads.clear();
        rd.acks.clear();
        return start_read_round();
    }

    result handle_read_index_reply(const read_index_reply& reply)
    {
        const auto m = read_meta();
        if (!m) [[unlikely]] {
            return stop_voting(m.error());
        }
        for (const auto r : reply.reads) {
            indexed_reads_.push_back({r, reply.read_index, m->promise});
        }
        return {};
    }

    std::uint32_t configuration_id_;
    std::uint64_t config_first_idx_{};
    std::optional<build_state> build_;
    node_id pid_;
    std::vector<node_id> peers_;
    std::vector<node_id> nodes_;
    S internal_storage_;
    std::optional<typename S::transaction> txn_;

    struct open_log_read {
        std::uint64_t from;
        std::uint64_t entry_writes;
        std::shared_ptr<const std::vector<entry>> entries;
    };
    std::optional<open_log_read> open_log_;
    bool step_open_{};
    bool the_transaction_holds_ended_steps_{};
    struct pending_commit {
        typename S::transaction txn;
        meta_record meta;
    };
    std::optional<pending_commit> pending_commit_;
    bool a_commit_waits_for_its_sync_{};
    bool sync_by_the_caller_{};
    struct unsynced_step {
        std::uint64_t sync_number;
        std::uint64_t log_len;
    };
    std::vector<unsynced_step> unsynced_steps_;
    std::uint64_t last_sync_number_{};
    std::uint64_t synced_sync_number_{};
    std::uint64_t synced_len_{};
    high_water_mark mark_{};
    std::pair<role, phase> state_{role::follower, phase::none};
    leader_state leader_state_{};
    sequence_number current_seq_num_{};
    struct applied_sync {
        ballot n;
        std::uint64_t session;
    } applied_sync_{};
    std::optional<prepare> answered_prepare_;
    std::uint64_t verified_idx_{};
    std::vector<entry> buffered_proposals_;
    std::vector<paxos_message> incoming_;
    std::vector<paxos_message> outgoing_;
    std::optional<std::error_code> error_;
    bool write_unfinished_{};
    bool step_ended_{};
    bool retired_{};
    std::uint64_t nonce_{};
    std::optional<konsens::hardware_fault> hardware_fault_;
    std::vector<peer_seen> peers_seen_;
    std::vector<peer_step_back> peer_step_backs_;
    std::vector<pending_read> read_pending_;
    konsens::read_round read_round_{};
    std::vector<indexed_read> indexed_reads_;
};

}

template <>
struct std::is_error_code_enum<konsens::paxos_errc> : std::true_type {};
