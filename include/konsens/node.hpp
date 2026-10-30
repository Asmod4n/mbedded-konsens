#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <stdexcept>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "konsens/ballot_leader_election.hpp"
#include "konsens/client.hpp"
#include "konsens/message.hpp"
#include "konsens/persistent_storage.hpp"
#include "konsens/selection.hpp"
#include "konsens/sequence_paxos.hpp"
#include "konsens/state_machine.hpp"
#include "konsens/storage.hpp"
#include "konsens/verdict.hpp"
#include "konsens/wire.hpp"

namespace konsens {

inline constexpr std::size_t max_batch_size = 1024;

static_assert(std::is_nothrow_move_constructible_v<message> && std::is_nothrow_move_constructible_v<received>);

struct node_config {
    std::uint32_t configuration_id;
    node_id pid;
    std::vector<node_id> peers;
    std::uint32_t leader_priority;
    std::uint64_t election_tick_timeout;
    std::uint64_t resend_message_tick_timeout;
    std::vector<node_id> build_from{};
    bool first_start{};
    bool sync_by_the_caller{};
};

struct sync_request {
    int file_descriptor;
    std::uint64_t sync_number;
};

inline start_kind kind_of_the_start(const node_config& config, const std::optional<clean_stop_mark>& mark,
                                    const meta_record& m, const std::optional<std::uint64_t>& config_first_idx)
{
    if (!config_first_idx && config.configuration_id > 1) {
        return start_kind::build;
    }
    if (mark && mark->pid == config.pid && mark->configuration_id == config.configuration_id && mark->meta == m) {
        return start_kind::clean;
    }
    if (config.first_start) {
        if (m.promise != ballot{}) [[unlikely]] {
            throw std::logic_error("konsens: a first start needs a store without a promise and without a clean-stop mark");
        }
        return start_kind::first;
    }
    return start_kind::retired;
}

struct logical_clock {
    std::uint64_t time;
    std::uint64_t timeout;

    bool tick_and_check_timeout()
    {
        time += 1;
        if (time < timeout) {
            return false;
        }
        time = 0;
        return true;
    }
};

struct client_read {
    std::uint64_t client_id;
    std::vector<std::byte> payload;
    konsens::ballot ballot;
    std::optional<std::uint64_t> read_index;
};

struct client_write {
    std::uint64_t client_id;
    konsens::ballot ballot;
};

class [[nodiscard]] transaction {
public:
    transaction& operator+=(const transaction& other)
    {
        throw_if_another_commit(other);
        return *this;
    }

private:
    template <storage, state_machine>
    friend class node;

    transaction(const std::uint64_t* const commit_count, const std::uint64_t number)
        : commit_count_(commit_count), number_(number)
    {
    }

    void throw_if_another_commit(const transaction& other) const
    {
        if (other.commit_count_ != commit_count_ || other.number_ != number_) [[unlikely]] {
            throw std::logic_error("konsens: the transaction belongs to another node, or its commit is done");
        }
    }

    const std::uint64_t* commit_count_;
    std::uint64_t number_;
};

template <storage S, state_machine M>
class node {
public:
    using result = std::expected<void, std::error_code>;

    static std::expected<node, std::error_code> with(const node_config& config, S internal_storage,
                                                     konsens::node_storage own_storage, M machine,
                                                     const std::uint64_t nonce)
    {
        if (config.configuration_id == 0 || config.election_tick_timeout == 0 ||
            config.resend_message_tick_timeout == 0) [[unlikely]] {
            throw std::logic_error("konsens: configuration_id and each tick timeout must not be 0");
        }
        const auto mark = own_storage.get_clean_stop_mark();
        if (!mark) [[unlikely]] {
            return std::unexpected(mark.error());
        }
        if (*mark) {
            if (auto d = own_storage.delete_the_clean_stop_mark(); !d) [[unlikely]] {
                return std::unexpected(d.error());
            }
        }
        const auto kind = read_the_kind_of_the_start(config, *mark, internal_storage);
        if (!kind) [[unlikely]] {
            return std::unexpected(kind.error());
        }
        auto sp = sequence_paxos<S>::with(
            {config.configuration_id, config.pid, config.peers, config.build_from, *kind,
             *kind == start_kind::clean ? (*mark)->verified_idx : 0, config.sync_by_the_caller},
            std::move(internal_storage),
            nonce);
        if (!sp) [[unlikely]] {
            return std::unexpected(sp.error());
        }
        if (sp->votes()) {
            if (auto r = sp->scan_the_whole_log(); !r && !error_is_a_damage(r.error())) [[unlikely]] {
                return std::unexpected(r.error());
            }
        }
        const auto proof = own_storage.get_fault_proof();
        if (!proof) [[unlikely]] {
            return std::unexpected(proof.error());
        }
        auto promise = std::expected<ballot, std::error_code>{ballot{}};
        if (sp->votes()) {
            promise = sp->get_promise();
        }
        if (!promise) [[unlikely]] {
            return std::unexpected(promise.error());
        }
        auto ble = ballot_leader_election::with(ble_config_of(config), *promise);
        node n(config, std::move(*sp), std::move(own_storage), std::move(ble), std::move(machine), nonce);
        n.build_started_ = n.seq_paxos_.building();
        if (n.seq_paxos_.votes()) {
            if (auto r = n.check_the_applied_state();
                !r && r.error() != make_error_code(paxos_errc::state_differs_from_log) && !error_is_a_damage(r.error()))
                [[unlikely]] {
                return std::unexpected(r.error());
            }
        }
        n.retire_after_a_fault_at_the_start(*proof);
        if (n.seq_paxos_.retired()) {
            static_cast<void>(n.ble_.take_outgoing_messages());
        }
        if (n.seq_paxos_.votes()) {
            if (auto r = n.apply_at_the_start(); !r) [[unlikely]] {
                return std::unexpected(r.error());
            }
        }
        n.release_the_output();
        return n;
    }

    void stop()
    {
        throw_if_stopped();
        throw_if_phase_two_is_open();
        seq_paxos_.throw_if_a_write_did_not_finish();
        seq_paxos_.throw_if_a_step_is_open();
        if (faulted_ || seq_paxos_.get_error() || seq_paxos_.retired()) [[unlikely]] {
            throw std::logic_error("konsens: stop on a node after a fault, after an error of its storage, or in retirement");
        }
        drop_held_messages_and_replies();
        reply_stopped_to_the_waiting_calls();
        seq_paxos_.commit_the_open_transaction();
        seq_paxos_.sync_the_last_commit();
        release_the_held_replies_after_the_last_sync();
        const clean_stop_mark mark{seq_paxos_.get_pid(), ble_config_.configuration_id, read_the_committed_meta(),
                                   seq_paxos_.get_verified_idx()};
        if (auto w = own_storage_.write_the_clean_stop_mark(mark); !w) [[unlikely]] {
            throw sync_error(w.error(), "konsens: the node storage failed to keep the clean-stop mark");
        }
        stopped_ = true;
    }

    bool retired() const { return seq_paxos_.retired(); }

    [[nodiscard]] std::expected<transaction, std::error_code> tick()
    {
        throw_if_stopped();
        throw_if_phase_two_is_open();
        return throw_on_a_damage([&]() -> std::expected<transaction, std::error_code> {
            if (election_clock_.tick_and_check_timeout() && seq_paxos_.votes()) {
                const auto promise = seq_paxos_.get_promise();
                if (!promise) [[unlikely]] {
                    return std::unexpected(promise.error());
                }
                if (const auto leader = ble_.hb_timeout(*promise)) {
                    if (auto r = seq_paxos_.handle_leader(ballot_above_promise(*leader, *promise)); !r) [[unlikely]] {
                        return std::unexpected(r.error());
                    }
                }
            }
            if (resend_message_clock_.tick_and_check_timeout()) {
                if (auto r = seq_paxos_.resend_message_timeout(); !r) [[unlikely]] {
                    return std::unexpected(r.error());
                }
                seq_paxos_.resend_heartbeats();
                resend_read_index_requests();
                resend_answer_ = true;
                resend_work_ = true;
            }
            return current_transaction();
        });
    }

    [[nodiscard]] std::expected<transaction, std::error_code> handle_incoming(std::vector<received> batch)
    {
        return keep_and_handle(std::move(batch));
    }

    [[nodiscard]] std::expected<transaction, std::error_code> handle_incoming(std::vector<message> batch)
    {
        return keep_and_handle(std::move(batch));
    }

    [[nodiscard]] result commit(const transaction t)
    {
        if (auto r = commit_phase_one(t); !r) [[unlikely]] {
            return r;
        }
        return commit_phase_two();
    }

    [[nodiscard]] result commit_phase_one(const transaction t)
    {
        throw_if_stopped();
        throw_if_phase_two_is_open();
        return end_the_step(t);
    }

    [[nodiscard]] result commit_phase_two()
    {
        throw_if_stopped();
        if (!phase_two_open_) [[unlikely]] {
            throw std::logic_error("konsens: commit_phase_two without commit_phase_one");
        }
        phase_two_open_ = false;
        return throw_on_a_damage([&]() -> result {
            seq_paxos_.commit_phase_two();
            tag_or_drop_the_pending_steps();
            return finish_the_step();
        });
    }

