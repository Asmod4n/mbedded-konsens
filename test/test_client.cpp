#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <ranges>
#include <span>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

#include "batches.hpp"
#include "konsens/client.hpp"
#include "konsens/node.hpp"
#include "lmdb_storage.hpp"

namespace {

using konsens::client_errc;
using konsens::entry_kind;
using konsens::message;
using konsens::node_id;
using konsens::paxos_message;
using konsens::persistent_storage;
using konsens::phase;
using konsens::role;

// The echo machine of the other tests holds no state, so a read cannot
// show which requests it saw. This machine keeps the concatenation of the
// applied payloads, and a query gives that value.
struct concatenation {
    std::vector<std::byte> value;

    std::vector<std::byte> apply(const std::span<const std::byte> payload)
    {
        value.insert(value.end(), payload.begin(), payload.end());
        return value;
    }

    std::vector<std::byte> query([[maybe_unused]] const std::span<const std::byte> payload) const { return value; }
};

static_assert(konsens::state_machine<concatenation>);

using node_t = konsens::node<persistent_storage, concatenation>;

constexpr std::size_t node_count = 3;
constexpr std::uint64_t election_ticks = 3;
constexpr std::uint64_t resend_ticks = 2;
constexpr int rounds = 40;
constexpr std::uint64_t client_a = 10;
constexpr std::uint64_t client_b = 20;

konsens::node_config config_of(const node_id pid)
{
    std::vector<node_id> peers;
    for (node_id p = 1; p <= node_count; ++p) {
        if (p != pid) {
            peers.push_back(p);
        }
    }
    return {1, pid, peers, 0, election_ticks, resend_ticks};
}

std::vector<std::byte> bytes_of(const std::string_view s)
{
    const auto b = std::as_bytes(std::span(s));
    return {b.begin(), b.end()};
}

using batches::from_of;
using batches::to_of;

template <typename T>
bool holds(const message& m)
{
    const auto* p = std::get_if<paxos_message>(&m);
    return p != nullptr && std::holds_alternative<T>(p->msg);
}

// An error check reads the error only when there is one, so that a result
// with a value fails the check and is not undefined behaviour.
template <typename T>
bool failed_with(const std::expected<T, std::error_code>& r, const konsens::client_errc e)
{
    return !r.has_value() && r.error() == e;
}

std::size_t once([[maybe_unused]] const message& m)
{
    return 1;
}

// Three nodes on FIFO channels. deliver() hands each message to its
// receiver as often as `copies` says: 0 is a loss, 2 a duplicate. A
// stopped node sends and receives nothing.
struct cluster {
    std::vector<node_t> nodes;
    std::array<bool, node_count> alive{true, true, true};

    node_t& at(const node_id p) { return nodes[p - 1]; }

    template <typename Copies>
    void deliver(const Copies copies)
    {
        batches::deliver(nodes, [this, &copies](const message& m) -> std::size_t {
            return alive.at(from_of(m) - 1) && alive.at(to_of(m) - 1) ? copies(m) : 0;
        });
    }

    void tick()
    {
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            if (alive[i]) {
                batches::tick_and_commit(nodes[i]);
            }
        }
    }

    template <typename Copies>
    void run(const int count, const Copies copies)
    {
        for (int i = 0; i < count; ++i) {
            deliver(copies);
            tick();
        }
    }
};

cluster elected()
{
    cluster c;
    for (node_id p = 1; p <= node_count; ++p) {
        c.nodes.push_back(*node_t::with(config_of(p), lmdb_storage::open_new(), concatenation{}, 0));
    }
    c.run(rounds, once);
    REQUIRE(c.at(3).get_sequence_paxos().get_state() == std::pair{role::leader, phase::accept});
    return c;
}

std::uint64_t registered(cluster& c, const node_id at, const std::uint64_t client_id)
{
    batches::commit(c.at(at), c.at(at).register_client(client_id));
    c.deliver(once);
    const auto replies = c.at(at).take_client_replies();
    REQUIRE(replies.size() == 1);
    REQUIRE(replies[0].client_id == client_id);
    REQUIRE(replies[0].response);
    return replies[0].session;
}

}

