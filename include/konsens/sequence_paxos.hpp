#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iterator>
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

namespace konsens {

template <storage S, state_machine M>
class node;

static_assert(std::is_nothrow_move_constructible_v<paxos_message>);

enum class paxos_errc { hardware_fault = 1, inconsistent_storage };

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

struct hardware_fault {
    std::uint64_t index;
    node_id pid;
    std::uint32_t crc;
    node_id other_pid;
    std::uint32_t other_crc;
};

enum class role : std::uint8_t { follower, leader };

enum class phase : std::uint8_t { prepare, accept, recover, none };

struct promise_meta_data {
    ballot n_accepted;
    std::uint64_t accepted_idx;
    std::uint64_t decided_idx;
    node_id pid;
};

struct not_promised {};

struct promised_higher {};

using promise_state = std::variant<not_promised, promise_meta_data, promised_higher>;

struct leader_state {
    ballot n_leader;
    std::vector<node_id> nodes;
    std::vector<promise_state> promises_meta;
    std::vector<sequence_number> follower_seq_nums;
    std::vector<std::uint64_t> accepted_indexes;
    promise_meta_data max_promise_meta;
    std::optional<konsens::log_sync> max_promise_sync;

    static leader_state with(ballot n_leader, std::span<const node_id> nodes)
    {
        return {n_leader,
                std::vector<node_id>(nodes.begin(), nodes.end()),
                std::vector<promise_state>(nodes.size(), not_promised{}),
                std::vector<sequence_number>(nodes.size(), sequence_number{}),
                std::vector<std::uint64_t>(nodes.size(), 0),
                promise_meta_data{},
                std::nullopt};
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

    std::uint64_t get_max_decided_idx() const
    {
        std::uint64_t max = 0;
        for (const auto& p : promises_meta) {
            if (const auto* m = std::get_if<promise_meta_data>(&p)) {
                max = std::max(max, m->decided_idx);
            }
        }
        return max;
    }

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

struct sequence_paxos_config {
    node_id pid;
    std::vector<node_id> peers;
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

template <storage S>
class sequence_paxos {
public:
    using result = std::expected<void, std::error_code>;

    static std::expected<sequence_paxos, std::error_code> with(const sequence_paxos_config& config, S internal_storage,
                                                               const std::uint64_t nonce)
    {
        validate_cluster(config.pid, config.peers);
        sequence_paxos sp(config, std::move(internal_storage));
        const auto m = sp.read_meta_between_steps();
        if (!m) [[unlikely]] {
            return std::unexpected(m.error());
        }
        sp.mark_ = mark_of(*m);
        sp.leader_state_ = leader_state::with(m->promise, sp.nodes_);
        sp.nonce_ = nonce;
        if (m->promise != ballot{}) {
            sp.state_ = {role::follower, phase::recover};
            sp.recovering_ = true;
            sp.send_recovery_to_all_peers();
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

    [[nodiscard]] result commit()
    {
        throw_if_a_write_did_not_finish();
        if (error_) [[unlikely]] {
            txn_.reset();
            drop_incoming_and_outgoing();
            return std::unexpected(*error_);
        }
        if (auto r = begin_step(); !r) [[unlikely]] {
            drop_incoming_and_outgoing();
            return r;
        }
        const bool accepts = state_ == std::pair{role::leader, phase::accept};
        if (accepts) {
            if (auto r = accept_entries_leader(buffered_proposals_); !r) [[unlikely]] {
                drop_incoming_and_outgoing();
                return r;
            }
        }
        const auto m = read_checked_meta(*txn_);
        if (!m) [[unlikely]] {
            const auto stopped = stop_voting(m.error());
            drop_incoming_and_outgoing();
            return stopped;
        }
        write_unfinished_ = true;
        auto txn = std::move(*txn_);
        txn_.reset();
        if (auto c = std::move(txn).commit(); !c) [[unlikely]] {
            throw write_error(c.error(), "konsens: the storage failed to commit the step");
        }
        write_unfinished_ = false;
        mark_ = raise_mark(mark_, mark_of(*m));
        if (accepts) {
            buffered_proposals_.clear();
        }
        incoming_.clear();
        return {};
    }

private:
    template <storage, state_machine>
    friend class node;

    template <typename T>
        requires requires(const T& msg) { paxos_msg{std::in_place_type<T>, msg}; }
    result handle(node_id from, node_id to, const T& msg)
    {
        if (error_) [[unlikely]] {
            return std::unexpected(*error_);
        }
        if (to != pid_ || from == pid_ || !std::ranges::contains(peers_, from)) [[unlikely]] {
            return {};
        }
        if (recovering_ && !std::is_same_v<T, recovery> && !std::is_same_v<T, recovery_response> &&
            !std::is_same_v<T, konsens::get_state> && !std::is_same_v<T, new_state>) {
            return {};
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
        } else if constexpr (std::is_same_v<T, recovery>) {
            return handle_recovery(msg, from);
        } else if constexpr (std::is_same_v<T, recovery_response>) {
            return handle_recovery_response(msg, from);
        } else if constexpr (std::is_same_v<T, konsens::get_state>) {
            return handle_get_state(msg, from);
        } else if constexpr (std::is_same_v<T, new_state>) {
            return handle_new_state(msg, from);
        } else if constexpr (std::is_same_v<T, read_index_request>) {
            return handle_read_index_request(msg, from);
        } else if constexpr (std::is_same_v<T, heartbeat>) {
            return handle_heartbeat(msg, from);
        } else if constexpr (std::is_same_v<T, heartbeat_ack>) {
            return handle_heartbeat_ack(msg, from);
        } else if constexpr (std::is_same_v<T, read_index_reply>) {
            return handle_read_index_reply(msg);
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
        if (recovering_) {
            return {};
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
        write_or_throw(txn_->set_promise(n));
        leader_state_ = leader_state::with(n, nodes_);
        leader_state_.set_promise({m->accepted_round, m->log_len, m->decided_idx, pid_}, std::nullopt, true);
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
        if (auto r = refuse_if_recovering(); !r) [[unlikely]] {
            return r;
        }
        if (state_ == std::pair{role::leader, phase::prepare} || state_ == std::pair{role::leader, phase::accept}) {
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
        if (error_) [[unlikely]] {
            return std::unexpected(*error_);
        }
        if (recovering_) {
            send_recovery_to_all_peers();
            if (state_request_) {
                outgoing_.push_back({pid_, state_request_->first, state_request_->second});
            }
            return {};
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
                for (const auto f : leader_state_.get_promised_followers()) {
                    send_decide(f, m->decided_idx);
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

    result refuse_if_recovering() const
    {
        if (recovering_) [[unlikely]] {
            return std::unexpected(make_error_code(client_errc::recovering));
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
        return read_meta_between_steps().transform([](const meta_record& m) { return m.promise; });
    }
    std::expected<ballot, std::error_code> get_accepted_round() const
    {
        return read_meta_between_steps().transform([](const meta_record& m) { return m.accepted_round; });
    }
    std::expected<std::uint64_t, std::error_code> get_decided_idx() const
    {
        return read_meta_between_steps().transform([](const meta_record& m) { return m.decided_idx; });
    }
    std::expected<std::uint64_t, std::error_code> get_accepted_idx() const
    {
        return read_meta_between_steps().transform([](const meta_record& m) { return m.log_len; });
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
        const auto log = read_range(*txn_, std::min(from_idx, m->log_len), m->log_len);
        if (!log) [[unlikely]] {
            return stop_voting(log.error());
        }
        return std::ranges::contains(*log, e);
    }
    node_id get_pid() const { return pid_; }
    bool votes() const { return !error_; }
    bool recovering() const { return recovering_; }
    std::optional<std::error_code> get_error() const { return error_; }
    std::optional<konsens::hardware_fault> get_hardware_fault() const { return hardware_fault_; }

    result reconnected(const node_id pid)
    {
        if (error_) [[unlikely]] {
            return std::unexpected(*error_);
        }
        if (pid == pid_ || recovering_) {
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
        : pid_(config.pid), peers_(config.peers), internal_storage_(std::move(internal_storage))
    {
        nodes_ = peers_;
        nodes_.push_back(pid_);
        std::ranges::sort(nodes_);
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
        if ((m->log_len > 0 && m->accepted_round == ballot{}) || m->accepted_round > m->promise ||
            m->decided_idx > m->log_len || is_below_mark(mark_of(*m), mark_)) [[unlikely]] {
            return std::unexpected(make_error_code(paxos_errc::inconsistent_storage));
        }
        if (m->decided_idx > 0) {
            const auto state = t.get_entry_state(m->decided_idx - 1);
            if (!state) [[unlikely]] {
                return std::unexpected(state.error());
            }
            if (state->value_or(entry_state::in_work) != entry_state::committed) [[unlikely]] {
                return std::unexpected(make_error_code(paxos_errc::inconsistent_storage));
            }
        }
        if (m->decided_idx < m->log_len) {
            const auto state = t.get_entry_state(m->decided_idx);
            if (!state) [[unlikely]] {
                return std::unexpected(state.error());
            }
            if (state->value_or(entry_state::committed) != entry_state::in_work) [[unlikely]] {
                return std::unexpected(make_error_code(paxos_errc::inconsistent_storage));
            }
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

    result begin_step()
    {
        throw_if_a_write_did_not_finish();
        if (txn_) {
            return {};
        }
        auto t = internal_storage_.begin();
        if (!t) [[unlikely]] {
            return stop_voting(t.error());
        }
        txn_.emplace(std::move(*t));
        const auto m = read_checked_meta(*txn_);
        if (!m) [[unlikely]] {
            return stop_voting(m.error());
        }
        return {};
    }

    void write_or_throw(const std::expected<void, std::error_code>& written)
    {
        if (!written) [[unlikely]] {
            txn_.reset();
            write_unfinished_ = true;
            throw write_error(written.error(), "konsens: the storage failed to write in the step");
        }
    }

    std::unexpected<std::error_code> stop_voting(std::error_code e)
    {
        error_ = e;
        txn_.reset();
        return std::unexpected(e);
    }

    void throw_if_a_step_is_open() const
    {
        if (txn_) [[unlikely]] {
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
        std::optional<konsens::log_sync> sync;
        if (m->accepted_round > prep.n_accepted) {
            auto s = create_log_sync(prep.decided_idx, *m);
            if (!s) [[unlikely]] {
                return std::unexpected(s.error());
            }
            sync = std::move(*s);
        } else if (m->accepted_round == prep.n_accepted && m->log_len > prep.accepted_idx) {
            auto s = create_log_sync(prep.accepted_idx, *m);
            if (!s) [[unlikely]] {
                return std::unexpected(s.error());
            }
            sync = std::move(*s);
        }
        return konsens::promise{prep.n, m->accepted_round, m->decided_idx, m->log_len, std::move(sync)};
    }

    void send_preparereq_to_all_peers(const ballot promise)
    {
        for (const auto peer : peers_) {
            outgoing_.push_back({pid_, peer, prepare_req{promise}});
        }
    }

    void send_recovery_to_all_peers()
    {
        for (const auto peer : peers_) {
            outgoing_.push_back({pid_, peer, recovery{nonce_}});
        }
    }

    void send_prepare(const node_id to, const meta_record& m)
    {
        outgoing_.push_back({pid_, to, prepare{leader_state_.n_leader, m.decided_idx, m.accepted_round, m.log_len}});
    }

    void send_decide(const node_id to, const std::uint64_t decided_idx)
    {
        outgoing_.push_back({pid_, to, decide{leader_state_.n_leader, leader_state_.next_seq_num(to), decided_idx}});
    }

    void reconnected(const node_id pid, const meta_record& m)
    {
        if (pid == pid_ || recovering_) {
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

    result accept_entries_leader(const std::span<const entry> entries)
    {
        if (entries.empty()) {
            return {};
        }
        write_or_throw(txn_->append_entries(entries));
        const auto m = read_meta();
        if (!m) [[unlikely]] {
            return stop_voting(m.error());
        }
        leader_state_.set_accepted_idx(pid_, m->log_len);
        for (const auto f : leader_state_.get_promised_followers()) {
            outgoing_.push_back({pid_, f,
                                 accept_decide{leader_state_.n_leader,
                                               leader_state_.next_seq_num(f),
                                               m->decided_idx,
                                               {entries.begin(), entries.end()}}});
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
        if (!(m->promise < prep.n || (m->promise == prep.n && state_.second == phase::recover))) {
            return {};
        }
        write_or_throw(txn_->set_promise(prep.n));
        auto p = promise_for(prep);
        if (!p) [[unlikely]] {
            return stop_voting(p.error());
        }
        state_ = {role::follower, phase::prepare};
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
        const promise_meta_data meta{prom.n_accepted, prom.accepted_idx, prom.decided_idx, from};
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
        const promise_meta_data meta{prom.n_accepted, prom.accepted_idx, prom.decided_idx, from};
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
        const auto decided_idx = leader_state_.get_max_decided_idx();
        const auto sync_idx = sync ? sync->sync_idx : m->log_len;
        const auto suffix = sync ? std::move(sync->suffix) : std::vector<entry>{};
        const auto proposals = std::exchange(buffered_proposals_, {});
        write_or_throw(txn_->append_on_prefix(sync_idx, suffix));
        write_or_throw(txn_->append_entries(proposals));
        write_or_throw(txn_->set_accepted_round(leader_state_.n_leader));
        write_or_throw(txn_->set_decided_index(decided_idx));
        const auto after = read_meta();
        if (!after) [[unlikely]] {
            return stop_voting(after.error());
        }
        state_ = {role::leader, phase::accept};
        leader_state_.set_accepted_idx(pid_, after->log_len);
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
        const auto valid = fm.n_accepted == leader_state_.n_leader ? fm.accepted_idx
                           : fm.n_accepted == mm.n_accepted        ? std::min(mm.accepted_idx, fm.accepted_idx)
                                                                   : fm.decided_idx;
        auto sync = create_log_sync(valid, *m);
        if (!sync) [[unlikely]] {
            return stop_voting(sync.error());
        }
        leader_state_.increment_seq_num_session(to);
        const auto seq_num = leader_state_.next_seq_num(to);
        outgoing_.push_back({pid_, to, accept_sync{leader_state_.n_leader, seq_num, m->decided_idx, std::move(*sync)}});
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
        write_or_throw(txn_->append_on_prefix(sync_idx, accsync.log_sync.suffix));
        write_or_throw(txn_->set_accepted_round(accsync.n));
        write_or_throw(txn_->set_decided_index(std::max(m->decided_idx, accsync.decided_idx)));
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
        outgoing_.push_back({pid_, from, accepted{accsync.n, after->log_len}});
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
        }
        if (u.decided_idx != m->decided_idx) {
            write_or_throw(txn_->set_decided_index(u.decided_idx));
        }
        if (count > 0 || u.flushed) {
            const auto after = read_meta();
            if (!after) [[unlikely]] {
                return stop_voting(after.error());
            }
            outgoing_.push_back({pid_, acc_dec.n.pid, accepted{acc_dec.n, after->log_len}});
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
        if (u.decided_idx != m->decided_idx) {
            write_or_throw(txn_->set_decided_index(u.decided_idx));
        }
        if (u.flushed) {
            outgoing_.push_back({pid_, dec.n.pid, accepted{dec.n, m->log_len}});
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
        leader_state_.set_accepted_idx(from, acc.accepted_idx);
        leader_state_.set_accepted_idx(pid_, m->log_len);
        if (acc.accepted_idx <= m->decided_idx || !leader_state_.is_chosen(acc.accepted_idx)) {
            return {};
        }
        write_or_throw(txn_->set_decided_index(acc.accepted_idx));
        for (const auto f : leader_state_.get_promised_followers()) {
            send_decide(f, acc.accepted_idx);
        }
        return {};
    }

    std::expected<std::uint32_t, std::error_code> read_last_entry_crc() const
    {
        return txn_->get_last_entry().transform(
            [](const std::optional<last_entry>& l) { return l ? l->entry_crc : std::uint32_t{0}; });
    }

    result handle_recovery(const recovery& rec, const node_id from)
    {
        const auto m = read_meta();
        if (!m) [[unlikely]] {
            return stop_voting(m.error());
        }
        const auto last_crc = read_last_entry_crc();
        if (!last_crc) [[unlikely]] {
            return stop_voting(last_crc.error());
        }
        const bool primary = !recovering_ && state_ == std::pair{role::leader, phase::accept};
        std::vector<entry> log;
        if (primary) {
            auto l = read_range(*txn_, 0, m->log_len);
            if (!l) [[unlikely]] {
                return stop_voting(l.error());
            }
            log = std::move(*l);
        }
        outgoing_.push_back({pid_, from,
                             recovery_response{rec.nonce, m->promise, primary, m->accepted_round, std::move(log),
                                               m->decided_idx, recovering_, m->log_len, *last_crc}});
        return {};
    }

    result handle_recovery_response(const recovery_response& resp, const node_id from)
    {
        if (!recovering_ || resp.nonce != nonce_) {
            return {};
        }
        keep_recovery_response(from, resp);
        return select_recovery_source();
    }

    void keep_recovery_response(const node_id from, const recovery_response& resp)
    {
        std::erase_if(recovery_responses_, [from](const auto& r) { return r.first == from; });
        recovery_responses_.emplace_back(from, resp);
    }

    result select_recovery_source()
    {
        state_request_.reset();
        if (const auto* l = find_recovery_primary()) {
            const auto m = read_meta();
            if (!m) [[unlikely]] {
                return stop_voting(m.error());
            }
            return complete_recovery(0, l->log, std::max(m->promise, l->promise), l->n_accepted, l->decided_idx);
        }
        if (recovery_responses_.size() < peers_.size() ||
            std::ranges::any_of(recovery_responses_, [](const auto& r) { return r.second.primary; })) {
            return {};
        }
        const auto m = read_meta();
        if (!m) [[unlikely]] {
            return stop_voting(m.error());
        }
        const auto last_crc = read_last_entry_crc();
        if (!last_crc) [[unlikely]] {
            return stop_voting(last_crc.error());
        }
        auto all = recovery_responses_;
        all.emplace_back(
            pid_, recovery_response{
                      nonce_, m->promise, false, m->accepted_round, {}, m->decided_idx, true, m->log_len, *last_crc});
        if (const auto fault = find_hardware_fault(all)) [[unlikely]] {
            return stop_voting(*fault);
        }
        const auto pr = std::ranges::max(all | std::views::values | std::views::transform(&recovery_response::promise));
        const auto& [source, l] = *std::ranges::min_element(all, [this](const auto& x, const auto& y) {
            if (x.second.n_accepted != y.second.n_accepted) {
                return x.second.n_accepted > y.second.n_accepted;
            }
            if (x.second.accepted_idx != y.second.accepted_idx) {
                return x.second.accepted_idx > y.second.accepted_idx;
            }
            if ((x.first == pid_) != (y.first == pid_)) {
                return x.first == pid_;
            }
            return x.first < y.first;
        });
        if (source == pid_) {
            return complete_recovery(m->log_len, {}, pr, m->accepted_round, m->decided_idx);
        }
        const auto from_idx = m->accepted_round == l.n_accepted ? m->log_len : m->decided_idx;
        state_request_.emplace(source, konsens::get_state{nonce_, l.n_accepted, l.accepted_idx, from_idx});
        outgoing_.push_back({pid_, source, state_request_->second});
        return {};
    }

    static std::optional<konsens::hardware_fault>
    find_hardware_fault(const std::span<const std::pair<node_id, recovery_response>> all)
    {
        for (const auto& [i, a] : std::views::enumerate(all)) {
            for (const auto& b : all.subspan(static_cast<std::size_t>(i) + 1)) {
                if (a.second.accepted_idx > 0 && a.second.n_accepted == b.second.n_accepted &&
                    a.second.accepted_idx == b.second.accepted_idx && a.second.last_crc != b.second.last_crc)
                    [[unlikely]] {
                    return konsens::hardware_fault{a.second.accepted_idx, a.first, a.second.last_crc, b.first,
                                                   b.second.last_crc};
                }
            }
        }
        return std::nullopt;
    }

    result handle_get_state(const konsens::get_state& req, const node_id from)
    {
        const auto m = read_meta();
        if (!m) [[unlikely]] {
            return stop_voting(m.error());
        }
        const auto last_crc = read_last_entry_crc();
        if (!last_crc) [[unlikely]] {
            return stop_voting(last_crc.error());
        }
        std::vector<entry> entries;
        if (m->accepted_round == req.n_accepted && m->log_len == req.accepted_idx) {
            auto e = read_range(*txn_, std::min(req.from_idx, m->log_len), m->log_len);
            if (!e) [[unlikely]] {
                return stop_voting(e.error());
            }
            entries = std::move(*e);
        }
        outgoing_.push_back({pid_, from,
                             new_state{req.nonce, m->promise, m->accepted_round, m->log_len, m->decided_idx, *last_crc,
                                       req.from_idx, std::move(entries)}});
        return {};
    }

    result handle_new_state(const new_state& ns, const node_id from)
    {
        if (!recovering_ || !state_request_ || state_request_->first != from) {
            return {};
        }
        const auto& req = state_request_->second;
        if (ns.nonce != req.nonce || ns.from_idx != req.from_idx) {
            return {};
        }
        if (ns.n_accepted != req.n_accepted || ns.accepted_idx != req.accepted_idx) {
            const auto old = std::ranges::find(recovery_responses_, from, [](const auto& r) { return r.first; });
            const bool source_recovering = old == recovery_responses_.end() || old->second.recovering;
            keep_recovery_response(from, recovery_response{nonce_,
                                                           ns.promise,
                                                           false,
                                                           ns.n_accepted,
                                                           {},
                                                           ns.decided_idx,
                                                           source_recovering,
                                                           ns.accepted_idx,
                                                           ns.last_crc});
            return select_recovery_source();
        }
        if (ns.entries.empty() && ns.from_idx < ns.accepted_idx) {
            return {};
        }
        if (ns.from_idx > ns.accepted_idx || ns.from_idx + ns.entries.size() != ns.accepted_idx) [[unlikely]] {
            return stop_voting(konsens::hardware_fault{ns.accepted_idx, pid_, 0, from, ns.last_crc});
        }
        std::uint32_t crc = 0;
        if (!ns.entries.empty()) {
            crc = compute_entry_crc(ns.accepted_idx - 1, ns.entries.back());
        } else if (ns.accepted_idx > 0) {
            const auto own = read_range(*txn_, ns.accepted_idx - 1, ns.accepted_idx);
            if (!own) [[unlikely]] {
                return stop_voting(own.error());
            }
            crc = compute_entry_crc(ns.accepted_idx - 1, own->front());
        }
        if (crc != ns.last_crc) [[unlikely]] {
            return stop_voting(konsens::hardware_fault{ns.accepted_idx, pid_, crc, from, ns.last_crc});
        }
        const auto m = read_meta();
        if (!m) [[unlikely]] {
            return stop_voting(m.error());
        }
        const auto answered = std::ranges::max(recovery_responses_ | std::views::values |
                                               std::views::transform(&recovery_response::promise));
        const auto pr = std::max({m->promise, ns.promise, answered});
        return complete_recovery(ns.from_idx, ns.entries, pr, ns.n_accepted, ns.decided_idx);
    }

    const recovery_response* find_recovery_primary()
    {
        auto normal = recovery_responses_ | std::views::filter([](const auto& r) { return !r.second.recovering; });
        if (static_cast<std::size_t>(std::ranges::distance(normal)) < leader_state_.majority()) {
            return nullptr;
        }
        const auto highest =
            std::ranges::max(normal | std::views::values | std::views::transform(&recovery_response::promise));
        const auto l = std::ranges::find_if(normal, [highest](const auto& r) {
            return r.second.primary && r.second.promise == highest && highest.pid == r.first;
        });
        return l == normal.end() ? nullptr : &l->second;
    }

    result complete_recovery(const std::uint64_t from_idx, const std::span<const entry> entries, const ballot pr,
                             const ballot n_accepted, const std::uint64_t source_decided_idx)
    {
        const auto m = read_meta();
        if (!m) [[unlikely]] {
            return stop_voting(m.error());
        }
        write_or_throw(txn_->append_on_prefix(from_idx, entries));
        write_or_throw(txn_->set_accepted_round(n_accepted));
        write_or_throw(txn_->set_decided_index(std::max(m->decided_idx, source_decided_idx)));
        write_or_throw(txn_->set_promise(pr));
        state_ = {role::follower, phase::recover};
        current_seq_num_ = {};
        answered_prepare_.reset();
        buffered_proposals_.clear();
        leader_state_ = leader_state::with(pr, nodes_);
        recovering_ = false;
        recovery_responses_.clear();
        state_request_.reset();
        send_preparereq_to_all_peers(pr);
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
        if (state_ == std::pair{role::leader, phase::prepare}) {
            buffered_proposals_.insert(buffered_proposals_.end(), entries.begin(), entries.end());
            return {};
        }
        if (state_ == std::pair{role::leader, phase::accept}) {
            return accept_entries_leader(entries);
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

    node_id pid_;
    std::vector<node_id> peers_;
    std::vector<node_id> nodes_;
    S internal_storage_;
    std::optional<typename S::transaction> txn_;
    high_water_mark mark_{};
    std::pair<role, phase> state_{role::follower, phase::none};
    leader_state leader_state_{};
    sequence_number current_seq_num_{};
    struct applied_sync {
        ballot n;
        std::uint64_t session;
    } applied_sync_{};
    std::optional<prepare> answered_prepare_;
    std::vector<entry> buffered_proposals_;
    std::vector<paxos_message> incoming_;
    std::vector<paxos_message> outgoing_;
    std::optional<std::error_code> error_;
    bool write_unfinished_{};
    bool recovering_{};
    std::uint64_t nonce_{};
    std::vector<std::pair<node_id, recovery_response>> recovery_responses_;
    std::optional<std::pair<node_id, konsens::get_state>> state_request_;
    std::optional<konsens::hardware_fault> hardware_fault_;
    std::vector<pending_read> read_pending_;
    konsens::read_round read_round_{};
    std::vector<indexed_read> indexed_reads_;
};

}

template <>
struct std::is_error_code_enum<konsens::paxos_errc> : std::true_type {};
