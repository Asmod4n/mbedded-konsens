#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

#include "batches.hpp"
#include "echo.hpp"
#include "konsens/node.hpp"
#include "sqlite_damage.hpp"
#include "sqlite_storage.hpp"

namespace {

using konsens::node_id;
using konsens::persistent_storage;
using node_t = konsens::node<persistent_storage, echo>;

constexpr std::uint64_t election_ticks = 3;
constexpr std::uint64_t resend_ticks = 2;
constexpr int rounds = 40;

konsens::node_config config_of(node_id pid)
{
    std::vector<node_id> peers;
    for (node_id p = 1; p <= 3; ++p) {
        if (p != pid) {
            peers.push_back(p);
        }
    }
    return {1, pid, peers, 0, election_ticks, resend_ticks, {}, true};
}

konsens::entry make_entry(std::uint8_t v)
{
    return {konsens::entry_kind::normal, {std::byte{v}}};
}

void step(std::vector<node_t>& nodes, const std::array<bool, 3>& alive)
{
    batches::deliver(nodes, [&alive](const konsens::message& m) -> std::size_t {
        return alive.at(batches::from_of(m) - 1) && alive.at(batches::to_of(m) - 1) ? 1 : 0;
    });
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        if (alive[i]) {
            batches::tick_and_commit(nodes[i]);
        }
    }
}

}

// The test of the owner in one process: three nodes elect a leader through
// BLE, a follower appends, and every node reads the decided entry. Then
// the leader stops, the others elect a new leader, and a new entry is
// decided by the two nodes that remain. The entry is appended after the
// new election: an entry forwarded to a stopped leader is lost, as in
// mod.rs:371-384 forward_proposals.
TEST_CASE("three nodes elect, decide and survive the loss of the leader")
{
    std::vector<node_t> nodes;
    for (node_id p = 1; p <= 3; ++p) {
        nodes.push_back(*node_t::with(config_of(p), sqlite_storage::open_new(), sqlite_storage::open_new_node_storage(),
                                      echo{}, 0));
    }
    const std::array<bool, 3> all{true, true, true};
    for (int i = 0; i < rounds; ++i) {
        step(nodes, all);
    }
    batches::commit(nodes[0], nodes[0].append(make_entry(1)));
    for (int i = 0; i < rounds; ++i) {
        step(nodes, all);
    }
    for (const auto& n : nodes) {
        CHECK(*n.read_decided_suffix(0) == std::vector{make_entry(1)});
    }
    const auto leader = nodes[0].get_sequence_paxos().get_promise()->pid;
    REQUIRE(leader == 3);
    std::array<bool, 3> without_leader = all;
    without_leader[leader - 1] = false;
    for (int i = 0; i < rounds; ++i) {
        step(nodes, without_leader);
    }
    batches::commit(nodes[0], nodes[0].append(make_entry(2)));
    for (int i = 0; i < rounds; ++i) {
        step(nodes, without_leader);
    }
    CHECK(*nodes[0].read_decided_suffix(0) == std::vector{make_entry(1), make_entry(2)});
    CHECK(*nodes[1].read_decided_suffix(0) == std::vector{make_entry(1), make_entry(2)});
}

// Each tick timeout of 0 would never fire: a wrong use that fails loudly,
// as ServerConfig::validate in omni_paxos.rs:189-200.
TEST_CASE("a tick timeout of 0 is a wrong use")
{
    auto c = config_of(1);
    c.election_tick_timeout = 0;
    CHECK_THROWS_AS(static_cast<void>(node_t::with(c, sqlite_storage::open_new(),
                                                   sqlite_storage::open_new_node_storage(), echo{}, 0)),
                    std::logic_error);
}

// Each error value has a message of its own, because a log or a client
// reads the message and not the number.
TEST_CASE("each client error has its own message")
{
    const std::array codes{konsens::client_errc::building,      konsens::client_errc::session_expired,
                           konsens::client_errc::stale_request, konsens::client_errc::retry,
                           konsens::client_errc::rejected,      konsens::client_errc::reconfiguring,
                           konsens::client_errc::retired,       konsens::client_errc::stopped};
    std::vector<std::string> messages;
    for (const auto c : codes) {
        const std::error_code e = c;
        CHECK(e.category().name() == std::string_view{"konsens.client"});
        messages.push_back(e.message());
    }
    std::ranges::sort(messages);
    CHECK(std::ranges::adjacent_find(messages) == messages.end());
    CHECK(std::ranges::find(messages, "Unknown client error") == messages.end());
}