// F11: a start applies the decided log from index 0, so a new machine
// reaches the state of the decided prefix and the client table comes back
// with it. A start answers no call, because no caller waits on a new node.
TEST_CASE("a start applies the decided log from index 0")
{
    auto c = elected();
    const auto s = registered(c, 3, client_a);
    batches::commit(c.at(3), c.at(3).client_request(client_a, s, 1, bytes_of("a")));
    c.deliver(once);
    auto kept = lmdb_storage::open_copy_of(c.at(1).get_sequence_paxos().get_internal_storage());
    REQUIRE(c.at(1).get_applied_idx() == 2);
    auto n = *node_t::with(config_of(1), std::move(kept), concatenation{}, 0);
    CHECK(n.get_applied_idx() == 2);
    CHECK(n.get_state_machine().value == bytes_of("a"));
    REQUIRE(n.get_client_table().contains(s));
    CHECK(n.get_client_table().at(s).request_number == 1);
    CHECK(n.get_client_table().at(s).reply == bytes_of("a"));
    CHECK(n.take_client_replies().empty());
}

// F11: the library keeps the client table, and only the payload of a
// ClientRequest reaches the state machine. A normal entry and a
// registration change no state of the user.
TEST_CASE("only the payload of a client request reaches the state machine")
{
    auto c = elected();
    batches::commit(c.at(3), c.at(3).append({entry_kind::normal, bytes_of("n")}));
    const auto s = registered(c, 3, client_a);
    batches::commit(c.at(3), c.at(3).client_request(client_a, s, 1, bytes_of("a")));
    c.deliver(once);
    for (const auto& n : c.nodes) {
        CHECK(n.get_applied_idx() == 3);
        CHECK(n.get_state_machine().value == bytes_of("a"));
    }
}

// F10: the session id is the log index of the RegisterClient entry, and
// that entry carries the client id and the pid of the node that took the
// call. Two clients get two sessions. A client that calls at two nodes
// gets from each node the session of the entry of that node, so each
// answer belongs to the call that was made there.
TEST_CASE("each registration gives the session of its own entry")
{
    auto c = elected();
    batches::commit(c.at(1), c.at(1).register_client(client_a));
    batches::commit(c.at(2), c.at(2).register_client(client_a));
    batches::commit(c.at(2), c.at(2).register_client(client_b));
    c.deliver(once);
    const auto log = *c.at(3).read_decided_suffix(0);
    REQUIRE(log.size() == 3);
    for (const node_id p : std::array<node_id, 2>{1, 2}) {
        for (const auto& r : c.at(p).take_client_replies()) {
            REQUIRE(r.session >= 1);
            REQUIRE(r.session <= log.size());
            const auto& e = log[r.session - 1];
            CHECK(e.kind == entry_kind::register_client);
            CHECK(e.client_id == r.client_id);
            CHECK(e.pid == p);
            CHECK(r.response);
        }
    }
    CHECK(c.at(3).take_client_replies().empty());
}

// E1 and VR Revisited 4.1, with two clients, a loss, a resend and a
// duplicate. A follower forwards each call to the leader. A request that
// is decided twice changes the state once; the same request number gives
// the kept reply, and a lower one gives stale_request. The failure of
// earlier work: a test without loss, duplicates and resends does not fail
// when a session guard is removed.
TEST_CASE("two clients: a loss, a resend and a duplicate execute each request once")
{
    auto c = elected();
    const auto sa = registered(c, 1, client_a);
    const auto sb = registered(c, 2, client_b);
    REQUIRE(sa != sb);

    batches::commit(c.at(1), c.at(1).client_request(client_a, sa, 1, bytes_of("a")));
    c.deliver([](const message& m) -> std::size_t { return holds<konsens::proposal_forward>(m) ? 0 : 1; });
    c.run(rounds, once);
    CHECK(c.at(1).take_client_replies().empty());
    batches::commit(c.at(1), c.at(1).client_request(client_a, sa, 1, bytes_of("a")));
    c.deliver(once);
    auto replies = c.at(1).take_client_replies();
    REQUIRE(replies.size() == 1);
    CHECK(replies[0].client_id == client_a);
    CHECK(replies[0].session == sa);
    CHECK(replies[0].response == bytes_of("a"));

    batches::commit(c.at(2), c.at(2).client_request(client_b, sb, 1, bytes_of("b")));
    c.deliver([](const message& m) -> std::size_t { return holds<konsens::proposal_forward>(m) ? 2 : 1; });
    replies = c.at(2).take_client_replies();
    REQUIRE(replies.size() == 1);
    CHECK(replies[0].client_id == client_b);
    CHECK(replies[0].response == bytes_of("ab"));

    batches::commit(c.at(3), c.at(3).client_request(client_a, sa, 1, bytes_of("a")));
    c.deliver(once);
    replies = c.at(3).take_client_replies();
    REQUIRE(replies.size() == 1);
    CHECK(replies[0].response == bytes_of("a"));

    batches::commit(c.at(1), c.at(1).client_request(client_a, sa, 2, bytes_of("c")));
    batches::commit(c.at(1), c.at(1).client_request(client_a, sa, 1, bytes_of("a")));
    c.deliver(once);
    replies = c.at(1).take_client_replies();
    REQUIRE(replies.size() == 2);
    CHECK(replies[0].response == bytes_of("abc"));
    CHECK(failed_with(replies[1].response, client_errc::stale_request));

    for (const auto& n : c.nodes) {
        CHECK(n.get_state_machine().value == bytes_of("abc"));
    }
}

