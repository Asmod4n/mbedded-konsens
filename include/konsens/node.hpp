#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iterator>
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
#include "konsens/sequence_paxos.hpp"
#include "konsens/state_machine.hpp"
#include "konsens/storage.hpp"
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
            config.resend_message_tick_timeout == 0) [[unlikely]] {
            throw std::logic_error("konsens: configuration_id and each tick timeout must not be 0");
        }
        auto sp = sequence_paxos<S>::with({config.pid, config.peers}, std::move(internal_storage), nonce);
        if (!sp) [[unlikely]] {
            return std::unexpected(sp.error());
        }
        const auto record = own_storage.get_repair_record();
        if (!record) [[unlikely]] {
            return std::unexpected(record.error());
        }
        if (sp->votes() && record->open) {
            if (auto c = own_storage.close_the_repair(); !c) [[unlikely]] {
                return std::unexpected(c.error());
            }
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
        if (auto r = n.repair_after_a_damage(); !r) [[unlikely]] {
            return std::unexpected(r.error());
        }
        if (n.seq_paxos_.votes()) {
            if (auto r = n.apply_decided_entries(); !r) [[unlikely]] {
                return std::unexpected(r.error());
            }
        }
        n.release_messages_and_replies();
        return n;
    }

    [[nodiscard]] std::expected<transaction, std::error_code> tick()
    {
        if (auto r = repair_after_a_damage(); !r) [[unlikely]] {
            return std::unexpected(r.error());
        }
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
        }
        return current_transaction();
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
        current_transaction().throw_if_another_commit(t);
        *commit_count_ += 1;
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
        release_messages_and_replies();
        return {};
    }

    [[nodiscard]] std::expected<transaction, std::error_code> append(entry e)
    {
        if (auto r = seq_paxos_.append(std::move(e)); !r) [[unlikely]] {
            return std::unexpected(r.error());
        }
        return current_transaction();
    }

    [[nodiscard]] std::expected<transaction, std::error_code> register_client(const std::uint64_t client_id)
    {
        if (auto r = seq_paxos_.refuse_if_recovering(); !r) [[unlikely]] {
            return std::unexpected(r.error());
        }
        if (auto r = seq_paxos_.append({entry_kind::register_client, {}, 0, 0, client_id, seq_paxos_.get_pid()}); !r)
            [[unlikely]] {
            return std::unexpected(r.error());
        }
        registrations_.insert(client_id);
        return current_transaction();
    }

    [[nodiscard]] std::expected<transaction, std::error_code> client_request(const std::uint64_t client_id,
                                                                             const std::uint64_t session,
                                                                             const std::uint64_t request_number,
                                                                             std::vector<std::byte> payload)
    {
        if (auto r = seq_paxos_.refuse_if_recovering(); !r) [[unlikely]] {
            return std::unexpected(r.error());
        }
        if (request_number == 0) [[unlikely]] {
            throw std::logic_error("konsens: a request number starts at 1");
        }
        if (auto r = seq_paxos_.append({entry_kind::client_request, std::move(payload), session, request_number}); !r)
            [[unlikely]] {
            return std::unexpected(r.error());
        }
        requests_.insert_or_assign({session, request_number}, client_id);
        return current_transaction();
    }

    [[nodiscard]] std::expected<transaction, std::error_code> client_query(const std::uint64_t client_id,
                                                                           std::vector<std::byte> payload)
    {
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
    }

    [[nodiscard]] std::expected<transaction, std::error_code> expire_session(const std::uint64_t client_id,
                                                                             const std::uint64_t session)
    {
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
        if (!*undecided) {
            if (auto r = seq_paxos_.append(expiry); !r) [[unlikely]] {
                return std::unexpected(r.error());
            }
        }
        expirations_.insert_or_assign(session, client_id);
        return current_transaction();
    }

    std::vector<client_reply> take_client_replies() { return std::exchange(released_replies_, {}); }

    std::vector<message> take_outgoing_messages() { return std::exchange(released_messages_, {}); }

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
    [[nodiscard]] result allow_one_more_repair() { return own_storage_.allow_one_more_repair(); }
    const ballot_leader_election& get_ballot_leader_election() const { return ble_; }
    const M& get_state_machine() const { return machine_; }
    const konsens::state_storage& get_state_storage() const { return state_; }
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
        if (auto r = seq_paxos_.reconnected(pid); !r) [[unlikely]] {
            return std::unexpected(r.error());
        }
        return current_transaction();
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
        released_replies_.insert(released_replies_.end(), std::make_move_iterator(client_replies_.begin()),
                                 std::make_move_iterator(client_replies_.end()));
        client_replies_.clear();
        received_batch_.clear();
        message_batch_.clear();
    }

    void drop_held_messages_and_replies()
    {
        static_cast<void>(seq_paxos_.take_outgoing_messages());
        static_cast<void>(ble_.take_outgoing_messages());
        client_replies_.clear();
        received_batch_.clear();
        message_batch_.clear();
    }

    template <typename T>
    result handle(node_id from, node_id to, const T& msg)
    {
        if constexpr (requires { paxos_msg{std::in_place_type<T>, msg}; }) {
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
          resend_message_clock_{0, config.resend_message_tick_timeout}, machine_(std::move(machine)), nonce_(nonce)
    {
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

    result repair_after_a_damage()
    {
        const auto e = seq_paxos_.get_error();
        if (!e || !error_is_a_damage(*e) || waits_for_the_operator_) {
            return {};
        }
        const auto record = own_storage_.get_repair_record();
        if (!record) [[unlikely]] {
            return std::unexpected(record.error());
        }
        if (!record->open) {
            if (record->count >= auto_repair_count) {
                waits_for_the_operator_ = true;
                return {};
            }
            if (auto c = own_storage_.count_a_repair(); !c) [[unlikely]] {
                return c;
            }
        }
        nonce_ += 1;
        repair_started_ = true;
        return seq_paxos_.start_repair(nonce_);
    }

    result close_the_repair()
    {
        repair_started_ = false;
        if (auto c = own_storage_.close_the_repair(); !c) [[unlikely]] {
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
        if (auto r = apply_decided_entries(); !r) [[unlikely]] {
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

    result apply_decided_entries()
    {
        auto w = state_.begin();
        if (!w) [[unlikely]] {
            throw write_error(w.error(), "konsens: the state storage failed to begin a write");
        }
        const auto a = read_or_throw(w->get_applied());
        const auto entries = seq_paxos_.read_decided_suffix(a.index);
        if (!entries) [[unlikely]] {
            return std::unexpected(entries.error());
        }
        if (entries->empty()) {
            return {};
        }
        auto index = a.index;
        for (const auto& e : *entries) {
            index += 1;
            apply_entry(e, index, *w);
        }
        const auto crc = seq_paxos_.get_prefix_chain_crc(index);
        if (!crc) [[unlikely]] {
            return std::unexpected(crc.error());
        }
        write_or_throw(w->set_applied({index, *crc}));
        return std::move(*w).commit();
    }

    void apply_entry(const entry& e, const std::uint64_t index, state_storage::transaction& w)
    {
        switch (e.kind) {
        case entry_kind::register_client:
            write_or_throw(w.insert_or_assign_session(index, {}));
            if (e.pid == seq_paxos_.get_pid() && registrations_.erase(e.client_id) > 0) {
                client_replies_.push_back({e.client_id, index, std::vector<std::byte>{}});
            }
            break;
        case entry_kind::client_request: {
            auto response = execute_request(e, w);
            if (const auto it = requests_.find({e.session, e.request_number}); it != requests_.end()) {
                client_replies_.push_back({it->second, e.session, std::move(response)});
                requests_.erase(it);
            }
            break;
        }
        case entry_kind::expire_session:
            write_or_throw(w.erase_session(e.session));
            if (const auto it = expirations_.find(e.session); it != expirations_.end()) {
                client_replies_.push_back({it->second, e.session, std::vector<std::byte>{}});
                expirations_.erase(it);
            }
            break;
        case entry_kind::normal:
        case entry_kind::stop_sign:
            break;
        }
    }

    std::expected<std::vector<std::byte>, std::error_code> execute_request(const entry& e,
                                                                           state_storage::transaction& w)
    {
        const auto s = read_or_throw(w.find_session(e.session));
        if (!s) [[unlikely]] {
            return std::unexpected(make_error_code(client_errc::session_expired));
        }
        if (e.request_number > s->request_number) {
            auto a = machine_.apply(e.payload, write_view{w});
            for (const auto& m : a.mutations) {
                if (m.value) {
                    write_or_throw(w.insert_or_assign(m.key, *m.value));
                } else {
                    write_or_throw(w.erase(m.key));
                }
            }
            write_or_throw(w.insert_or_assign_session(e.session, {e.request_number, a.reply}));
            return std::move(a.reply);
        }
        if (e.request_number == s->request_number && s->request_number != 0) {
            return s->reply;
        }
        return std::unexpected(make_error_code(client_errc::stale_request));
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
    konsens::state_storage state_;
    ballot_leader_election ble_;
    ble_config ble_config_;
    logical_clock election_clock_;
    logical_clock resend_message_clock_;
    M machine_;
    std::vector<client_reply> client_replies_;
    std::vector<client_reply> released_replies_;
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
    bool waits_for_the_operator_{};
};

}
