#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <fstream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

#include <unistd.h>

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
    static_cast<void>(sp.commit());
    return sp.take_outgoing_messages();
}

template <typename S>
const S& committed_storage_of(konsens::sequence_paxos<S>& sp)
{
    static_cast<void>(sp.commit());
    return sp.get_internal_storage();
}

template <typename S>
std::expected<std::vector<entry>, std::error_code> committed_decided_suffix_of(konsens::sequence_paxos<S>& sp,
                                                                               const std::uint64_t from_idx)
{
    static_cast<void>(sp.commit());
    return sp.read_decided_suffix(from_idx);
}

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

sp_t make(node_id pid, persistent_storage s = lmdb_storage::open_new(), std::uint64_t nonce = first_nonce)
{
    return *sp_t::with({pid, peers_of(pid)}, std::move(s), nonce);
}

entry make_entry(std::uint8_t v)
{
    return {konsens::entry_kind::normal, {std::byte{v}}};
}

ballot ballot_of(std::uint32_t n, node_id pid)
{
    return {config_id, n, 0, pid};
}

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
            for (auto& m : committed_messages_of(*n)) {
                more = true;
                REQUIRE(nodes[m.to - 1]->handle(std::move(m)));
            }
        }
    }
}

persistent_storage storage_with_promise(const ballot b)
{
    auto s = lmdb_storage::open_new();
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
            for (auto& m : committed_messages_of(*nodes[i])) {
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

std::vector<entry> decided_of(sp_t& sp)
{
    const auto d = committed_decided_suffix_of(sp, 0);
    REQUIRE(d);
    return *d;
}

bool compatible(const std::vector<entry>& a, const std::vector<entry>& b)
{
    const auto n = std::min(a.size(), b.size());
    return std::ranges::equal(std::span(a).first(n), std::span(b).first(n));
}

persistent_storage storage_of(const ballot promise, const ballot n_accepted, const std::vector<entry>& log,
                              const std::uint64_t decided_idx)
{
    auto s = lmdb_storage::open_new();
    const std::array<konsens::storage_op, 4> ops{konsens::append_entries{log}, konsens::set_promise{promise},
                                                 konsens::set_accepted_round{n_accepted},
                                                 konsens::set_decided_index{decided_idx}};
    REQUIRE(s.write_atomically(ops));
    return s;
}

std::uint32_t last_crc_of(const std::vector<entry>& log)
{
    return log.empty() ? 0 : konsens::compute_entry_crc(log.size() - 1, log.back());
}

konsens::recovery_response answer_of(const std::uint64_t nonce, const ballot promise, const ballot n_accepted,
                                     const std::vector<entry>& log, const std::uint64_t decided_idx)
{
    return {nonce, promise, false, n_accepted, {}, decided_idx, true, log.size(), last_crc_of(log)};
}

konsens::new_state new_state_of(const std::uint64_t nonce, const ballot promise, const ballot n_accepted,
                                const std::vector<entry>& log, const std::uint64_t decided_idx,
                                const std::uint64_t from_idx)
{
    return {nonce,       promise,
            n_accepted,  log.size(),
            decided_idx, last_crc_of(log),
            from_idx,    std::vector(std::next(log.begin(), static_cast<std::ptrdiff_t>(from_idx)), log.end())};
}

std::vector<entry> log_of(sp_t& sp)
{
    const auto& st = committed_storage_of(sp);
    const auto l = st.get_entries(0, *st.get_log_len());
    REQUIRE(l);
    return *l;
}

constexpr std::size_t largest_map_pages = 64;

// A storage whose map has no room for one more write. open takes the
// smallest map in which LMDB can create the databases of the storage, so
// the next write gives MDB_MAP_FULL, which the storage gives as
// no_space_on_device. The fault comes from LMDB and not from a wrapper.
persistent_storage storage_with_full_map()
{
    const auto directory = lmdb_storage::new_directory();
    const auto page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    for (std::size_t pages = 1; pages <= largest_map_pages; ++pages) {
        auto opened = persistent_storage::open(directory, pages * page, lmdb_storage::non_root_user_id);
        if (opened) {
            return std::move(*opened);
        }
    }
    FAIL("no map of up to largest_map_pages pages holds the databases");
    return lmdb_storage::open_new();
}

// A storage whose last entry has a flipped byte on the disk, so that each
// read of its CRC32C gives crc_mismatch. The fault is in the file that
// LMDB reads and not in a wrapper.
persistent_storage storage_with_flipped_last_entry()
{
    constexpr std::string_view marked = "konsens-flipped-entry";
    const auto directory = lmdb_storage::new_directory();
    {
        auto storage = lmdb_storage::open_in(directory);
        const auto bytes = std::as_bytes(std::span(marked));
        const std::vector written{entry{konsens::entry_kind::normal, {bytes.begin(), bytes.end()}}};
        const std::array<konsens::storage_op, 1> ops{konsens::append_entries{written}};
        REQUIRE(storage.write_atomically(ops));
    }
    const auto data_file = directory / "data.mdb";
    std::vector<char> contents;
    {
        std::ifstream in{data_file, std::ios::binary};
        contents.assign(std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{});
    }
    const auto found = std::ranges::search(contents, marked);
    REQUIRE_FALSE(found.empty());
    found.front() = static_cast<char>(found.front() ^ 0x01);
    {
        std::ofstream out{data_file, std::ios::binary | std::ios::trunc};
        out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    }
    return lmdb_storage::open_in(directory);
}

}

// mod.rs:53-128 `with`: a node with no promise starts as (Follower, None)
// and sends nothing. A node that sent PrepareReq at the first start would
// look like a node in recovery.
TEST_CASE("a new node is a follower in phase none")
{
    auto sp = make(1);
    CHECK(sp.get_state() == std::pair{role::follower, phase::none});
    CHECK(committed_messages_of(sp).empty());
}

// mod.rs:53-128 `with`: a node that finds a promise in storage restarts in
// Phase::Recover. Restart in SequencePaxos.tla with Recovery: the node
// first sends Recovery with the nonce of the caller to each peer, and sends
// no PrepareReq until its recovery completes (VR Revisited 4.3).
TEST_CASE("a node with a stored promise restarts in recover and sends recovery")
{
    auto sp = make(1, storage_with_promise(ballot_of(1, 2)), 7);
    CHECK(sp.get_state() == std::pair{role::follower, phase::recover});
    CHECK(sp.recovering());
    const auto out = committed_messages_of(sp);
    CHECK(messages_of<konsens::prepare_req>(out).empty());
    const auto rs = messages_of<konsens::recovery>(out);
    REQUIRE(rs.size() == 2);
    CHECK(rs[0].nonce == 7);
    CHECK(rs[1].nonce == 7);
}

// leader.rs:16-60 handle_leader: the elected node writes its promise and
// sends Prepare to each peer. A lower ballot changes nothing.
TEST_CASE("handle_leader with the own ballot sends prepare")
{
    auto sp = make(3);
    REQUIRE(sp.handle_leader(ballot_of(1, 3)));
    CHECK(sp.get_state() == std::pair{role::leader, phase::prepare});
    CHECK(*committed_storage_of(sp).get_promise() == ballot_of(1, 3));
    CHECK(messages_of<konsens::prepare>(committed_messages_of(sp)).size() == 2);
    REQUIRE(sp.handle_leader(ballot_of(1, 2)));
    CHECK(committed_messages_of(sp).empty());
}

// follower.rs:13-51 handle_prepare: a follower promises a higher ballot and
// reports its accepted state. It ignores a ballot that is not higher.
TEST_CASE("handle_prepare promises only a higher ballot")
{
    auto sp = make(1);
    REQUIRE(sp.handle({3, 1, konsens::prepare{ballot_of(1, 3), 0, ballot{}, 0}}));
    CHECK(sp.get_state() == std::pair{role::follower, phase::prepare});
    CHECK(*committed_storage_of(sp).get_promise() == ballot_of(1, 3));
    const auto out = committed_messages_of(sp);
    REQUIRE(out.size() == 1);
    CHECK(out[0].to == 3);
    CHECK(std::get<konsens::promise>(out[0].msg).n == ballot_of(1, 3));
    REQUIRE(sp.handle({2, 1, konsens::prepare{ballot_of(1, 2), 0, ballot{}, 0}}));
    CHECK(committed_messages_of(sp).empty());
}

// follower.rs:196-227 check_valid_ballot: a message of a lower ballot gets
// NotAccepted with the promise, so that the old leader learns of it.
TEST_CASE("a message of a lower ballot gets not_accepted")
{
    auto sp = make(1);
    REQUIRE(sp.handle({3, 1, konsens::prepare{ballot_of(2, 3), 0, ballot{}, 0}}));
    committed_messages_of(sp);
    REQUIRE(sp.handle({2, 1, konsens::decide{ballot_of(1, 2), {1, 1}, 0}}));
    const auto out = committed_messages_of(sp);
    REQUIRE(out.size() == 1);
    CHECK(out[0].to == 2);
    CHECK(std::get<konsens::not_accepted>(out[0].msg).n == ballot_of(2, 3));
}

// leader.rs:257-285 handle_majority_promises: with a majority of promises
// the leader takes phase Accept, appends its buffered proposals and sends
// AcceptSync to each promised follower.
TEST_CASE("a majority of promises moves the leader to accept")
{
    auto sp = make(3);
    REQUIRE(sp.handle_leader(ballot_of(1, 3)));
    REQUIRE(sp.append(make_entry(7)));
    committed_messages_of(sp);
    REQUIRE(sp.handle({1, 3, konsens::promise{ballot_of(1, 3), ballot{}, 0, 0, std::nullopt}}));
    CHECK(sp.get_state() == std::pair{role::leader, phase::accept});
    CHECK(sp.get_accepted_idx() == 1);
    CHECK(*committed_storage_of(sp).get_accepted_round() == ballot_of(1, 3));
    const auto syncs = messages_of<konsens::accept_sync>(committed_messages_of(sp));
    REQUIRE(syncs.size() == 1);
    CHECK(syncs[0].log_sync.suffix == std::vector{make_entry(7)});
    CHECK(syncs[0].seq_num == konsens::sequence_number{1, 1});
}

// The fix for S7 (DecidedIdxMonotone TRUE in the model): a late AcceptSync
// with a lower decided_idx must not lower the decided_idx of a follower.
// The source at 532ff79 writes the value as given and violates S7.
TEST_CASE("accept_sync keeps the higher decided_idx")
{
    auto sp = make(1);
    REQUIRE(sp.handle({3, 1, konsens::prepare{ballot_of(1, 3), 0, ballot{}, 0}}));
    REQUIRE(sp.handle({3, 1, konsens::accept_sync{ballot_of(1, 3), {1, 1}, 2, {{make_entry(1), make_entry(2)}, 0}}}));
    CHECK(sp.get_decided_idx() == 2);
    REQUIRE(sp.handle({3, 1, konsens::prepare{ballot_of(2, 3), 2, ballot_of(1, 3), 2}}));
    REQUIRE(sp.handle({3, 1, konsens::accept_sync{ballot_of(2, 3), {1, 1}, 1, {{}, 2}}}));
    CHECK(sp.get_decided_idx() == 2);
    CHECK(*committed_storage_of(sp).get_decided_idx() == 2);
}

// follower.rs:230-238 handle_sequence_num: a gap in the sequence numbers
// means a lost AcceptDecide. The follower goes to Recover and asks the
// leader with PrepareReq, and it does not append the entries.
TEST_CASE("a gap in the sequence numbers starts recovery")
{
    auto sp = make(1);
    REQUIRE(sp.handle({3, 1, konsens::prepare{ballot_of(1, 3), 0, ballot{}, 0}}));
    REQUIRE(sp.handle({3, 1, konsens::accept_sync{ballot_of(1, 3), {1, 1}, 0, {{}, 0}}}));
    committed_messages_of(sp);
    REQUIRE(sp.handle({3, 1, konsens::accept_decide{ballot_of(1, 3), {1, 3}, 0, {make_entry(1)}}}));
    CHECK(sp.get_state() == std::pair{role::follower, phase::recover});
    CHECK(sp.get_accepted_idx() == 0);
    const auto out = committed_messages_of(sp);
    REQUIRE(messages_of<konsens::prepare_req>(out).size() == 1);
    CHECK(out[0].to == 3);
}

// follower.rs:230-238: a duplicate AcceptDecide is outdated and changes
// nothing; a second append of its entries would break S5.
TEST_CASE("a duplicate accept_decide is ignored")
{
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
TEST_CASE("three nodes decide forwarded entries")
{
    auto a = make(1);
    auto b = make(2);
    auto c = make(3);
    const std::array<sp_t*, 3> nodes{&a, &b, &c};
    REQUIRE(c.handle_leader(ballot_of(1, 3)));
    deliver_all(nodes);
    REQUIRE(a.append(make_entry(1)));
    REQUIRE(a.append(make_entry(2)));
    deliver_all(nodes);
    for (auto* n : nodes) {
        const auto d = committed_decided_suffix_of(*n, 0);
        REQUIRE(d);
        CHECK(*d == std::vector{make_entry(1), make_entry(2)});
    }
}

// A follower that gets its Decide before it has the entries (decided_idx
// above accepted_idx) decides only what it has, as
// update_decided_idx_and_get_accepted_idx in follower.rs:142-158.
TEST_CASE("update_decided caps the decided_idx at the accepted_idx")
{
    CHECK(konsens::update_decided(1, 3, 5).decided_idx == 3);
    CHECK(konsens::update_decided(1, 3, 5).flushed);
    CHECK(konsens::update_decided(1, 3, 2).decided_idx == 2);
    CHECK(!konsens::update_decided(1, 3, 2).flushed);
    CHECK(konsens::update_decided(2, 3, 1).decided_idx == 2);
}

// The decision of the owner (2026-10-21): a write that fails is a failure
// of the machine, and it is raised with the error code of the store. A
// handler that must write the waiting ops before it reads the storage
// raises it inside the batch. The program does not repeat the write, so a
// later commit is a wrong use, and no message of the batch leaves.
TEST_CASE("a failed write inside a batch throws write_error, and a later commit is a wrong use")
{
    auto sp = make(1, storage_with_full_map());
    REQUIRE(sp.handle({3, 1, konsens::prepare{ballot_of(1, 3), 0, ballot{}, 0}}));
    try {
        static_cast<void>(sp.handle({2, 1, konsens::recovery{4}}));
        FAIL("the handler did not throw");
    } catch (const konsens::write_error& e) {
        CHECK(e.code() == std::make_error_code(std::errc::no_space_on_device));
    }
    CHECK_THROWS_AS(static_cast<void>(sp.commit()), std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(sp.take_outgoing_messages()), std::logic_error);
}

// The same failure at the commit. The Promise of the Prepare depends on
// the promise that the failed write held, so it stays in the node, and a
// second commit is a wrong use.
TEST_CASE("a failed write at the commit throws write_error and sends nothing")
{
    auto sp = make(1, storage_with_full_map());
    REQUIRE(sp.handle({3, 1, konsens::prepare{ballot_of(1, 3), 0, ballot{}, 0}}));
    try {
        static_cast<void>(sp.commit());
        FAIL("the commit did not throw");
    } catch (const konsens::write_error& e) {
        CHECK(e.code() == std::make_error_code(std::errc::no_space_on_device));
    }
    CHECK_THROWS_AS(static_cast<void>(sp.take_outgoing_messages()), std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(sp.commit()), std::logic_error);
}

// A pid of 0 means "no leader" in mod.rs:371-384, so a node with pid 0 is
// a wrong use and must fail loudly.
TEST_CASE("a pid of 0 is a wrong use")
{
    CHECK_THROWS_AS(static_cast<void>(sp_t::with({0, {1, 2}}, lmdb_storage::open_new(), first_nonce)), std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(sp_t::with({1, {1, 2}}, lmdb_storage::open_new(), first_nonce)), std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(sp_t::with({1, {2, 2}}, lmdb_storage::open_new(), first_nonce)), std::logic_error);
}

// A message from a pid outside the cluster is dropped. The leader state
// has no index for that pid.
TEST_CASE("a message from an unknown pid is dropped")
{
    auto sp = make(1);
    REQUIRE(sp.handle({9, 1, konsens::prepare{ballot_of(1, 9), 0, ballot{}, 0}}));
    CHECK(sp.get_state() == std::pair{role::follower, phase::none});
    CHECK(committed_messages_of(sp).empty());
}

// A message whose `to` names another node is dropped, so that a message that
// reached the wrong node changes no state there.
TEST_CASE("a message to another node is dropped")
{
    auto sp = make(1);
    REQUIRE(sp.handle({2, 3, konsens::prepare{ballot_of(1, 2), 0, ballot{}, 0}}));
    CHECK(sp.get_state() == std::pair{role::follower, phase::none});
    CHECK(committed_messages_of(sp).empty());
}

// F5 of plan v0.1: a first start finds the default promise and does not
// recover. If it did, a new cluster would wait for a primary that does not
// exist, because each node starts with no promise.
TEST_CASE("a first start does not recover")
{
    auto sp = make(1);
    CHECK(!sp.recovering());
    CHECK(sp.refuse_if_recovering());
    CHECK(committed_messages_of(sp).empty());
    REQUIRE(sp.append(make_entry(1)));
}

// Dispatch in SequencePaxos.tla: a recovering node drops each message
// except Recovery and RecoveryResponse, takes no leadership (HandleLeader
// needs ~recovering) and refuses a proposal. The timeout sends Recovery
// again to every peer. The model sends it only to the peers that have not
// answered; then a node whose peers all answered before a primary existed
// waits forever (simulation seed 2). A later answer replaces the earlier
// answer of the same peer, as in HandleRecoveryResponse. This form is
// provisional and goes to the owner.
TEST_CASE("a recovering node drops messages, refuses proposals and resends recovery")
{
    auto sp = make(1, storage_with_promise(ballot_of(1, 2)), 7);
    committed_messages_of(sp);
    REQUIRE(sp.handle({3, 1, konsens::prepare{ballot_of(2, 3), 0, ballot{}, 0}}));
    CHECK(committed_messages_of(sp).empty());
    CHECK(*committed_storage_of(sp).get_promise() == ballot_of(1, 2));
    REQUIRE(sp.handle_leader(ballot_of(5, 1)));
    CHECK(sp.get_state() == std::pair{role::follower, phase::recover});
    CHECK(committed_messages_of(sp).empty());
    const auto refused = sp.append(make_entry(1));
    REQUIRE(!refused);
    CHECK(refused.error() == konsens::client_errc::recovering);
    REQUIRE(sp.handle({2, 1, konsens::recovery_response{7, ballot_of(1, 2), false, ballot{}, {}, 0, false, 0, 0}}));
    REQUIRE(sp.resend_message_timeout());
    const auto rs = messages_of<konsens::recovery>(committed_messages_of(sp));
    REQUIRE(rs.size() == 2);
    CHECK(rs[0].nonce == 7);
    CHECK(rs[1].nonce == 7);
}

// Item 2 of section 2 of doc/plan-power-loss-code.md and RecoveryResponseMsg
// with PowerLoss: a recovering node answers Recovery, so that a cluster in
// which every node restarted can recover. The answer says that the node
// recovers, is no primary, and carries the durable state and the CRC32C of
// the last entry. The answer changes no state of the node.
TEST_CASE("a recovering node answers recovery and changes no state")
{
    persistent_storage s = storage_with_promise(ballot_of(3, 1));
    const std::vector two{make_entry(1), make_entry(2)};
    const std::array<konsens::storage_op, 3> ops{
        konsens::append_entries{two}, konsens::set_accepted_round{ballot_of(2, 2)}, konsens::set_decided_index{1}};
    REQUIRE(s.write_atomically(ops));
    auto sp = make(1, std::move(s), 7);
    committed_messages_of(sp);
    REQUIRE(sp.handle({2, 1, konsens::recovery{9}}));
    const auto out = committed_messages_of(sp);
    REQUIRE(out.size() == 1);
    CHECK(out[0].to == 2);
    CHECK(
        std::get<konsens::recovery_response>(out[0].msg) ==
        konsens::recovery_response{
            9, ballot_of(3, 1), false, ballot_of(2, 2), {}, 1, true, 2, konsens::compute_entry_crc(1, make_entry(2))});
    CHECK(sp.recovering());
    CHECK(sp.get_state() == std::pair{role::follower, phase::recover});
    CHECK(sp.get_promise() == ballot_of(3, 1));
    CHECK(sp.get_accepted_idx() == 2);
    CHECK(sp.get_decided_idx() == 1);
    CHECK(*committed_storage_of(sp).get_entries(0, 2) == two);
}

// Run 3 of model/results/20261021T081623Z-power-loss-rollback-S2.md, steps
// 7 to 14. Node 1 holds v1 at ballot 3.3 and has decided it. It restarts.
// Node 2 is still the primary of the old ballot 2.2 with v2. Node 3 rolled
// back and answers as a recovering node with no promise. If the answer of
// node 3 counted, ballot 2.2 would be the highest of two answers, node 1
// would take v2 over its decided v1, and S2 would break. RecoveryComplete
// counts only the answers with recovering = FALSE, so node 1 waits.
TEST_CASE("the answer of a recovering node does not count in the normal recovery")
{
    persistent_storage s = storage_with_promise(ballot_of(3, 3));
    const std::vector v1{make_entry(1)};
    const std::array<konsens::storage_op, 3> ops{
        konsens::append_entries{v1}, konsens::set_accepted_round{ballot_of(3, 3)}, konsens::set_decided_index{1}};
    REQUIRE(s.write_atomically(ops));
    auto sp = make(1, std::move(s), 7);
    committed_messages_of(sp);
    REQUIRE(sp.handle({2, 1,
                       konsens::recovery_response{7,
                                                  ballot_of(2, 2),
                                                  true,
                                                  ballot_of(2, 2),
                                                  {make_entry(2)},
                                                  0,
                                                  false,
                                                  1,
                                                  konsens::compute_entry_crc(0, make_entry(2))}}));
    REQUIRE(sp.handle({3, 1, konsens::recovery_response{7, ballot{}, false, ballot{}, {}, 0, true, 0, 0}}));
    CHECK(sp.recovering());
    CHECK(sp.get_accepted_round() == ballot_of(3, 3));
    CHECK(decided_of(sp) == v1);
    CHECK(messages_of<konsens::prepare_req>(committed_messages_of(sp)).empty());
}

// The answers are kept one per peer: a later answer of a peer replaces its
// earlier answer. Node 2 first answers as the primary of an old ballot,
// node 3 answers with a higher promise, so node 2 is not the primary of the
// highest ballot. Node 2 then answers as the primary of a new ballot, and
// recovery completes with that primary. A primary among the answers keeps
// the power-loss path closed.
TEST_CASE("handle_recovery_response: a later answer of a peer replaces its earlier answer")
{
    auto sp = make(1, storage_with_promise(ballot_of(1, 2)), 7);
    committed_messages_of(sp);
    REQUIRE(
        sp.handle({2, 1, konsens::recovery_response{7, ballot_of(1, 2), true, ballot_of(1, 2), {}, 0, false, 0, 0}}));
    REQUIRE(sp.handle({3, 1, konsens::recovery_response{7, ballot_of(1, 3), false, ballot{}, {}, 0, false, 0, 0}}));
    CHECK(sp.recovering());
    REQUIRE(
        sp.handle({2, 1, konsens::recovery_response{7, ballot_of(2, 2), true, ballot_of(2, 2), {}, 0, false, 0, 0}}));
    CHECK(!sp.recovering());
    CHECK(sp.get_promise() == ballot_of(2, 2));
}

// HandleRecovery and RecoveryResponseMsg: the leader in Accept is the
// primary and sends its promise, accepted round, whole log and decided
// index, with the nonce of the request. A follower sends the same state
// with no log (item 1 of section 2 of doc/plan-power-loss-code.md: the code
// sends it always, the model only with PowerLoss). Each answer carries
// accepted_idx and the CRC32C of the last entry, and says that the node
// does not recover.
TEST_CASE("handle_recovery: the primary sends its state and log, a follower its state")
{
    auto a = make(1);
    auto b = make(2);
    auto c = make(3);
    const std::array<sp_t*, 3> nodes{&a, &b, &c};
    REQUIRE(c.handle_leader(ballot_of(1, 3)));
    deliver_all(nodes);
    REQUIRE(c.append(make_entry(1)));
    deliver_all(nodes);
    const auto crc = konsens::compute_entry_crc(0, make_entry(1));
    REQUIRE(c.handle({2, 3, konsens::recovery{4}}));
    auto out = committed_messages_of(c);
    REQUIRE(out.size() == 1);
    CHECK(out[0].to == 2);
    CHECK(std::get<konsens::recovery_response>(out[0].msg) ==
          konsens::recovery_response{4, ballot_of(1, 3), true, ballot_of(1, 3), {make_entry(1)}, 1, false, 1, crc});
    REQUIRE(a.handle({2, 1, konsens::recovery{4}}));
    out = committed_messages_of(a);
    REQUIRE(out.size() == 1);
    CHECK(std::get<konsens::recovery_response>(out[0].msg) ==
          konsens::recovery_response{4, ballot_of(1, 3), false, ballot_of(1, 3), {}, 1, false, 1, crc});
}

// IfVotes in HandleRecovery: a node that does not vote does not answer.
// Here the node stopped voting after a read of its storage failed inside a
// batch: the CRC32C of its last entry does not match the bytes on the disk.
TEST_CASE("handle_recovery: a node that does not vote does not answer")
{
    auto sp = make(1, storage_with_flipped_last_entry());
    REQUIRE(!sp.handle({2, 1, konsens::recovery{4}}));
    REQUIRE(!sp.votes());
    CHECK(!sp.handle({2, 1, konsens::recovery{5}}));
    CHECK(committed_messages_of(sp).empty());
}

// HandleRecoveryResponse: an answer with another nonce is ignored, because
// it answers an earlier start. RecoveryComplete needs f + 1 answers from
// peers and one primary. With three nodes, f + 1 answers are the answers of
// every peer, and with no primary among them the power-loss path completes
// (see the tests of that path), so this test gives one answer only.
TEST_CASE("handle_recovery_response: an old nonce is ignored and a primary is needed")
{
    auto sp = make(1, storage_with_promise(ballot_of(1, 2)), 7);
    committed_messages_of(sp);
    const konsens::recovery_response primary{7, ballot_of(1, 2), true, ballot_of(1, 2), {make_entry(1)}, 1, false, 1,
                                             0};
    auto old = primary;
    old.nonce = 6;
    REQUIRE(sp.handle({2, 1, old}));
    REQUIRE(sp.handle({3, 1, konsens::recovery_response{7, ballot_of(1, 2), false, ballot{}, {}, 0, false, 0, 0}}));
    CHECK(sp.recovering());
    CHECK(committed_messages_of(sp).empty());
    REQUIRE(sp.handle({2, 1, primary}));
    CHECK(!sp.recovering());
}

// RecoveryPrimaries: the primary is an answer with the highest promise
// among the answers, from the node whose pid is the pid of that ballot. A
// primary of a lower ballot (an old leader that has not yet seen the new
// one) is not taken, and an answer that claims primary for a ballot of
// another pid is not taken either.
TEST_CASE("handle_recovery_response: only the primary of the highest ballot counts")
{
    auto sp = make(1, storage_with_promise(ballot_of(1, 2)), 7);
    committed_messages_of(sp);
    REQUIRE(sp.handle(
        {2, 1,
         konsens::recovery_response{7, ballot_of(1, 2), true, ballot_of(1, 2), {make_entry(1)}, 1, false, 1, 0}}));
    REQUIRE(sp.handle({3, 1, konsens::recovery_response{7, ballot_of(2, 3), false, ballot{}, {}, 0, false, 0, 0}}));
    CHECK(sp.recovering());
    REQUIRE(sp.handle(
        {3, 1,
         konsens::recovery_response{7, ballot_of(2, 2), true, ballot_of(2, 2), {make_entry(1)}, 1, false, 1, 0}}));
    CHECK(sp.recovering());
}

// HandleRecoveryResponse when the answers are complete: the node takes the
// log, accepted round and decided index of the primary in one write (F7),
// keeps the higher promise (its own here), goes to (Follower, Recover) and
// sends PrepareReq with that promise to each peer (F4).
TEST_CASE("handle_recovery_response: the node takes the state of the primary")
{
    persistent_storage s = storage_with_promise(ballot_of(3, 1));
    const std::vector nines{make_entry(9), make_entry(9)};
    const std::array<konsens::storage_op, 2> ops{konsens::append_entries{nines},
                                                 konsens::set_accepted_round{ballot_of(1, 1)}};
    REQUIRE(s.write_atomically(ops));
    auto sp = make(1, std::move(s), 7);
    committed_messages_of(sp);
    REQUIRE(sp.handle({3, 1, konsens::recovery_response{7, ballot_of(2, 3), false, ballot{}, {}, 0, false, 0, 0}}));
    REQUIRE(sp.handle(
        {2, 1,
         konsens::recovery_response{7, ballot_of(2, 2), true, ballot_of(2, 2), {make_entry(1)}, 1, false, 1, 0}}));
    CHECK(sp.recovering());
    REQUIRE(
        sp.handle({3, 1,
                   konsens::recovery_response{
                       7, ballot_of(2, 3), true, ballot_of(2, 3), {make_entry(1), make_entry(2)}, 1, false, 2, 0}}));
    CHECK(!sp.recovering());
    CHECK(sp.get_state() == std::pair{role::follower, phase::recover});
    CHECK(sp.get_promise() == ballot_of(3, 1));
    CHECK(sp.get_accepted_round() == ballot_of(2, 3));
    CHECK(sp.get_accepted_idx() == 2);
    CHECK(sp.get_decided_idx() == 1);
    const auto& st = committed_storage_of(sp);
    CHECK(*st.get_promise() == ballot_of(3, 1));
    CHECK(*st.get_accepted_round() == ballot_of(2, 3));
    CHECK(*st.get_decided_idx() == 1);
    CHECK(*st.get_entries(0, *st.get_log_len()) == std::vector{make_entry(1), make_entry(2)});
    const auto reqs = messages_of<konsens::prepare_req>(committed_messages_of(sp));
    REQUIRE(reqs.size() == 2);
    CHECK(reqs[0].n == ballot_of(3, 1));
    CHECK(reqs[1].n == ballot_of(3, 1));
    REQUIRE(sp.append(make_entry(5)));
}

// The node keeps the higher promise: here the promise of the primary.
TEST_CASE("handle_recovery_response: the node takes a higher promise of the primary")
{
    auto sp = make(1, storage_with_promise(ballot_of(1, 2)), 7);
    committed_messages_of(sp);
    REQUIRE(
        sp.handle({2, 1, konsens::recovery_response{7, ballot_of(4, 2), true, ballot_of(4, 2), {}, 0, false, 0, 0}}));
    REQUIRE(sp.handle({3, 1, konsens::recovery_response{7, ballot_of(4, 2), false, ballot{}, {}, 0, false, 0, 0}}));
    CHECK(!sp.recovering());
    CHECK(sp.get_promise() == ballot_of(4, 2));
    CHECK(*committed_storage_of(sp).get_promise() == ballot_of(4, 2));
}

// S7 of SequencePaxos.tla: the decided index never falls. A primary in
// Accept can hold a decided index below that of the recovering node: the
// leader of a new ballot takes a majority of promises, and the promise
// with the higher decided index can come later. Simulation seed 29 shows
// it. HandleRecoveryResponse in the model takes the decided index of the
// primary, which breaks S7 there. The node keeps the higher decided index.
// The log of the primary holds each decided entry (S2b), so the index
// stays within the log.
TEST_CASE("handle_recovery_response: the decided index does not fall")
{
    persistent_storage s = storage_with_promise(ballot_of(1, 3));
    const std::vector written{make_entry(1), make_entry(2)};
    const std::array<konsens::storage_op, 3> ops{
        konsens::append_entries{written}, konsens::set_accepted_round{ballot_of(1, 3)}, konsens::set_decided_index{2}};
    REQUIRE(s.write_atomically(ops));
    auto sp = make(1, std::move(s), 7);
    committed_messages_of(sp);
    REQUIRE(sp.handle({2, 1,
                       konsens::recovery_response{7,
                                                  ballot_of(2, 2),
                                                  true,
                                                  ballot_of(2, 2),
                                                  {make_entry(1), make_entry(2), make_entry(3)},
                                                  0,
                                                  false,
                                                  3,
                                                  0}}));
    REQUIRE(sp.handle({3, 1, konsens::recovery_response{7, ballot_of(2, 2), false, ballot{}, {}, 0, false, 0, 0}}));
    CHECK(!sp.recovering());
    CHECK(sp.get_decided_idx() == 2);
    CHECK(*committed_storage_of(sp).get_decided_idx() == 2);
    CHECK(sp.get_accepted_idx() == 3);
}

// Section 2 of doc/plan-power-loss-code.md and PowerLossComplete with
// PowerLossSource in the model: when every node restarts at once, no node
// is a primary, and the normal recovery cannot complete. With an answer of
// every peer and no primary among them, each node takes the node with the
// highest (n_accepted, accepted_idx) as its source. Node 2 holds the highest
// accepted round, node 3 the longest log of a lower round. Each node ends
// with the log of node 2, its accepted round, the highest promise and the
// highest decided index. Before this mode, the cluster waited forever.
TEST_CASE("when every node restarts, each node takes the log of the highest accepted round")
{
    const std::vector one_two{make_entry(1), make_entry(2)};
    const std::vector one_three{make_entry(1), make_entry(3)};
    const std::vector one_two_four{make_entry(1), make_entry(2), make_entry(4)};
    auto a = make(1, storage_of(ballot_of(3, 2), ballot_of(2, 3), one_two, 1), 2);
    auto b = make(2, storage_of(ballot_of(3, 2), ballot_of(3, 2), one_three, 1), 2);
    auto c = make(3, storage_of(ballot_of(2, 3), ballot_of(2, 3), one_two_four, 1), 2);
    const std::array<sp_t*, 3> nodes{&a, &b, &c};
    deliver_all(nodes);
    for (auto* n : nodes) {
        CHECK(!n->recovering());
        CHECK(n->votes());
        CHECK(log_of(*n) == one_three);
        CHECK(n->get_accepted_round() == ballot_of(3, 2));
        CHECK(n->get_promise() == ballot_of(3, 2));
        CHECK(n->get_decided_idx() == 1);
    }
}

// Item 4 of section 2 and R7: a tie of (n_accepted, accepted_idx) goes to
// the node itself. Such a node needs no state of another node: it writes
// the highest promise of all answers and its own, and goes on as after the
// normal recovery, with PrepareReq to each peer. It sends no get_state.
TEST_CASE("a node that ties with the highest answer is its own source")
{
    const std::vector one_two{make_entry(1), make_entry(2)};
    auto sp = make(1, storage_of(ballot_of(2, 2), ballot_of(2, 2), one_two, 1), 7);
    committed_messages_of(sp);
    REQUIRE(sp.handle({2, 1, answer_of(7, ballot_of(2, 2), ballot_of(2, 2), one_two, 1)}));
    CHECK(sp.recovering());
    REQUIRE(sp.handle({3, 1, answer_of(7, ballot_of(5, 3), ballot_of(1, 1), {make_entry(1)}, 0)}));
    CHECK(!sp.recovering());
    CHECK(sp.get_promise() == ballot_of(5, 3));
    CHECK(*committed_storage_of(sp).get_promise() == ballot_of(5, 3));
    CHECK(log_of(sp) == one_two);
    CHECK(sp.get_accepted_round() == ballot_of(2, 2));
    const auto out = committed_messages_of(sp);
    CHECK(messages_of<konsens::get_state>(out).empty());
    CHECK(messages_of<konsens::prepare_req>(out).size() == 2);
}

// R7 and the rule of follower.rs:13-51: two peers tie, so the lowest id is
// the source. The node holds the same accepted round as the source, so its
// get_state starts at its own accepted_idx: its entries up to there are the
// entries of the source (S5).
TEST_CASE("get_state goes to the lowest id of a tie and starts at accepted_idx with the same round")
{
    const std::vector one_two{make_entry(1), make_entry(2)};
    auto sp = make(3, storage_of(ballot_of(2, 2), ballot_of(2, 2), {make_entry(1)}, 0), 7);
    committed_messages_of(sp);
    REQUIRE(sp.handle({2, 3, answer_of(7, ballot_of(2, 2), ballot_of(2, 2), one_two, 1)}));
    REQUIRE(sp.handle({1, 3, answer_of(7, ballot_of(2, 2), ballot_of(2, 2), one_two, 1)}));
    CHECK(sp.recovering());
    const auto out = committed_messages_of(sp);
    REQUIRE(out.size() == 1);
    CHECK(out[0].to == 1);
    CHECK(std::get<konsens::get_state>(out[0].msg) == konsens::get_state{7, ballot_of(2, 2), 2, 1});
}

// The rule of follower.rs:13-51 for a lower accepted round: the entries of
// the node after its decided_idx may differ from those of the source, so
// get_state starts at decided_idx. The node takes new_state: it keeps its
// decided prefix, replaces the rest by the entries of the source, takes the
// accepted round of the source, the higher decided index and the highest
// promise, in one write.
TEST_CASE("get_state starts at decided_idx with a lower round, and new_state completes the recovery")
{
    const std::vector source{make_entry(1), make_entry(2), make_entry(3)};
    auto sp = make(3, storage_of(ballot_of(1, 3), ballot_of(1, 1), {make_entry(1), make_entry(9)}, 1), 7);
    committed_messages_of(sp);
    REQUIRE(sp.handle({1, 3, answer_of(7, ballot_of(4, 1), ballot_of(2, 2), source, 2)}));
    REQUIRE(sp.handle({2, 3, answer_of(7, ballot_of(2, 2), ballot_of(1, 1), {make_entry(1)}, 0)}));
    const auto out = committed_messages_of(sp);
    REQUIRE(out.size() == 1);
    CHECK(out[0].to == 1);
    CHECK(std::get<konsens::get_state>(out[0].msg) == konsens::get_state{7, ballot_of(2, 2), 3, 1});
    REQUIRE(sp.handle({1, 3, new_state_of(7, ballot_of(4, 1), ballot_of(2, 2), source, 2, 1)}));
    CHECK(!sp.recovering());
    CHECK(log_of(sp) == source);
    CHECK(sp.get_accepted_idx() == 3);
    CHECK(sp.get_accepted_round() == ballot_of(2, 2));
    CHECK(sp.get_decided_idx() == 2);
    CHECK(sp.get_promise() == ballot_of(4, 1));
    const auto& st = committed_storage_of(sp);
    CHECK(*st.get_accepted_round() == ballot_of(2, 2));
    CHECK(*st.get_decided_idx() == 2);
    CHECK(*st.get_promise() == ballot_of(4, 1));
    CHECK(messages_of<konsens::prepare_req>(committed_messages_of(sp)).size() == 2);
}

// Item 4 of section 2, R12: two answers with one accepted round and one
// accepted_idx hold the same last entry (S5 with AcceptedIdxIsLogLength).
// A different CRC32C is a hardware fault. The node stops voting (PAR: it
// stays silent until it is repaired) with an error value that names both
// nodes, the index and both CRCs. Pairs with accepted_idx 0 hold no entry
// and are not compared.
TEST_CASE("a different CRC32C of one last entry stops voting with the evidence")
{
    const std::vector one_two{make_entry(1), make_entry(2)};
    auto empty = make(1, storage_of(ballot_of(1, 2), ballot{}, {}, 0), 7);
    REQUIRE(empty.handle({2, 1, konsens::recovery_response{7, ballot_of(1, 2), false, ballot{}, {}, 0, true, 0, 5}}));
    REQUIRE(empty.handle({3, 1, konsens::recovery_response{7, ballot_of(1, 2), false, ballot{}, {}, 0, true, 0, 6}}));
    CHECK(empty.votes());
    CHECK(!empty.recovering());

    auto sp = make(1, storage_of(ballot_of(2, 2), ballot_of(2, 2), one_two, 1), 7);
    committed_messages_of(sp);
    auto changed = answer_of(7, ballot_of(2, 2), ballot_of(2, 2), one_two, 1);
    changed.last_crc += 1;
    REQUIRE(sp.handle({2, 1, changed}));
    const auto r = sp.handle({3, 1, answer_of(7, ballot_of(2, 2), ballot_of(1, 1), {make_entry(1)}, 0)});
    REQUIRE(!r);
    CHECK(r.error() == konsens::paxos_errc::hardware_fault);
    CHECK(r.error().category().name() == std::string_view{"konsens.paxos"});
    CHECK(r.error().message() != "Unknown paxos error");
    CHECK(!sp.votes());
    const auto fault = sp.get_hardware_fault();
    REQUIRE(fault);
    CHECK(fault->index == 2);
    CHECK(fault->pid == 2);
    CHECK(fault->crc == changed.last_crc);
    CHECK(fault->other_pid == 1);
    CHECK(fault->other_crc == last_crc_of(one_two));
    CHECK(committed_messages_of(sp).empty());
    CHECK(!sp.handle({2, 1, konsens::recovery{8}}));
    CHECK(committed_messages_of(sp).empty());
}

namespace {

// Node 3 restarts with an empty log after node 1 and node 2 answered with
// the logs below; node 1 is the source.
const std::vector<entry> source_log{make_entry(1), make_entry(2)};

sp_t waiting_for_new_state()
{
    auto sp = make(3, storage_of(ballot_of(1, 3), ballot{}, {}, 0), 7);
    committed_messages_of(sp);
    REQUIRE(sp.handle({1, 3, answer_of(7, ballot_of(2, 1), ballot_of(2, 1), source_log, 1)}));
    REQUIRE(sp.handle({2, 3, answer_of(7, ballot_of(2, 1), ballot_of(1, 1), {make_entry(1)}, 1)}));
    const auto gs = messages_of<konsens::get_state>(committed_messages_of(sp));
    REQUIRE(gs.size() == 1);
    REQUIRE(gs[0] == konsens::get_state{7, ballot_of(2, 1), 2, 0});
    return sp;
}

}

// R2: the entries of new_state start at the start index and end at the
// accepted_idx of the source. A new_state that does not cover that range
// is a fault of the source (card R1 of the model plan), not a lost message,
// so the node stops voting with the evidence.
TEST_CASE("new_state whose entries do not end at its accepted_idx stops voting")
{
    auto sp = waiting_for_new_state();
    auto ns = new_state_of(7, ballot_of(2, 1), ballot_of(2, 1), source_log, 1, 0);
    ns.entries.pop_back();
    const auto r = sp.handle({1, 3, ns});
    REQUIRE(!r);
    CHECK(r.error() == konsens::paxos_errc::hardware_fault);
    REQUIRE(sp.get_hardware_fault());
    CHECK(sp.get_hardware_fault()->other_pid == 1);
    CHECK(sp.get_hardware_fault()->index == 2);
}

// R4: the CRC32C of the last received entry must equal the last_crc of the
// source. A difference means the entry changed on the way or on the disk of
// the source, and the node does not take it.
TEST_CASE("new_state whose last entry has another CRC32C stops voting")
{
    auto sp = waiting_for_new_state();
    auto ns = new_state_of(7, ballot_of(2, 1), ballot_of(2, 1), source_log, 1, 0);
    ns.entries.back() = make_entry(9);
    const auto r = sp.handle({1, 3, ns});
    REQUIRE(!r);
    CHECK(r.error() == konsens::paxos_errc::hardware_fault);
    REQUIRE(sp.get_hardware_fault());
    CHECK(sp.get_hardware_fault()->crc == konsens::compute_entry_crc(1, make_entry(9)));
    CHECK(sp.get_hardware_fault()->other_crc == last_crc_of(source_log));
    CHECK(sp.recovering());
}

// R4 when new_state sends no entry: the start index is the accepted_idx of
// the source, so the node compares its own entry at that index.
TEST_CASE("new_state with no entry is compared with the own entry at its accepted_idx")
{
    const std::vector one_two{make_entry(1), make_entry(2)};
    const auto restart = [&one_two] {
        auto sp = make(3, storage_of(ballot_of(2, 1), ballot_of(2, 1), {make_entry(1), make_entry(5)}, 2), 7);
        committed_messages_of(sp);
        REQUIRE(sp.handle({1, 3, answer_of(7, ballot_of(3, 1), ballot_of(3, 1), one_two, 2)}));
        REQUIRE(sp.handle({2, 3, answer_of(7, ballot_of(2, 1), ballot_of(1, 1), {make_entry(1)}, 1)}));
        const auto gs = messages_of<konsens::get_state>(committed_messages_of(sp));
        REQUIRE(gs.size() == 1);
        REQUIRE(gs[0].from_idx == 2);
        return sp;
    };
    auto differs = restart();
    CHECK(!differs.handle({1, 3, new_state_of(7, ballot_of(3, 1), ballot_of(3, 1), one_two, 2, 2)}));
    CHECK(differs.get_hardware_fault()->crc == konsens::compute_entry_crc(1, make_entry(5)));

    const std::vector one_five{make_entry(1), make_entry(5)};
    auto same = restart();
    REQUIRE(same.handle({1, 3, new_state_of(7, ballot_of(3, 1), ballot_of(3, 1), one_five, 2, 2)}));
    CHECK(!same.recovering());
    CHECK(log_of(same) == one_five);
    CHECK(same.get_accepted_round() == ballot_of(3, 1));
}

// R3: the node takes new_state only from the source it asked, with the
// nonce and the start index of the current request, and only while it
// recovers. Any other new_state is a stale or a wrong message and changes
// nothing; the right one completes the recovery, and a duplicate of it
// after the recovery changes nothing either.
TEST_CASE("only the new_state of the current request is taken")
{
    auto sp = waiting_for_new_state();
    const auto ns = new_state_of(7, ballot_of(2, 1), ballot_of(2, 1), source_log, 1, 0);
    REQUIRE(sp.handle({2, 3, ns}));
    auto old = ns;
    old.nonce = 6;
    REQUIRE(sp.handle({1, 3, old}));
    auto other_start = new_state_of(7, ballot_of(2, 1), ballot_of(2, 1), source_log, 1, 1);
    REQUIRE(sp.handle({1, 3, other_start}));
    CHECK(sp.recovering());
    CHECK(committed_messages_of(sp).empty());
    REQUIRE(sp.handle({1, 3, ns}));
    CHECK(!sp.recovering());
    CHECK(log_of(sp) == source_log);
    committed_messages_of(sp);
    REQUIRE(sp.handle({1, 3, new_state_of(7, ballot_of(9, 1), ballot_of(9, 1), {make_entry(7)}, 1, 0)}));
    CHECK(log_of(sp) == source_log);
    CHECK(sp.get_promise() == ballot_of(2, 1));
    CHECK(committed_messages_of(sp).empty());
}

// R5: a lost get_state or new_state is sent again. The timeout sends
// get_state of the current request again, and Recovery to every peer, so
// that a source that became a primary in the meantime is seen by the normal
// recovery.
TEST_CASE("the timeout sends get_state again while the node waits for new_state")
{
    auto sp = waiting_for_new_state();
    REQUIRE(sp.resend_message_timeout());
    const auto out = committed_messages_of(sp);
    const auto gs = messages_of<konsens::get_state>(out);
    REQUIRE(gs.size() == 1);
    CHECK(gs[0] == konsens::get_state{7, ballot_of(2, 1), 2, 0});
    CHECK(messages_of<konsens::recovery>(out).size() == 2);
}

// R6: a source whose state changed after its answer answers get_state with
// its new values and no entries. The node replaces the answer of the source
// with these values, selects again, and asks with the new values; the old
// request is void.
TEST_CASE("a changed source gives a new selection")
{
    auto sp = waiting_for_new_state();
    const std::vector three{make_entry(1), make_entry(2), make_entry(3)};
    auto changed = new_state_of(7, ballot_of(2, 1), ballot_of(2, 1), three, 2, 0);
    changed.entries.clear();
    REQUIRE(sp.handle({1, 3, changed}));
    CHECK(sp.recovering());
    const auto gs = messages_of<konsens::get_state>(committed_messages_of(sp));
    REQUIRE(gs.size() == 1);
    CHECK(gs[0] == konsens::get_state{7, ballot_of(2, 1), 3, 0});
    REQUIRE(sp.handle({1, 3, new_state_of(7, ballot_of(2, 1), ballot_of(2, 1), three, 2, 0)}));
    CHECK(!sp.recovering());
    CHECK(log_of(sp) == three);
}

// Seed 223 of counter_stress with --power-loss 10: node 3 asked node 1
// with accepted_idx 2. Node 1 had moved to 3 and answered that request with
// its new values and no entry. Before that answer arrived, a new answer of
// node 1 to Recovery made node 3 ask again with accepted_idx 3. The old
// new_state then has the nonce, the start index and the values of the
// current request, but no entry. Section 7 R3 cannot tell it from a full
// answer, and R2 would report a hardware fault that does not exist. A
// new_state with no entry below its accepted_idx is the answer of a changed
// source to an older request, so the node drops it and waits for the answer
// to the current request.
TEST_CASE("a new_state with no entry below its accepted_idx answers an older request and is dropped")
{
    auto sp = waiting_for_new_state();
    const std::vector three{make_entry(1), make_entry(2), make_entry(3)};
    REQUIRE(sp.handle({1, 3, answer_of(7, ballot_of(2, 1), ballot_of(2, 1), three, 2)}));
    const auto gs = messages_of<konsens::get_state>(committed_messages_of(sp));
    REQUIRE(gs.size() == 1);
    REQUIRE(gs[0] == konsens::get_state{7, ballot_of(2, 1), 3, 0});
    auto stale = new_state_of(7, ballot_of(2, 1), ballot_of(2, 1), three, 2, 0);
    stale.entries.clear();
    REQUIRE(sp.handle({1, 3, stale}));
    CHECK(sp.votes());
    CHECK(sp.recovering());
    CHECK(committed_messages_of(sp).empty());
    REQUIRE(sp.handle({1, 3, new_state_of(7, ballot_of(2, 1), ballot_of(2, 1), three, 2, 0)}));
    CHECK(!sp.recovering());
    CHECK(log_of(sp) == three);
}

// R11 and item 4 of section 2: a source answers get_state whether it
// recovers or not, and changes no state. With the expected values it sends
// its entries from the start index; with other values it sends its own
// values and no entry.
TEST_CASE("a recovering node answers get_state and changes no state")
{
    const std::vector one_two{make_entry(1), make_entry(2)};
    auto sp = make(1, storage_of(ballot_of(2, 1), ballot_of(2, 1), one_two, 1), 4);
    committed_messages_of(sp);
    REQUIRE(sp.handle({3, 1, konsens::get_state{9, ballot_of(2, 1), 2, 1}}));
    auto out = committed_messages_of(sp);
    REQUIRE(out.size() == 1);
    CHECK(out[0].to == 3);
    CHECK(std::get<konsens::new_state>(out[0].msg) == new_state_of(9, ballot_of(2, 1), ballot_of(2, 1), one_two, 1, 1));
    REQUIRE(sp.handle({3, 1, konsens::get_state{9, ballot_of(2, 1), 3, 1}}));
    out = committed_messages_of(sp);
    REQUIRE(out.size() == 1);
    auto changed = new_state_of(9, ballot_of(2, 1), ballot_of(2, 1), one_two, 1, 1);
    changed.entries.clear();
    CHECK(std::get<konsens::new_state>(out[0].msg) == changed);
    CHECK(sp.recovering());
    CHECK(log_of(sp) == one_two);
    CHECK(sp.get_promise() == ballot_of(2, 1));
}

// R13: node 2 recovers. It answers get_state of node 3 with its entries,
// then completes its own recovery with node 1 as its source. Its answer
// changed none of its state, so its recovery takes the state of node 1.
TEST_CASE("a source that answered get_state completes its recovery with another source")
{
    const std::vector one_two{make_entry(1), make_entry(2)};
    const std::vector one_two_three{make_entry(1), make_entry(2), make_entry(3)};
    auto sp = make(2, storage_of(ballot_of(2, 1), ballot_of(2, 1), one_two, 1), 4);
    committed_messages_of(sp);
    REQUIRE(sp.handle({3, 2, konsens::get_state{9, ballot_of(2, 1), 2, 0}}));
    CHECK(messages_of<konsens::new_state>(committed_messages_of(sp)).size() == 1);
    REQUIRE(sp.handle({1, 2, answer_of(4, ballot_of(3, 1), ballot_of(3, 1), one_two_three, 2)}));
    REQUIRE(sp.handle({3, 2, answer_of(4, ballot_of(2, 1), ballot{}, {}, 0)}));
    const auto gs = messages_of<konsens::get_state>(committed_messages_of(sp));
    REQUIRE(gs.size() == 1);
    CHECK(gs[0] == konsens::get_state{4, ballot_of(3, 1), 3, 1});
    REQUIRE(sp.handle({1, 2, new_state_of(4, ballot_of(3, 1), ballot_of(3, 1), one_two_three, 2, 1)}));
    CHECK(!sp.recovering());
    CHECK(log_of(sp) == one_two_three);
    CHECK(sp.get_accepted_round() == ballot_of(3, 1));
}

// R13: the power-loss path also runs outside a power loss. Node 3 is the
// leader and restarts; nodes 1 and 2 are its followers in Accept, so no
// peer is a primary. Each peer answers with recovering = false and no log.
// With an answer of every peer and no primary, node 3 recovers: its log is
// as long as theirs in the same round, so it is its own source. After a new
// election, the cluster decides a new entry.
TEST_CASE("one node restarts while no peer is a primary, and the power-loss path recovers it")
{
    auto a = make(1);
    auto b = make(2);
    auto c = make(3);
    const std::array<sp_t*, 3> nodes{&a, &b, &c};
    REQUIRE(c.handle_leader(ballot_of(1, 3)));
    deliver_all(nodes);
    REQUIRE(c.append(make_entry(1)));
    deliver_all(nodes);
    c = make(3, lmdb_storage::open_copy_of(committed_storage_of(c)), 2);
    REQUIRE(c.recovering());
    deliver_all(nodes);
    CHECK(!c.recovering());
    CHECK(log_of(c) == std::vector{make_entry(1)});
    CHECK(c.get_accepted_round() == ballot_of(1, 3));
    REQUIRE(a.handle_leader(ballot_of(2, 1)));
    deliver_all(nodes);
    REQUIRE(a.append(make_entry(2)));
    deliver_all(nodes);
    for (auto* n : nodes) {
        CHECK(decided_of(*n) == std::vector{make_entry(1), make_entry(2)});
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
TEST_CASE("recovery after a rollback keeps S1")
{
    auto a = make(1);
    auto b = make(2);
    auto c = make(3);
    const std::array<sp_t*, 3> nodes{&a, &b, &c};
    REQUIRE(c.handle_leader(ballot_of(1, 3)));
    deliver_all(nodes);
    auto before = lmdb_storage::open_copy_of(committed_storage_of(b));
    REQUIRE(c.append(make_entry(1)));
    deliver_among(nodes, {false, true, true});
    REQUIRE(decided_of(c) == std::vector{make_entry(1)});
    REQUIRE(a.get_accepted_idx() == 0);
    b = make(2, std::move(before), 2);
    deliver_among(nodes, {true, true, false}, cut::all_but_recovery);
    REQUIRE(a.handle_leader(ballot_of(2, 1)));
    deliver_among(nodes, {true, true, false});
    REQUIRE(a.append(make_entry(2)));
    deliver_among(nodes, {true, true, false});
    REQUIRE(!decided_of(a).empty());
    CHECK(compatible(decided_of(a), decided_of(c)));
    CHECK(compatible(decided_of(b), decided_of(c)));
}
