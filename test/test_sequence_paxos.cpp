#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

#include "konsens/memory_storage.hpp"
#include "konsens/sequence_paxos.hpp"

namespace {

using konsens::ballot;
using konsens::entry;
using konsens::memory_storage;
using konsens::node_id;
using konsens::paxos_message;
using konsens::phase;
using konsens::role;
using sp_t = konsens::sequence_paxos<memory_storage>;

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

constexpr std::uint64_t first_nonce = 1;

sp_t make(node_id pid, memory_storage s = {}, std::uint64_t nonce = first_nonce)
{
    return *sp_t::with({pid, peers_of(pid)}, std::move(s), nonce);
}

entry make_entry(std::uint8_t v) { return {konsens::entry_kind::normal, {std::byte{v}}}; }

ballot ballot_of(std::uint32_t n, node_id pid) { return {config_id, n, 0, pid}; }

template <typename T>
std::vector<T> messages_of(const std::vector<paxos_message>& out)
{
    std::vector<T> r;
    for (const auto& m : out) {
        if (const auto* p = std::get_if<T>(&m.msg)) {
            r.push_back(*p);
        }
    }
    return r;
}

void deliver_all(std::array<sp_t*, 3> nodes)
{
    for (bool more = true; more;) {
        more = false;
        for (auto* n : nodes) {
            for (auto& m : n->take_outgoing_messages()) {
                more = true;
                REQUIRE(nodes[m.to - 1]->handle(std::move(m)));
            }
        }
    }
}

memory_storage storage_with_promise(const ballot b)
{
    memory_storage s;
    const std::array<konsens::storage_op, 1> ops{konsens::set_promise{b}};
    REQUIRE(s.write_atomically(ops));
    return s;
}

enum class cut { all_messages, all_but_recovery };

void deliver_among(const std::array<sp_t*, 3> nodes, const std::array<bool, 3> connected,
                   const cut kind = cut::all_messages)
{
    for (bool more = true; more;) {
        more = false;
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            for (auto& m : nodes[i]->take_outgoing_messages()) {
                more = true;
                const bool recovery = std::holds_alternative<konsens::recovery>(m.msg) ||
                                      std::holds_alternative<konsens::recovery_response>(m.msg);
                if ((connected[i] && connected[m.to - 1]) || (kind == cut::all_but_recovery && recovery)) {
                    REQUIRE(nodes[m.to - 1]->handle(std::move(m)));
                }
            }
        }
    }
}

std::vector<entry> decided_of(const sp_t& sp)
{
    const auto d = sp.read_decided_suffix(0);
    REQUIRE(d);
    return *d;
}

bool compatible(const std::vector<entry>& a, const std::vector<entry>& b)
{
    const auto n = std::min(a.size(), b.size());
    return std::ranges::equal(std::span(a).first(n), std::span(b).first(n));
}

struct failing_storage {
    memory_storage inner;
    bool fail;

    std::expected<void, std::error_code> write_atomically(std::span<const konsens::storage_op> ops)
    {
        if (fail) {
            return std::unexpected(std::make_error_code(std::errc::no_space_on_device));
        }
        return inner.write_atomically(ops);
    }
    auto get_entries(std::uint64_t from, std::uint64_t to) const { return inner.get_entries(from, to); }
    auto get_log_len() const { return inner.get_log_len(); }
    auto get_promise() const { return inner.get_promise(); }
    auto get_decided_idx() const { return inner.get_decided_idx(); }
    auto get_accepted_round() const { return inner.get_accepted_round(); }
    auto get_last_entry_crc() const { return inner.get_last_entry_crc(); }
};

}

// mod.rs:53-128 `with`: a node with no promise starts as (Follower, None)
// and sends nothing. A node that sent PrepareReq at the first start would
// look like a node in recovery.
TEST_CASE("a new node is a follower in phase none") {
    auto sp = make(1);
    CHECK(sp.get_state() == std::pair{role::follower, phase::none});
    CHECK(sp.take_outgoing_messages().empty());
}