// F14 and the model action ExpireSession: only the leader in Accept
// appends ExpireSession, only for an open session, and only once. A
// request of an expired session is not executed (Raft 6.3,
// SESSION_EXPIRED).
TEST_CASE("expire_session closes a session at the leader once")
{
    auto c = elected();
    const auto s = registered(c, 2, client_b);
    CHECK(failed_with(c.at(1).expire_session(client_b, s), client_errc::retry));
    c.deliver(once);
    CHECK(c.at(3).get_sequence_paxos().get_accepted_idx() == 1);

    batches::commit(c.at(3), c.at(3).expire_session(client_b, s));
    batches::commit(c.at(3), c.at(3).expire_session(client_b, s));
    c.deliver(once);
    const auto log = *c.at(3).read_decided_suffix(0);
    CHECK(std::ranges::count(log, entry_kind::expire_session, &konsens::entry::kind) == 1);
    auto replies = c.at(3).take_client_replies();
    REQUIRE(replies.size() == 1);
    CHECK(replies[0].client_id == client_b);
    CHECK(replies[0].session == s);
    CHECK(replies[0].response);

    CHECK(failed_with(c.at(3).expire_session(client_b, s), client_errc::session_expired));
    batches::commit(c.at(2), c.at(2).client_request(client_b, s, 1, bytes_of("b")));
    c.deliver(once);
    replies = c.at(2).take_client_replies();
    REQUIRE(replies.size() == 1);
    CHECK(failed_with(replies[0].response, client_errc::session_expired));
    for (const auto& n : c.nodes) {
        CHECK(n.get_state_machine().value.empty());
    }
}

// R1 across a change of leader (finding 1 of the review of plan v0.1).
// Leader 3 decides and answers request "a", and its Decide to the others
// is lost. Node 2 becomes leader with "a" accepted but not decided. Node 1
// misses the first AcceptSync of node 2, so node 2 cannot decide "a" yet.
// A read at node 1 must still see "a": the read index is the accepted_idx
// of node 2, and node 1 answers only when it has applied up to that index.
TEST_CASE("a read after a change of leader sees a request that the old leader decided")
{
    auto c = elected();
    const auto s = registered(c, 3, client_a);
    batches::commit(c.at(3), c.at(3).client_request(client_a, s, 1, bytes_of("a")));
    c.deliver([](const message& m) -> std::size_t { return holds<konsens::decide>(m) ? 0 : 1; });
    const auto answered = c.at(3).take_client_replies();
    REQUIRE(answered.size() == 1);
    REQUIRE(answered[0].response == bytes_of("a"));
    REQUIRE(c.at(1).get_applied_idx() == 1);
    REQUIRE(c.at(2).get_sequence_paxos().get_accepted_idx() == 2);

    c.alive[2] = false;
    const auto no_accept_sync = [](const message& m) -> std::size_t {
        return holds<konsens::accept_sync>(m) && to_of(m) == 1 ? 0 : 1;
    };
    for (int i = 0; i < rounds && c.at(2).get_sequence_paxos().get_state().first != role::leader; ++i) {
        c.run(1, no_accept_sync);
    }
    c.deliver(no_accept_sync);
    REQUIRE(c.at(2).get_sequence_paxos().get_state() == std::pair{role::leader, phase::accept});
    REQUIRE(c.at(1).get_sequence_paxos().get_promise() == c.at(2).get_sequence_paxos().get_promise());
    REQUIRE(c.at(1).get_applied_idx() == 1);

    batches::commit(c.at(1), c.at(1).client_query(client_b, {}));
    c.deliver(no_accept_sync);
    CHECK(c.at(1).get_applied_idx() == 1);
    CHECK(c.at(1).take_client_replies().empty());

    c.run(rounds, once);
    const auto replies = c.at(1).take_client_replies();
    REQUIRE(replies.size() == 1);
    CHECK(replies[0].client_id == client_b);
    CHECK(replies[0].response == bytes_of("a"));
}

