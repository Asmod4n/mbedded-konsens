#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "konsens/ballot_leader_election.hpp"

namespace {

using konsens::ballot;
using konsens::ballot_leader_election;
using konsens::node_id;

constexpr std::uint32_t config_id = 1;

std::vector<node_id> peers_of(node_id pid)
{
    std::vector<node_id> peers;
    for (node_id p = 1; p <= 3; ++p) {
        if (p != pid) {
            peers.push_back(p);
        }
    }
    return peers;
}

ballot_leader_election make(node_id pid, std::uint32_t priority, ballot recovered = {})
{
    return ballot_leader_election::with({config_id, pid, peers_of(pid), priority}, recovered);
}

using cluster = std::array<ballot_leader_election, 3>;

std::array<std::optional<ballot>, 3> run_round(cluster& c, const std::array<bool, 3>& alive)
{
    for (bool more = true; more;) {
        more = false;
        for (std::size_t i = 0; i < c.size(); ++i) {
            for (const auto& m : c[i].take_outgoing_messages()) {
                more = true;
                if (alive[i] && alive[m.to - 1]) {
                    c[m.to - 1].handle(m);
                }
            }
        }
    }
    std::array<std::optional<ballot>, 3> r{};
    for (std::size_t i = 0; i < c.size(); ++i) {
        if (alive[i]) {
            r[i] = c[i].hb_timeout(ballot{});
        }
    }
    return r;
}

}

// ballot_leader_election.rs:222-229 update_leader: after one round with all
// replies, every node takes the highest ballot as leader. The ballot with
// n = 1 orders by priority first, so the node with the highest priority is
// elected, whatever its pid.
TEST_CASE("the highest priority is elected") {
    cluster c{make(1, 5), make(2, 0), make(3, 0)};
    const auto r = run_round(c, {true, true, true});
    const ballot expected{config_id, 1, 5, 1};
    CHECK(r[0] == expected);
    CHECK(!r[1]);
    CHECK(!r[2]);
    for (const auto& b : c) {
        CHECK(b.get_leader() == expected);
    }
}

// Equal priorities: the pid decides the order of Ballot (n, priority, pid).
TEST_CASE("with equal priority the highest pid is elected") {
    cluster c{make(1, 0), make(2, 0), make(3, 0)};
    const auto r = run_round(c, {true, true, true});
    CHECK(r[2] == ballot{config_id, 1, 0, 3});
}

// ballot_leader_election.rs:260-274 check_takeover: when the leader is
// silent, the other nodes become unhappy, and a node that hears a quorum
// of unhappy nodes takes over with n of the leader plus one. This is what
// the owner sees when the leader stops. Both followers can take over in
// the same round; one more round lets both agree on the higher ballot.
TEST_CASE("a silent leader is replaced with a higher n") {
    cluster c{make(1, 5), make(2, 0), make(3, 0)};
    run_round(c, {true, true, true});
    std::optional<ballot> elected;
    for (int i = 0; i < 4 && !elected; ++i) {
        const auto r = run_round(c, {false, true, true});
        elected = r[2] ? r[2] : r[1];
    }
    REQUIRE(elected);
    CHECK(elected->n == 2);
    run_round(c, {false, true, true});
    CHECK(c[1].get_leader() == c[2].get_leader());
    CHECK(c[1].get_leader().n == 2);
}

// ballot_leader_election.rs:103-117 `with`: a node that restarts with a
// promise takes that ballot as leader and uses n = 0 for its own ballot, so
// that it does not keep the leadership of the ballot it had.
TEST_CASE("a restarted node does not keep its old ballot") {
    const ballot old{config_id, 1, 5, 1};
    const auto b = make(1, 5, old);
    CHECK(b.get_leader() == old);
    CHECK(b.get_current_ballot().n == ballot_leader_election::recovery_round);
}

// ballot_leader_election.rs:206-214: a promise of Sequence Paxos above the
// leader of BLE is taken as leader, so that BLE does not elect a ballot
// that Sequence Paxos has passed.
TEST_CASE("hb_timeout follows a higher paxos promise") {
    auto b = make(1, 0);
    const ballot higher{config_id, 7, 0, 2};
    CHECK(!b.hb_timeout(higher));
    CHECK(b.get_leader() == higher);
}