// mod.rs:53-128 `with`: a node that finds a promise in storage restarts in
// Phase::Recover. Restart in SequencePaxos.tla with Recovery: the node
// first sends Recovery with the nonce of the caller to each peer, and sends
// no PrepareReq until its recovery completes (VR Revisited 4.3).
TEST_CASE("a node with a stored promise restarts in recover and sends recovery") {
    auto sp = make(1, storage_with_promise(ballot_of(1, 2)), 7);
    CHECK(sp.get_state() == std::pair{role::follower, phase::recover});
    CHECK(sp.recovering());
    const auto out = sp.take_outgoing_messages();
    CHECK(messages_of<konsens::prepare_req>(out).empty());
    const auto rs = messages_of<konsens::recovery>(out);
    REQUIRE(rs.size() == 2);
    CHECK(rs[0].nonce == 7);
    CHECK(rs[1].nonce == 7);
}

// leader.rs:16-60 handle_leader: the elected node writes its promise and
// sends Prepare to each peer. A lower ballot changes nothing.
TEST_CASE("handle_leader with the own ballot sends prepare") {
    auto sp = make(3);
    REQUIRE(sp.handle_leader(ballot_of(1, 3)));
    CHECK(sp.get_state() == std::pair{role::leader, phase::prepare});
    CHECK(*sp.get_internal_storage().get_promise() == ballot_of(1, 3));
    CHECK(messages_of<konsens::prepare>(sp.take_outgoing_messages()).size() == 2);
    REQUIRE(sp.handle_leader(ballot_of(1, 2)));
    CHECK(sp.take_outgoing_messages().empty());
}

// follower.rs:13-51 handle_prepare: a follower promises a higher ballot and
// reports its accepted state. It ignores a ballot that is not higher.
TEST_CASE("handle_prepare promises only a higher ballot") {
    auto sp = make(1);
    REQUIRE(sp.handle({3, 1, konsens::prepare{ballot_of(1, 3), 0, ballot{}, 0}}));
    CHECK(sp.get_state() == std::pair{role::follower, phase::prepare});
    CHECK(*sp.get_internal_storage().get_promise() == ballot_of(1, 3));
    const auto out = sp.take_outgoing_messages();
    REQUIRE(out.size() == 1);
    CHECK(out[0].to == 3);
    CHECK(std::get<konsens::promise>(out[0].msg).n == ballot_of(1, 3));
    REQUIRE(sp.handle({2, 1, konsens::prepare{ballot_of(1, 2), 0, ballot{}, 0}}));
    CHECK(sp.take_outgoing_messages().empty());
}

// follower.rs:196-227 check_valid_ballot: a message of a lower ballot gets
// NotAccepted with the promise, so that the old leader learns of it.
TEST_CASE("a message of a lower ballot gets not_accepted") {
    auto sp = make(1);
    REQUIRE(sp.handle({3, 1, konsens::prepare{ballot_of(2, 3), 0, ballot{}, 0}}));
    sp.take_outgoing_messages();
    REQUIRE(sp.handle({2, 1, konsens::decide{ballot_of(1, 2), {1, 1}, 0}}));
    const auto out = sp.take_outgoing_messages();
    REQUIRE(out.size() == 1);
    CHECK(out[0].to == 2);
    CHECK(std::get<konsens::not_accepted>(out[0].msg).n == ballot_of(2, 3));
}

