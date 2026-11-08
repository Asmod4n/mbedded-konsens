#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
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
#include "sqlite_damage.hpp"
#include "sqlite_storage.hpp"

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
    static constexpr std::array key{std::byte{'v'}};

    static std::vector<std::byte> value_in(const konsens::state_view auto& view)
    {
        const auto found = view.find(key);
        if (!found) {
            return {};
        }
        return {found->begin(), found->end()};
    }

    konsens::applied apply(const std::span<const std::byte> payload, const konsens::state_view auto& view) const
    {
        auto value = value_in(view);
        value.insert(value.end(), payload.begin(), payload.end());
        return {value, {{{key.begin(), key.end()}, value}}};
    }

    std::vector<std::byte> query(std::span<const std::byte>, const konsens::state_view auto& view) const
    {
        return value_in(view);
    }
};

static_assert(konsens::state_machine<concatenation>);

using node_t = konsens::node<persistent_storage, concatenation>;

std::vector<std::byte> concatenation_of(const node_t& n)
{
    const auto t = n.get_sequence_paxos().get_internal_storage().begin_read_only();
    REQUIRE(t.has_value());
    return concatenation::value_in(konsens::read_view{*t});
}

constexpr std::size_t node_count = 3;
constexpr std::uint64_t election_ticks = 3;
constexpr std::uint64_t resend_ticks = 2;
constexpr int rounds = 40;
constexpr std::uint64_t client_a = 10;
constexpr std::uint64_t client_b = 20;
// Each request is one done record in the log (plan-one-environment 3.3): a
// registration at index 1 and the next request at index 2.
constexpr std::uint64_t registration_done = 1;
constexpr std::uint64_t request_done = 2;

konsens::node_config config_of(const node_id pid)
{
    std::vector<node_id> peers;
    for (node_id p = 1; p <= node_count; ++p) {
        if (p != pid) {
            peers.push_back(p);
        }
    }
    return {1, pid, peers, 0, election_ticks, resend_ticks, {}, true};
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
        c.nodes.push_back(*node_t::with(config_of(p), sqlite_storage::open_new(),
                                        sqlite_storage::open_new_node_storage(), concatenation{}, 0));
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
    auto kept = sqlite_storage::open_copy_of(c.at(1).get_sequence_paxos().get_internal_storage());
    REQUIRE(c.at(1).get_applied_idx() == request_done);
    auto own = sqlite_storage::open_node_storage_stopped_cleanly(kept, 1);
    auto n = *node_t::with(config_of(1), std::move(kept), std::move(own), concatenation{}, 0);
    CHECK(n.get_applied_idx() == request_done);
    CHECK(concatenation_of(n) == bytes_of("a"));
    CHECK(n.find_client_session(s) == konsens::session_record{1, bytes_of("a")});
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
    constexpr std::uint64_t normal_and_two_requests = 3;
    for (const auto& n : c.nodes) {
        CHECK(n.get_applied_idx() == normal_and_two_requests);
        CHECK(concatenation_of(n) == bytes_of("a"));
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
    REQUIRE(std::ranges::count(log, entry_kind::register_client, &konsens::entry::kind) == 3);
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
        CHECK(concatenation_of(n) == bytes_of("abc"));
    }
}

// F14 and the model action ExpireSession: the leader in Accept appends
// ExpireSession only for an open session, and only once. A request of an
// expired session is not executed (Raft 6.3, SESSION_EXPIRED).
TEST_CASE("expire_session closes a session at the leader once")
{
    auto c = elected();
    const auto s = registered(c, 2, client_b);

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
        CHECK(concatenation_of(n).empty());
    }
}