// Plan clean-stop 2.4: a leader that starts after a crash retires. It
// answers no BLE heartbeat, so the peers do not keep it as leader, it
// refuses each call with retired, and the other two nodes elect a leader
// and decide without it.
TEST_CASE("a leader that starts after a crash retires and answers no heartbeat")
{
    std::vector<node_t> nodes;
    for (node_id p = 1; p <= 3; ++p) {
        nodes.push_back(*node_t::with(config_of(p), sqlite_storage::open_new(), sqlite_storage::open_new_node_storage(),
                                      echo{}, 0));
    }
    const std::array<bool, 3> all{true, true, true};
    for (int i = 0; i < rounds; ++i) {
        step(nodes, all);
    }
    batches::commit(nodes[0], nodes[0].append(make_entry(1)));
    for (int i = 0; i < rounds; ++i) {
        step(nodes, all);
    }
    REQUIRE(nodes[2].get_sequence_paxos().get_state() == std::pair{konsens::role::leader, konsens::phase::accept});
    auto kept = sqlite_storage::open_copy_of(nodes[2].get_sequence_paxos().get_internal_storage());
    auto crashed = config_of(3);
    crashed.first_start = false;
    nodes[2] = *node_t::with(crashed, std::move(kept), sqlite_storage::open_new_node_storage(), echo{}, 1);
    REQUIRE(nodes[2].retired());
    CHECK(nodes[2].take_outgoing_messages().empty());
    std::vector<konsens::message> heartbeat;
    heartbeat.emplace_back(konsens::ble_message{1, 3, konsens::heartbeat_request{1}});
    batches::commit(nodes[2], nodes[2].handle_incoming(std::move(heartbeat)));
    CHECK(nodes[2].take_outgoing_messages().empty());
    const auto refused = [](const auto& r) { return !r && r.error() == konsens::client_errc::retired; };
    CHECK(refused(nodes[2].append(make_entry(2))));
    CHECK(refused(nodes[2].register_client(1)));
    CHECK(refused(nodes[2].client_request(1, 1, 1, {std::byte{1}})));
    CHECK(refused(nodes[2].client_query(1, {std::byte{1}})));
    CHECK(refused(nodes[2].expire_session(1, 1)));
    for (int i = 0; i < rounds; ++i) {
        step(nodes, all);
    }
    batches::commit(nodes[0], nodes[0].append(make_entry(2)));
    for (int i = 0; i < rounds; ++i) {
        step(nodes, all);
    }
    for (const auto i : {std::size_t{0}, std::size_t{1}}) {
        CHECK(*nodes.at(i).read_decided_suffix(0) == std::vector{make_entry(1), make_entry(2)});
    }
}