    std::optional<sync_request> take_the_sync_request()
    {
        throw_if_stopped();
        throw_if_phase_two_is_open();
        if (requested_sync_ || seq_paxos_.get_last_sync_number() <= seq_paxos_.get_synced_sync_number()) {
            return std::nullopt;
        }
        requested_sync_ = seq_paxos_.get_last_sync_number();
        return sync_request{seq_paxos_.wal_file_descriptor(), *requested_sync_};
    }

    [[nodiscard]] std::expected<transaction, std::error_code> the_log_is_synced(const std::uint64_t sync_number)
    {
        throw_if_stopped();
        throw_if_phase_two_is_open();
        throw_if_the_sync_was_not_requested(sync_number);
        requested_sync_.reset();
        return throw_on_a_damage([&]() -> std::expected<transaction, std::error_code> {
            release_the_synced_output(sync_number);
            if (auto r = seq_paxos_.the_log_is_synced(sync_number); !r) [[unlikely]] {
                return std::unexpected(r.error());
            }
            return current_transaction();
        });
    }

    [[noreturn]] void throw_after_a_failed_sync(const std::uint64_t sync_number, const std::error_code e)
    {
        throw_if_stopped();
        throw_if_phase_two_is_open();
        throw_if_the_sync_was_not_requested(sync_number);
        requested_sync_.reset();
        held_steps_.clear();
        drop_held_messages_and_replies();
        seq_paxos_.the_sync_failed();
        throw sync_error(e, "konsens: the caller failed to sync the log");
    }

private:
    void throw_if_the_sync_was_not_requested(const std::uint64_t sync_number) const
    {
        if (requested_sync_ != sync_number) [[unlikely]] {
            throw std::logic_error("konsens: a report of a sync that take_the_sync_request did not give");
        }
    }

    std::vector<node_id> peers_with_held_messages() const
    {
        std::vector<node_id> peers;
        for (const auto& step : held_steps_) {
            for (const auto& m : step.messages) {
                peers.push_back(std::visit([](const auto& x) { return x.to; }, m));
            }
        }
        return peers;
    }

    void release_the_synced_output(const std::uint64_t sync_number)
    {
        const auto synced = std::ranges::find_if(
            held_steps_, [sync_number](const held_step& h) { return !h.sync_number || *h.sync_number > sync_number; });
        for (auto& h : std::ranges::subrange(held_steps_.begin(), synced)) {
            std::ranges::move(h.messages, std::back_inserter(released_messages_));
            std::ranges::move(h.replies, std::back_inserter(released_replies_));
        }
        held_steps_.erase(held_steps_.begin(), synced);
    }

    void release_the_held_replies_after_the_last_sync()
    {
        for (auto& h : std::exchange(held_steps_, {})) {
            std::ranges::move(h.replies, std::back_inserter(released_replies_));
        }
    }

    void release_the_read_replies_from(const std::size_t first)
    {
        const auto reads = std::next(client_replies_.begin(), static_cast<std::ptrdiff_t>(first));
        std::ranges::move(reads, client_replies_.end(), std::back_inserter(released_replies_));
        client_replies_.erase(reads, client_replies_.end());
    }

    result end_the_step(const transaction t)
    {
        return throw_on_a_damage([&]() -> result {
            current_transaction().throw_if_another_commit(t);
            *commit_count_ += 1;
            if (auto r = propose_before_the_step(); !r) [[unlikely]] {
                static_cast<void>(seq_paxos_.commit());
                drop_held_messages_and_replies();
                return r;
            }
            if (auto r = seq_paxos_.end_step(); !r) [[unlikely]] {
                drop_held_messages_and_replies();
                return r;
            }
            if (seq_paxos_.txn_ && seq_paxos_.votes()) {
                if (auto r = serve_clients_in_the_step(); !r) [[unlikely]] {
                    roll_back_the_step(r.error());
                    static_cast<void>(seq_paxos_.commit());
                    drop_held_messages_and_replies();
                    return r;
                }
            }
            const auto w = seq_paxos_.commit_phase_one();
            if (!w) [[unlikely]] {
                drop_held_messages_and_replies();
                return w;
            }
            release_the_output_before_phase_two();
            phase_two_open_ = true;
            return {};
        });
    }

    void release_the_output_before_phase_two()
    {
        for (auto& m : seq_paxos_.take_the_messages_before_the_sync(peers_with_held_messages())) {
            released_messages_.emplace_back(std::move(m));
        }
        std::ranges::move(replies_before_the_sync_, std::back_inserter(released_replies_));
        replies_before_the_sync_.clear();
    }

    result finish_the_step()
    {
        return throw_on_a_damage([&]() -> result {
            if (build_started_ && !seq_paxos_.building() && seq_paxos_.votes()) {
                start_the_election_after_the_build();
            }
            if (!seq_paxos_.votes()) {
                release_the_output();
                return {};
            }
            drop_the_reads_for_the_stop_sign();
            const auto promise = seq_paxos_.get_promise();
            if (!promise) [[unlikely]] {
                drop_held_messages_and_replies();
                return std::unexpected(promise.error());
            }
            drop_writes_of_an_old_promise(*promise);
            const auto replies_before_the_reads = client_replies_.size();
            serve_reads(*promise);
            if (sync_by_the_caller_) {
                release_the_read_replies_from(replies_before_the_reads);
            }
            release_the_output();
            return {};
        });
    }

public:
    [[nodiscard]] std::expected<transaction, std::error_code> reconfigure(const cluster_config& next)
    {
        throw_if_stopped();
        throw_if_phase_two_is_open();
        validate_next_cluster(ble_config_.configuration_id, ble_config_.pid, ble_config_.peers, next);
        auto payload = encode(next);
        if (!payload) [[unlikely]] {
            return std::unexpected(payload.error());
        }
        return append_an_entry({entry_kind::stop_sign, std::move(*payload)});
    }

    const std::optional<cluster_config>& reconfigured() const { return next_config_; }

    [[nodiscard]] std::expected<transaction, std::error_code> append(entry e)
    {
        if (entry_is_a_stop_sign(e)) [[unlikely]] {
            throw std::logic_error("konsens: a StopSign comes from reconfigure, not from append");
        }
        return append_an_entry(std::move(e));
    }

private:
    std::expected<transaction, std::error_code> append_an_entry(entry e)
    {
        throw_if_stopped();
        throw_if_phase_two_is_open();
        return throw_on_a_damage([&]() -> std::expected<transaction, std::error_code> {
            if (auto r = seq_paxos_.refuse_if_retired(); !r) [[unlikely]] {
                return std::unexpected(r.error());
            }
            if (auto r = refuse_if_reconfiguring(); !r) [[unlikely]] {
                return std::unexpected(r.error());
            }
            if (entry_is_done(e)) [[unlikely]] {
                throw std::logic_error("konsens: only the leader appends a done record");
            }
            if (e.kind == entry_kind::stop_sign && seq_paxos_.get_state().first == role::leader &&
                holds_a_request_in_work()) [[unlikely]] {
                return std::unexpected(make_error_code(client_errc::retry));
            }
            if (auto r = seq_paxos_.append(std::move(e)); !r) [[unlikely]] {
                return std::unexpected(r.error());
            }
            return current_transaction();
        });
    }

public:
    [[nodiscard]] std::expected<transaction, std::error_code> register_client(const std::uint64_t client_id)
    {
        throw_if_stopped();
        throw_if_phase_two_is_open();
        return throw_on_a_damage([&]() -> std::expected<transaction, std::error_code> {
            if (auto r = seq_paxos_.refuse_if_retired(); !r) [[unlikely]] {
                return std::unexpected(r.error());
            }
            if (auto r = seq_paxos_.refuse_if_building(); !r) [[unlikely]] {
                return std::unexpected(r.error());
            }
            if (auto r = refuse_if_reconfiguring(); !r) [[unlikely]] {
                return std::unexpected(r.error());
            }
            if (auto r = propose_a_request({entry_kind::register_client, {}, 0, 0, client_id, seq_paxos_.get_pid()}); !r)
                [[unlikely]] {
                return std::unexpected(r.error());
            }
            registrations_.insert(client_id);
            return current_transaction();
        });
    }

    [[nodiscard]] std::expected<transaction, std::error_code> client_request(const std::uint64_t client_id,
                                                                             const std::uint64_t session,
                                                                             const std::uint64_t request_number,
                                                                             std::vector<std::byte> payload)
    {
        throw_if_stopped();
        throw_if_phase_two_is_open();
        return throw_on_a_damage([&]() -> std::expected<transaction, std::error_code> {
            if (auto r = seq_paxos_.refuse_if_retired(); !r) [[unlikely]] {
                return std::unexpected(r.error());
            }
            if (auto r = seq_paxos_.refuse_if_building(); !r) [[unlikely]] {
                return std::unexpected(r.error());
            }
            if (auto r = refuse_if_reconfiguring(); !r) [[unlikely]] {
                return std::unexpected(r.error());
            }
            if (request_number == 0) [[unlikely]] {
                throw std::logic_error("konsens: a request number starts at 1");
            }
            const auto promise = seq_paxos_.get_promise();
            if (!promise) [[unlikely]] {
                return std::unexpected(promise.error());
            }
            if (auto r = propose_a_request({entry_kind::client_request, std::move(payload), session, request_number}); !r)
                [[unlikely]] {
                return std::unexpected(r.error());
            }
            requests_.insert_or_assign({session, request_number}, client_write{client_id, *promise});
            return current_transaction();
        });
    }