// leader.rs:257-285 handle_majority_promises: with a majority of promises
// the leader takes phase Accept, appends its buffered proposals and sends
// AcceptSync to each promised follower.
TEST_CASE("a majority of promises moves the leader to accept") {
    auto sp = make(3);
    REQUIRE(sp.handle_leader(ballot_of(1, 3)));
    REQUIRE(sp.append(make_entry(7)));
    sp.take_outgoing_messages();
    REQUIRE(sp.handle({1, 3, konsens::promise{ballot_of(1, 3), ballot{}, 0, 0, std::nullopt}}));
    CHECK(sp.get_state() == std::pair{role::leader, phase::accept});
    CHECK(sp.get_accepted_idx() == 1);
    CHECK(*sp.get_internal_storage().get_accepted_round() == ballot_of(1, 3));
    const auto syncs = messages_of<konsens::accept_sync>(sp.take_outgoing_messages());
    REQUIRE(syncs.size() == 1);
    CHECK(syncs[0].log_sync.suffix == std::vector{make_entry(7)});
    CHECK(syncs[0].seq_num == konsens::sequence_number{1, 1});
}

// The fix for S7 (DecidedIdxMonotone TRUE in the model): a late AcceptSync
// with a lower decided_idx must not lower the decided_idx of a follower.
// The source at 532ff79 writes the value as given and violates S7.
TEST_CASE("accept_sync keeps the higher decided_idx") {
    auto sp = make(1);
    REQUIRE(sp.handle({3, 1, konsens::prepare{ballot_of(1, 3), 0, ballot{}, 0}}));
    REQUIRE(sp.handle({3, 1, konsens::accept_sync{ballot_of(1, 3), {1, 1}, 2,
                                                 {{make_entry(1), make_entry(2)}, 0}}}));
    CHECK(sp.get_decided_idx() == 2);
    REQUIRE(sp.handle({3, 1, konsens::prepare{ballot_of(2, 3), 2, ballot_of(1, 3), 2}}));
    REQUIRE(sp.handle({3, 1, konsens::accept_sync{ballot_of(2, 3), {1, 1}, 1, {{}, 2}}}));
    CHECK(sp.get_decided_idx() == 2);
    CHECK(*sp.get_internal_storage().get_decided_idx() == 2);
}

// follower.rs:230-238 handle_sequence_num: a gap in the sequence numbers
// means a lost AcceptDecide. The follower goes to Recover and asks the
// leader with PrepareReq, and it does not append the entries.
TEST_CASE("a gap in the sequence numbers starts recovery") {
    auto sp = make(1);
    REQUIRE(sp.handle({3, 1, konsens::prepare{ballot_of(1, 3), 0, ballot{}, 0}}));
    REQUIRE(sp.handle({3, 1, konsens::accept_sync{ballot_of(1, 3), {1, 1}, 0, {{}, 0}}}));
    sp.take_outgoing_messages();
    REQUIRE(sp.handle({3, 1, konsens::accept_decide{ballot_of(1, 3), {1, 3}, 0, {make_entry(1)}}}));
    CHECK(sp.get_state() == std::pair{role::follower, phase::recover});
    CHECK(sp.get_accepted_idx() == 0);
    const auto out = sp.take_outgoing_messages();
    REQUIRE(messages_of<konsens::prepare_req>(out).size() == 1);
    CHECK(out[0].to == 3);
}

// follower.rs:230-238: a duplicate AcceptDecide is outdated and changes
// nothing; a second append of its entries would break S5.
TEST_CASE("a duplicate accept_decide is ignored") {
    auto sp = make(1);
    REQUIRE(sp.handle({3, 1, konsens::prepare{ballot_of(1, 3), 0, ballot{}, 0}}));
    REQUIRE(sp.handle({3, 1, konsens::accept_sync{ballot_of(1, 3), {1, 1}, 0, {{}, 0}}}));
    const paxos_message ad{3, 1, konsens::accept_decide{ballot_of(1, 3), {1, 2}, 0, {make_entry(1)}}};
    REQUIRE(sp.handle(ad));
    REQUIRE(sp.handle(ad));
    CHECK(sp.get_accepted_idx() == 1);
}

