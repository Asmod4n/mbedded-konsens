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

#include "konsens/memory_storage.hpp"
#include "konsens/node.hpp"
#include "echo.hpp"

namespace {

using konsens::memory_storage;
using konsens::node_id;
using node_t = konsens::node<memory_storage, echo>;

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
    return {1, pid, peers, 0, election_ticks, resend_ticks};
}

konsens::entry make_entry(std::uint8_t v) { return {konsens::entry_kind::normal, {std::byte{v}}}; }

void step(std::vector<node_t>& nodes, const std::array<bool, 3>& alive)
{
    for (bool more = true; more;) {
        more = false;
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            for (auto& m : nodes[i].take_outgoing_messages()) {
                more = true;
                const auto to = std::visit([](const auto& x) { return x.to; }, m);
                if (alive[i] && alive[to - 1]) {
                    REQUIRE(nodes[to - 1].handle(std::move(m)));
                }
            }
        }
    }
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        if (alive[i]) {
            REQUIRE(nodes[i].tick());
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
TEST_CASE("three nodes elect, decide and survive the loss of the leader") {
    std::vector<node_t> nodes;
    for (node_id p = 1; p <= 3; ++p) {
        nodes.push_back(*node_t::with(config_of(p), memory_storage{}, echo{}, 0));
    }
    const std::array<bool, 3> all{true, true, true};
    for (int i = 0; i < rounds; ++i) {
        step(nodes, all);
    }
    REQUIRE(nodes[0].append(make_entry(1)));
    for (int i = 0; i < rounds; ++i) {
        step(nodes, all);
    }
    for (const auto& n : nodes) {
        CHECK(*n.read_decided_suffix(0) == std::vector{make_entry(1)});
    }
    const auto leader = nodes[0].get_sequence_paxos().get_promise().pid;
    REQUIRE(leader == 3);
    std::array<bool, 3> without_leader = all;
    without_leader[leader - 1] = false;
    for (int i = 0; i < rounds; ++i) {
        step(nodes, without_leader);
    }
    REQUIRE(nodes[0].append(make_entry(2)));
    for (int i = 0; i < rounds; ++i) {
        step(nodes, without_leader);
    }
    CHECK(*nodes[0].read_decided_suffix(0) == std::vector{make_entry(1), make_entry(2)});
    CHECK(*nodes[1].read_decided_suffix(0) == std::vector{make_entry(1), make_entry(2)});
}

// Each tick timeout of 0 would never fire: a wrong use that fails loudly,
// as ServerConfig::validate in omni_paxos.rs:189-200.
TEST_CASE("a tick timeout of 0 is a wrong use") {
    auto c = config_of(1);
    c.election_tick_timeout = 0;
    CHECK_THROWS_AS(static_cast<void>(node_t::with(c, memory_storage{}, echo{}, 0)), std::logic_error);
}

// Each error value has a message of its own, because a log or a client
// reads the message and not the number.
TEST_CASE("each client error has its own message") {
    const std::array codes{konsens::client_errc::recovering, konsens::client_errc::session_expired,
                           konsens::client_errc::stale_request, konsens::client_errc::retry};
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

// F4 of plan v0.1 at the node: a recovering node drops every message
// except RecoveryResponse, BLE heartbeats included, and refuses each call
// with recovering. Simulation seed 134 shows why the heartbeats count: the
// leader 3 restarted, still answered heartbeats, so nodes 1 and 2 kept it
// as leader, no node was a primary, and recovery waited forever. When the
// restarted leader answers no heartbeat, nodes 1 and 2 elect a new leader,
// whose answer completes the recovery.
TEST_CASE("a restarted leader answers no heartbeat while it recovers") {
    std::vector<node_t> nodes;
    for (node_id p = 1; p <= 3; ++p) {
        nodes.push_back(*node_t::with(config_of(p), memory_storage{}, echo{}, 0));
    }
    const std::array<bool, 3> all{true, true, true};
    for (int i = 0; i < rounds; ++i) {
        step(nodes, all);
    }
    REQUIRE(nodes[0].append(make_entry(1)));
    for (int i = 0; i < rounds; ++i) {
        step(nodes, all);
    }
    REQUIRE(nodes[2].get_sequence_paxos().get_state() ==
            std::pair{konsens::role::leader, konsens::phase::accept});
    const memory_storage kept = nodes[2].get_sequence_paxos().get_internal_storage();
    nodes[2] = *node_t::with(config_of(3), kept, echo{}, 1);
    REQUIRE(nodes[2].get_sequence_paxos().recovering());
    static_cast<void>(nodes[2].take_outgoing_messages());
    REQUIRE(nodes[2].handle(konsens::ble_message{1, 3, konsens::heartbeat_request{1}}));
    CHECK(nodes[2].take_outgoing_messages().empty());
    const auto refused = [](const node_t::result& r) { return !r && r.error() == konsens::client_errc::recovering; };
    CHECK(refused(nodes[2].append(make_entry(2))));
    CHECK(refused(nodes[2].register_client(1)));
    CHECK(refused(nodes[2].client_request(1, 1, 1, {std::byte{1}})));
    CHECK(refused(nodes[2].client_query(1, {std::byte{1}})));
    CHECK(refused(nodes[2].expire_session(1, 1)));
    for (int i = 0; i < rounds; ++i) {
        step(nodes, all);
    }
    CHECK(!nodes[2].get_sequence_paxos().recovering());
    CHECK(nodes[0].get_sequence_paxos().get_promise().pid != 3);
    REQUIRE(nodes[2].append(make_entry(2)));
    for (int i = 0; i < rounds; ++i) {
        step(nodes, all);
    }
    for (const auto& n : nodes) {
        CHECK(*n.read_decided_suffix(0) == std::vector{make_entry(1), make_entry(2)});
    }
}

namespace {

constexpr node_id behind = 1;
constexpr node_id elected = 3;

template <typename Lost>
bool deliver_losing(std::vector<node_t>& nodes, const Lost lost)
{
    bool any = false;
    for (bool more = true; more;) {
        more = false;
        for (auto& n : nodes) {
            for (auto& m : n.take_outgoing_messages()) {
                more = true;
                const auto* p = std::get_if<konsens::paxos_message>(&m);
                if (p != nullptr && lost(*p)) {
                    any = true;
                    continue;
                }
                const auto to = std::visit([](const auto& x) { return x.to; }, m);
                REQUIRE(nodes[to - 1].handle(std::move(m)));
            }
        }
    }
    return any;
}

void deliver_all(std::vector<node_t>& nodes)
{
    static_cast<void>(deliver_losing(nodes, [](const konsens::paxos_message&) { return false; }));
}

std::vector<konsens::paxos_message> step_recording(std::vector<node_t>& nodes, const node_id watched)
{
    std::vector<konsens::paxos_message> seen;
    for (bool more = true; more;) {
        more = false;
        for (auto& n : nodes) {
            for (auto& m : n.take_outgoing_messages()) {
                more = true;
                if (const auto* p = std::get_if<konsens::paxos_message>(&m);
                    p != nullptr && (p->to == watched || p->from == watched)) {
                    seen.push_back(*p);
                }
                const auto to = std::visit([](const auto& x) { return x.to; }, m);
                REQUIRE(nodes[to - 1].handle(std::move(m)));
            }
        }
    }
    for (auto& n : nodes) {
        REQUIRE(n.tick());
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
        nodes.push_back(*node_t::with(config_of(p), memory_storage{}, echo{}, 0));
    }
    const std::array<bool, 3> all{true, true, true};
    for (int i = 0; i < rounds; ++i) {
        step(nodes, all);
    }
    REQUIRE(nodes[behind - 1].get_sequence_paxos().get_promise().pid == elected);
    return nodes;
}

std::vector<node_t> lose_the_last_decide()
{
    auto nodes = elect_three();
    deliver_all(nodes);
    REQUIRE(nodes[elected - 1].append(make_entry(1)));
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
    CHECK(f.get_internal_storage().get_entries(0, f.get_accepted_idx()) ==
          l.get_internal_storage().get_entries(0, l.get_accepted_idx()));
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
TEST_CASE("a follower that lost the last Decide catches up after reconnected") {
    auto nodes = lose_the_last_decide();
    nodes[behind - 1].reconnected(elected);
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
TEST_CASE("a follower that lost the last Decide catches up after a resend timeout") {
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
TEST_CASE("a follower that lost an AcceptDecide with an entry catches up after a resend timeout") {
    auto nodes = elect_three();
    REQUIRE(nodes[elected - 1].append(make_entry(1)));
    deliver_all(nodes);
    REQUIRE(nodes[behind - 1].get_sequence_paxos().get_decided_idx() == 1);
    REQUIRE(nodes[elected - 1].append(make_entry(2)));
    const bool lost = deliver_losing(nodes, [](const konsens::paxos_message& p) {
        return p.to == behind &&
               (std::holds_alternative<konsens::accept_decide>(p.msg) || std::holds_alternative<konsens::decide>(p.msg));
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
    memory_storage inner;
    std::vector<const konsens::entry*>* appended;

    std::expected<void, std::error_code> write_atomically(std::span<const konsens::storage_op> ops)
    {
        for (const auto& op : ops) {
            if (const auto* a = std::get_if<konsens::append_entries>(&op)) {
                appended->push_back(a->entries.data());
            }
        }
        return inner.write_atomically(ops);
    }
    auto get_entries(std::uint64_t from, std::uint64_t to) const { return inner.get_entries(from, to); }
    auto get_log_len() const { return inner.get_log_len(); }
    auto get_promise() const { return inner.get_promise(); }
    auto get_decided_idx() const { return inner.get_decided_idx(); }
    auto get_accepted_round() const { return inner.get_accepted_round(); }
};

}

// The rule "Nothing is copied to pass it": the entries of a decoded
// AcceptDecide reach the storage of the follower where cbor::ref holds
// them. The test compares the address of the first entry, not an address
// in the wire bytes, because decode copies each payload into its entry.
// The follower must be in (Follower, Accept) with the next sequence
// number, or the handler writes nothing.
TEST_CASE("a decoded AcceptDecide reaches the storage with no copy") {
    using recording_node = konsens::node<recording_storage, echo>;
    std::array<std::vector<const konsens::entry*>, 3> appended;
    std::vector<recording_node> nodes;
    for (node_id p = 1; p <= 3; ++p) {
        nodes.push_back(*recording_node::with(config_of(p), recording_storage{{}, &appended.at(p - 1)}, echo{}, 0));
    }
    const auto deliver = [&nodes] {
        for (bool more = true; more;) {
            more = false;
            for (auto& n : nodes) {
                for (const auto& m : n.take_outgoing_messages()) {
                    more = true;
                    const auto to = std::visit([](const auto& x) { return x.to; }, m);
                    REQUIRE(nodes.at(to - 1).handle(m));
                }
            }
        }
    };
    for (int i = 0; i < rounds; ++i) {
        deliver();
        for (auto& n : nodes) {
            REQUIRE(n.tick());
        }
    }
    deliver();
    const node_id leader = nodes.front().get_sequence_paxos().get_promise().pid;
    REQUIRE(leader != 0);
    const node_id follower = leader == 1 ? 2 : 1;
    REQUIRE(nodes.at(leader - 1).append(make_entry(7)));
    bool checked = false;
    for (const auto& m : nodes.at(leader - 1).take_outgoing_messages()) {
        const auto to = std::visit([](const auto& x) { return x.to; }, m);
        if (to != follower) {
            continue;
        }
        auto bytes = konsens::encode(m);
        REQUIRE(bytes);
        const auto r = konsens::decode(std::move(*bytes));
        REQUIRE(r);
        const auto* acc = std::get_if<cbor::ref<konsens::accept_decide>>(&r->msg);
        if (acc == nullptr) {
            continue;
        }
        auto& seen = appended.at(follower - 1);
        seen.clear();
        REQUIRE(nodes.at(follower - 1).handle(*r));
        REQUIRE(seen.size() == 1);
        CHECK(seen.front() == (*acc)->entries.data());
        checked = true;
    }
    CHECK(checked);
}