    [[nodiscard]] std::expected<transaction, std::error_code> client_query(const std::uint64_t client_id,
                                                                           std::vector<std::byte> payload)
    {
        throw_if_stopped();
        throw_if_phase_two_is_open();
        return throw_on_a_damage([&]() -> std::expected<transaction, std::error_code> {
            if (auto r = seq_paxos_.refuse_if_retired(); !r) [[unlikely]] {
                return std::unexpected(r.error());
            }
            if (auto r = seq_paxos_.refuse_if_building(); !r) [[unlikely]] {
                return std::unexpected(r.error());
            }
            if (auto r = refuse_if_reconfiguring(); !r) [[unlikely]] {
                return std::unexpected(r.error());
            }
            const auto read = last_read_ + 1;
            if (auto r = seq_paxos_.start_read(read); !r) [[unlikely]] {
                return std::unexpected(r.error());
            }
            const auto promise = seq_paxos_.get_promise();
            if (!promise) [[unlikely]] {
                return std::unexpected(promise.error());
            }
            last_read_ = read;
            reads_.emplace(read, client_read{client_id, std::move(payload), *promise, std::nullopt});
            return current_transaction();
        });
    }

    [[nodiscard]] std::expected<transaction, std::error_code> expire_session(const std::uint64_t client_id,
                                                                             const std::uint64_t session)
    {
        throw_if_stopped();
        throw_if_phase_two_is_open();
        return throw_on_a_damage([&]() -> std::expected<transaction, std::error_code> {
            if (auto r = seq_paxos_.refuse_if_retired(); !r) [[unlikely]] {
                return std::unexpected(r.error());
            }
            if (auto r = seq_paxos_.refuse_if_building(); !r) [[unlikely]] {
                return std::unexpected(r.error());
            }
            if (auto r = refuse_if_reconfiguring(); !r) [[unlikely]] {
                return std::unexpected(r.error());
            }
            if (!read_the_newest_state([session](const auto& t) { return t.find_session(session); })) [[unlikely]] {
                return std::unexpected(make_error_code(client_errc::session_expired));
            }
            const auto applied_idx = read_the_newest_state([](const auto& t) { return t.get_meta(); }).applied_idx;
            const entry expiry{entry_kind::expire_session, {}, session};
            const auto undecided = holds_the_request(applied_idx, expiry);
            if (!undecided) [[unlikely]] {
                return std::unexpected(undecided.error());
            }
            if (!*undecided && !std::ranges::contains(waiting_, expiry)) {
                if (auto r = propose_a_request(expiry); !r) [[unlikely]] {
                    return std::unexpected(r.error());
                }
            }
            expirations_.insert_or_assign(session, client_id);
            return current_transaction();
        });
    }

    node(node&&) noexcept = default;
    node& operator=(node&&) noexcept = default;
    ~node() = default;

    std::vector<client_reply> take_client_replies() { return std::exchange(released_replies_, {}); }

    std::vector<message> take_outgoing_messages() { return std::exchange(released_messages_, {}); }

    std::vector<fault_proof> take_log_records() { return std::exchange(released_log_records_, {}); }

    std::expected<std::vector<entry>, std::error_code> read_decided_suffix(std::uint64_t from_idx) const
    {
        return seq_paxos_.read_decided_suffix(from_idx);
    }

    const sequence_paxos<S>& get_sequence_paxos() const { return seq_paxos_; }
    bool waits_for_the_operator() const { return waits_for_the_operator_; }
    std::expected<std::optional<fault_proof>, std::error_code> get_fault_proof() const
    {
        return own_storage_.get_fault_proof();
    }
    const ballot_leader_election& get_ballot_leader_election() const { return ble_; }
    const M& get_state_machine() const { return machine_; }
    const std::optional<konsens::selection>& get_selection() const { return selection_; }
    const std::optional<refused>& get_refusal() const { return refusal_; }
    std::vector<std::uint64_t> get_own_result_indexes() const
    {
        std::vector<std::uint64_t> indexes;
        for (const auto& o : own_) {
            indexes.push_back(o.first);
        }
        return indexes;
    }
    std::uint64_t get_applied_idx() const
    {
        return read_the_newest_state([](const auto& t) { return t.get_meta(); }).applied_idx;
    }
    std::optional<session_record> find_client_session(const std::uint64_t session) const
    {
        return read_the_newest_state([session](const auto& t) { return t.find_session(session); });
    }

    [[nodiscard]] std::expected<transaction, std::error_code> reconnected(const node_id pid)
    {
        throw_if_stopped();
        throw_if_phase_two_is_open();
        return throw_on_a_damage([&]() -> std::expected<transaction, std::error_code> {
            if (auto r = seq_paxos_.reconnected(pid); !r) [[unlikely]] {
                return std::unexpected(r.error());
            }
            if (seq_paxos_.get_state() == std::pair{role::leader, phase::accept} &&
                tasks_ballot_ == seq_paxos_.get_leader_state().n_leader) {
                for (const auto& [index, e] : tasks_) {
                    send_work(index, e, pid);
                }
            }
            return current_transaction();
        });
    }

private:
    template <typename T>
    std::vector<T>& batch_of()
    {
        if constexpr (std::is_same_v<T, received>) {
            return received_batch_;
        } else {
            return message_batch_;
        }
    }

    void throw_if_batch_exceeds_max_batch_size(const std::size_t count) const
    {
        if (count > max_batch_size - received_batch_.size() - message_batch_.size()) [[unlikely]] {
            throw std::logic_error("konsens: more messages than max_batch_size wait for a commit");
        }
    }

    template <typename T>
    std::expected<transaction, std::error_code> keep_and_handle(std::vector<T> batch)
    {
        throw_if_stopped();
        throw_if_phase_two_is_open();
        return throw_on_a_damage([&]() -> std::expected<transaction, std::error_code> {
            throw_if_batch_exceeds_max_batch_size(batch.size());
            auto& held = batch_of<T>();
            const auto first = held.size();
            held.insert(held.end(), std::make_move_iterator(batch.begin()), std::make_move_iterator(batch.end()));
            for (const auto& m : std::span(held).subspan(first)) {
                if (auto r = handle_one(m); !r) [[unlikely]] {
                    return std::unexpected(r.error());
                }
            }
            return current_transaction();
        });
    }

    transaction current_transaction() const
    {
        if (!commit_count_) [[unlikely]] {
            throw std::logic_error("konsens: the node was moved from");
        }
        return {commit_count_.get(), *commit_count_};
    }

    void throw_if_stopped() const
    {
        if (stopped_) [[unlikely]] {
            throw std::logic_error("konsens: a call of a node after its stop");
        }
    }

    void throw_if_phase_two_is_open() const
    {
        if (phase_two_open_) [[unlikely]] {
            throw std::logic_error("konsens: a call of a node between commit_phase_one and commit_phase_two");
        }
    }

    static std::expected<start_kind, std::error_code> read_the_kind_of_the_start(
        const node_config& config, const std::optional<clean_stop_mark>& mark, const S& internal_storage)
    {
        const auto t = internal_storage.begin_read_only();
        if (!t) [[unlikely]] {
            return std::unexpected(t.error());
        }
        const auto m = t->get_meta();
        if (!m) [[unlikely]] {
            if (error_is_a_damage(m.error())) {
                return start_kind::retired;
            }
            return std::unexpected(m.error());
        }
        const auto first = t->get_config_first_idx();
        if (!first) [[unlikely]] {
            if (error_is_a_damage(first.error())) {
                return start_kind::retired;
            }
            return std::unexpected(first.error());
        }
        return kind_of_the_start(config, mark, *m, *first);
    }

    void reply_stopped_to_the_waiting_calls()
    {
        const auto stopped = std::unexpected(make_error_code(client_errc::stopped));
        for (const auto client_id : std::exchange(registrations_, {})) {
            released_replies_.push_back({client_id, 0, stopped});
        }
        for (const auto& [request, w] : std::exchange(requests_, {})) {
            released_replies_.push_back({w.client_id, request.first, stopped});
        }
        for (const auto& [session, client_id] : std::exchange(expirations_, {})) {
            released_replies_.push_back({client_id, session, stopped});
        }
        for (const auto& [read, r] : std::exchange(reads_, {})) {
            released_replies_.push_back({r.client_id, 0, stopped});
        }
    }

    result handle_one(const received& r)
    {
        return std::visit([this, &r](const auto& body) { return this->handle(r.from, r.to, *body); }, r.msg);
    }

    result handle_one(const message& m)
    {
        return std::visit(
            [this](const auto& outer) -> result {
                return std::visit([this, &outer](const auto& msg) { return this->handle(outer.from, outer.to, msg); },
                                  outer.msg);
            },
            m);
    }

