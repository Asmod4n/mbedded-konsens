#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

#include "batches.hpp"
#include "konsens/client.hpp"
#include "konsens/sequence_paxos.hpp"
#include "lmdb_storage.hpp"

namespace {

using konsens::ballot;
using konsens::entry;
using konsens::node_id;
using konsens::paxos_message;
using konsens::persistent_storage;
using konsens::phase;
using konsens::role;
using sp_t = konsens::sequence_paxos<persistent_storage>;

constexpr std::uint32_t config_id = 1;

// sequence_paxos writes its storage and releases its messages only at a
// commit. A node that stopped voting gives its error from the commit and
// drops its messages, and the tests of such a node check that nothing
// leaves, so these helpers do not read the result of the commit.
template <typename S>
std::vector<paxos_message> committed_messages_of(konsens::sequence_paxos<S>& sp)
{
    static_cast<void>(batches::commit_and_sync(sp));
    return sp.take_outgoing_messages();
}

constexpr node_id three = 3;
constexpr node_id five = 5;

std::vector<node_id> peers_of(const node_id pid, const node_id count)
{
    std::vector<node_id> peers;
    for (node_id p = 1; p <= count; ++p) {
        if (p != pid) {
            peers.push_back(p);
        }
    }
    return peers;
}

constexpr std::uint64_t first_nonce = 1;

sp_t make(const node_id pid, const node_id count = three)
{
    return *sp_t::with({pid, peers_of(pid, count)}, lmdb_storage::open_new(), first_nonce);
}

entry make_entry(const std::uint8_t v)
{
    return {konsens::entry_kind::normal, {std::byte{v}}};
}

ballot ballot_of(const std::uint32_t n, const node_id pid)
{
    return {config_id, n, 0, pid};
}

template <typename T>
std::vector<std::pair<node_id, T>> messages_of(const std::vector<paxos_message>& out)
{
    std::vector<std::pair<node_id, T>> r;
    for (const auto& m : out) {
        if (const auto* p = std::get_if<T>(&m.msg)) {
            r.emplace_back(m.to, *p);
        }
    }
    return r;
}

// An error check reads the error only when there is one, so that a result
// with a value fails the check and is not undefined behaviour.
template <typename T>
bool failed_with(const std::expected<T, std::error_code>& r, const konsens::client_errc e)
{
    return !r.has_value() && r.error() == e;
}

konsens::promise empty_promise(const ballot n)
{
    return {n, ballot{}, 0, 0, std::nullopt, 0, 0};
}

// Node 3 leads ballot (1, 3) in phase Accept with one accepted and not yet
// decided entry, so that its accepted_idx (1) differs from its decided_idx
// (0). The promises of nodes 1 to `promised` are given.
sp_t leader_in_accept(const node_id count = three, const node_id promised = 1)
{
    auto sp = make(count, count);
    REQUIRE(sp.handle_leader(ballot_of(1, count)));
    REQUIRE(sp.append(make_entry(1)));
    for (node_id p = 1; p <= promised; ++p) {
        REQUIRE(sp.handle({p, count, empty_promise(ballot_of(1, count))}));
    }
    REQUIRE(sp.get_state() == std::pair{role::leader, phase::accept});
    committed_messages_of(sp);
    return sp;
}

}

// StartReadRound: the read index is the accepted_idx of the leader at the
// start of the round, not its decided_idx. A new leader can have a
// decided_idx below an entry that the old leader decided and answered
// (finding 1 of the review of plan v0.1). The round sends one Heartbeat to
// each peer.
TEST_CASE("a read at a leader in accept starts a round at the accepted index")
{
    auto sp = leader_in_accept();
    REQUIRE(sp.get_accepted_idx() == 1);
    REQUIRE(sp.get_decided_idx() == 0);
    REQUIRE(sp.start_read(7));
    const auto& rd = sp.get_read_round();
    CHECK(rd.read_index == 1);
    CHECK(rd.n == ballot_of(1, 3));
    CHECK(rd.id == 1);
    CHECK(rd.acks == std::vector<node_id>{3});
    const auto hbs = messages_of<konsens::heartbeat>(committed_messages_of(sp));
    REQUIRE(hbs.size() == 2);
    for (const auto& [to, hb] : hbs) {
        CHECK(hb.n == ballot_of(1, 3));
        CHECK(hb.id == 1);
    }
}