// ReplyOnlyIfUnchanged of the model: node 1 has the read index from leader
// 3 and has not applied up to it. Then node 1 promises a new leader. The
// read is dropped with retry, because the confirmation of leader 3 no
// longer says that no higher ballot decided an entry.
TEST_CASE("a read is dropped with retry when the promise changes before the answer")
{
    auto c = elected();
    const auto s = registered(c, 3, client_a);
    const auto lost_to_1 = [](const message& m) -> std::size_t {
        return (holds<konsens::accept_decide>(m) || holds<konsens::decide>(m)) && to_of(m) == 1 ? 0 : 1;
    };
    batches::commit(c.at(3), c.at(3).client_request(client_a, s, 1, bytes_of("a")));
    c.deliver(lost_to_1);
    REQUIRE(c.at(1).get_applied_idx() == 1);

    batches::commit(c.at(1), c.at(1).client_query(client_b, {}));
    c.deliver(lost_to_1);
    CHECK(c.at(1).take_client_replies().empty());

    c.alive[2] = false;
    c.run(rounds, once);
    REQUIRE(c.at(1).get_sequence_paxos().get_promise().pid == 2);
    REQUIRE(c.at(1).get_applied_idx() == 2);
    const auto replies = c.at(1).take_client_replies();
    REQUIRE(replies.size() == 1);
    CHECK(replies[0].client_id == client_b);
    CHECK(failed_with(replies[0].response, client_errc::retry));
}

// A lost ReadIndex request would leave the read open for good. The resend
// timeout of the node sends each read without an index again to the leader
// of its promise.
TEST_CASE("a follower sends a lost read index request again")
{
    auto c = elected();
    const auto s = registered(c, 3, client_a);
    batches::commit(c.at(3), c.at(3).client_request(client_a, s, 1, bytes_of("a")));
    c.deliver(once);
    batches::commit(c.at(1), c.at(1).client_query(client_b, {}));
    c.deliver([](const message& m) -> std::size_t { return holds<konsens::read_index_request>(m) ? 0 : 1; });
    CHECK(c.at(1).take_client_replies().empty());
    c.run(rounds, once);
    const auto replies = c.at(1).take_client_replies();
    REQUIRE(replies.size() == 1);
    CHECK(replies[0].response == bytes_of("a"));
}

// A read at the leader goes through the same round: the leader keeps its
// own reads and answers them from its state when the round has a majority.
TEST_CASE("a read at the leader is answered after one round")
{
    auto c = elected();
    const auto s = registered(c, 2, client_a);
    batches::commit(c.at(2), c.at(2).client_request(client_a, s, 1, bytes_of("a")));
    c.deliver(once);
    batches::commit(c.at(3), c.at(3).client_query(client_b, {}));
    CHECK(c.at(3).take_client_replies().empty());
    c.deliver(once);
    const auto replies = c.at(3).take_client_replies();
    REQUIRE(replies.size() == 1);
    CHECK(replies[0].client_id == client_b);
    CHECK(replies[0].session == 0);
    CHECK(replies[0].response == bytes_of("a"));
}

// A lost Heartbeat would stop the round and every later read. The resend
// timeout of the leader sends the heartbeat of the round again.
TEST_CASE("a leader sends a lost heartbeat again")
{
    auto c = elected();
    batches::commit(c.at(3), c.at(3).client_query(client_b, {}));
    c.deliver([](const message& m) -> std::size_t { return holds<konsens::heartbeat>(m) ? 0 : 1; });
    CHECK(c.at(3).take_client_replies().empty());
    c.run(rounds, once);
    const auto replies = c.at(3).take_client_replies();
    REQUIRE(replies.size() == 1);
    CHECK(replies[0].response == std::vector<std::byte>{});
}