// The path that succeeds: three nodes, one leader, two entries that a
// follower proposes. Every node decides both entries in order.
TEST_CASE("three nodes decide forwarded entries") {
    auto a = make(1);
    auto b = make(2);
    auto c = make(3);
    const std::array<sp_t*, 3> nodes{&a, &b, &c};
    REQUIRE(c.handle_leader(ballot_of(1, 3)));
    deliver_all(nodes);
    REQUIRE(a.append(make_entry(1)));
    REQUIRE(a.append(make_entry(2)));
    deliver_all(nodes);
    for (const auto* n : nodes) {
        const auto d = n->read_decided_suffix(0);
        REQUIRE(d);
        CHECK(*d == std::vector{make_entry(1), make_entry(2)});
    }
}

// A follower that gets its Decide before it has the entries (decided_idx
// above accepted_idx) decides only what it has, as
// update_decided_idx_and_get_accepted_idx in follower.rs:142-158.
TEST_CASE("update_decided caps the decided_idx at the accepted_idx") {
    CHECK(konsens::update_decided(1, 3, 5).decided_idx == 3);
    CHECK(konsens::update_decided(1, 3, 5).flushed);
    CHECK(konsens::update_decided(1, 3, 2).decided_idx == 2);
    CHECK(!konsens::update_decided(1, 3, 2).flushed);
    CHECK(konsens::update_decided(2, 3, 1).decided_idx == 2);
}

// The decision of the owner for an error of the storage: the handler gives
// the error as a value, and the node then stops voting. Every later call
// gives the same error and sends nothing.
TEST_CASE("a storage error stops the node") {
    auto sp = *konsens::sequence_paxos<failing_storage>::with({1, peers_of(1)}, failing_storage{{}, true}, first_nonce);
    const auto r = sp.handle({3, 1, konsens::prepare{ballot_of(1, 3), 0, ballot{}, 0}});
    REQUIRE(!r);
    CHECK(r.error() == std::make_error_code(std::errc::no_space_on_device));
    CHECK(!sp.votes());
    CHECK(sp.take_outgoing_messages().empty());
    CHECK(!sp.handle({3, 1, konsens::prepare{ballot_of(2, 3), 0, ballot{}, 0}}));
    CHECK(!sp.append(make_entry(1)));
    CHECK(sp.take_outgoing_messages().empty());
}

// A pid of 0 means "no leader" in mod.rs:371-384, so a node with pid 0 is
// a wrong use and must fail loudly.
TEST_CASE("a pid of 0 is a wrong use") {
    CHECK_THROWS_AS(static_cast<void>(sp_t::with({0, {1, 2}}, {}, first_nonce)), std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(sp_t::with({1, {1, 2}}, {}, first_nonce)), std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(sp_t::with({1, {2, 2}}, {}, first_nonce)), std::logic_error);
}

// A message from a pid outside the cluster is dropped. The leader state
// has no index for that pid.
TEST_CASE("a message from an unknown pid is dropped") {
    auto sp = make(1);
    REQUIRE(sp.handle({9, 1, konsens::prepare{ballot_of(1, 9), 0, ballot{}, 0}}));
    CHECK(sp.get_state() == std::pair{role::follower, phase::none});
    CHECK(sp.take_outgoing_messages().empty());
}

// A message whose `to` names another node is dropped, so that a message that
// reached the wrong node changes no state there.
TEST_CASE("a message to another node is dropped") {
    auto sp = make(1);
    REQUIRE(sp.handle({2, 3, konsens::prepare{ballot_of(1, 2), 0, ballot{}, 0}}));
    CHECK(sp.get_state() == std::pair{role::follower, phase::none});
    CHECK(sp.take_outgoing_messages().empty());
}

// F5 of plan v0.1: a first start finds the default promise and does not
// recover. If it did, a new cluster would wait for a primary that does not
// exist, because each node starts with no promise.
TEST_CASE("a first start does not recover") {
    auto sp = make(1);
    CHECK(!sp.recovering());
    CHECK(sp.refuse_if_recovering());
    CHECK(sp.take_outgoing_messages().empty());
    REQUIRE(sp.append(make_entry(1)));
}