// ReadStart, Raft 6.4.1: a follower asks the leader of its promise for the
// read index. A node that knows no leader cannot start the read, and the
// caller tries again (error retry, plan section 7).
TEST_CASE("a read at a follower goes to the leader of its promise")
{
    auto sp = make(1);
    CHECK(failed_with(sp.start_read(4), konsens::client_errc::retry));
    CHECK(committed_messages_of(sp).empty());
    REQUIRE(sp.handle({3, 1, konsens::prepare{ballot_of(1, 3), 0, ballot{}, 0}}));
    committed_messages_of(sp);
    REQUIRE(sp.start_read(4));
    const auto out = committed_messages_of(sp);
    REQUIRE(out.size() == 1);
    CHECK(out[0].to == 3);
    CHECK(std::get<konsens::read_index_request>(out[0].msg).read == 4);
}

// HandleReadIndex: the leader takes the read of a follower into a round. A
// node that is no leader drops it; were it to keep it, it would answer the
// read after a later election with a reply that the follower did not ask of
// it.
TEST_CASE("handle_read_index_request: only a leader takes the read")
{
    auto leader = leader_in_accept();
    REQUIRE(leader.handle({1, 3, konsens::read_index_request{9}}));
    CHECK(leader.get_read_round().reads.size() == 1);
    CHECK(messages_of<konsens::heartbeat>(committed_messages_of(leader)).size() == 2);

    auto follower = make(1);
    REQUIRE(follower.handle({2, 1, konsens::read_index_request{9}}));
    CHECK(committed_messages_of(follower).empty());
    REQUIRE(follower.handle_leader(ballot_of(1, 1)));
    REQUIRE(follower.handle({2, 1, empty_promise(ballot_of(1, 1))}));
    REQUIRE(follower.start_read(5));
    committed_messages_of(follower);
    const auto& rd = follower.get_read_round();
    REQUIRE(rd.reads.size() == 1);
    CHECK(rd.reads[0].node == 1);
    REQUIRE(follower.handle({2, 1, konsens::heartbeat_ack{rd.n, rd.id}}));
    CHECK(messages_of<konsens::read_index_reply>(committed_messages_of(follower)).empty());
}

// HandleHeartbeat, Raft 6.4: a node acknowledges only the heartbeat of the
// ballot it promised. An acknowledgement from a node with another promise
// would count a node that may already serve a higher leader.
TEST_CASE("handle_heartbeat answers only the ballot of the promise")
{
    auto sp = make(1);
    REQUIRE(sp.handle({3, 1, konsens::prepare{ballot_of(2, 3), 0, ballot{}, 0}}));
    committed_messages_of(sp);
    REQUIRE(sp.handle({2, 1, konsens::heartbeat{ballot_of(1, 2), 4}}));
    CHECK(committed_messages_of(sp).empty());
    REQUIRE(sp.handle({3, 1, konsens::heartbeat{ballot_of(2, 3), 4}}));
    const auto acks = messages_of<konsens::heartbeat_ack>(committed_messages_of(sp));
    REQUIRE(acks.size() == 1);
    CHECK(acks[0].first == 3);
    CHECK(acks[0].second.n == ballot_of(2, 3));
    CHECK(acks[0].second.id == 4);
}

// HandleHeartbeatAck, Raft 6.4: with a majority the read index holds for
// each read of the round. The leader keeps its own reads and sends each
// follower one ReadIndexReply with the reads of that follower. Reads 7 and
// 8 come while round 1 runs, so round 2 holds both.
TEST_CASE("handle_heartbeat_ack: a majority indexes the reads of the round")
{
    auto sp = leader_in_accept();
    REQUIRE(sp.start_read(6));
    REQUIRE(sp.start_read(7));
    REQUIRE(sp.handle({1, 3, konsens::read_index_request{8}}));
    const auto first = sp.get_read_round();
    REQUIRE(sp.handle({2, 3, konsens::heartbeat_ack{first.n, first.id}}));
    REQUIRE(sp.take_indexed_reads().size() == 1);
    committed_messages_of(sp);
    const auto rd = sp.get_read_round();
    REQUIRE(rd.reads.size() == 2);
    REQUIRE(sp.handle({2, 3, konsens::heartbeat_ack{rd.n, rd.id}}));
    const auto indexed = sp.take_indexed_reads();
    REQUIRE(indexed.size() == 1);
    CHECK(indexed[0].read == 7);
    CHECK(indexed[0].read_index == 1);
    CHECK(indexed[0].ballot == ballot_of(1, 3));
    const auto replies = messages_of<konsens::read_index_reply>(committed_messages_of(sp));
    REQUIRE(replies.size() == 1);
    CHECK(replies[0].first == 1);
    CHECK(replies[0].second.reads == std::vector<std::uint64_t>{8});
    CHECK(replies[0].second.read_index == 1);
    CHECK(sp.get_read_round().reads.empty());
}

