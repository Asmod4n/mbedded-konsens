#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
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