// Dispatch in SequencePaxos.tla: a recovering node drops each message
// except RecoveryResponse, takes no leadership (HandleLeader needs
// ~recovering) and refuses a proposal. The timeout sends Recovery again to
// every peer. The model sends it only to the peers that have not answered;
// then a node whose peers all answered before a primary existed waits
// forever (simulation seed 2). A later answer replaces the earlier answer
// of the same peer, as in HandleRecoveryResponse. This form is provisional
// and goes to the owner.
TEST_CASE("a recovering node drops messages, refuses proposals and resends recovery") {
    auto sp = make(1, storage_with_promise(ballot_of(1, 2)), 7);
    sp.take_outgoing_messages();
    REQUIRE(sp.handle({3, 1, konsens::prepare{ballot_of(2, 3), 0, ballot{}, 0}}));
    REQUIRE(sp.handle({2, 1, konsens::recovery{9}}));
    CHECK(sp.take_outgoing_messages().empty());
    CHECK(*sp.get_internal_storage().get_promise() == ballot_of(1, 2));
    REQUIRE(sp.handle_leader(ballot_of(5, 1)));
    CHECK(sp.get_state() == std::pair{role::follower, phase::recover});
    CHECK(sp.take_outgoing_messages().empty());
    const auto refused = sp.append(make_entry(1));
    REQUIRE(!refused);
    CHECK(refused.error() == konsens::client_errc::recovering);
    REQUIRE(sp.handle({2, 1, konsens::recovery_response{7, ballot_of(1, 2), false, ballot{}, {}, 0, false, 0, 0}}));
    REQUIRE(sp.resend_message_timeout());
    const auto rs = messages_of<konsens::recovery>(sp.take_outgoing_messages());
    REQUIRE(rs.size() == 2);
    CHECK(rs[0].nonce == 7);
    CHECK(rs[1].nonce == 7);
}

// The answers are kept one per peer: a later answer of a peer replaces its
// earlier answer. Node 2 first answers as a follower, then as the primary
// of a new ballot, and recovery completes with that primary.
TEST_CASE("handle_recovery_response: a later answer of a peer replaces its earlier answer") {
    auto sp = make(1, storage_with_promise(ballot_of(1, 2)), 7);
    sp.take_outgoing_messages();
    REQUIRE(sp.handle({2, 1, konsens::recovery_response{7, ballot_of(1, 3), false, ballot{}, {}, 0, false, 0, 0}}));
    REQUIRE(sp.handle({3, 1, konsens::recovery_response{7, ballot_of(1, 3), false, ballot{}, {}, 0, false, 0, 0}}));
    CHECK(sp.recovering());
    REQUIRE(sp.handle({2, 1, konsens::recovery_response{7, ballot_of(2, 2), true, ballot_of(2, 2), {}, 0,
                                                        false, 0, 0}}));
    CHECK(!sp.recovering());
    CHECK(sp.get_promise() == ballot_of(2, 2));
}

// HandleRecovery and RecoveryResponseMsg: the leader in Accept is the
// primary and sends its promise, accepted round, whole log and decided
// index, with the nonce of the request. A follower sends only its promise.
TEST_CASE("handle_recovery: the primary sends its state, a follower its promise") {
    auto a = make(1);
    auto b = make(2);
    auto c = make(3);
    const std::array<sp_t*, 3> nodes{&a, &b, &c};
    REQUIRE(c.handle_leader(ballot_of(1, 3)));
    deliver_all(nodes);
    REQUIRE(c.append(make_entry(1)));
    deliver_all(nodes);
    REQUIRE(c.handle({2, 3, konsens::recovery{4}}));
    auto out = c.take_outgoing_messages();
    REQUIRE(out.size() == 1);
    CHECK(out[0].to == 2);
    const auto p = std::get<konsens::recovery_response>(out[0].msg);
    CHECK(p.nonce == 4);
    CHECK(p.primary);
    CHECK(p.promise == ballot_of(1, 3));
    CHECK(p.n_accepted == ballot_of(1, 3));
    CHECK(p.log == std::vector{make_entry(1)});
    CHECK(p.decided_idx == 1);
    REQUIRE(a.handle({2, 1, konsens::recovery{4}}));
    out = a.take_outgoing_messages();
    REQUIRE(out.size() == 1);
    const auto f = std::get<konsens::recovery_response>(out[0].msg);
    CHECK(!f.primary);
    CHECK(f.promise == ballot_of(1, 3));
    CHECK(f.n_accepted == ballot{});
    CHECK(f.log.empty());
    CHECK(f.decided_idx == 0);
}