// HandleHeartbeatAck: an acknowledgement counts only for the round with the
// same id. A late or duplicated acknowledgement of an earlier round says
// nothing about the time after the new round started.
TEST_CASE("handle_heartbeat_ack: an acknowledgement of an earlier round does not count")
{
    auto sp = leader_in_accept();
    REQUIRE(sp.start_read(1));
    const auto first = sp.get_read_round();
    REQUIRE(sp.handle({2, 3, konsens::heartbeat_ack{first.n, first.id}}));
    REQUIRE(sp.take_indexed_reads().size() == 1);
    REQUIRE(sp.start_read(2));
    REQUIRE(sp.get_read_round().id == first.id + 1);
    REQUIRE(sp.handle({2, 3, konsens::heartbeat_ack{first.n, first.id}}));
    REQUIRE(sp.handle({1, 3, konsens::heartbeat_ack{first.n, first.id}}));
    CHECK(sp.take_indexed_reads().empty());
}

// HandleHeartbeatAck: an acknowledgement counts only with the ballot of the
// round. The ack of another ballot comes from a node with another promise.
TEST_CASE("handle_heartbeat_ack: an acknowledgement of another ballot does not count")
{
    auto sp = leader_in_accept();
    REQUIRE(sp.start_read(1));
    const auto rd = sp.get_read_round();
    REQUIRE(sp.handle({2, 3, konsens::heartbeat_ack{ballot_of(2, 2), rd.id}}));
    CHECK(sp.take_indexed_reads().empty());
}

// HandleHeartbeatAck: the acknowledgements are a set of nodes. With five
// nodes a majority is three; two copies of one acknowledgement are one
// node, and a duplicate on the channel must not complete the round.
TEST_CASE("handle_heartbeat_ack: a duplicate acknowledgement counts once")
{
    auto sp = leader_in_accept(five, 2);
    REQUIRE(sp.start_read(1));
    const auto rd = sp.get_read_round();
    REQUIRE(sp.handle({1, 5, konsens::heartbeat_ack{rd.n, rd.id}}));
    REQUIRE(sp.handle({1, 5, konsens::heartbeat_ack{rd.n, rd.id}}));
    CHECK(sp.take_indexed_reads().empty());
    REQUIRE(sp.handle({2, 5, konsens::heartbeat_ack{rd.n, rd.id}}));
    CHECK(sp.take_indexed_reads().size() == 1);
}

// HandleHeartbeatAck: only a leader in Accept completes a round. A leader
// that promised a higher ballot meanwhile would index its read with the new
// promise, and the node would no longer see that the promise changed.
TEST_CASE("handle_heartbeat_ack: a node that lost the leadership completes no round")
{
    auto sp = leader_in_accept();
    REQUIRE(sp.start_read(1));
    const auto rd = sp.get_read_round();
    REQUIRE(sp.handle({2, 3, konsens::prepare{ballot_of(2, 2), 0, ballot{}, 0}}));
    REQUIRE(sp.get_state().first == role::follower);
    REQUIRE(sp.handle({1, 3, konsens::heartbeat_ack{rd.n, rd.id}}));
    CHECK(sp.take_indexed_reads().empty());
}