    struct held_output {
        std::vector<message> messages;
        std::vector<client_reply> replies;
        std::vector<fault_proof> log_records;
        std::vector<received> received_batch;
        std::vector<message> message_batch;
    };

    held_output take_held_output()
    {
        held_output out;
        for (auto& m : seq_paxos_.take_outgoing_messages()) {
            out.messages.emplace_back(std::move(m));
        }
        for (auto& m : ble_.take_outgoing_messages()) {
            out.messages.emplace_back(std::move(m));
        }
        std::ranges::move(execute_verify_outgoing_, std::back_inserter(out.messages));
        execute_verify_outgoing_.clear();
        out.replies = std::exchange(replies_before_the_sync_, {});
        std::ranges::move(client_replies_, std::back_inserter(out.replies));
        client_replies_.clear();
        out.log_records = std::exchange(log_records_, {});
        out.received_batch = std::exchange(received_batch_, {});
        out.message_batch = std::exchange(message_batch_, {});
        return out;
    }

    void release_the_output()
    {
        if (!sync_by_the_caller_) {
            release(take_held_output());
            return;
        }
        hold_the_output_until_its_sync();
    }

    struct held_step {
        std::optional<std::uint64_t> sync_number;
        std::vector<message> messages;
        std::vector<client_reply> replies;
    };

    void tag_or_drop_the_pending_steps()
    {
        if (seq_paxos_.the_transaction_holds_ended_steps()) {
            return;
        }
        const auto pending = [](const held_step& h) { return !h.sync_number; };
        if (seq_paxos_.get_error()) [[unlikely]] {
            std::erase_if(held_steps_, pending);
            return;
        }
        for (auto& h : held_steps_ | std::views::filter(pending)) {
            h.sync_number = seq_paxos_.get_last_sync_number();
        }
        release_the_synced_output(seq_paxos_.get_synced_sync_number());
    }

    void roll_back_the_step(const std::error_code cause)
    {
        if (seq_paxos_.the_transaction_holds_ended_steps()) [[unlikely]] {
            static_cast<void>(seq_paxos_.stop_voting(cause));
            return;
        }
        seq_paxos_.roll_back_the_transaction();
    }

    void hold_the_output_until_its_sync()
    {
        tag_or_drop_the_pending_steps();
        for (auto& m : ble_.take_outgoing_messages()) {
            released_messages_.emplace_back(std::move(m));
        }
        std::ranges::move(replies_before_the_sync_, std::back_inserter(released_replies_));
        replies_before_the_sync_.clear();
        std::ranges::move(log_records_, std::back_inserter(released_log_records_));
        log_records_.clear();
        received_batch_.clear();
        message_batch_.clear();
        held_step step{seq_paxos_.the_transaction_holds_ended_steps()
                           ? std::nullopt
                           : std::optional{seq_paxos_.get_last_sync_number()},
                       {},
                       std::exchange(client_replies_, {})};
        for (auto& m : seq_paxos_.take_outgoing_messages()) {
            step.messages.emplace_back(std::move(m));
        }
        std::ranges::move(execute_verify_outgoing_, std::back_inserter(released_messages_));
        execute_verify_outgoing_.clear();
        if (held_steps_.empty() && step.sync_number && *step.sync_number <= seq_paxos_.get_synced_sync_number()) {
            std::ranges::move(step.messages, std::back_inserter(released_messages_));
            std::ranges::move(step.replies, std::back_inserter(released_replies_));
            return;
        }
        if (!step.messages.empty() || !step.replies.empty()) {
            held_steps_.push_back(std::move(step));
        }
    }

    void release(held_output&& out)
    {
        std::ranges::move(out.messages, std::back_inserter(released_messages_));
        std::ranges::move(out.replies, std::back_inserter(released_replies_));
        std::ranges::move(out.log_records, std::back_inserter(released_log_records_));
    }

    void drop_held_messages_and_replies()
    {
        tag_or_drop_the_pending_steps();
        static_cast<void>(seq_paxos_.take_outgoing_messages());
        static_cast<void>(ble_.take_outgoing_messages());
        execute_verify_outgoing_.clear();
        client_replies_.clear();
        replies_before_the_sync_.clear();
        log_records_.clear();
        received_batch_.clear();
        message_batch_.clear();
    }

    template <typename T>
    result handle(node_id from, node_id to, const T& msg)
    {
        if constexpr (std::is_same_v<T, answer> || std::is_same_v<T, try_again> || std::is_same_v<T, refused> ||
                      std::is_same_v<T, work>) {
            if (to != seq_paxos_.get_pid() || !std::ranges::contains(seq_paxos_.peers_, from) ||
                seq_paxos_.building() || seq_paxos_.retired()) [[unlikely]] {
                return {};
            }
            if constexpr (std::is_same_v<T, answer>) {
                answers_in_.emplace_back(from, msg);
            } else if constexpr (std::is_same_v<T, try_again>) {
                try_agains_in_.emplace_back(from, msg);
            } else if constexpr (std::is_same_v<T, work>) {
                works_in_.emplace_back(from, msg);
            } else {
                refusals_in_.emplace_back(from, msg);
            }
            return {};
        } else if constexpr (requires { paxos_msg{std::in_place_type<T>, msg}; }) {
            return seq_paxos_.handle(from, to, msg);
        } else {
            if (seq_paxos_.building() || seq_paxos_.retired()) {
                return {};
            }
            ble_.handle(ble_message{from, to, heartbeat_msg{std::in_place_type<T>, msg}});
            return {};
        }
    }

    using nested_transaction = nested_transaction_of<typename S::transaction>;

    node(const node_config& config, sequence_paxos<S> seq_paxos, konsens::node_storage own_storage,
         ballot_leader_election ble, M machine, const std::uint64_t nonce)
        : seq_paxos_(std::move(seq_paxos)), own_storage_(std::move(own_storage)), ble_(std::move(ble)),
          ble_config_(ble_config_of(config)), election_clock_{0, config.election_tick_timeout},
          resend_message_clock_{0, config.resend_message_tick_timeout}, machine_(std::move(machine)), nonce_(nonce),
          sync_by_the_caller_(config.sync_by_the_caller)
    {
        nodes_ = config.peers;
        nodes_.push_back(config.pid);
        std::ranges::sort(nodes_);
    }

    template <typename T>
    static T read_or_throw(std::expected<T, std::error_code> r)
    {
        if (!r) [[unlikely]] {
            throw read_error(r.error(), "konsens: the storage failed to read the state");
        }
        return std::move(*r);
    }

    static void write_or_throw(const std::expected<void, std::error_code>& written)
    {
        if (!written) [[unlikely]] {
            throw write_error(written.error(), "konsens: the storage failed to write the state");
        }
    }

    template <typename Read>
    auto read_the_newest_state(const Read read) const
    {
        if (seq_paxos_.txn_) {
            return read_or_throw(read(*seq_paxos_.txn_));
        }
        return read_or_throw(read(read_or_throw(seq_paxos_.internal_storage_.begin_read_only())));
    }

    meta_record read_the_committed_meta() const
    {
        return read_or_throw(read_or_throw(seq_paxos_.internal_storage_.begin_read_only()).get_meta());
    }

    nested_transaction begin_the_nested_transaction()
    {
        auto n = seq_paxos_.txn_->begin_nested();
        if (!n) [[unlikely]] {
            throw write_error(n.error(), "konsens: the storage failed to begin a nested transaction");
        }
        return std::move(*n);
    }

    result check_the_applied_state()
    {
        const auto m = read_the_committed_meta();
        return seq_paxos_.check_applied_chain_crc(m.applied_idx, m.applied_chain_crc);
    }

    static ble_config ble_config_of(const node_config& config)
    {
        return {config.configuration_id, config.pid, config.peers, config.leader_priority};
    }

    void retire_after_a_fault_at_the_start(const std::optional<fault_proof>& proof)
    {
        const auto e = seq_paxos_.get_error();
        if (e == make_error_code(storage_errc::missing_meta_record)) [[unlikely]] {
            waits_for_the_operator_ = true;
        }
        if (proof || (e && error_is_a_damage(*e))) [[unlikely]] {
            seq_paxos_.retired_ = true;
        }
    }

    std::uint64_t index_of_the_damage() const
    {
        const auto fault = seq_paxos_.get_hardware_fault();
        if (seq_paxos_.get_error() != make_error_code(paxos_errc::hardware_fault) || !fault) {
            return 0;
        }
        return fault->index;
    }

    template <typename Step>
    auto throw_on_a_damage(const Step step) -> decltype(step())
    {
        auto r = step();
        const auto e = seq_paxos_.get_error();
        if (e && error_is_a_damage(*e) && !waits_for_the_operator_ && !seq_paxos_.retired()) [[unlikely]] {
            const auto fault = seq_paxos_.get_hardware_fault();
            const bool known = *e == make_error_code(paxos_errc::hardware_fault) && fault;
            write_the_proof_and_throw({proof_kind::damage, index_of_the_damage(), seq_paxos_.mark_.promise,
                                       known ? fault->crc : 0, known ? fault->other_crc : 0},
                                      *e, "konsens: a step found a damage of the own log");
        }
        return r;
    }