namespace {

constexpr node_id behind = 1;
constexpr node_id elected = 3;

template <typename Lost>
bool deliver_losing(std::vector<node_t>& nodes, const Lost lost)
{
    bool any = false;
    batches::deliver(nodes, [&any, &lost](const konsens::message& m) -> std::size_t {
        const auto* p = std::get_if<konsens::paxos_message>(&m);
        if (p != nullptr && lost(*p)) {
            any = true;
            return 0;
        }
        return 1;
    });
    return any;
}

void deliver_all(std::vector<node_t>& nodes)
{
    static_cast<void>(deliver_losing(nodes, [](const konsens::paxos_message&) { return false; }));
}

std::vector<konsens::paxos_message> step_recording(std::vector<node_t>& nodes, const node_id watched)
{
    std::vector<konsens::paxos_message> seen;
    batches::deliver(nodes, [&seen, watched](const konsens::message& m) -> std::size_t {
        if (const auto* p = std::get_if<konsens::paxos_message>(&m);
            p != nullptr && (p->to == watched || p->from == watched)) {
            seen.push_back(*p);
        }
        return 1;
    });
    for (auto& n : nodes) {
        batches::tick_and_commit(n);
    }
    return seen;
}

template <typename T>
bool holds_any(const std::vector<konsens::paxos_message>& seen)
{
    return std::ranges::any_of(seen, [](const auto& p) { return std::holds_alternative<T>(p.msg); });
}

std::vector<node_t> elect_three()
{
    std::vector<node_t> nodes;
    for (node_id p = 1; p <= 3; ++p) {
        nodes.push_back(*node_t::with(config_of(p), sqlite_storage::open_new(), sqlite_storage::open_new_node_storage(),
                                      echo{}, 0));
    }
    const std::array<bool, 3> all{true, true, true};
    for (int i = 0; i < rounds; ++i) {
        step(nodes, all);
    }
    REQUIRE(nodes[behind - 1].get_sequence_paxos().get_promise()->pid == elected);
    return nodes;
}

std::vector<node_t> lose_the_last_decide()
{
    auto nodes = elect_three();
    deliver_all(nodes);
    batches::commit(nodes[elected - 1], nodes[elected - 1].append(make_entry(1)));
    const bool lost = deliver_losing(nodes, [](const konsens::paxos_message& p) {
        return p.to == behind && std::holds_alternative<konsens::decide>(p.msg);
    });
    REQUIRE(lost);
    REQUIRE(nodes[elected - 1].get_sequence_paxos().get_decided_idx() == 1);
    REQUIRE(nodes[behind - 1].get_sequence_paxos().get_decided_idx() == 0);
    return nodes;
}

void check_caught_up(const std::vector<node_t>& nodes)
{
    const auto& f = nodes[behind - 1].get_sequence_paxos();
    const auto& l = nodes[elected - 1].get_sequence_paxos();
    CHECK(f.get_decided_idx() == l.get_decided_idx());
    CHECK(*nodes[behind - 1].read_decided_suffix(0) == *nodes[elected - 1].read_decided_suffix(0));
    CHECK(sqlite_storage::log_of(f.get_internal_storage()) == sqlite_storage::log_of(l.get_internal_storage()));
}

void check_resynced(const std::vector<konsens::paxos_message>& seen)
{
    CHECK(holds_any<konsens::decide>(seen));
    CHECK(holds_any<konsens::prepare_req>(seen));
    CHECK(holds_any<konsens::prepare>(seen));
    CHECK(holds_any<konsens::promise>(seen));
    CHECK(holds_any<konsens::accept_sync>(seen));
}

}

// Seed 4 of counter_stress: the last Decide to a follower is lost. The
// caller reports the dropped connection with reconnected, as
// OmniPaxos::reconnected in omni_paxos.rs:394-398, and the follower
// catches up through PrepareReq and AcceptSync.
TEST_CASE("a follower that lost the last Decide catches up after reconnected")
{
    auto nodes = lose_the_last_decide();
    const auto t = nodes[behind - 1].reconnected(elected);
    REQUIRE(t);
    REQUIRE(nodes[behind - 1].commit(*t));
    const std::array<bool, 3> all{true, true, true};
    for (int i = 0; i < rounds; ++i) {
        step(nodes, all);
    }
    CHECK(*nodes[behind - 1].read_decided_suffix(0) == std::vector{make_entry(1)});
    check_caught_up(nodes);
}

// The same loss with no call to reconnected. The leader sends a Decide
// with the next sequence number at each resend timeout
// (doc/plan-decide-on-resend.md). The follower sees DroppedPreceding,
// goes to Recover and catches up through PrepareReq, Prepare, Promise and
// AcceptSync. Without the Decide at the resend timeout, no later message
// shows the gap and the follower stays one entry behind.
TEST_CASE("a follower that lost the last Decide catches up after a resend timeout")
{
    auto nodes = lose_the_last_decide();
    std::vector<konsens::paxos_message> seen;
    for (int i = 0; i < rounds; ++i) {
        std::ranges::move(step_recording(nodes, behind), std::back_inserter(seen));
    }
    check_resynced(seen);
    CHECK(*nodes[behind - 1].read_decided_suffix(0) == std::vector{make_entry(1)});
    check_caught_up(nodes);
}