// StartReadRound: one round at a time. The reads that come during a round
// wait, and the next round takes all of them when the round completes, as
// in group commit. A second round that started at once would replace the
// reads of the first, and those reads would never get an index.
TEST_CASE("a read during a round waits for the next round")
{
    auto sp = leader_in_accept();
    REQUIRE(sp.start_read(1));
    committed_messages_of(sp);
    REQUIRE(sp.start_read(2));
    CHECK(messages_of<konsens::heartbeat>(committed_messages_of(sp)).empty());
    const auto first = sp.get_read_round();
    REQUIRE(sp.handle({2, 3, konsens::heartbeat_ack{first.n, first.id}}));
    auto indexed = sp.take_indexed_reads();
    REQUIRE(indexed.size() == 1);
    CHECK(indexed[0].read == 1);
    CHECK(messages_of<konsens::heartbeat>(committed_messages_of(sp)).size() == 2);
    const auto second = sp.get_read_round();
    CHECK(second.id == first.id + 1);
    REQUIRE(sp.handle({1, 3, konsens::heartbeat_ack{second.n, second.id}}));
    indexed = sp.take_indexed_reads();
    REQUIRE(indexed.size() == 1);
    CHECK(indexed[0].read == 2);
}

// StartReadRound needs phase Accept. In Prepare the log of the new leader
// can miss an entry that the old leader decided: here node 1 holds entry 1
// decided at ballot (1, 1), and node 3 has an empty log. A round in Prepare
// would take read index 0, and node 1, which promised (2, 3), would
// acknowledge it. The round starts after the sync, with read index 1.
TEST_CASE("a leader in prepare starts its read round only in accept")
{
    auto sp = make(3);
    REQUIRE(sp.handle_leader(ballot_of(2, 3)));
    committed_messages_of(sp);
    REQUIRE(sp.start_read(1));
    CHECK(messages_of<konsens::heartbeat>(committed_messages_of(sp)).empty());
    REQUIRE(sp.handle({1, 3, konsens::heartbeat_ack{ballot_of(2, 3), 1}}));
    CHECK(sp.take_indexed_reads().empty());
    REQUIRE(sp.handle(
        {1, 3, konsens::promise{ballot_of(2, 3), ballot_of(1, 1), 1, 1, konsens::log_sync{{make_entry(1)}, 0}, 0, 0}}));
    REQUIRE(sp.get_state() == std::pair{role::leader, phase::accept});
    CHECK(messages_of<konsens::heartbeat>(committed_messages_of(sp)).size() == 2);
    const auto rd = sp.get_read_round();
    CHECK(rd.read_index == 1);
    REQUIRE(sp.handle({1, 3, konsens::heartbeat_ack{rd.n, rd.id}}));
    const auto indexed = sp.take_indexed_reads();
    REQUIRE(indexed.size() == 1);
    CHECK(indexed[0].read_index == 1);
}

// HandleReadIndexReply, Raft 6.4.1: the follower keeps the read index for
// each of its reads, with the promise it holds now, so that the node can
// drop the read when the promise changes.
TEST_CASE("handle_read_index_reply indexes each read with the promise")
{
    auto sp = make(1);
    REQUIRE(sp.handle({3, 1, konsens::prepare{ballot_of(1, 3), 0, ballot{}, 0}}));
    REQUIRE(sp.handle({3, 1, konsens::read_index_reply{{4, 6}, 2}}));
    const auto indexed = sp.take_indexed_reads();
    REQUIRE(indexed.size() == 2);
    CHECK(indexed[0].read == 4);
    CHECK(indexed[1].read == 6);
    for (const auto& x : indexed) {
        CHECK(x.read_index == 2);
        CHECK(x.ballot == ballot_of(1, 3));
    }
}

// A lost Heartbeat or HeartbeatAck would stop the round, and every read
// after it, for good. The resend timeout sends the heartbeat of the round
// again to each peer that has not acknowledged it; the round id stays.
TEST_CASE("resend_heartbeats repeats the round to the peers without an acknowledgement")
{
    auto sp = leader_in_accept(five, 2);
    sp.resend_heartbeats();
    CHECK(committed_messages_of(sp).empty());
    REQUIRE(sp.start_read(1));
    committed_messages_of(sp);
    const auto rd = sp.get_read_round();
    REQUIRE(sp.handle({1, 5, konsens::heartbeat_ack{rd.n, rd.id}}));
    sp.resend_heartbeats();
    const auto hbs = messages_of<konsens::heartbeat>(committed_messages_of(sp));
    REQUIRE(hbs.size() == 3);
    for (const auto& [to, hb] : hbs) {
        CHECK(to != 1);
        CHECK(hb.id == rd.id);
    }
}