    [[noreturn]] void write_the_proof_and_throw(const fault_proof& proof, const std::error_code e,
                                                const char* const what)
    {
        faulted_ = true;
        if (auto w = own_storage_.write_the_proof(proof); !w) [[unlikely]] {
            throw sync_error(w.error(), "konsens: the node storage failed to keep the proof of a fault");
        }
        throw node_fault(e, what);
    }

    void start_the_election_after_the_build()
    {
        build_started_ = false;
        ble_ = ballot_leader_election::with(ble_config_, ballot{});
    }

    ballot ballot_above_promise(const ballot elected, const ballot promise) const
    {
        if (elected.pid != seq_paxos_.get_pid() || elected > promise || seq_paxos_.get_state().first == role::leader) {
            return elected;
        }
        return {elected.config_id, promise.n + 1, elected.priority, elected.pid};
    }

    result serve_clients_in_the_step()
    {
        if (auto r = apply_the_done_records(); !r) [[unlikely]] {
            return r;
        }
        return execute_verify();
    }

    result apply_at_the_start()
    {
        if (auto r = seq_paxos_.begin_step(); !r) [[unlikely]] {
            return r;
        }
        if (auto r = read_the_applied_stop_sign(); !r) [[unlikely]] {
            roll_back_the_step(r.error());
            return r;
        }
        if (auto r = apply_the_done_records(); !r) [[unlikely]] {
            roll_back_the_step(r.error());
            return std::unexpected(r.error());
        }
        if (auto r = seq_paxos_.commit(); !r) [[unlikely]] {
            return r;
        }
        return finish_the_step();
    }

    result read_the_applied_stop_sign()
    {
        const auto m = seq_paxos_.read_meta();
        if (!m) [[unlikely]] {
            return seq_paxos_.stop_voting(m.error());
        }
        const auto last = sequence_paxos<S>::read_range(
            *seq_paxos_.txn_, m->applied_idx - std::min(m->applied_idx, std::uint64_t{1}), m->applied_idx);
        if (!last) [[unlikely]] {
            return seq_paxos_.stop_voting(last.error());
        }
        if (!last->empty() && entry_is_a_stop_sign(last->front()) && m->applied_idx > seq_paxos_.get_config_first_idx())
            [[unlikely]] {
            next_config_ = read_the_next_configuration(last->front());
        }
        return {};
    }

    void serve_reads(const ballot promise)
    {
        index_reads();
        drop_reads_of_an_old_promise(promise);
        answer_indexed_reads();
    }

    struct own_result {
        entry request;
        request_result result;
        std::vector<std::byte> bytes;
        std::uint32_t crc;
    };

    struct done_log {
        std::uint64_t applied_idx;
        std::span<const entry> done;

        std::uint64_t log_len() const { return applied_idx + done.size(); }
    };

    result propose_a_request(entry e)
    {
        if (seq_paxos_.get_state() == std::pair{role::leader, phase::accept}) {
            waiting_.push_back(std::move(e));
            return {};
        }
        return seq_paxos_.append(std::move(e));
    }

    result refuse_if_reconfiguring() const
    {
        if (next_config_ || seq_paxos_.holds_a_stop_sign() || std::ranges::any_of(waiting_, entry_is_a_stop_sign) ||
            std::ranges::any_of(tasks_, [](const auto& t) { return entry_is_a_stop_sign(t.second); })) [[unlikely]] {
            return std::unexpected(make_error_code(client_errc::reconfiguring));
        }
        return {};
    }

    void drop_the_reads_for_the_stop_sign()
    {
        if (!refuse_if_reconfiguring()) [[unlikely]] {
            std::erase_if(reads_, [this](const auto& r) {
                if (r.second.read_index) {
                    return false;
                }
                client_replies_.push_back(
                    {r.second.client_id, 0, std::unexpected(make_error_code(client_errc::reconfiguring))});
                return true;
            });
        }
    }

    bool holds_a_request_in_work() const
    {
        return std::ranges::any_of(waiting_, entry_is_a_request) ||
               std::ranges::any_of(seq_paxos_.buffered_proposals_, entry_is_a_request) ||
               std::ranges::any_of(tasks_, [](const auto& t) { return entry_is_a_request(t.second); });
    }

    std::expected<bool, std::error_code> holds_the_request(const std::uint64_t from_idx, const entry& e)
    {
        const auto same = [&e](const entry& x) { return request_of(x) == e; };
        if (std::ranges::any_of(waiting_, same) || std::ranges::any_of(seq_paxos_.buffered_proposals_, same) ||
            std::ranges::any_of(tasks_, [&same](const auto& t) { return same(t.second); })) {
            return true;
        }
        if (auto r = seq_paxos_.begin_step(); !r) [[unlikely]] {
            return std::unexpected(r.error());
        }
        const auto m = seq_paxos_.read_meta();
        if (!m) [[unlikely]] {
            return seq_paxos_.stop_voting(m.error());
        }
        const auto log = sequence_paxos<S>::read_range(*seq_paxos_.txn_, std::min(from_idx, m->log_len), m->log_len);
        if (!log) [[unlikely]] {
            return seq_paxos_.stop_voting(log.error());
        }
        return std::ranges::any_of(*log, same);
    }

    result check_the_done_records()
    {
        if (!seq_paxos_.votes() || seq_paxos_.get_state().second != phase::accept) {
            return {};
        }
        if (auto r = seq_paxos_.begin_step(); !r) [[unlikely]] {
            return r;
        }
        const auto m = seq_paxos_.read_meta();
        if (!m) [[unlikely]] {
            return seq_paxos_.stop_voting(m.error());
        }
        if (refusal_ && refusal_->n != m->promise) {
            refusal_.reset();
        }
        const auto first = std::min(seq_paxos_.get_verified_idx(), m->applied_idx);
        const auto tail = sequence_paxos<S>::read_range(*seq_paxos_.txn_, std::min(first, m->log_len), m->log_len);
        if (!tail) [[unlikely]] {
            return seq_paxos_.stop_voting(tail.error());
        }
        const auto at = [&tail, first](const std::uint64_t k) -> const entry& { return (*tail)[k - first - 1]; };
        auto verified = seq_paxos_.get_verified_idx();
        std::optional<std::uint64_t> passed;
        std::optional<refusal_reason> reason;
        std::optional<nested_transaction> base;
        auto base_below = m->applied_idx + 1;
        for (;;) {
            const auto index = verified + 1;
            if (index > m->log_len || (refusal_ && refusal_->index == index)) {
                break;
            }
            if (index > m->decided_idx && index > m->applied_idx) {
                const done_log b{m->applied_idx, std::span<const entry>(*tail).subspan(m->applied_idx - first,
                                                                                      index - 1 - m->applied_idx)};
                if (!base) {
                    base.emplace(begin_the_nested_transaction());
                }
                write_the_done_results(b, base_below, index, *base);
                base_below = index;
                reason = check_the_done_record(b, at(index), *base);
                if (reason) {
                    break;
                }
            }
            passed = index;
            verified = m->log_len;
            for (auto k = index + 1; k <= m->log_len; ++k) {
                if (entry_is_a_request(at(k))) {
                    verified = k - 1;
                    break;
                }
            }
        }
        base.reset();
        if (passed) {
            if (auto r = seq_paxos_.pass_the_checked_done_record(*passed); !r) [[unlikely]] {
                return r;
            }
        }
        if (reason) {
            refusal_ = refused{m->promise, verified + 1, *reason};
            send_the_refusal(m->promise);
        }
        return {};
    }

    void send_the_refusal(const ballot promise)
    {
        if (!refusal_ || refusal_->n != promise) {
            return;
        }
        if (promise.pid == seq_paxos_.get_pid()) {
            if (selection_ && selection_->get_ballot() == promise) {
                selection_->count_a_refusal(promise.pid, refusal_->index);
            }
            return;
        }
        if (promise.pid != 0) {
            execute_verify_outgoing_.emplace_back(paxos_message{seq_paxos_.get_pid(), promise.pid, *refusal_});
        }
    }

    std::span<const std::byte> find_the_own_result_on(const done_log& b, const entry& record) const
    {
        for (auto k = b.applied_idx + 1; k <= b.log_len(); ++k) {
            const auto& e = b.done[k - b.applied_idx - 1];
            if (!entry_is_a_request(e)) {
                continue;
            }
            const auto o = own_.find(k);
            if (o == own_.end() || e.rejected || o->second.bytes != e.result) {
                return {};
            }
        }
        const auto o = own_.find(b.log_len() + 1);
        if (o == own_.end() || o->second.request != request_of(record)) {
            return {};
        }
        return o->second.bytes;
    }