// The lost messages carry an entry: the AcceptDecide with entry 2 and the
// Decide after it. The follower lacks the entry itself, not only the
// decided index, so the AcceptSync must carry the entry.
TEST_CASE("a follower that lost an AcceptDecide with an entry catches up after a resend timeout")
{
    auto nodes = elect_three();
    batches::commit(nodes[elected - 1], nodes[elected - 1].append(make_entry(1)));
    deliver_all(nodes);
    REQUIRE(nodes[behind - 1].get_sequence_paxos().get_decided_idx() == 1);
    batches::commit(nodes[elected - 1], nodes[elected - 1].append(make_entry(2)));
    const bool lost = deliver_losing(nodes, [](const konsens::paxos_message& p) {
        return p.to == behind && (std::holds_alternative<konsens::accept_decide>(p.msg) ||
                                  std::holds_alternative<konsens::decide>(p.msg));
    });
    REQUIRE(lost);
    REQUIRE(nodes[elected - 1].get_sequence_paxos().get_decided_idx() == 2);
    REQUIRE(nodes[behind - 1].get_sequence_paxos().get_accepted_idx() == 1);
    std::vector<konsens::paxos_message> seen;
    for (int i = 0; i < rounds; ++i) {
        std::ranges::move(step_recording(nodes, behind), std::back_inserter(seen));
    }
    check_resynced(seen);
    CHECK(*nodes[behind - 1].read_decided_suffix(0) == std::vector{make_entry(1), make_entry(2)});
    check_caught_up(nodes);
}

namespace {

struct recording_storage {
    using read_only_transaction = persistent_storage::read_only_transaction;

    class transaction {
    public:
        transaction(persistent_storage::transaction t, std::vector<const konsens::entry*>* appended)
            : t_{std::move(t)}, appended_{appended}
        {
        }

        auto get_meta() const { return t_.get_meta(); }
        auto get_entries(const std::uint64_t from, const std::uint64_t to) const { return t_.get_entries(from, to); }
        auto get_last_entry() const { return t_.get_last_entry(); }
        auto get_entry_state(const std::uint64_t index) const { return t_.get_entry_state(index); }
        auto get_chain_crc(const std::uint64_t index) const { return t_.get_chain_crc(index); }
        auto get_config_first_idx() const { return t_.get_config_first_idx(); }
        auto find(const std::span<const std::byte> key) const { return t_.find(key); }
        auto find_session(const std::uint64_t session) const { return t_.find_session(session); }
        auto insert_or_assign(const std::span<const std::byte> key, const std::span<const std::byte> value)
        {
            return t_.insert_or_assign(key, value);
        }
        auto erase(const std::span<const std::byte> key) { return t_.erase(key); }
        auto insert_or_assign_session(const std::uint64_t session, const konsens::session_record& record)
        {
            return t_.insert_or_assign_session(session, record);
        }
        auto erase_session(const std::uint64_t session) { return t_.erase_session(session); }
        auto set_applied(const std::uint64_t index, const std::uint32_t crc)
        {
            return t_.set_applied(index, crc);
        }
        auto begin_nested() { return t_.begin_nested(); }
        bool has_writes() const { return t_.has_writes(); }
        bool has_log_writes() const { return t_.has_log_writes(); }

        auto append_entries(const std::span<const konsens::entry> entries)
        {
            appended_->push_back(entries.data());
            return t_.append_entries(entries);
        }
        auto append_on_prefix(const std::uint64_t from_idx, const std::span<const konsens::entry> entries)
        {
            return t_.append_on_prefix(from_idx, entries);
        }
        auto set_promise(const konsens::ballot b) { return t_.set_promise(b); }
        auto set_accepted_round(const konsens::ballot b) { return t_.set_accepted_round(b); }
        auto set_decided_index(const std::uint64_t index) { return t_.set_decided_index(index); }
        auto set_config_first_idx(const std::uint64_t index) { return t_.set_config_first_idx(index); }
        std::expected<void, std::error_code> commit() && { return std::move(t_).commit(); }
        std::expected<void, std::error_code> commit_without_a_sync() &&
        {
            return std::move(t_).commit_without_a_sync();
        }

    private:
        persistent_storage::transaction t_;
        std::vector<const konsens::entry*>* appended_;
    };

    persistent_storage inner;
    std::vector<const konsens::entry*>* appended;

    std::expected<transaction, std::error_code> begin()
    {
        return inner.begin().transform(
            [this](persistent_storage::transaction t) { return transaction{std::move(t), appended}; });
    }
    std::expected<read_only_transaction, std::error_code> begin_read_only() const { return inner.begin_read_only(); }
};

}