// IfVotes in HandleRecovery: a node that does not vote does not answer.
// Here the node stopped voting after a storage error.
TEST_CASE("handle_recovery: a node that does not vote does not answer") {
    auto sp = *konsens::sequence_paxos<failing_storage>::with({1, peers_of(1)}, failing_storage{{}, true},
                                                              first_nonce);
    REQUIRE(!sp.handle({3, 1, konsens::prepare{ballot_of(1, 3), 0, ballot{}, 0}}));
    CHECK(!sp.handle({2, 1, konsens::recovery{4}}));
    CHECK(sp.take_outgoing_messages().empty());
}

// HandleRecoveryResponse: an answer with another nonce is ignored, because
// it answers an earlier start. RecoveryComplete needs f + 1 answers from
// peers and one primary; f + 1 answers with no primary keep the node
// waiting.
TEST_CASE("handle_recovery_response: an old nonce is ignored and a primary is needed") {
    auto sp = make(1, storage_with_promise(ballot_of(1, 2)), 7);
    sp.take_outgoing_messages();
    const konsens::recovery_response primary{7, ballot_of(1, 2), true, ballot_of(1, 2), {make_entry(1)}, 1,
                 false, 1, 0};
    auto old = primary;
    old.nonce = 6;
    REQUIRE(sp.handle({2, 1, old}));
    REQUIRE(sp.handle({3, 1, konsens::recovery_response{7, ballot_of(1, 2), false, ballot{}, {}, 0, false, 0, 0}}));
    CHECK(sp.recovering());
    REQUIRE(sp.handle({2, 1, konsens::recovery_response{7, ballot_of(1, 2), false, ballot{}, {}, 0, false, 0, 0}}));
    CHECK(sp.recovering());
    CHECK(sp.take_outgoing_messages().empty());
    REQUIRE(sp.handle({2, 1, primary}));
    CHECK(!sp.recovering());
}

// RecoveryPrimaries: the primary is an answer with the highest promise
// among the answers, from the node whose pid is the pid of that ballot. A
// primary of a lower ballot (an old leader that has not yet seen the new
// one) is not taken, and an answer that claims primary for a ballot of
// another pid is not taken either.
TEST_CASE("handle_recovery_response: only the primary of the highest ballot counts") {
    auto sp = make(1, storage_with_promise(ballot_of(1, 2)), 7);
    sp.take_outgoing_messages();
    REQUIRE(sp.handle(
        {2, 1, konsens::recovery_response{7, ballot_of(1, 2), true, ballot_of(1, 2), {make_entry(1)}, 1,
                                          false, 1, 0}}));
    REQUIRE(sp.handle({3, 1, konsens::recovery_response{7, ballot_of(2, 3), false, ballot{}, {}, 0, false, 0, 0}}));
    CHECK(sp.recovering());
    REQUIRE(sp.handle(
        {3, 1, konsens::recovery_response{7, ballot_of(2, 2), true, ballot_of(2, 2), {make_entry(1)}, 1,
                                          false, 1, 0}}));
    CHECK(sp.recovering());
}