    std::optional<refusal_reason> check_the_done_record(const done_log& b, const entry& e, nested_transaction& w)
    {
        if (!entry_is_a_request(e)) {
            return std::nullopt;
        }
        const auto q = b.log_len() + 1;
        if (compute_result_crc(q, e.result) != e.result_crc) [[unlikely]] {
            return refusal_reason::crc_mismatch;
        }
        auto own = find_the_own_result_on(b, e);
        std::vector<std::byte> ran;
        if (own.empty()) {
            ran = run_on_the_base(request_of(e), q, w).bytes;
            own = ran;
        }
        if (e.rejected) {
            return check_the_rejected_record(e, q, own);
        }
        const bool again = !results_agree(own, e.result, float_rule_of(machine_));
        const bool still =
            again && !results_agree(run_on_the_base(request_of(e), q, w).bytes, e.result, float_rule_of(machine_));
        const bool listed = std::ranges::contains(e.out, seq_paxos_.get_pid());
        if (listed ? !still : still) {
            return listed ? refusal_reason::wrong_list : refusal_reason::result_differs;
        }
        if (!decode_result(e.result)) [[unlikely]] {
            return refusal_reason::result_differs;
        }
        return std::nullopt;
    }

    std::optional<refusal_reason> check_the_rejected_record(const entry& e, const std::uint64_t q,
                                                            const std::span<const std::byte> own) const
    {
        const auto answers = decode_chosen_results(e.result);
        if (!answers) [[unlikely]] {
            return refusal_reason::rejected_has_a_majority;
        }
        const auto self = seq_paxos_.get_pid();
        std::vector<std::span<const std::byte>> counted{own};
        for (const auto& c : *answers) {
            if (c.node != self && c.chosen.index == q) {
                counted.push_back(c.chosen.result);
            }
        }
        const auto majority = nodes_.size() / 2 + 1;
        for (const auto& c : counted) {
            const auto agreeing = std::ranges::count_if(
                counted, [this, &c](const auto& x) { return results_agree(x, c, float_rule_of(machine_)); });
            if (static_cast<std::size_t>(agreeing) >= majority) {
                return refusal_reason::rejected_has_a_majority;
            }
        }
        return std::nullopt;
    }

    result propose_before_the_step()
    {
        if (auto r = check_the_done_records(); !r) [[unlikely]] {
            return r;
        }
        if (!seq_paxos_.votes()) {
            return {};
        }
        if (seq_paxos_.get_state() != std::pair{role::leader, phase::accept}) {
            for (auto& e : std::exchange(waiting_, {})) {
                if (auto r = seq_paxos_.append(std::move(e)); !r) [[unlikely]] {
                    return r;
                }
            }
            if (seq_paxos_.get_state().first != role::leader) {
                answers_in_.clear();
            }
            return {};
        }
        if (auto r = seq_paxos_.begin_step(); !r) [[unlikely]] {
            return r;
        }
        const auto m = seq_paxos_.read_meta();
        if (!m) [[unlikely]] {
            return seq_paxos_.stop_voting(m.error());
        }
        const auto n = seq_paxos_.get_leader_state().n_leader;
        if (!selection_ || selection_->get_ballot() != n) {
            selection_ = selection::with(n, nodes_, m->log_len, float_rule_of(machine_));
        }
        assign_the_tasks(n, m->log_len);
        count_the_answers();
        for (const auto& [from, x] : std::exchange(refusals_in_, {})) {
            if (x.n == selection_->get_ballot()) {
                selection_->count_a_refusal(from, x.index);
            }
        }
        if (const auto refused = selection_->find_the_entries_that_a_majority_refused(); !refused.empty()) [[unlikely]] {
            write_the_proof_and_throw({proof_kind::refused_by_a_majority, refused.front(), selection_->get_ballot(), 0, 0},
                                      make_error_code(paxos_errc::result_differs),
                                      "konsens: a majority refused a done record of this leader");
        }
        return append_the_done_records(m->log_len);
    }

    void assign_the_tasks(const ballot n, const std::uint64_t log_len)
    {
        if (tasks_ballot_ != n) {
            tasks_.clear();
            tasks_ballot_ = n;
        }
        auto next = log_len + tasks_.size() + 1;
        if (resend_work_) {
            for (const auto& [index, e] : tasks_) {
                send_work(index, e, std::nullopt);
            }
            resend_work_ = false;
        }
        auto proposals = std::exchange(seq_paxos_.buffered_proposals_, {});
        std::ranges::move(std::exchange(waiting_, {}), std::back_inserter(proposals));
        for (auto& e : proposals) {
            if (!refuse_if_reconfiguring()) [[unlikely]] {
                break;
            }
            if (entry_is_a_stop_sign(e) && !decode_cluster_config(e.payload)) [[unlikely]] {
                continue;
            }
            auto request = request_of(std::move(e));
            send_work(next, request, std::nullopt);
            tasks_.emplace(next, std::move(request));
            next += 1;
        }
    }

    void send_work(const std::uint64_t index, const entry& e, const std::optional<node_id> only)
    {
        const auto n = seq_paxos_.get_leader_state().n_leader;
        const auto crc = compute_entry_crc(index, e);
        for (const auto f : seq_paxos_.get_leader_state().get_promised_followers()) {
            if (!only || *only == f) {
                execute_verify_outgoing_.emplace_back(paxos_message{seq_paxos_.get_pid(), f, work{n, index, e, crc}});
            }
        }
    }

    std::vector<entry> open_tasks_after(const std::uint64_t log_len) const
    {
        std::vector<entry> open;
        for (auto it = tasks_.upper_bound(log_len); it != tasks_.end() && it->first == log_len + open.size() + 1;
             ++it) {
            open.push_back(it->second);
        }
        return open;
    }

    result append_the_done_records(const std::uint64_t log_len)
    {
        const auto open = open_tasks_after(log_len);
        if (open.empty()) {
            return {};
        }
        const leader_log log{log_len, open};
        ask_to_run_again(selection_->select_results(log));
        const auto done = selection_->find_the_done_records(log);
        if (done.empty()) {
            return {};
        }
        std::vector<indexed_result> chosen;
        for (std::uint64_t k = 0; k < done.size(); ++k) {
            const auto index = log_len + 1 + k;
            if (!entry_is_a_request(done[k])) {
                continue;
            }
            selection_->count_an_appended_done_record(index);
            if (!done[k].rejected) {
                chosen.push_back({index, done[k].result, done[k].result_crc});
            }
        }
        if (auto r = seq_paxos_.accept_entries_leader(done); !r) [[unlikely]] {
            return r;
        }
        const auto len = log_len + done.size();
        tasks_.erase(tasks_.begin(), tasks_.upper_bound(len));
        selection_->move_pointer(len, chosen);
        return check_the_done_records();
    }

    void count_the_answers()
    {
        for (const auto& [from, x] : std::exchange(answers_in_, {})) {
            selection_->count_answer(from, x);
        }
    }

    void ask_to_run_again(const std::span<const try_again_target> targets)
    {
        for (const auto& t : targets) {
            if (t.node != seq_paxos_.get_pid()) {
                execute_verify_outgoing_.emplace_back(
                    paxos_message{seq_paxos_.get_pid(), t.node, try_again{selection_->get_ballot(), t.index}});
            }
        }
    }

    result apply_the_done_records()
    {
        auto& w = *seq_paxos_.txn_;
        const auto m = seq_paxos_.read_meta();
        if (!m) [[unlikely]] {
            return seq_paxos_.stop_voting(m.error());
        }
        if (m->applied_idx >= m->decided_idx) {
            return {};
        }
        const auto entries = sequence_paxos<S>::read_range(w, m->applied_idx, m->decided_idx);
        if (!entries) [[unlikely]] {
            return seq_paxos_.stop_voting(entries.error());
        }
        for (auto k = m->applied_idx + 1; k <= m->decided_idx; ++k) {
            const auto& e = (*entries)[k - m->applied_idx - 1];
            if (entry_is_a_stop_sign(e) && k > seq_paxos_.get_config_first_idx()) [[unlikely]] {
                next_config_ = read_the_next_configuration(e);
            }
            if (!entry_is_a_request(e)) {
                continue;
            }
            const auto r = e.rejected ? std::nullopt : decode_result(e.result);
            if (!r) [[unlikely]] {
                reply_to_the_client(e, k, nullptr);
                continue;
            }
            if (const auto crc = compute_result_crc(k, e.result); crc != e.result_crc) [[unlikely]] {
                write_the_proof_and_throw({proof_kind::result_crc, k, seq_paxos_.mark_.promise, e.result_crc, crc},
                                          make_error_code(paxos_errc::result_differs),
                                          "konsens: a chosen result fails its CRC on this node");
            }
            if (std::ranges::contains(e.out, seq_paxos_.get_pid())) {
                const auto own = encode_result(run_request(request_of(e), begin_the_nested_transaction()));
                const fault_proof proof{proof_kind::listed_result_differs, k, seq_paxos_.mark_.promise, e.result_crc,
                                        compute_result_crc(k, own)};
                if (!results_agree(own, e.result, float_rule_of(machine_))) [[unlikely]] {
                    write_the_proof_and_throw(proof, make_error_code(paxos_errc::result_differs),
                                              "konsens: the result of this node differs from the chosen result");
                }
                log_records_.push_back({proof_kind::wrong_list, proof.index, proof.n, proof.crc, proof.other_crc});
            }
            write_the_result(e, k, *r, w);
            if (!written_values_read_back(*r, w)) [[unlikely]] {
                write_the_proof_and_throw({proof_kind::read_back, k, seq_paxos_.mark_.promise, e.result_crc, 0},
                                          make_error_code(paxos_errc::result_differs),
                                          "konsens: a value that the node wrote reads back different");
            }
            reply_to_the_client(e, k, &*r);
        }
        const auto crc = seq_paxos_.read_prefix_chain_crc(m->decided_idx);
        if (!crc) [[unlikely]] {
            return seq_paxos_.stop_voting(crc.error());
        }
        write_or_throw(w.set_applied(m->decided_idx, *crc));
        own_.erase(own_.begin(), own_.upper_bound(m->decided_idx));
        return {};
    }

