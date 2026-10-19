#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

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

sp_t make(node_id pid, memory_storage s = {}) { return *sp_t::with({pid, peers_of(pid)}, std::move(s)); }

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
// Phase::Recover and sends PrepareReq to each peer. This is Restart in the
// model, the path that a crash takes.
TEST_CASE("a node with a stored promise restarts in recover") {
    memory_storage s;
    const std::array<konsens::storage_op, 1> ops{konsens::set_promise{ballot_of(1, 2)}};
    REQUIRE(s.write_atomically(ops));
    auto sp = make(1, s);
    CHECK(sp.get_state() == std::pair{role::follower, phase::recover});
    const auto out = sp.take_outgoing_messages();
    CHECK(messages_of<konsens::prepare_req>(out).size() == 2);
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
    auto sp = *konsens::sequence_paxos<failing_storage>::with({1, peers_of(1)}, failing_storage{{}, true});
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
    CHECK_THROWS_AS(sp_t::with({0, {1, 2}}, {}), std::logic_error);
    CHECK_THROWS_AS(sp_t::with({1, {1, 2}}, {}), std::logic_error);
    CHECK_THROWS_AS(sp_t::with({1, {2, 2}}, {}), std::logic_error);
}

// A message from a pid outside the cluster is dropped. The leader state
// has no index for that pid.
TEST_CASE("a message from an unknown pid is dropped") {
    auto sp = make(1);
    REQUIRE(sp.handle({9, 1, konsens::prepare{ballot_of(1, 9), 0, ballot{}, 0}}));
    CHECK(sp.get_state() == std::pair{role::follower, phase::none});
    CHECK(sp.take_outgoing_messages().empty());
}