// HandleRecoveryResponse when the answers are complete: the node takes the
// log, accepted round and decided index of the primary in one write (F7),
// keeps the higher promise (its own here), goes to (Follower, Recover) and
// sends PrepareReq with that promise to each peer (F4).
TEST_CASE("handle_recovery_response: the node takes the state of the primary") {
    memory_storage s = storage_with_promise(ballot_of(3, 1));
    const std::vector nines{make_entry(9), make_entry(9)};
    const std::array<konsens::storage_op, 2> ops{konsens::append_entries{nines},
                                                 konsens::set_accepted_round{ballot_of(1, 1)}};
    REQUIRE(s.write_atomically(ops));
    auto sp = make(1, s, 7);
    sp.take_outgoing_messages();
    REQUIRE(sp.handle({3, 1, konsens::recovery_response{7, ballot_of(2, 3), false, ballot{}, {}, 0, false, 0, 0}}));
    REQUIRE(sp.handle(
        {2, 1, konsens::recovery_response{7, ballot_of(2, 2), true, ballot_of(2, 2), {make_entry(1)}, 1,
                                          false, 1, 0}}));
    CHECK(sp.recovering());
    REQUIRE(sp.handle({3, 1, konsens::recovery_response{7, ballot_of(2, 3), true, ballot_of(2, 3),
                                                        {make_entry(1), make_entry(2)}, 1, false, 2, 0}}));
    CHECK(!sp.recovering());
    CHECK(sp.get_state() == std::pair{role::follower, phase::recover});
    CHECK(sp.get_promise() == ballot_of(3, 1));
    CHECK(sp.get_accepted_round() == ballot_of(2, 3));
    CHECK(sp.get_accepted_idx() == 2);
    CHECK(sp.get_decided_idx() == 1);
    const auto& st = sp.get_internal_storage();
    CHECK(*st.get_promise() == ballot_of(3, 1));
    CHECK(*st.get_accepted_round() == ballot_of(2, 3));
    CHECK(*st.get_decided_idx() == 1);
    CHECK(*st.get_entries(0, *st.get_log_len()) == std::vector{make_entry(1), make_entry(2)});
    const auto reqs = messages_of<konsens::prepare_req>(sp.take_outgoing_messages());
    REQUIRE(reqs.size() == 2);
    CHECK(reqs[0].n == ballot_of(3, 1));
    CHECK(reqs[1].n == ballot_of(3, 1));
    REQUIRE(sp.append(make_entry(5)));
}

// The node keeps the higher promise: here the promise of the primary.
TEST_CASE("handle_recovery_response: the node takes a higher promise of the primary") {
    auto sp = make(1, storage_with_promise(ballot_of(1, 2)), 7);
    sp.take_outgoing_messages();
    REQUIRE(sp.handle({2, 1, konsens::recovery_response{7, ballot_of(4, 2), true, ballot_of(4, 2), {}, 0,
                                                        false, 0, 0}}));
    REQUIRE(sp.handle({3, 1, konsens::recovery_response{7, ballot_of(4, 2), false, ballot{}, {}, 0, false, 0, 0}}));
    CHECK(!sp.recovering());
    CHECK(sp.get_promise() == ballot_of(4, 2));
    CHECK(*sp.get_internal_storage().get_promise() == ballot_of(4, 2));
}