    static bool written_values_read_back(const request_result& r, const typename S::transaction& w)
    {
        for (auto m = r.mutations.begin(); m != r.mutations.end(); ++m) {
            if (std::any_of(std::next(m), r.mutations.end(), [&m](const mutation& later) { return later.key == m->key; })) {
                continue;
            }
            const auto found = read_or_throw(w.find(m->key));
            if (found.has_value() != m->value.has_value() ||
                (found && !std::ranges::equal(*found, *m->value))) [[unlikely]] {
                return false;
            }
        }
        return true;
    }

    static std::error_code error_of(const std::uint64_t error)
    {
        for (const auto known : {client_errc::session_expired, client_errc::stale_request}) {
            if (error == static_cast<std::uint64_t>(std::to_underlying(known))) {
                return make_error_code(known);
            }
        }
        return make_error_code(client_errc::rejected);
    }

    void reply_to_the_client(const entry& e, const std::uint64_t index, const request_result* const r)
    {
        const auto response = [r]() -> std::expected<std::vector<std::byte>, std::error_code> {
            if (r == nullptr) [[unlikely]] {
                return std::unexpected(make_error_code(client_errc::rejected));
            }
            if (r->error != 0) [[unlikely]] {
                return std::unexpected(error_of(r->error));
            }
            return r->reply;
        };
        auto& replies = index <= seq_paxos_.mark_.log_len ? replies_before_the_sync_ : client_replies_;
        switch (e.kind) {
        case entry_kind::register_client:
            if (e.pid == seq_paxos_.get_pid() && registrations_.erase(e.client_id) > 0) {
                replies.push_back({e.client_id, index, response()});
            }
            break;
        case entry_kind::client_request:
            if (const auto it = requests_.find({e.session, e.request_number}); it != requests_.end()) {
                replies.push_back({it->second.client_id, e.session, response()});
                requests_.erase(it);
            }
            break;
        case entry_kind::expire_session:
            if (const auto it = expirations_.find(e.session); it != expirations_.end()) {
                replies.push_back({it->second, e.session, response()});
                expirations_.erase(it);
            }
            break;
        case entry_kind::normal:
        case entry_kind::stop_sign:
            break;
        }
    }

    static cluster_config read_the_next_configuration(const entry& stop_sign)
    {
        auto next = decode_cluster_config(stop_sign.payload);
        if (!next) [[unlikely]] {
            throw std::logic_error("konsens: a decided StopSign carries no cluster_config");
        }
        return std::move(*next);
    }

    static std::optional<float_rule> float_rule_of(const M& m)
    {
        if constexpr (requires { m.float_digits; m.float_denominator; }) {
            return float_rule{m.float_digits, m.float_denominator};
        } else if constexpr (requires { m.float_digits; }) {
            return float_rule{m.float_digits, std::nullopt};
        } else {
            return std::nullopt;
        }
    }

    static std::vector<std::byte> round_the_data_item(std::vector<std::byte> data_item, const float_rule& rule)
    {
        if (!holds_one_data_item(data_item)) [[unlikely]] {
            throw std::logic_error(
                "konsens: a state machine with float_digits gives its reply and each value as one encoded CBOR data item");
        }
        if (!rule.float_denominator) {
            return data_item;
        }
        return round_each_float(data_item, *rule.float_denominator).value();
    }

    request_result run_request(const entry& e, const nested_transaction& t) const
    {
        if (e.kind != entry_kind::client_request) {
            return {0, {}, {}};
        }
        const auto s = read_or_throw(t.find_session(e.session));
        if (!s) [[unlikely]] {
            return {static_cast<std::uint64_t>(std::to_underlying(client_errc::session_expired)), {}, {}};
        }
        if (e.request_number > s->request_number) {
            auto a = machine_.apply(e.payload, write_view{t});
            if constexpr (requires { machine_.float_digits; }) {
                a.reply = round_the_data_item(std::move(a.reply), *float_rule_of(machine_));
                for (auto& m : a.mutations) {
                    if (m.value) {
                        m.value = round_the_data_item(std::move(*m.value), *float_rule_of(machine_));
                    }
                }
            }
            return {0, std::move(a.reply), std::move(a.mutations)};
        }
        if (e.request_number == s->request_number && s->request_number != 0) {
            return {0, s->reply, {}};
        }
        return {static_cast<std::uint64_t>(std::to_underlying(client_errc::stale_request)), {}, {}};
    }

    template <typename T>
    static void write_the_result(const entry& e, const std::uint64_t index, const request_result& r, T& w)
    {
        switch (e.kind) {
        case entry_kind::register_client:
            write_or_throw(w.insert_or_assign_session(index, {}));
            break;
        case entry_kind::expire_session:
            write_or_throw(w.erase_session(e.session));
            break;
        case entry_kind::client_request:
            if (r.error != 0) {
                break;
            }
            for (const auto& m : r.mutations) {
                if (m.value) {
                    write_or_throw(w.insert_or_assign(m.key, *m.value));
                } else {
                    write_or_throw(w.erase(m.key));
                }
            }
            write_or_throw(w.insert_or_assign_session(e.session, {e.request_number, r.reply}));
            break;
        case entry_kind::normal:
        case entry_kind::stop_sign:
            break;
        }
    }

    void forget_the_runs()
    {
        own_.clear();
        tasks_.clear();
        tasks_ballot_.reset();
        works_in_.clear();
        answered_.reset();
        selection_.reset();
        answers_in_.clear();
        try_agains_in_.clear();
        refusals_in_.clear();
        refusal_.reset();
    }

    nested_transaction begin_the_base(const done_log& b, const std::uint64_t below)
    {
        auto w = begin_the_nested_transaction();
        write_the_done_results(b, b.applied_idx + 1, below, w);
        for (auto it = own_.upper_bound(b.log_len()); it != own_.end() && it->first < below; ++it) {
            write_the_result(it->second.request, it->first, it->second.result, w);
        }
        return w;
    }

    static void write_the_done_results(const done_log& b, const std::uint64_t from, const std::uint64_t below,
                                       nested_transaction& w)
    {
        for (auto k = from; k <= b.log_len() && k < below; ++k) {
            const auto& e = b.done[k - b.applied_idx - 1];
            if (!entry_is_a_request(e) || e.rejected) {
                continue;
            }
            if (const auto r = decode_result(e.result)) {
                write_the_result(e, k, *r, w);
            }
        }
    }

    own_result run_on_the_base(const entry& e, const std::uint64_t index, nested_transaction& w) const
    {
        auto r = run_request(e, w);
        auto bytes = encode_result(r);
        const auto crc = compute_result_crc(index, bytes);
        return {e, std::move(r), std::move(bytes), crc};
    }

    void keep_the_tasks(const ballot promise, const std::uint64_t log_len)
    {
        if (tasks_ballot_ != promise) {
            tasks_.clear();
            own_.erase(own_.upper_bound(log_len), own_.end());
            tasks_ballot_ = promise;
        }
        for (const auto& [from, w] : std::exchange(works_in_, {})) {
            if (w.n != promise || from != promise.pid || w.index <= log_len ||
                compute_entry_crc(w.index, w.request) != w.entry_crc) [[unlikely]] {
                continue;
            }
            if (const auto it = tasks_.find(w.index); it != tasks_.end() && it->second != w.request) {
                own_.erase(own_.lower_bound(w.index), own_.end());
            }
            tasks_.insert_or_assign(w.index, w.request);
        }
        tasks_.erase(tasks_.begin(), tasks_.upper_bound(log_len));
        for (auto it = own_.upper_bound(log_len); it != own_.end(); ++it) {
            if (const auto t = tasks_.find(it->first); t == tasks_.end() || t->second != it->second.request) {
                own_.erase(it, own_.end());
                return;
            }
        }
    }

    void check_the_own_results(const done_log& b)
    {
        for (auto k = b.applied_idx + 1; k <= b.log_len(); ++k) {
            const auto& e = b.done[k - b.applied_idx - 1];
            const auto o = own_.find(k);
            if (!entry_is_a_request(e) || o == own_.end()) {
                continue;
            }
            if (e.rejected || o->second.bytes != e.result || o->second.request != request_of(e)) {
                own_.erase(o, own_.end());
                return;
            }
        }
    }