// A node program expires the session of a client connection at the node
// that took the connection. A follower forwards the expiry to the leader as
// it forwards a request, and answers when the expiry is decided.
TEST_CASE("expire_session at a follower closes the session through the leader")
{
    auto c = elected();
    const auto s = registered(c, 2, client_b);
    batches::commit(c.at(1), c.at(1).expire_session(client_b, s));
    c.deliver(once);
    const auto log = *c.at(3).read_decided_suffix(0);
    CHECK(std::ranges::count(log, entry_kind::expire_session, &konsens::entry::kind) == 1);
    const auto replies = c.at(1).take_client_replies();
    REQUIRE(replies.size() == 1);
    CHECK(replies[0].client_id == client_b);
    CHECK(replies[0].session == s);
    CHECK(replies[0].response);
    CHECK(c.at(3).take_client_replies().empty());
    CHECK(failed_with(c.at(1).expire_session(client_b, s), client_errc::session_expired));
}

// R1 across a change of leader (finding 1 of the review of plan v0.1).
// Leader 3 decides and answers request "a", and its Decide of the done
// record to the others is lost. Node 2 becomes leader with the done record
// accepted but not decided. Node 1 misses the first AcceptSync of node 2,
// so node 2 cannot decide the done record yet. A read at node 1 must still see
// "a": the read index is the accepted_idx of node 2, and node 1 answers
// only when it has applied up to that index.
TEST_CASE("a read after a change of leader sees a request that the old leader decided")
{
    auto c = elected();
    const auto s = registered(c, 3, client_a);
    batches::commit(c.at(3), c.at(3).client_request(client_a, s, 1, bytes_of("a")));
    c.deliver([](const message& m) -> std::size_t {
        return holds<konsens::decide>(m) &&
                       std::get<konsens::decide>(std::get<paxos_message>(m).msg).decided_idx >= request_done
                   ? 0
                   : 1;
    });
    const auto answered = c.at(3).take_client_replies();
    REQUIRE(answered.size() == 1);
    REQUIRE(answered[0].response == bytes_of("a"));
    REQUIRE(c.at(1).get_applied_idx() == registration_done);
    REQUIRE(c.at(2).get_sequence_paxos().get_accepted_idx() == request_done);

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
    REQUIRE(c.at(1).get_applied_idx() == registration_done);

    batches::commit(c.at(1), c.at(1).client_query(client_b, {}));
    c.deliver(no_accept_sync);
    CHECK(c.at(1).get_applied_idx() == registration_done);
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
    REQUIRE(c.at(1).get_applied_idx() == registration_done);

    batches::commit(c.at(1), c.at(1).client_query(client_b, {}));
    c.deliver(lost_to_1);
    CHECK(c.at(1).take_client_replies().empty());

    c.alive[2] = false;
    c.run(rounds, once);
    REQUIRE(c.at(1).get_sequence_paxos().get_promise()->pid == 2);
    REQUIRE(c.at(1).get_applied_idx() == request_done);
    const auto replies = c.at(1).take_client_replies();
    REQUIRE(replies.size() == 1);
    CHECK(replies[0].client_id == client_b);
    CHECK(failed_with(replies[0].response, client_errc::retry));
}

// The leader 3 takes a request and then loses its majority before any
// other node has the request. The new leader never learns of it, so the
// request is never decided. Without an answer the client waits for good.
// The session table makes a second send of the same request number safe,
// so the node answers retry when its promise changes.
TEST_CASE("a write is answered with retry when its leader loses the promise before the decision")
{
    auto c = elected();
    const auto s = registered(c, 3, client_a);
    const auto cut_off_3 = [](const message& m) -> std::size_t { return from_of(m) == 3 || to_of(m) == 3 ? 0 : 1; };
    batches::commit(c.at(3), c.at(3).client_request(client_a, s, 1, bytes_of("a")));
    c.run(rounds, cut_off_3);
    REQUIRE(c.at(1).get_sequence_paxos().get_promise()->pid != 3);
    CHECK(c.at(3).take_client_replies().empty());

    c.run(rounds, once);
    REQUIRE(c.at(3).get_sequence_paxos().get_promise()->pid != 3);
    const auto replies = c.at(3).take_client_replies();
    REQUIRE(replies.size() == 1);
    CHECK(replies[0].client_id == client_a);
    CHECK(replies[0].session == s);
    CHECK(failed_with(replies[0].response, client_errc::retry));
}