// S7 of SequencePaxos.tla: the decided index never falls. A primary in
// Accept can hold a decided index below that of the recovering node: the
// leader of a new ballot takes a majority of promises, and the promise
// with the higher decided index can come later. Simulation seed 29 shows
// it. HandleRecoveryResponse in the model takes the decided index of the
// primary, which breaks S7 there. The node keeps the higher decided index.
// The log of the primary holds each decided entry (S2b), so the index
// stays within the log.
TEST_CASE("handle_recovery_response: the decided index does not fall") {
    memory_storage s = storage_with_promise(ballot_of(1, 3));
    const std::vector written{make_entry(1), make_entry(2)};
    const std::array<konsens::storage_op, 3> ops{konsens::append_entries{written},
                                                 konsens::set_accepted_round{ballot_of(1, 3)},
                                                 konsens::set_decided_index{2}};
    REQUIRE(s.write_atomically(ops));
    auto sp = make(1, s, 7);
    sp.take_outgoing_messages();
    REQUIRE(sp.handle({2, 1, konsens::recovery_response{7, ballot_of(2, 2), true, ballot_of(2, 2),
                                                        {make_entry(1), make_entry(2), make_entry(3)}, 0,
                 false, 3, 0}}));
    REQUIRE(sp.handle({3, 1, konsens::recovery_response{7, ballot_of(2, 2), false, ballot{}, {}, 0, false, 0, 0}}));
    CHECK(!sp.recovering());
    CHECK(sp.get_decided_idx() == 2);
    CHECK(*sp.get_internal_storage().get_decided_idx() == 2);
    CHECK(sp.get_accepted_idx() == 3);
}

// Plan v0.1 section 7, provisional form "recovery waits": when every node
// restarts at the same time, no node is a primary and a recovering node
// does not answer Recovery. So no node completes its recovery. The model
// excludes this case (Rollback: "a full cluster from a backup is no
// fault"). The operator acts.
TEST_CASE("when all nodes restart at once, recovery waits") {
    auto a = make(1, storage_with_promise(ballot_of(1, 3)), 2);
    auto b = make(2, storage_with_promise(ballot_of(1, 3)), 2);
    auto c = make(3, storage_with_promise(ballot_of(1, 3)), 2);
    const std::array<sp_t*, 3> nodes{&a, &b, &c};
    for (std::uint32_t round = 0; round < 10; ++round) {
        deliver_all(nodes);
        for (auto* n : nodes) {
            REQUIRE(n->handle_leader(ballot_of(2 + round, n->get_pid())));
            REQUIRE(n->resend_message_timeout());
        }
    }
    deliver_all(nodes);
    for (const auto* n : nodes) {
        CHECK(n->recovering());
        CHECK(n->get_promise() == ballot_of(1, 3));
    }
}

// Run F6b of SequencePaxos.tla in one test: node 2 accepted entry 1 for
// leader 3, and 3 decided it with 2 while node 1 was cut off. Node 2 then
// restarts from an earlier copy of its storage (a rollback: a lost fsync
// or a backup) that does not hold the entry. Node 3 is now cut off for
// every message except the recovery exchange, so that node 3 cannot repair
// node 2 through Prepare and AcceptSync. Node 1 becomes leader with node 2.
// Without recovery, node 2 promises with its old empty log, node 1 decides
// entry 2 at index 0, and S1 breaks: the
// decided prefixes of nodes 1 and 3 differ. With recovery, node 2 first
// takes the log of the primary 3, so node 1 learns entry 1 in its Prepare
// phase. The rollback is needed: a crash alone keeps the durable state.
TEST_CASE("recovery after a rollback keeps S1") {
    auto a = make(1);
    auto b = make(2);
    auto c = make(3);
    const std::array<sp_t*, 3> nodes{&a, &b, &c};
    REQUIRE(c.handle_leader(ballot_of(1, 3)));
    deliver_all(nodes);
    const memory_storage before = b.get_internal_storage();
    REQUIRE(c.append(make_entry(1)));
    deliver_among(nodes, {false, true, true});
    REQUIRE(decided_of(c) == std::vector{make_entry(1)});
    REQUIRE(a.get_accepted_idx() == 0);
    b = make(2, before, 2);
    deliver_among(nodes, {true, true, false}, cut::all_but_recovery);
    REQUIRE(a.handle_leader(ballot_of(2, 1)));
    deliver_among(nodes, {true, true, false});
    REQUIRE(a.append(make_entry(2)));
    deliver_among(nodes, {true, true, false});
    REQUIRE(!decided_of(a).empty());
    CHECK(compatible(decided_of(a), decided_of(c)));
    CHECK(compatible(decided_of(b), decided_of(c)));
}