// The rule "Nothing is copied to pass it": the entries of a decoded
// AcceptDecide reach the storage of the follower where cbor::ref holds
// them. The test compares the address of the first entry, not an address
// in the wire bytes, because decode copies each payload into its entry.
// The follower must be in (Follower, Accept) with the next sequence
// number, or the handler writes nothing.
TEST_CASE("a decoded AcceptDecide reaches the storage with no copy")
{
    using recording_node = konsens::node<recording_storage, echo>;
    std::array<std::vector<const konsens::entry*>, 3> appended;
    std::vector<recording_node> nodes;
    for (node_id p = 1; p <= 3; ++p) {
        nodes.push_back(*recording_node::with(config_of(p),
                                              recording_storage{sqlite_storage::open_new(), &appended.at(p - 1)},
                                              sqlite_storage::open_new_node_storage(), echo{}, 0));
    }
    const auto deliver = [&nodes] {
        batches::deliver(nodes, [](const konsens::message&) -> std::size_t { return 1; });
    };
    for (int i = 0; i < rounds; ++i) {
        deliver();
        for (auto& n : nodes) {
            batches::tick_and_commit(n);
        }
    }
    deliver();
    const node_id leader = nodes.front().get_sequence_paxos().get_promise()->pid;
    REQUIRE(leader != 0);
    const node_id follower = leader == 1 ? 2 : 1;
    batches::commit(nodes.at(leader - 1), nodes.at(leader - 1).append(make_entry(7)));
    bool checked = false;
    for (const auto& m : nodes.at(leader - 1).take_outgoing_messages()) {
        const auto to = std::visit([](const auto& x) { return x.to; }, m);
        if (to != follower) {
            continue;
        }
        auto bytes = konsens::encode(m);
        REQUIRE(bytes);
        auto r = konsens::decode(std::move(*bytes));
        REQUIRE(r);
        const auto* acc = std::get_if<cbor::ref<konsens::accept_decide>>(&r->msg);
        if (acc == nullptr) {
            continue;
        }
        const auto* first = (*acc)->entries.data();
        auto& seen = appended.at(follower - 1);
        seen.clear();
        std::vector<konsens::received> batch;
        batch.push_back(std::move(*r));
        batches::commit(nodes.at(follower - 1), nodes.at(follower - 1).handle_incoming(std::move(batch)));
        REQUIRE(seen.size() == 1);
        CHECK(seen.front() == first);
        checked = true;
    }
    CHECK(checked);
}

namespace {

void lose_the_last_entry_in(const std::filesystem::path& directory)
{
    sqlite_damage::change_with_sqlite(directory, [](sqlite3* db) { sqlite_damage::delete_the_last_entry(db); });
}

void end_without_stop(node_t& n)
{
    const auto gone = std::move(n);
}

}

// Plan clean-stop 2.6, decided 2026-10-27: a damage that the start
// checks find retires the node, also after a clean stop. The two other
// nodes elect a leader among them when the damaged node led, and decide
// without it.
TEST_CASE("a damage that the start checks find retires the node")
{
    const auto log_directory = sqlite_storage::new_directory();
    const auto own_directory = sqlite_storage::new_directory();
    std::vector<node_t> nodes;
    for (node_id p = 1; p <= 2; ++p) {
        nodes.push_back(*node_t::with(config_of(p), sqlite_storage::open_new(), sqlite_storage::open_new_node_storage(),
                                      echo{}, 0));
    }
    nodes.push_back(*node_t::with(config_of(3), sqlite_storage::open_in(log_directory),
                                  sqlite_storage::open_node_storage_in(own_directory), echo{}, 0));
    const std::array<bool, 3> all{true, true, true};
    for (int i = 0; i < rounds; ++i) {
        step(nodes, all);
    }
    batches::commit(nodes[0], nodes[0].append(make_entry(1)));
    for (int i = 0; i < rounds; ++i) {
        step(nodes, all);
    }
    nodes[2].stop();
    end_without_stop(nodes[2]);
    lose_the_last_entry_in(log_directory);
    auto later = config_of(3);
    later.first_start = false;
    nodes[2] = *node_t::with(later, sqlite_storage::open_in(log_directory),
                             sqlite_storage::open_node_storage_in(own_directory), echo{}, 10);
    CHECK(nodes[2].retired());
    CHECK_FALSE(nodes[2].get_sequence_paxos().votes());
    CHECK(nodes[2].get_sequence_paxos().get_error() == make_error_code(konsens::storage_errc::missing_entry));
    for (int i = 0; i < rounds; ++i) {
        step(nodes, all);
    }
    const auto leader = nodes[0].get_sequence_paxos().get_promise()->pid;
    REQUIRE((leader == 1 || leader == 2));
    batches::commit(nodes.at(leader - 1), nodes.at(leader - 1).append(make_entry(2)));
    for (int i = 0; i < rounds; ++i) {
        step(nodes, all);
    }
    for (const auto i : {std::size_t{0}, std::size_t{1}}) {
        CHECK(*nodes.at(i).read_decided_suffix(0) == std::vector{make_entry(1), make_entry(2)});
    }
}

