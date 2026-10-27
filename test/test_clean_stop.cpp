#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

#include "batches.hpp"
#include "konsens/client.hpp"
#include "konsens/node.hpp"
#include "konsens/sequence_paxos.hpp"
#include "node_cluster.hpp"
#include "sqlite_storage.hpp"

using namespace node_cluster;
using konsens::client_errc;

namespace {

bool decides(cluster& c, const std::size_t at, const std::uint8_t v)
{
    const auto before = value_of(c.nodes.at(at).get_sequence_paxos().get_decided_idx());
    batches::commit(c.nodes.at(at), c.nodes.at(at).append(make_entry(v)));
    c.run(rounds);
    return value_of(c.nodes.at(at).get_sequence_paxos().get_decided_idx()) > before;
}

}

// Plan clean-stop 4.2: a clean stop loses no sync, so the node keeps its
// identity. It starts as the fail-recovery of Omni-Paxos does, sends
// PrepareReq, and the cluster decides with it while the third node is
// retired.
TEST_CASE("a node that stops cleanly and starts again votes")
{
    auto c = elected();
    const auto l = c.leader();
    const auto f = first_follower(l);
    c.stop_and_start(f);
    CHECK(!c.nodes.at(f).retired());
    CHECK(c.nodes.at(f).get_sequence_paxos().get_state() == std::pair{konsens::role::follower, konsens::phase::recover});
    const auto out = c.nodes.at(f).take_outgoing_messages();
    CHECK(std::ranges::count_if(out, holds<konsens::prepare_req>) == 2);
    c.restart(second_follower(l));
    REQUIRE(c.nodes.at(second_follower(l)).retired());
    c.run(rounds);
    CHECK(decides(c, c.leader(), 1));
}

// Plan clean-stop 4.2 and review 1 item 13: a leader that stops and starts
// again never leads with its old ballot, because a follower may hold a
// promise of that ballot with other entries.
TEST_CASE("a leader that stops cleanly leads again only with a higher ballot")
{
    auto c = elected();
    const auto l = c.leader();
    const auto old = c.nodes.at(l).get_sequence_paxos().get_leader_state().n_leader;
    for (std::size_t i = 0; i < node_count; ++i) {
        if (i != l) {
            c.nodes.at(i).stop();
        }
    }
    c.stop_and_start(l);
    for (std::size_t i = 0; i < node_count; ++i) {
        if (i != l) {
            c.restart(i);
        }
    }
    c.run(rounds);
    const auto now = c.leader();
    CHECK(c.nodes.at(now).get_sequence_paxos().get_leader_state().n_leader > old);
    CHECK(decides(c, now, 2));
}

// Plan clean-stop 4.3 and 4.4: a start without the mark may follow a lost
// sync, so the node retires. It sends nothing, refuses each call, and the
// mark of an earlier clean stop does not help, because the start before
// deleted it.
TEST_CASE("a start without a clean stop retires the node, also at each later start")
{
    auto c = elected();
    const auto f = first_follower(c.leader());
    c.stop_and_start(f);
    c.run(rounds);
    REQUIRE(!c.nodes.at(f).retired());
    c.restart(f);
    CHECK(c.nodes.at(f).retired());
    c.run(rounds);
    CHECK(c.nodes.at(f).take_outgoing_messages().empty());
    const auto refused = [](const auto& r) { return !r && r.error() == client_errc::retired; };
    CHECK(refused(c.nodes.at(f).append(make_entry(3))));
    CHECK(refused(c.nodes.at(f).register_client(client_a)));
    CHECK(refused(c.nodes.at(f).client_request(client_a, 1, 1, {std::byte{1}})));
    CHECK(refused(c.nodes.at(f).client_query(client_a, {std::byte{1}})));
    CHECK(refused(c.nodes.at(f).expire_session(client_a, 1)));
    c.restart(f);
    CHECK(c.nodes.at(f).retired());
    CHECK(decides(c, c.leader(), 4));
}

// Plan clean-stop 4.5: the mark names the pid, the configuration and the
// whole meta record. A store restored from an older copy, or the node
// storage of another node, holds another state than the one that stopped.
TEST_CASE("a mark that does not agree with the store retires the node")
{
    auto c = elected();
    const auto l = c.leader();
    const auto f = first_follower(l);
    const auto older = sqlite_storage::open_copy_of(c.disks.at(f)->store);
    CHECK(decides(c, l, 5));
    c.nodes.at(f).stop();
    c.disks.at(f)->store = sqlite_storage::open_copy_of(older);
    c.restart(f);
    CHECK(c.nodes.at(f).retired());

    const auto s = second_follower(l);
    c.nodes.at(s).stop();
    std::swap(c.states.at(s), c.states.at(l));
    c.restart(s);
    CHECK(c.nodes.at(s).retired());
}

// Plan clean-stop 2.2 and 4.6: a stop with an open step would write a mark
// for a state that the node did not commit. A retired node has no state
// to keep. After the stop, the node takes no call. A call that waits for
// its decide gets stopped, because the node cannot answer it any more.
TEST_CASE("the wrong uses of stop, and a call that waits at the stop")
{
    auto c = elected();
    const auto l = c.leader();
    const auto f = first_follower(l);
    const auto open = c.nodes.at(f).append(make_entry(6));
    REQUIRE(open);
    CHECK_THROWS_AS(c.nodes.at(f).stop(), std::logic_error);
    REQUIRE(c.nodes.at(f).commit(*open));

    batches::commit(c.nodes.at(f), c.nodes.at(f).register_client(client_a));
    c.nodes.at(f).stop();
    const auto replies = c.nodes.at(f).take_client_replies();
    REQUIRE(replies.size() == 1);
    CHECK(replies[0].client_id == client_a);
    CHECK(replies[0].response.error() == client_errc::stopped);
    CHECK_THROWS_AS(static_cast<void>(c.nodes.at(f).tick()), std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(c.nodes.at(f).append(make_entry(7))), std::logic_error);
    CHECK_THROWS_AS(c.nodes.at(f).stop(), std::logic_error);

    c.restart(second_follower(l));
    REQUIRE(c.nodes.at(second_follower(l)).retired());
    CHECK_THROWS_AS(c.nodes.at(second_follower(l)).stop(), std::logic_error);
}

// Plan clean-stop 4.8, the question of the owner at 13:10: when every node
// stops cleanly, the cluster starts again without the operator, elects a
// leader and decides.
TEST_CASE("a whole cluster that stops cleanly starts again and decides")
{
    auto c = elected();
    CHECK(decides(c, c.leader(), 8));
    for (auto& n : c.nodes) {
        n.stop();
    }
    for (std::size_t i = 0; i < node_count; ++i) {
        c.restart(i);
        CHECK(!c.nodes.at(i).retired());
    }
    c.run(rounds);
    CHECK(decides(c, c.leader(), 9));
}