    std::optional<std::uint64_t> run_the_tasks(const done_log& b)
    {
        std::optional<std::uint64_t> first;
        std::optional<nested_transaction> base;
        for (auto q = b.log_len() + 1;; ++q) {
            const auto t = tasks_.find(q);
            if (t == tasks_.end()) {
                break;
            }
            if (!entry_is_a_request(t->second) || own_.contains(q)) {
                continue;
            }
            if (!base) {
                base.emplace(begin_the_base(b, q));
            }
            auto o = run_on_the_base(t->second, q, *base);
            write_the_result(o.request, q, o.result, *base);
            own_.emplace(q, std::move(o));
            if (!first) {
                first = q;
            }
        }
        return first;
    }

    void run_again(const done_log& b, const std::uint64_t index)
    {
        auto w = begin_the_base(b, index);
        auto& held = own_.at(index);
        auto o = run_on_the_base(held.request, index, w);
        if (o.bytes != held.bytes) {
            held = std::move(o);
            own_.erase(own_.upper_bound(index), own_.end());
        }
    }

    answer answer_from(const ballot n, const std::uint64_t pointer, const std::uint64_t from,
                       const std::uint64_t second_run) const
    {
        answer a{n, pointer, from, 0, {}, second_run};
        for (auto it = own_.upper_bound(pointer); it != own_.end(); ++it) {
            if (it->first < from) {
                a.base_crc = extend_base_crc(a.base_crc, it->first, it->second.crc);
            } else {
                a.results.push_back({it->first, it->second.bytes, it->second.crc});
            }
        }
        return a;
    }

    void send_answer(const node_id to, answer a)
    {
        if (to == 0) {
            return;
        }
        if (to == seq_paxos_.get_pid()) {
            if (selection_ && selection_->get_ballot() == a.n) {
                selection_->count_answer(to, a);
            }
            return;
        }
        execute_verify_outgoing_.emplace_back(paxos_message{seq_paxos_.get_pid(), to, std::move(a)});
    }

    void follow_the_leader(const done_log& b)
    {
        if (seq_paxos_.get_state() != std::pair{role::leader, phase::accept}) {
            selection_.reset();
            return;
        }
        const auto n = seq_paxos_.get_leader_state().n_leader;
        if (!selection_ || selection_->get_ballot() != n) {
            selection_ = selection::with(n, nodes_, b.log_len(), float_rule_of(machine_));
            return;
        }
        std::vector<indexed_result> chosen;
        for (auto k = b.applied_idx + 1; k <= b.log_len(); ++k) {
            const auto& e = b.done[k - b.applied_idx - 1];
            if (entry_is_a_request(e) && !e.rejected) {
                chosen.push_back({k, e.result, e.result_crc});
            }
        }
        selection_->move_pointer(b.log_len(), chosen);
    }

    void run_again_for_the_leader(const done_log& b, const ballot n)
    {
        const auto self = seq_paxos_.get_pid();
        for (const auto index : get_own_result_indexes()) {
            if (index <= b.log_len() || !own_.contains(index) || !selection_->asked_to_run_again(self, index) ||
                selection_->ran_again(self, index)) {
                continue;
            }
            run_again(b, index);
            selection_->count_answer(self, answer_from(n, b.log_len(), index, index));
        }
    }

    result execute_verify()
    {
        if (!seq_paxos_.votes()) {
            forget_the_runs();
            return {};
        }
        const auto m = seq_paxos_.read_meta();
        if (!m) [[unlikely]] {
            return seq_paxos_.stop_voting(m.error());
        }
        const auto promise = m->promise;
        const auto log =
            sequence_paxos<S>::read_range(*seq_paxos_.txn_, std::min(m->applied_idx, m->log_len), m->log_len);
        if (!log) [[unlikely]] {
            return seq_paxos_.stop_voting(log.error());
        }
        const done_log b{m->log_len - log->size(), *log};
        const auto pointer = b.log_len();
        follow_the_leader(b);
        keep_the_tasks(promise, pointer);
        check_the_own_results(b);
        const auto first = run_the_tasks(b);
        const auto answered = std::pair{promise, pointer};
        if (own_.upper_bound(pointer) != own_.end() && (resend_answer_ || answered_ != answered)) {
            send_answer(promise.pid, answer_from(promise, pointer, pointer + 1, 0));
            answered_ = answered;
        } else if (first) {
            send_answer(promise.pid, answer_from(promise, pointer, *first, 0));
            answered_ = answered;
        }
        if (resend_answer_) {
            send_the_refusal(promise);
        }
        resend_answer_ = false;
        for (const auto& [from, t] : std::exchange(try_agains_in_, {})) {
            if (t.n != promise || t.index <= pointer || !own_.contains(t.index)) {
                continue;
            }
            run_again(b, t.index);
            send_answer(from, answer_from(promise, pointer, t.index, t.index));
        }
        if (!selection_) {
            answers_in_.clear();
            return {};
        }
        count_the_answers();
        const auto n = selection_->get_ballot();
        run_again_for_the_leader(b, n);
        if (seq_paxos_.get_state().second != phase::accept) {
            return {};
        }
        const auto open = open_tasks_after(pointer);
        ask_to_run_again(selection_->select_results({pointer, open}));
        run_again_for_the_leader(b, n);
        return {};
    }

    void index_reads()
    {
        for (const auto& x : seq_paxos_.take_indexed_reads()) {
            const auto it = reads_.find(x.read);
            if (it == reads_.end() || it->second.read_index) {
                continue;
            }
            it->second.read_index = x.read_index;
            it->second.ballot = x.ballot;
        }
    }

    void drop_reads_of_an_old_promise(const ballot promise)
    {
        std::erase_if(reads_, [this, promise](const auto& r) {
            if (r.second.ballot == promise) {
                return false;
            }
            client_replies_.push_back({r.second.client_id, 0, std::unexpected(make_error_code(client_errc::retry))});
            return true;
        });
    }

    void drop_writes_of_an_old_promise(const ballot promise)
    {
        std::erase_if(requests_, [this, promise](const auto& r) {
            if (r.second.ballot == promise) {
                return false;
            }
            client_replies_.push_back(
                {r.second.client_id, r.first.first, std::unexpected(make_error_code(client_errc::retry))});
            return true;
        });
    }

    void answer_indexed_reads()
    {
        if (reads_.empty()) {
            return;
        }
        const auto t = read_or_throw(seq_paxos_.internal_storage_.begin_read_only());
        const auto applied_idx = read_or_throw(seq_paxos_.read_checked_meta(t)).applied_idx;
        const read_view view{t};
        std::erase_if(reads_, [this, applied_idx, &view](const auto& r) {
            if (!r.second.read_index || *r.second.read_index > applied_idx) {
                return false;
            }
            client_replies_.push_back({r.second.client_id, 0, machine_.query(r.second.payload, view)});
            return true;
        });
    }

    void resend_read_index_requests()
    {
        if (seq_paxos_.get_state().first == role::leader) {
            return;
        }
        std::erase_if(reads_, [this](const auto& r) {
            if (r.second.read_index) {
                return false;
            }
            if (seq_paxos_.start_read(r.first)) {
                return false;
            }
            client_replies_.push_back({r.second.client_id, 0, std::unexpected(make_error_code(client_errc::retry))});
            return true;
        });
    }

    sequence_paxos<S> seq_paxos_;
    konsens::node_storage own_storage_;
    ballot_leader_election ble_;
    ble_config ble_config_;
    logical_clock election_clock_;
    logical_clock resend_message_clock_;
    M machine_;
    std::vector<client_reply> client_replies_;
    std::vector<client_reply> replies_before_the_sync_;
    std::vector<client_reply> released_replies_;
    std::vector<fault_proof> log_records_;
    std::vector<fault_proof> released_log_records_;
    std::vector<message> released_messages_;
    std::vector<received> received_batch_;
    std::vector<message> message_batch_;
    std::set<std::uint64_t> registrations_;
    std::map<std::pair<std::uint64_t, std::uint64_t>, client_write> requests_;
    std::map<std::uint64_t, std::uint64_t> expirations_;
    std::map<std::uint64_t, client_read> reads_;
    std::uint64_t last_read_{};
    std::unique_ptr<std::uint64_t> commit_count_ = std::make_unique<std::uint64_t>();
    std::uint64_t nonce_;
    bool sync_by_the_caller_;
    std::vector<held_step> held_steps_;
    std::optional<std::uint64_t> requested_sync_;
    bool build_started_{};
    bool waits_for_the_operator_{};
    bool faulted_{};
    bool stopped_{};
    bool phase_two_open_{};
    std::vector<node_id> nodes_;
    std::map<std::uint64_t, own_result> own_;
    std::optional<std::pair<ballot, std::uint64_t>> answered_;
    std::optional<konsens::selection> selection_;
    std::vector<std::pair<node_id, answer>> answers_in_;
    std::vector<std::pair<node_id, try_again>> try_agains_in_;
    std::vector<std::pair<node_id, refused>> refusals_in_;
    std::vector<std::pair<node_id, work>> works_in_;
    std::map<std::uint64_t, entry> tasks_;
    std::optional<ballot> tasks_ballot_;
    bool resend_work_{};
    std::optional<refused> refusal_;
    std::vector<message> execute_verify_outgoing_;
    std::vector<entry> waiting_;
    bool resend_answer_{};
    std::optional<cluster_config> next_config_;
};

}