// Owner, 2026-10-26: the meta record and the log stand in one environment,
// and each commit writes both. A store with no meta record was changed from
// outside the library. The node waits for the operator and does not
// vote.
TEST_CASE("a start without a meta record waits for the operator")
{
    const auto log_directory = sqlite_storage::new_directory();
    const auto own_directory = sqlite_storage::new_directory();
    {
        auto s = sqlite_storage::open_in(log_directory);
        sqlite_storage::write(s, [](auto& t) { REQUIRE(t.set_promise({1, 1, 0, 2})); });
    }
    sqlite_damage::change_with_sqlite(log_directory, [](sqlite3* db) { sqlite_damage::delete_meta_record(db); });
    auto n = *node_t::with(config_of(3), sqlite_storage::open_in(log_directory),
                           sqlite_storage::open_node_storage_in(own_directory), echo{}, 5);
    CHECK(n.waits_for_the_operator());
    CHECK_FALSE(n.get_sequence_paxos().votes());
    CHECK(n.get_sequence_paxos().get_error() == make_error_code(konsens::storage_errc::missing_meta_record));
}

// Plan execute-verify 3.1, and plan clean-stop section 6 question 2:
// the proof survives a close of the node storage, so each later start
// reads it and retires the node. Each field of the proof comes back.
TEST_CASE("the node storage keeps the proof of a fault")
{
    const auto directory = sqlite_storage::new_directory();
    const konsens::fault_proof proof{konsens::proof_kind::listed_result_differs, 7, {1, 2, 0, 3}, 11, 12};
    {
        auto own = sqlite_storage::open_node_storage_in(directory);
        CHECK(own.get_fault_proof().value() == std::optional<konsens::fault_proof>{});
        REQUIRE(own.write_the_proof(proof));
    }
    const auto own = sqlite_storage::open_node_storage_in(directory);
    CHECK(own.get_fault_proof().value() == std::optional{proof});
}

// Plan execute-verify 3.6: the start reads the whole log, so an entry that
// is missing in the middle of the log is a damage at the start, though the
// meta record and the last entry are whole. Since plan clean-stop 2.6 the
// damage retires the node.
TEST_CASE("the start scan finds an entry that is missing in the middle of the log")
{
    const auto log_directory = sqlite_storage::new_directory();
    const auto own_directory = sqlite_storage::new_directory();
    std::vector<node_t> nodes;
    for (node_id p = 1; p <= 2; ++p) {
        nodes.push_back(*node_t::with(config_of(p), sqlite_storage::open_new(), sqlite_storage::open_new_node_storage(),
                                      echo{}, 0));
    }
    nodes.push_back(*node_t::with(config_of(3), sqlite_storage::open_in(log_directory),
                                  sqlite_storage::open_node_storage_in(own_directory), echo{}, 0));
    const std::array<bool, 3> all{true, true, true};
    for (int i = 0; i < rounds; ++i) {
        step(nodes, all);
    }
    const std::vector<konsens::entry> entries{make_entry(1), make_entry(2), make_entry(3)};
    for (const auto& e : entries) {
        batches::commit(nodes[0], nodes[0].append(e));
        for (int i = 0; i < rounds; ++i) {
            step(nodes, all);
        }
    }
    REQUIRE(*nodes[2].read_decided_suffix(0) == entries);
    nodes[2].stop();
    end_without_stop(nodes[2]);
    sqlite_damage::change_with_sqlite(log_directory, [](sqlite3* db) { sqlite_damage::delete_entry(db, 1); });
    auto later = config_of(3);
    later.first_start = false;
    nodes[2] = *node_t::with(later, sqlite_storage::open_in(log_directory),
                             sqlite_storage::open_node_storage_in(own_directory), echo{}, 10);
    CHECK(nodes[2].retired());
    const auto e = nodes[2].get_sequence_paxos().get_error();
    REQUIRE(e.has_value());
    CHECK(konsens::error_is_a_damage(*e));
}