// The case where retry must be safe: a majority accepted the entry, but
// the old leader never decided it. The new leader decides it, so the write
// is applied after the client got retry. The retry answer erases the wait,
// so no second answer comes, and a second send of the same request number
// gets the kept reply of the session table and applies nothing again.
TEST_CASE("a write that gets retry and is then decided is applied once")
{
    auto c = elected();
    const auto s = registered(c, 3, client_a);
    const auto no_accepted_to_3 = [](const message& m) -> std::size_t {
        return holds<konsens::accepted>(m) && to_of(m) == 3 ? 0 : 1;
    };
    const auto cut_off_3 = [](const message& m) -> std::size_t { return from_of(m) == 3 || to_of(m) == 3 ? 0 : 1; };
    batches::commit(c.at(3), c.at(3).client_request(client_a, s, 1, bytes_of("a")));
    c.run(rounds, no_accepted_to_3);
    REQUIRE(c.at(3).get_applied_idx() == registration_done);
    REQUIRE(c.at(3).take_client_replies().empty());

    c.run(rounds, cut_off_3);
    REQUIRE(c.at(1).get_sequence_paxos().get_promise()->pid != 3);
    REQUIRE(concatenation_of(c.at(1)) == bytes_of("a"));

    c.run(rounds, once);
    REQUIRE(c.at(3).get_applied_idx() >= request_done);
    const auto replies = c.at(3).take_client_replies();
    REQUIRE(replies.size() == 1);
    CHECK(failed_with(replies[0].response, client_errc::retry));
    CHECK(concatenation_of(c.at(3)) == bytes_of("a"));

    batches::commit(c.at(3), c.at(3).client_request(client_a, s, 1, bytes_of("a")));
    c.run(rounds, once);
    const auto again = c.at(3).take_client_replies();
    REQUIRE(again.size() == 1);
    CHECK(again[0].response == bytes_of("a"));
    for (node_id p = 1; p <= node_count; ++p) {
        CHECK(concatenation_of(c.at(p)) == bytes_of("a"));
    }
}

