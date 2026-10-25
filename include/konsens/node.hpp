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
inline constexpr std::uint64_t auto_repair_count = 1;

static_assert(std::is_nothrow_move_constructible_v<message> && std::is_nothrow_move_constructible_v<received>);

struct node_config {
    std::uint32_t configuration_id;
    node_id pid;
    std::vector<node_id> peers;
    std::uint32_t leader_priority;
    std::uint64_t election_tick_timeout;
    std::uint64_t resend_message_tick_timeout;
    std::uint64_t max_in_flight;
};

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
                                                     konsens::node_storage own_storage, konsens::state_storage state,
                                                     M machine, const std::uint64_t nonce)
    {
        if (config.configuration_id == 0 || config.election_tick_timeout == 0 ||
            config.resend_message_tick_timeout == 0 || config.max_in_flight == 0) [[unlikely]] {
            throw std::logic_error("konsens: configuration_id, each tick timeout and max_in_flight must not be 0");
        }
        auto sp = sequence_paxos<S>::with({config.pid, config.peers}, std::move(internal_storage), nonce);
        if (!sp) [[unlikely]] {
            return std::unexpected(sp.error());
        }
        if (sp->votes()) {
            if (auto r = sp->scan_the_whole_log(); !r && !error_is_a_damage(r.error())) [[unlikely]] {
                return std::unexpected(r.error());
            }
        }
        const auto record = own_storage.get_repair_record();
        if (!record) [[unlikely]] {
            return std::unexpected(record.error());
        }
        auto promise = std::expected<ballot, std::error_code>{ballot{}};
        if (sp->votes()) {
            promise = sp->get_promise();
        }
        if (!promise) [[unlikely]] {
            return std::unexpected(promise.error());
        }
        auto ble = ballot_leader_election::with(ble_config_of(config), *promise);
        node n(config, std::move(*sp), std::move(own_storage), std::move(state), std::move(ble), std::move(machine),
               nonce);
        if (n.seq_paxos_.votes()) {
            if (auto r = n.check_the_applied_state();
                !r && r.error() != make_error_code(paxos_errc::state_differs_from_log) && !error_is_a_damage(r.error()))
                [[unlikely]] {
                return std::unexpected(r.error());
            }
        }
        if (auto r = n.count_the_fault_at_the_start(*record); !r) [[unlikely]] {
            return std::unexpected(r.error());
        }
        if (n.seq_paxos_.votes()) {
            if (auto r = n.apply_chosen_results(); !r) [[unlikely]] {
                return std::unexpected(r.error());
            }
        }
        n.release_messages_and_replies();
        return n;
    }

    [[nodiscard]] std::expected<transaction, std::error_code> tick()
    {
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
        return throw_on_a_damage([&]() -> result {
            current_transaction().throw_if_another_commit(t);
            *commit_count_ += 1;
            if (auto r = propose_before_the_step(); !r) [[unlikely]] {
                static_cast<void>(seq_paxos_.commit());
                drop_held_messages_and_replies();
                return r;
            }
            const auto w = seq_paxos_.commit();
            if (!w) [[unlikely]] {
                drop_held_messages_and_replies();
                return w;
            }
            if (repair_started_ && !seq_paxos_.repairing() && seq_paxos_.votes()) {
                if (auto r = close_the_repair(); !r) [[unlikely]] {
                    drop_held_messages_and_replies();
                    return r;
                }
                static_cast<void>(check_the_applied_state());
            }
            if (!seq_paxos_.votes()) {
                release_messages_and_replies();
                return {};
            }
            if (auto r = serve_clients(); !r) [[unlikely]] {
                drop_held_messages_and_replies();
                return r;
            }
            if (open_mark_ && !repair_started_ && get_applied_idx() >= *open_mark_) {
                if (auto r = close_the_open_mark(); !r) [[unlikely]] {
                    drop_held_messages_and_replies();
                    return r;
                }
            }
            release_messages_and_replies();
            return {};
        });
    }

    [[nodiscard]] std::expected<transaction, std::error_code> append(entry e)
    {
        return throw_on_a_damage([&]() -> std::expected<transaction, std::error_code> {
            if (entry_is_a_verdict(e)) [[unlikely]] {
                throw std::logic_error("konsens: only the leader appends a verdict entry");
            }
            if (e.kind == entry_kind::stop_sign) {
                const auto open = holds_a_request_without_a_verdict();
                if (!open) [[unlikely]] {
                    return std::unexpected(open.error());
                }
                if (*open) [[unlikely]] {
                    return std::unexpected(make_error_code(client_errc::retry));
                }
            }
            if (auto r = seq_paxos_.append(std::move(e)); !r) [[unlikely]] {
                return std::unexpected(r.error());
            }
            return current_transaction();
        });
    }

    [[nodiscard]] std::expected<transaction, std::error_code> register_client(const std::uint64_t client_id)
    {
        return throw_on_a_damage([&]() -> std::expected<transaction, std::error_code> {
            if (auto r = seq_paxos_.refuse_if_recovering(); !r) [[unlikely]] {
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
        return throw_on_a_damage([&]() -> std::expected<transaction, std::error_code> {
            if (auto r = seq_paxos_.refuse_if_recovering(); !r) [[unlikely]] {
                return std::unexpected(r.error());
            }
            if (request_number == 0) [[unlikely]] {
                throw std::logic_error("konsens: a request number starts at 1");
            }
            if (auto r = propose_a_request({entry_kind::client_request, std::move(payload), session, request_number}); !r)
                [[unlikely]] {
                return std::unexpected(r.error());
            }
            requests_.insert_or_assign({session, request_number}, client_id);
            return current_transaction();
        });
    }

    [[nodiscard]] std::expected<transaction, std::error_code> client_query(const std::uint64_t client_id,
                                                                           std::vector<std::byte> payload)
    {
        return throw_on_a_damage([&]() -> std::expected<transaction, std::error_code> {
            if (auto r = seq_paxos_.refuse_if_recovering(); !r) [[unlikely]] {
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
        return throw_on_a_damage([&]() -> std::expected<transaction, std::error_code> {
            if (auto r = seq_paxos_.refuse_if_recovering(); !r) [[unlikely]] {
                return std::unexpected(r.error());
            }
            if (seq_paxos_.get_state() != std::pair{role::leader, phase::accept}) [[unlikely]] {
                return std::unexpected(make_error_code(client_errc::retry));
            }
            const auto t = read_or_throw(state_.begin_read_only());
            if (!read_or_throw(t.find_session(session))) [[unlikely]] {
                return std::unexpected(make_error_code(client_errc::session_expired));
            }
            const entry expiry{entry_kind::expire_session, {}, session};
            const auto undecided =
                seq_paxos_.log_or_buffered_proposals_contain(read_or_throw(t.get_applied()).index, expiry);
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
    ~node() { open_.reset(); }

    std::vector<client_reply> take_client_replies() { return std::exchange(released_replies_, {}); }

    std::vector<message> take_outgoing_messages() { return std::exchange(released_messages_, {}); }

    std::vector<fault_proof> take_log_records() { return std::exchange(released_log_records_, {}); }

    std::expected<std::vector<entry>, std::error_code> read_decided_suffix(std::uint64_t from_idx) const
    {
        return seq_paxos_.read_decided_suffix(from_idx);
    }

    const sequence_paxos<S>& get_sequence_paxos() const { return seq_paxos_; }
    bool waits_for_the_operator() const { return waits_for_the_operator_; }
    std::expected<std::uint64_t, std::error_code> get_repair_count() const
    {
        return own_storage_.get_repair_record().transform([](const repair_record& r) { return r.count; });
    }
    std::expected<repair_record, std::error_code> get_repair_record() const { return own_storage_.get_repair_record(); }
    [[nodiscard]] result allow_one_more_repair() { return own_storage_.allow_one_more_repair(); }
    const ballot_leader_election& get_ballot_leader_election() const { return ble_; }
    const M& get_state_machine() const { return machine_; }
    const konsens::state_storage& get_state_storage() const { return state_; }
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
        return read_or_throw(read_or_throw(state_.begin_read_only()).get_applied()).index;
    }
    std::optional<session_record> find_client_session(const std::uint64_t session) const
    {
        return read_or_throw(read_or_throw(state_.begin_read_only()).find_session(session));
    }

    [[nodiscard]] std::expected<transaction, std::error_code> reconnected(const node_id pid)
    {
        return throw_on_a_damage([&]() -> std::expected<transaction, std::error_code> {
            if (auto r = seq_paxos_.reconnected(pid); !r) [[unlikely]] {
                return std::unexpected(r.error());
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

    void release_messages_and_replies()
    {
        for (auto& m : seq_paxos_.take_outgoing_messages()) {
            released_messages_.emplace_back(std::move(m));
        }
        for (auto& m : ble_.take_outgoing_messages()) {
            released_messages_.emplace_back(std::move(m));
        }
        for (auto& m : execute_verify_outgoing_) {
            released_messages_.emplace_back(std::move(m));
        }
        execute_verify_outgoing_.clear();
        released_replies_.insert(released_replies_.end(), std::make_move_iterator(client_replies_.begin()),
                                 std::make_move_iterator(client_replies_.end()));
        client_replies_.clear();
        released_log_records_.insert(released_log_records_.end(), log_records_.begin(), log_records_.end());
        log_records_.clear();
        received_batch_.clear();
        message_batch_.clear();
    }

    void drop_held_messages_and_replies()
    {
        static_cast<void>(seq_paxos_.take_outgoing_messages());
        static_cast<void>(ble_.take_outgoing_messages());
        execute_verify_outgoing_.clear();
        client_replies_.clear();
        log_records_.clear();
        received_batch_.clear();
        message_batch_.clear();
    }

    template <typename T>
    result handle(node_id from, node_id to, const T& msg)
    {
        if constexpr (std::is_same_v<T, answer> || std::is_same_v<T, try_again> || std::is_same_v<T, refused>) {
            if (to != seq_paxos_.get_pid() || !std::ranges::contains(seq_paxos_.peers_, from) ||
                seq_paxos_.recovering()) [[unlikely]] {
                return {};
            }
            if constexpr (std::is_same_v<T, answer>) {
                answers_in_.emplace_back(from, msg);
            } else if constexpr (std::is_same_v<T, try_again>) {
                try_agains_in_.emplace_back(from, msg);
            } else {
                refusals_in_.emplace_back(from, msg);
            }
            return {};
        } else if constexpr (requires { paxos_msg{std::in_place_type<T>, msg}; }) {
            return seq_paxos_.handle(from, to, msg);
        } else {
            if (seq_paxos_.recovering()) {
                return {};
            }
            ble_.handle(ble_message{from, to, heartbeat_msg{std::in_place_type<T>, msg}});
            return {};
        }
    }

    node(const node_config& config, sequence_paxos<S> seq_paxos, konsens::node_storage own_storage,
         konsens::state_storage state, ballot_leader_election ble, M machine, const std::uint64_t nonce)
        : seq_paxos_(std::move(seq_paxos)), own_storage_(std::move(own_storage)), state_(std::move(state)),
          ble_(std::move(ble)), ble_config_(ble_config_of(config)), election_clock_{0, config.election_tick_timeout},
          resend_message_clock_{0, config.resend_message_tick_timeout}, machine_(std::move(machine)), nonce_(nonce),
          max_in_flight_(config.max_in_flight)
    {
        nodes_ = config.peers;
        nodes_.push_back(config.pid);
        std::ranges::sort(nodes_);
    }

    template <typename T>
    static T read_or_throw(std::expected<T, std::error_code> r)
    {
        if (!r) [[unlikely]] {
            throw read_error(r.error(), "konsens: the state storage failed to read");
        }
        return std::move(*r);
    }

    static void write_or_throw(const std::expected<void, std::error_code>& written)
    {
        if (!written) [[unlikely]] {
            throw write_error(written.error(), "konsens: the state storage failed to write");
        }
    }

    result check_the_applied_state()
    {
        const auto a = read_or_throw(read_or_throw(state_.begin_read_only()).get_applied());
        return seq_paxos_.check_applied_chain_crc(a.index, a.chain_crc);
    }

    static ble_config ble_config_of(const node_config& config)
    {
        return {config.configuration_id, config.pid, config.peers, config.leader_priority};
    }

    result count_the_fault_at_the_start(const repair_record& record)
    {
        open_mark_ = record.open_mark;
        const auto e = seq_paxos_.get_error();
        const bool damaged = e && error_is_a_damage(*e);
        const bool covered = record.proof && record.open_mark && record.proof->index <= *record.open_mark &&
                             !record.mark_covered;
        if (covered) {
            if (auto c = own_storage_.cover_the_proof(); !c) [[unlikely]] {
                return c;
            }
        } else if (record.proof || (damaged && !record.open_mark)) {
            if (record.count >= auto_repair_count) {
                waits_for_the_operator_ = true;
                if (!damaged) {
                    static_cast<void>(seq_paxos_.stop_voting(make_error_code(paxos_errc::result_differs)));
                }
                return {};
            }
            const auto index = record.proof ? record.proof->index : index_of_the_damage();
            if (auto c = own_storage_.count_a_repair(index); !c) [[unlikely]] {
                return c;
            }
            open_mark_ = index;
        }
        if (!damaged) {
            return {};
        }
        nonce_ += 1;
        repair_started_ = true;
        forget_the_runs();
        return seq_paxos_.start_repair(nonce_);
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
        if (e && error_is_a_damage(*e) && !waits_for_the_operator_) [[unlikely]] {
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
        if (auto w = own_storage_.write_the_proof(proof); !w) [[unlikely]] {
            throw sync_error(w.error(), "konsens: the node storage failed to keep the proof of a fault");
        }
        throw node_fault(e, what);
    }

    result close_the_open_mark()
    {
        open_mark_.reset();
        return own_storage_.close_the_repair();
    }

    result close_the_repair()
    {
        repair_started_ = false;
        if (auto c = close_the_open_mark(); !c) [[unlikely]] {
            return c;
        }
        const auto promise = seq_paxos_.get_promise();
        if (!promise) [[unlikely]] {
            return std::unexpected(promise.error());
        }
        ble_ = ballot_leader_election::with(ble_config_, *promise);
        return {};
    }

    ballot ballot_above_promise(const ballot elected, const ballot promise) const
    {
        if (elected.pid != seq_paxos_.get_pid() || elected > promise || seq_paxos_.get_state().first == role::leader) {
            return elected;
        }
        return {elected.config_id, promise.n + 1, elected.priority, elected.pid};
    }

    result serve_clients()
    {
        index_reads();
        auto chosen = apply_chosen_results();
        if (!chosen) [[unlikely]] {
            return std::unexpected(chosen.error());
        }
        if (auto r = execute_verify(*chosen); !r) [[unlikely]] {
            return r;
        }
        const auto promise = seq_paxos_.get_promise();
        if (!promise) [[unlikely]] {
            return std::unexpected(promise.error());
        }
        drop_reads_of_an_old_promise(*promise);
        answer_indexed_reads();
        return {};
    }

    struct own_result {
        entry request;
        request_result result;
        std::vector<std::byte> bytes;
        std::uint32_t crc;
    };

    result propose_a_request(entry e)
    {
        if (seq_paxos_.get_state() == std::pair{role::leader, phase::accept}) {
            waiting_.push_back(std::move(e));
            return {};
        }
        return seq_paxos_.append(std::move(e));
    }

    std::expected<bool, std::error_code> holds_a_request_without_a_verdict()
    {
        if (std::ranges::any_of(waiting_, entry_is_a_request) ||
            std::ranges::any_of(seq_paxos_.buffered_proposals_, entry_is_a_request)) {
            return true;
        }
        const auto a = read_or_throw(read_or_throw(state_.begin_read_only()).get_applied());
        if (auto r = seq_paxos_.begin_step(); !r) [[unlikely]] {
            return std::unexpected(r.error());
        }
        const auto m = seq_paxos_.read_meta();
        if (!m) [[unlikely]] {
            return seq_paxos_.stop_voting(m.error());
        }
        const auto log = sequence_paxos<S>::read_range(*seq_paxos_.txn_, std::min(a.index, m->log_len), m->log_len);
        if (!log) [[unlikely]] {
            return seq_paxos_.stop_voting(log.error());
        }
        return std::ranges::any_of(*log, entry_is_a_request);
    }

    result check_the_verdict_entries()
    {
        if (!seq_paxos_.votes() || seq_paxos_.recovering() || seq_paxos_.get_state().second != phase::accept) {
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
        const auto a = read_or_throw(read_or_throw(state_.begin_read_only()).get_applied());
        for (;;) {
            const auto index = seq_paxos_.get_verified_idx() + 1;
            if (index > m->log_len || (refusal_ && refusal_->index == index)) {
                return {};
            }
            std::optional<refusal_reason> reason;
            if (index > m->decided_idx && index > a.index) {
                const auto log = sequence_paxos<S>::read_range(*seq_paxos_.txn_, a.index, index);
                if (!log) [[unlikely]] {
                    return seq_paxos_.stop_voting(log.error());
                }
                reason = check_the_verdict_entry(a, *log);
            }
            if (!reason) {
                if (auto r = seq_paxos_.pass_the_checked_verdict_entry(index); !r) [[unlikely]] {
                    return r;
                }
                continue;
            }
            refusal_ = refused{m->promise, index, *reason};
            send_the_refusal(m->promise);
            return {};
        }
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

    struct check_base {
        std::uint64_t applied_idx;
        std::span<const entry> entries;
        std::map<std::uint64_t, std::span<const std::byte>> results;
    };

    std::span<const std::byte> find_the_own_result_on(const check_base& b, const std::uint64_t index) const
    {
        for (auto k = b.applied_idx + 1; k < index; ++k) {
            if (!entry_is_a_request(b.entries[k - b.applied_idx - 1])) {
                continue;
            }
            const auto o = own_.find(k);
            const auto r = b.results.find(k);
            if (o == own_.end() || r == b.results.end() || !std::ranges::equal(o->second.bytes, r->second)) {
                return {};
            }
        }
        if (const auto o = own_.find(index); o != own_.end()) {
            return o->second.bytes;
        }
        return {};
    }

    std::optional<refusal_reason> check_the_verdict_entry(const applied_record& a, const std::span<const entry> log)
    {
        const auto index = a.index + log.size();
        const auto before = find_the_verdicts(a.pointer, a.index + 1, log.first(log.size() - 1));
        const auto with = find_the_verdicts(a.pointer, a.index + 1, log);
        if (with.pointer == before.pointer) {
            return std::nullopt;
        }
        const auto& e = log.back();
        open_.reset();
        auto w = state_.begin();
        if (!w) [[unlikely]] {
            throw write_error(w.error(), "konsens: the state storage failed to begin a write");
        }
        check_base b{a.index, log, {}};
        for (const auto& c : before.chosen) {
            b.results.emplace(c.chosen.index, c.chosen.result);
        }
        for (auto k = a.index + 1; k <= before.pointer; ++k) {
            const auto& request = log[k - a.index - 1];
            const auto c = b.results.find(k);
            if (!entry_is_a_request(request) || c == b.results.end()) {
                continue;
            }
            if (const auto r = decode_result(c->second)) {
                write_the_result(request, k, *r, *w);
            }
        }
        if (e.kind == entry_kind::rejected) {
            return check_the_rejected_entry(e, b, *w);
        }
        std::vector<const chosen_result*> listed;
        for (const auto& c : with.chosen) {
            if (c.chosen.index > before.pointer) {
                listed.push_back(&c);
            }
        }
        for (auto k = std::max(e.from_idx, a.index + 1); k <= e.to_idx && k < index; ++k) {
            if (entry_is_a_request(log[k - a.index - 1]) &&
                std::ranges::none_of(listed, [k](const chosen_result* c) { return c->chosen.index == k; })) {
                return refusal_reason::wrong_list;
            }
        }
        const auto self = seq_paxos_.get_pid();
        for (const auto* c : listed) {
            const auto q = c->chosen.index;
            if (compute_result_crc(q, c->chosen.result) != c->chosen.crc) [[unlikely]] {
                return refusal_reason::crc_mismatch;
            }
            const auto& request = log[q - a.index - 1];
            auto own = find_the_own_result_on(b, q);
            std::vector<std::byte> first;
            if (own.empty()) {
                first = run_on_the_base(request, q, *w).bytes;
                own = first;
            }
            const bool again = !results_agree(own, c->chosen.result);
            const bool still = again && !results_agree(run_on_the_base(request, q, *w).bytes, c->chosen.result);
            if (std::ranges::contains(c->out, self) ? !still : still) {
                return std::ranges::contains(c->out, self) ? refusal_reason::wrong_list
                                                           : refusal_reason::result_differs;
            }
            const auto r = decode_result(c->chosen.result);
            if (!r) [[unlikely]] {
                return refusal_reason::result_differs;
            }
            write_the_result(request, q, *r, *w);
            b.results.insert_or_assign(q, c->chosen.result);
        }
        return std::nullopt;
    }

    std::optional<refusal_reason> check_the_rejected_entry(const entry& e, const check_base& b,
                                                           state_storage::transaction& w)
    {
        const auto answers = decode_chosen_results(e.payload);
        if (!answers) [[unlikely]] {
            return refusal_reason::rejected_has_a_majority;
        }
        const auto q = e.from_idx;
        const auto& request = b.entries[q - b.applied_idx - 1];
        auto own = find_the_own_result_on(b, q);
        std::vector<std::byte> ran;
        if (own.empty()) {
            ran = run_on_the_base(request, q, w).bytes;
            own = ran;
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
            const auto agreeing = std::ranges::count_if(counted, [&c](const auto& x) { return results_agree(x, c); });
            if (static_cast<std::size_t>(agreeing) >= majority) {
                return refusal_reason::rejected_has_a_majority;
            }
        }
        return std::nullopt;
    }

    result propose_before_the_step()
    {
        if (auto r = check_the_verdict_entries(); !r) [[unlikely]] {
            return r;
        }
        if (!seq_paxos_.votes() || seq_paxos_.recovering()) {
            return {};
        }
        const bool leads = seq_paxos_.get_state() == std::pair{role::leader, phase::accept};
        if (!waiting_.empty() && (!leads || own_.size() < max_in_flight_)) {
            for (auto& e : std::exchange(waiting_, {})) {
                if (auto r = seq_paxos_.append(std::move(e)); !r) [[unlikely]] {
                    return r;
                }
            }
        }
        if (seq_paxos_.get_state().first != role::leader) {
            answers_in_.clear();
            return {};
        }
        if (!selection_ || selection_->get_ballot() != seq_paxos_.get_leader_state().n_leader) {
            return {};
        }
        count_the_answers();
        for (const auto& [from, x] : std::exchange(refusals_in_, {})) {
            if (x.n == selection_->get_ballot()) {
                selection_->count_a_refusal(from, x.index);
            }
        }
        if (const auto refused = selection_->find_the_entries_that_a_majority_refused(); !refused.empty()) [[unlikely]] {
            write_the_proof_and_throw({proof_kind::refused_by_a_majority, refused.front(), selection_->get_ballot(), 0, 0},
                                      make_error_code(paxos_errc::result_differs),
                                      "konsens: a majority refused a verdict entry of this leader");
        }
        if (!leads) {
            return {};
        }
        if (auto r = seq_paxos_.begin_step(); !r) [[unlikely]] {
            return r;
        }
        const auto m = seq_paxos_.read_meta();
        if (!m) [[unlikely]] {
            return seq_paxos_.stop_voting(m.error());
        }
        const auto a = read_or_throw(read_or_throw(state_.begin_read_only()).get_applied());
        if (a.index > m->decided_idx) [[unlikely]] {
            return {};
        }
        auto log = sequence_paxos<S>::read_range(*seq_paxos_.txn_, a.pointer, m->log_len);
        if (!log) [[unlikely]] {
            return seq_paxos_.stop_voting(log.error());
        }
        const auto decided = std::span<const entry>(*log).subspan(a.index - a.pointer, m->decided_idx - a.index);
        if (find_the_verdicts(a.pointer, a.index + 1, decided).pointer != selection_->get_pointer()) {
            return {};
        }
        ask_to_run_again(selection_->select_results({a.pointer, m->decided_idx, *log}));
        log->insert(log->end(), seq_paxos_.buffered_proposals_.begin(), seq_paxos_.buffered_proposals_.end());
        auto verdict = selection_->find_the_verdict_entry({a.pointer, m->decided_idx, *log});
        if (!verdict) {
            return {};
        }
        selection_->count_an_appended_verdict_entry(a.pointer + log->size() + 1);
        return seq_paxos_.append(std::move(*verdict));
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

    std::expected<std::vector<chosen_result>, std::error_code> apply_chosen_results()
    {
        const auto a = read_or_throw(read_or_throw(state_.begin_read_only()).get_applied());
        const auto entries = seq_paxos_.read_decided_suffix(a.index);
        if (!entries) [[unlikely]] {
            return std::unexpected(entries.error());
        }
        if (entries->empty()) {
            return std::vector<chosen_result>{};
        }
        auto v = find_the_verdicts(a.pointer, a.index + 1, *entries);
        if (v.applied_idx == a.index) {
            return std::vector<chosen_result>{};
        }
        std::map<std::uint64_t, const chosen_result*> chosen_at;
        for (const auto& c : v.chosen) {
            chosen_at.emplace(c.chosen.index, &c);
        }
        bool own_holds = true;
        for (auto k = a.index + 1; k <= v.pointer; ++k) {
            if (!entry_is_a_request((*entries)[k - a.index - 1])) {
                continue;
            }
            const auto c = chosen_at.find(k);
            const auto o = own_.find(k);
            if (c == chosen_at.end() || o == own_.end() || o->second.bytes != c->second->chosen.result) {
                own_holds = false;
            }
        }
        open_.reset();
        if (!own_holds) {
            own_.clear();
        }
        auto w = state_.begin();
        if (!w) [[unlikely]] {
            throw write_error(w.error(), "konsens: the state storage failed to begin a write");
        }
        for (auto k = a.index + 1; k <= v.applied_idx; ++k) {
            const auto& e = (*entries)[k - a.index - 1];
            if (!entry_is_a_request(e)) {
                continue;
            }
            const auto c = chosen_at.find(k);
            const auto r = c == chosen_at.end() ? std::nullopt : decode_result(c->second->chosen.result);
            if (!r) [[unlikely]] {
                reply_to_the_client(e, k, nullptr);
                continue;
            }
            const auto& chosen = c->second->chosen;
            if (const auto crc = compute_result_crc(k, chosen.result); crc != chosen.crc) [[unlikely]] {
                write_the_proof_and_throw({proof_kind::result_crc, k, seq_paxos_.mark_.promise, chosen.crc, crc},
                                          make_error_code(paxos_errc::result_differs),
                                          "konsens: a chosen result fails its CRC on this node");
            }
            if (std::ranges::contains(c->second->out, seq_paxos_.get_pid()) && !(open_mark_ && k <= *open_mark_)) {
                const auto own = encode_result(run_request(e, *w));
                const fault_proof proof{proof_kind::listed_result_differs, k, seq_paxos_.mark_.promise, chosen.crc,
                                        compute_result_crc(k, own)};
                if (!results_agree(own, chosen.result)) [[unlikely]] {
                    write_the_proof_and_throw(proof, make_error_code(paxos_errc::result_differs),
                                              "konsens: the result of this node differs from the chosen result");
                }
                log_records_.push_back({proof_kind::wrong_list, proof.index, proof.n, proof.crc, proof.other_crc});
            }
            write_the_result(e, k, *r, *w);
            if (!written_values_read_back(*r, *w)) [[unlikely]] {
                write_the_proof_and_throw({proof_kind::read_back, k, seq_paxos_.mark_.promise, chosen.crc, 0},
                                          make_error_code(paxos_errc::result_differs),
                                          "konsens: a value that the node wrote reads back different");
            }
            reply_to_the_client(e, k, &*r);
        }
        const auto crc = seq_paxos_.get_prefix_chain_crc(v.applied_idx);
        if (!crc) [[unlikely]] {
            return std::unexpected(crc.error());
        }
        write_or_throw(w->set_applied({v.applied_idx, *crc, v.pointer}));
        if (auto c = std::move(*w).commit(); !c) [[unlikely]] {
            return std::unexpected(c.error());
        }
        own_.erase(own_.begin(), own_.upper_bound(v.applied_idx));
        return std::move(v.chosen);
    }

    static bool written_values_read_back(const request_result& r, const state_storage::transaction& w)
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
        switch (e.kind) {
        case entry_kind::register_client:
            if (e.pid == seq_paxos_.get_pid() && registrations_.erase(e.client_id) > 0) {
                client_replies_.push_back({e.client_id, index, response()});
            }
            break;
        case entry_kind::client_request:
            if (const auto it = requests_.find({e.session, e.request_number}); it != requests_.end()) {
                client_replies_.push_back({it->second, e.session, response()});
                requests_.erase(it);
            }
            break;
        case entry_kind::expire_session:
            if (const auto it = expirations_.find(e.session); it != expirations_.end()) {
                client_replies_.push_back({it->second, e.session, response()});
                expirations_.erase(it);
            }
            break;
        case entry_kind::normal:
        case entry_kind::stop_sign:
        case entry_kind::results:
        case entry_kind::rejected:
            break;
        }
    }

    request_result run_request(const entry& e, const state_storage::transaction& t) const
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
            return {0, std::move(a.reply), std::move(a.mutations)};
        }
        if (e.request_number == s->request_number && s->request_number != 0) {
            return {0, s->reply, {}};
        }
        return {static_cast<std::uint64_t>(std::to_underlying(client_errc::stale_request)), {}, {}};
    }

    static void write_the_result(const entry& e, const std::uint64_t index, const request_result& r,
                                 state_storage::transaction& w)
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
        case entry_kind::results:
        case entry_kind::rejected:
            break;
        }
    }

    void forget_the_runs()
    {
        open_.reset();
        own_.clear();
        answered_.reset();
        selection_.reset();
        answers_in_.clear();
        try_agains_in_.clear();
        refusals_in_.clear();
        refusal_.reset();
    }

    state_storage::transaction& open_the_base(const std::uint64_t below)
    {
        if (open_) {
            return *open_;
        }
        auto w = state_.begin();
        if (!w) [[unlikely]] {
            throw write_error(w.error(), "konsens: the state storage failed to begin a write");
        }
        for (const auto& [index, o] : own_) {
            if (index >= below) {
                break;
            }
            write_the_result(o.request, index, o.result, *w);
        }
        return open_.emplace(std::move(*w));
    }

    own_result run_on_the_base(const entry& e, const std::uint64_t index, state_storage::transaction& w) const
    {
        auto r = run_request(e, w);
        auto bytes = encode_result(r);
        const auto crc = compute_result_crc(index, bytes);
        return {e, std::move(r), std::move(bytes), crc};
    }

    std::optional<std::uint64_t> run_the_decided_requests(const std::uint64_t applied_idx,
                                                          const std::span<const entry> decided)
    {
        std::optional<std::uint64_t> first;
        for (std::uint64_t k = 0; k < decided.size(); ++k) {
            const auto index = applied_idx + 1 + k;
            if (!entry_is_a_request(decided[k]) || own_.contains(index)) {
                continue;
            }
            auto& w = open_the_base(std::numeric_limits<std::uint64_t>::max());
            auto o = run_on_the_base(decided[k], index, w);
            write_the_result(o.request, index, o.result, w);
            own_.emplace(index, std::move(o));
            if (!first) {
                first = index;
            }
        }
        return first;
    }

    void run_again(const std::uint64_t index)
    {
        open_.reset();
        auto& w = open_the_base(index);
        auto& held = own_.at(index);
        auto o = run_on_the_base(held.request, index, w);
        if (o.bytes != held.bytes) {
            held = std::move(o);
            own_.erase(own_.upper_bound(index), own_.end());
        }
        for (auto it = own_.find(index); it != own_.end(); ++it) {
            write_the_result(it->second.request, it->first, it->second.result, w);
        }
    }

    answer answer_from(const ballot n, const std::uint64_t pointer, const std::uint64_t from,
                       const std::uint64_t second_run) const
    {
        answer a{n, pointer, from, 0, {}, second_run};
        for (const auto& [index, o] : own_) {
            if (index < from) {
                a.base_crc = extend_base_crc(a.base_crc, index, o.crc);
            } else {
                a.results.push_back({index, o.bytes, o.crc});
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

    void follow_the_leader(const std::uint64_t pointer, const std::span<const chosen_result> chosen)
    {
        if (seq_paxos_.get_state().first != role::leader) {
            selection_.reset();
            return;
        }
        const auto n = seq_paxos_.get_leader_state().n_leader;
        if (!selection_ || selection_->get_ballot() != n) {
            selection_ = selection::with(n, nodes_, pointer);
            return;
        }
        selection_->move_pointer(pointer, chosen);
    }

    void run_again_for_the_leader(const ballot n, const std::uint64_t pointer)
    {
        const auto self = seq_paxos_.get_pid();
        for (const auto index : get_own_result_indexes()) {
            if (!own_.contains(index) || !selection_->asked_to_run_again(self, index) ||
                selection_->ran_again(self, index)) {
                continue;
            }
            run_again(index);
            selection_->count_answer(self, answer_from(n, pointer, index, index));
        }
    }

    result execute_verify(const std::span<const chosen_result> chosen)
    {
        if (seq_paxos_.recovering() || !seq_paxos_.votes()) {
            forget_the_runs();
            return {};
        }
        const auto promise = seq_paxos_.get_promise();
        if (!promise) [[unlikely]] {
            return std::unexpected(promise.error());
        }
        const auto a = read_or_throw(read_or_throw(state_.begin_read_only()).get_applied());
        follow_the_leader(a.pointer, chosen);
        const auto decided = seq_paxos_.read_decided_suffix(a.pointer);
        if (!decided) [[unlikely]] {
            return std::unexpected(decided.error());
        }
        const auto first = run_the_decided_requests(a.index, std::span(*decided).subspan(a.index - a.pointer));
        const auto answered = std::pair{*promise, a.pointer};
        if (!own_.empty() && (resend_answer_ || answered_ != answered)) {
            send_answer(promise->pid, answer_from(*promise, a.pointer, a.pointer + 1, 0));
            answered_ = answered;
        } else if (first) {
            send_answer(promise->pid, answer_from(*promise, a.pointer, *first, 0));
            answered_ = answered;
        }
        if (resend_answer_) {
            send_the_refusal(*promise);
        }
        resend_answer_ = false;
        for (const auto& [from, t] : std::exchange(try_agains_in_, {})) {
            if (t.n != *promise || !own_.contains(t.index)) {
                continue;
            }
            run_again(t.index);
            send_answer(from, answer_from(*promise, a.pointer, t.index, t.index));
        }
        if (!selection_) {
            answers_in_.clear();
            return {};
        }
        count_the_answers();
        const auto n = selection_->get_ballot();
        run_again_for_the_leader(n, a.pointer);
        if (seq_paxos_.get_state().second != phase::accept) {
            return {};
        }
        const auto decided_idx = a.pointer + decided->size();
        ask_to_run_again(selection_->select_results({a.pointer, decided_idx, *decided}));
        run_again_for_the_leader(n, a.pointer);
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

    void answer_indexed_reads()
    {
        if (reads_.empty()) {
            return;
        }
        const auto t = read_or_throw(state_.begin_read_only());
        const auto applied_idx = read_or_throw(t.get_applied()).index;
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
    std::optional<state_storage::transaction> open_;
    konsens::state_storage state_;
    ballot_leader_election ble_;
    ble_config ble_config_;
    logical_clock election_clock_;
    logical_clock resend_message_clock_;
    M machine_;
    std::vector<client_reply> client_replies_;
    std::vector<client_reply> released_replies_;
    std::vector<fault_proof> log_records_;
    std::vector<fault_proof> released_log_records_;
    std::vector<message> released_messages_;
    std::vector<received> received_batch_;
    std::vector<message> message_batch_;
    std::set<std::uint64_t> registrations_;
    std::map<std::pair<std::uint64_t, std::uint64_t>, std::uint64_t> requests_;
    std::map<std::uint64_t, std::uint64_t> expirations_;
    std::map<std::uint64_t, client_read> reads_;
    std::uint64_t last_read_{};
    std::unique_ptr<std::uint64_t> commit_count_ = std::make_unique<std::uint64_t>();
    std::uint64_t nonce_;
    bool repair_started_{};
    std::optional<std::uint64_t> open_mark_;
    bool waits_for_the_operator_{};
    std::uint64_t max_in_flight_;
    std::vector<node_id> nodes_;
    std::map<std::uint64_t, own_result> own_;
    std::optional<std::pair<ballot, std::uint64_t>> answered_;
    std::optional<konsens::selection> selection_;
    std::vector<std::pair<node_id, answer>> answers_in_;
    std::vector<std::pair<node_id, try_again>> try_agains_in_;
    std::vector<std::pair<node_id, refused>> refusals_in_;
    std::optional<refused> refusal_;
    std::vector<message> execute_verify_outgoing_;
    std::vector<entry> waiting_;
    bool resend_answer_{};
};

}