// A follower sends a request to the leader of its promise. When that
// leader goes before the request reaches a majority, the follower is the
// only node that can still answer the client.
TEST_CASE("a write sent through a follower is answered with retry when the promise changes")
{
    auto c = elected();
    const auto s = registered(c, 1, client_a);
    const auto lost_to_3 = [](const message& m) -> std::size_t { return to_of(m) == 3 ? 0 : 1; };
    batches::commit(c.at(1), c.at(1).client_request(client_a, s, 1, bytes_of("a")));
    c.deliver(lost_to_3);
    c.alive[2] = false;
    c.run(rounds, once);
    REQUIRE(c.at(1).get_sequence_paxos().get_promise()->pid != 3);
    const auto replies = c.at(1).take_client_replies();
    REQUIRE(replies.size() == 1);
    CHECK(replies[0].client_id == client_a);
    CHECK(replies[0].session == s);
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

// Plan state-in-storage 3.2 to 3.4: the state of the machine changes in the
// state storage only after the entry is decided, so a leader that loses
// its round before the decide keeps no state of the entry.
TEST_CASE("a state change commits only after the decide")
{
    auto c = elected();
    const auto s = registered(c, 3, client_a);
    batches::commit(c.at(3), c.at(3).client_request(client_a, s, 1, bytes_of("a")));
    CHECK(concatenation_of(c.at(3)).empty());
    CHECK(c.at(3).get_applied_idx() == registration_done);
    c.deliver(once);
    CHECK(concatenation_of(c.at(3)) == bytes_of("a"));
    CHECK(c.at(3).get_applied_idx() == request_done);
}

namespace {

std::filesystem::path decided_log_of_two_entries(cluster& c)
{
    const auto s = registered(c, 3, client_a);
    batches::commit(c.at(3), c.at(3).client_request(client_a, s, 1, bytes_of("a")));
    c.deliver(once);
    REQUIRE(c.at(1).get_applied_idx() == request_done);
    const auto directory = sqlite_storage::new_directory();
    REQUIRE(c.at(1).get_sequence_paxos().get_internal_storage().copy(directory));
    return directory;
}

}

// Plan state-in-storage 1.3: the storage keeps the applied index over
// a restart. A start runs only the entries after it, so the second start
// does not execute the request a second time.
TEST_CASE("a start runs only the entries after the applied index of the state storage")
{
    auto c = elected();
    const auto log_directory = decided_log_of_two_entries(c);
    for (std::uint64_t nonce = 1; nonce <= 2; ++nonce) {
        auto storage = sqlite_storage::open_in(log_directory);
        auto own = sqlite_storage::open_node_storage_stopped_cleanly(storage, 1);
        const auto n = *node_t::with(config_of(1), std::move(storage), std::move(own), concatenation{}, nonce);
        CHECK(n.get_sequence_paxos().votes());
        CHECK(n.get_applied_idx() == request_done);
        CHECK(concatenation_of(n) == bytes_of("a"));
    }
}

// Plan state-in-storage 1.4: the storage keeps the chain hash of the
// log at its applied index. A start whose state belongs to another log,
// or to a longer log, stops the vote and keeps both CRCs as the proof.
TEST_CASE("a start whose applied chain hash differs from the log stops the vote")
{
    auto c = elected();
    const auto decided = decided_log_of_two_entries(c);
    const auto own = c.at(1).get_sequence_paxos().get_prefix_chain_hash(2);
    REQUIRE(own.has_value());
    const auto log_len = c.at(1).get_sequence_paxos().get_accepted_idx();
    REQUIRE(log_len.has_value());
    const auto differs = make_error_code(konsens::paxos_errc::state_differs_from_log);
    struct applied_point {
        std::uint64_t index;
        konsens::chain_hash chain;
    };
    auto other = *own;
    other.front() ^= std::byte{1};
    for (const auto& applied : {applied_point{2, other}, applied_point{*log_len + 1, *own}}) {
        const auto log_directory = sqlite_storage::new_directory();
        {
            auto storage = sqlite_storage::open_in(decided);
            REQUIRE(storage.copy(log_directory));
        }
        {
            auto storage = sqlite_storage::open_in(log_directory);
            auto w = storage.begin();
            REQUIRE(w.has_value());
            REQUIRE(w->set_applied(applied.index, applied.chain));
            REQUIRE(std::move(*w).commit());
        }
        auto storage = sqlite_storage::open_in(log_directory);
        auto stopped = sqlite_storage::open_node_storage_stopped_cleanly(storage, 1);
        const auto n = *node_t::with(config_of(1), std::move(storage), std::move(stopped), concatenation{}, 1);
        CHECK(n.get_sequence_paxos().get_error() == differs);
        CHECK_FALSE(n.waits_for_the_operator());
        CHECK(n.get_applied_idx() == applied.index);
        if (applied.index == 2) {
            const auto fault = n.get_sequence_paxos().get_hardware_fault();
            REQUIRE(fault.has_value());
            CHECK(fault->index == 2);
            CHECK(fault->chain == *own);
            CHECK(fault->other_chain == other);
        }
    }
}

// Jepsen, Redis-Raft "crossed wires", and VR Revisited's client table: a
// session may have several requests in work, so the reply names its
// request number, and a caller who uses one client_id for both requests
// still finds the reply of each. Both requests go to the leader, so they
// enter the log in their order; through a follower the second could come
// first, and the first would get stale_request.
TEST_CASE("two requests of one session in work get replies with their request numbers")
{
    auto c = elected();
    const auto s = registered(c, 3, client_a);
    batches::commit(c.at(3), c.at(3).client_request(client_a, s, 1, bytes_of("a")));
    batches::commit(c.at(3), c.at(3).client_request(client_a, s, 2, bytes_of("b")));
    c.deliver(once);
    const auto replies = c.at(3).take_client_replies();
    REQUIRE(replies.size() == 2);
    CHECK(std::ranges::count(replies, std::uint64_t{1}, &konsens::client_reply::request_number) == 1);
    CHECK(std::ranges::count(replies, std::uint64_t{2}, &konsens::client_reply::request_number) == 1);
    for (const auto& r : replies) {
        CHECK(r.client_id == client_a);
        CHECK(r.session == s);
        CHECK(r.response);
    }
}

// Plan reply-carries-the-request-number 2.3: 0 names no request, so a reply
// to a registration, a query or an expiry carries 0.
TEST_CASE("a reply that answers no request carries request number 0")
{
    auto c = elected();
    batches::commit(c.at(3), c.at(3).register_client(client_a));
    c.deliver(once);
    auto replies = c.at(3).take_client_replies();
    REQUIRE(replies.size() == 1);
    CHECK(replies[0].request_number == 0);
    const auto s = replies[0].session;
    batches::commit(c.at(3), c.at(3).client_query(client_b, {}));
    c.deliver(once);
    replies = c.at(3).take_client_replies();
    REQUIRE(replies.size() == 1);
    CHECK(replies[0].request_number == 0);
    batches::commit(c.at(3), c.at(3).expire_session(client_a, s));
    c.deliver(once);
    replies = c.at(3).take_client_replies();
    REQUIRE(replies.size() == 1);
    CHECK(replies[0].request_number == 0);
}

// Plan reply-carries-the-request-number 2.2: a request in work when the
// node stops gets the error with its number, so the caller knows which
// request did not end.
TEST_CASE("a request in work at the stop gets stopped with its request number")
{
    auto c = elected();
    const auto s = registered(c, 3, client_a);
    batches::commit(c.at(3), c.at(3).client_request(client_a, s, 7, bytes_of("a")));
    c.at(3).stop();
    const auto replies = c.at(3).take_client_replies();
    REQUIRE(replies.size() == 1);
    CHECK(failed_with(replies[0].response, client_errc::stopped));
    CHECK(replies[0].session == s);
    CHECK(replies[0].request_number == 7);
}

namespace {

// A cluster whose stores stand in known directories, so that a test can
// damage a row of one node as a disk would.
struct cluster_in_directories {
    cluster c;
    std::vector<std::filesystem::path> directories;
};

cluster_in_directories elected_in_directories(const node_id with_check_threads = 0)
{
    cluster_in_directories d;
    for (node_id p = 1; p <= node_count; ++p) {
        d.directories.push_back(sqlite_storage::new_directory());
        auto config = config_of(p);
        if (p == with_check_threads) {
            config.check_threads = 2;
        }
        d.c.nodes.push_back(*node_t::with(config, sqlite_storage::open_in(d.directories.back()),
                                          sqlite_storage::open_new_node_storage(), concatenation{}, 0));
    }
    d.c.run(rounds, once);
    REQUIRE(d.c.at(3).get_sequence_paxos().get_state() == std::pair{role::leader, phase::accept});
    return d;
}

struct state_rows {
    std::vector<std::byte> value;
    std::vector<std::byte> session;

    friend bool operator==(const state_rows&, const state_rows&) = default;
};

state_rows state_rows_in(const std::filesystem::path& directory, const std::uint64_t session)
{
    state_rows rows;
    sqlite_damage::change_with_sqlite(directory, [&](sqlite3* db) {
        rows.value = sqlite_damage::data_row_of(db, concatenation::key);
        rows.session = sqlite_damage::session_row_of(db, session);
    });
    return rows;
}

void flip_the_first_byte_of_the_value(const std::filesystem::path& directory)
{
    sqlite_damage::change_with_sqlite(directory, [](sqlite3* db) {
        auto row = sqlite_damage::data_row_of(db, concatenation::key);
        row.at(0) ^= std::byte{1};
        sqlite_damage::put_data_row(db, concatenation::key, row);
    });
}

void flip_the_first_byte_of_the_session(const std::filesystem::path& directory, const std::uint64_t session)
{
    sqlite_damage::change_with_sqlite(directory, [session](sqlite3* db) {
        auto row = sqlite_damage::session_row_of(db, session);
        row.at(0) ^= std::byte{1};
        sqlite_damage::put_session_row(db, session, row);
    });
}

std::size_t state_damage_records(std::vector<konsens::fault_proof> records)
{
    return static_cast<std::size_t>(std::ranges::count(records, konsens::proof_kind::state_damage, &konsens::fault_proof::kind));
}

bool holds_the_damaged_bytes(const std::vector<konsens::client_reply>& replies, const std::vector<std::byte>& damaged)
{
    return std::ranges::any_of(replies, [&damaged](const konsens::client_reply& r) {
        return r.response && *r.response == damaged;
    });
}

std::vector<std::byte> damaged_value_of(const std::vector<std::byte>& value)
{
    auto damaged = value;
    damaged.at(0) ^= std::byte{1};
    return damaged;
}

}

// Plan data-row-crc 3.5: a follower whose row of data is damaged finds the
// damage at a read. It gives no reply with the damaged bytes, builds its
// state again from its own log in one repair, keeps one record of the
// damage for the log sink, and votes again. The row then holds the bytes
// it held before the damage.
TEST_CASE("a damaged row of data is built again from the own log, and the node votes again")
{
    auto d = elected_in_directories();
    auto& c = d.c;
    const auto s = registered(c, 3, client_a);
    batches::commit(c.at(3), c.at(3).client_request(client_a, s, 1, bytes_of("a")));
    c.deliver(once);
    c.run(rounds, once);
    const auto before = state_rows_in(d.directories[0], s);
    flip_the_first_byte_of_the_value(d.directories[0]);
    batches::commit(c.at(1), c.at(1).client_query(client_b, {}));
    std::vector<konsens::client_reply> replies;
    std::vector<konsens::fault_proof> records;
    for (int i = 0; i < rounds; ++i) {
        c.run(1, once);
        std::ranges::move(c.at(1).take_client_replies(), std::back_inserter(replies));
        std::ranges::move(c.at(1).take_log_records(), std::back_inserter(records));
    }
    CHECK_FALSE(holds_the_damaged_bytes(replies, damaged_value_of(bytes_of("a"))));
    CHECK(state_damage_records(records) == 1);
    CHECK(c.at(1).get_sequence_paxos().votes());
    CHECK_FALSE(c.at(1).waits_for_the_operator());
    CHECK(state_rows_in(d.directories[0], s) == before);
    CHECK(concatenation_of(c.at(1)) == bytes_of("a"));
}

// Plan data-row-crc 3.5: the repair writes every row, so a damaged session
// record that no read found yet is repaired with the row that the read
// found, in one repair. The run again of the log answers no client a
// second time and writes no record of a wrong list.
TEST_CASE("one repair of the state repairs a row that no read found, and replies to nobody again")
{
    auto d = elected_in_directories();
    auto& c = d.c;
    const auto s = registered(c, 3, client_a);
    batches::commit(c.at(3), c.at(3).client_request(client_a, s, 1, bytes_of("a")));
    c.deliver(once);
    c.run(rounds, once);
    static_cast<void>(c.at(1).take_client_replies());
    static_cast<void>(c.at(1).take_log_records());
    const auto before = state_rows_in(d.directories[0], s);
    flip_the_first_byte_of_the_value(d.directories[0]);
    flip_the_first_byte_of_the_session(d.directories[0], s);
    batches::commit(c.at(1), c.at(1).client_query(client_b, {}));
    std::vector<konsens::client_reply> replies;
    std::vector<konsens::fault_proof> records;
    for (int i = 0; i < rounds; ++i) {
        c.run(1, once);
        std::ranges::move(c.at(1).take_client_replies(), std::back_inserter(replies));
        std::ranges::move(c.at(1).take_log_records(), std::back_inserter(records));
    }
    CHECK(state_rows_in(d.directories[0], s) == before);
    CHECK(state_damage_records(records) == 1);
    CHECK(std::ranges::count(records, konsens::proof_kind::wrong_list, &konsens::fault_proof::kind) == 0);
    CHECK(std::ranges::none_of(replies, [](const konsens::client_reply& r) { return r.client_id == client_a; }));
    CHECK(c.at(1).get_sequence_paxos().votes());
}

// Plan data-row-crc 3.5 and the repair plan, item 8: one repair runs by
// itself. A second damage of the state leaves the node out of the vote
// until the operator allows one more repair.
TEST_CASE("a second damage of the state waits for the operator, and allow_one_more_repair repairs it")
{
    auto d = elected_in_directories();
    auto& c = d.c;
    const auto s = registered(c, 3, client_a);
    batches::commit(c.at(3), c.at(3).client_request(client_a, s, 1, bytes_of("a")));
    c.deliver(once);
    c.run(rounds, once);
    const auto before = state_rows_in(d.directories[0], s);
    const auto damage_and_read = [&]() {
        flip_the_first_byte_of_the_value(d.directories[0]);
        batches::commit(c.at(1), c.at(1).client_query(client_b, {}));
        c.run(rounds, once);
    };
    damage_and_read();
    REQUIRE(c.at(1).get_sequence_paxos().votes());
    damage_and_read();
    CHECK(c.at(1).waits_for_the_operator());
    CHECK_FALSE(c.at(1).get_sequence_paxos().votes());
    REQUIRE(c.at(1).allow_one_more_repair());
    c.run(rounds, once);
    CHECK_FALSE(c.at(1).waits_for_the_operator());
    CHECK(c.at(1).get_sequence_paxos().votes());
    CHECK(state_rows_in(d.directories[0], s) == before);
}

// Plan data-row-crc 3.5: a request reads the session record of its
// client. A damaged session record is found there and repaired, and the
// node answers the next request of the client.
TEST_CASE("a damaged session record is found by a request and built again from the own log")
{
    auto d = elected_in_directories();
    auto& c = d.c;
    const auto s = registered(c, 1, client_a);
    batches::commit(c.at(1), c.at(1).client_request(client_a, s, 1, bytes_of("a")));
    c.run(rounds, once);
    static_cast<void>(c.at(1).take_client_replies());
    flip_the_first_byte_of_the_session(d.directories[0], s);
    batches::commit(c.at(1), c.at(1).client_request(client_a, s, 2, bytes_of("b")));
    c.run(rounds, once);
    CHECK(c.at(1).get_sequence_paxos().votes());
    CHECK(concatenation_of(c.at(1)) == bytes_of("ab"));
    static_cast<void>(c.at(1).take_client_replies());
    batches::commit(c.at(1), c.at(1).client_request(client_a, s, 2, bytes_of("b")));
    c.run(rounds, once);
    const auto replies = c.at(1).take_client_replies();
    REQUIRE(replies.size() == 1);
    CHECK(replies[0].response == bytes_of("ab"));
}

// Plan data-row-crc 3.5: a follower with check threads runs the request of
// the leader on a check thread, which reads the damaged row there. The
// damage reaches the node thread with its type, and the node repairs.
TEST_CASE("a damage that a check thread reads is repaired as well")
{
    auto d = elected_in_directories(1);
    auto& c = d.c;
    const auto s = registered(c, 3, client_a);
    batches::commit(c.at(3), c.at(3).client_request(client_a, s, 1, bytes_of("a")));
    c.run(rounds, once);
    flip_the_first_byte_of_the_value(d.directories[0]);
    batches::commit(c.at(3), c.at(3).client_request(client_a, s, 2, bytes_of("b")));
    c.run(rounds, once);
    CHECK(c.at(1).get_sequence_paxos().votes());
    CHECK(concatenation_of(c.at(1)) == bytes_of("ab"));
    CHECK(concatenation_of(c.at(3)) == bytes_of("ab"));
}

// Plan data-row-crc 3.5: find_client_session is a read outside a step. It
// cannot start a repair, so it gives the damage as a value and raises
// nothing.
TEST_CASE("find_client_session gives crc_mismatch for a damaged session record")
{
    auto d = elected_in_directories();
    auto& c = d.c;
    const auto s = registered(c, 3, client_a);
    c.run(rounds, once);
    flip_the_first_byte_of_the_session(d.directories[0], s);
    const auto found = c.at(1).find_client_session(s);
    REQUIRE_FALSE(found.has_value());
    CHECK(found.error() == konsens::storage_errc::crc_mismatch);
}
