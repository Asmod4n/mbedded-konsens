#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
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

#include <sys/resource.h>
#include "batches.hpp"
#include "konsens/sequence_paxos.hpp"
#include "lmdb_damage.hpp"
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

template <typename S>
const S& committed_storage_of(konsens::sequence_paxos<S>& sp)
{
    static_cast<void>(batches::commit_and_sync(sp));
    return sp.get_internal_storage();
}

template <typename S>
std::expected<std::vector<entry>, std::error_code> committed_decided_suffix_of(konsens::sequence_paxos<S>& sp,
                                                                               const std::uint64_t from_idx)
{
    static_cast<void>(batches::commit_and_sync(sp));
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
    lmdb_storage::write(s, [&](auto& t) { REQUIRE(t.set_promise(b)); });
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
    lmdb_storage::write(s, [&](auto& t) {
        REQUIRE(t.append_entries(log));
        REQUIRE(t.set_promise(promise));
        REQUIRE(t.set_accepted_round(n_accepted));
        REQUIRE(t.set_decided_index(decided_idx));
    });
    return s;
}

std::uint32_t chain_of(const std::vector<entry>& log, const std::uint64_t length)
{
    std::uint32_t chain = 0;
    for (std::uint64_t i = 0; i < length; ++i) {
        chain = konsens::compute_chain_crc(chain, i, log.at(i));
    }
    return chain;
}

std::uint32_t last_crc_of(const std::vector<entry>& log)
{
    return chain_of(log, log.size());
}

konsens::recovery_response answer_of(const std::uint64_t nonce, const ballot promise, const ballot n_accepted,
                                     const std::vector<entry>& log, const std::uint64_t decided_idx)
{
    return {nonce,       promise, false,      n_accepted,       {},
            decided_idx, true,    log.size(), last_crc_of(log), chain_of(log, decided_idx)};
}

konsens::new_state new_state_of(const std::uint64_t nonce, const ballot promise, const ballot n_accepted,
                                const std::vector<entry>& log, const std::uint64_t decided_idx,
                                const std::uint64_t from_idx)
{
    return {nonce,
            promise,
            n_accepted,
            log.size(),
            decided_idx,
            last_crc_of(log),
            from_idx,
            std::vector(std::next(log.begin(), static_cast<std::ptrdiff_t>(from_idx)), log.end()),
            chain_of(log, decided_idx)};
}

std::vector<entry> log_of(sp_t& sp)
{
    return lmdb_storage::log_of(committed_storage_of(sp));
}

// The limit of the file size holds for the test program while the object
// lives. SIGXFSZ is ignored, so that a write beyond the limit fails with
// EFBIG and does not end the program.
class file_size_limit {
public:
    explicit file_size_limit(const std::uintmax_t bytes)
    {
        REQUIRE(getrlimit(RLIMIT_FSIZE, &old_) == 0);
        old_handler_ = std::signal(SIGXFSZ, SIG_IGN);
        const rlimit limit{static_cast<rlim_t>(bytes), old_.rlim_max};
        REQUIRE(setrlimit(RLIMIT_FSIZE, &limit) == 0);
    }
    file_size_limit(const file_size_limit&) = delete;
    file_size_limit& operator=(const file_size_limit&) = delete;
    ~file_size_limit()
    {
        setrlimit(RLIMIT_FSIZE, &old_);
        std::signal(SIGXFSZ, old_handler_);
    }

private:
    rlimit old_{};
    void (*old_handler_)(int) = nullptr;
};

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

constexpr std::size_t large_entry_size = 3000;

entry large_entry_of(const std::uint8_t fill)
{
    return {konsens::entry_kind::normal, std::vector(large_entry_size, std::byte{fill})};
}

// A storage of three large entries, accepted in round b, whose middle entry
// has a flipped byte on the disk. A large entry stands on pages of its own,
// so the step check at the start, which reads the meta record and the last
// entry, does not meet the flipped page. The first read of the middle entry
// fails. The fault is in the file that LMDB reads and not in a wrapper.
persistent_storage storage_with_flipped_middle_entry(const ballot b)
{
    constexpr std::uint8_t middle_fill = 0x5a;
    const auto directory = lmdb_storage::new_directory();
    {
        auto storage = lmdb_storage::open_in(directory);
        const std::vector written{large_entry_of(0x11), large_entry_of(middle_fill), large_entry_of(0x22)};
        lmdb_storage::write(storage, [&](auto& t) {
            REQUIRE(t.append_entries(written));
            REQUIRE(t.set_promise(b));
            REQUIRE(t.set_accepted_round(b));
        });
    }
    const auto data_file = directory / "data.mdb";
    std::vector<char> contents;
    {
        std::ifstream in{data_file, std::ios::binary};
        contents.assign(std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{});
    }
    const std::string marked(large_entry_size / 2, static_cast<char>(middle_fill));
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
    CHECK(lmdb_storage::meta_of(committed_storage_of(sp)).promise == ballot_of(1, 3));
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
    CHECK(lmdb_storage::meta_of(committed_storage_of(sp)).promise == ballot_of(1, 3));
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
    REQUIRE(sp.handle({2, 1, konsens::decide{ballot_of(1, 2), {1, 1}, 0, 0}}));
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
    REQUIRE(sp.handle({1, 3, konsens::promise{ballot_of(1, 3), ballot{}, 0, 0, std::nullopt, 0, 0}}));
    CHECK(sp.get_state() == std::pair{role::leader, phase::accept});
    CHECK(sp.get_accepted_idx() == 1);
    CHECK(lmdb_storage::meta_of(committed_storage_of(sp)).accepted_round == ballot_of(1, 3));
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
    REQUIRE(sp.handle({3, 1,
                       konsens::accept_sync{ballot_of(1, 3),
                                            {1, 1},
                                            2,
                                            {{make_entry(1), make_entry(2)}, 0},
                                            chain_of({make_entry(1), make_entry(2)}, 2)}}));
    CHECK(sp.get_decided_idx() == 2);
    REQUIRE(sp.handle({3, 1, konsens::prepare{ballot_of(2, 3), 2, ballot_of(1, 3), 2}}));
    REQUIRE(sp.handle({3, 1, konsens::accept_sync{ballot_of(2, 3), {1, 1}, 1, {{}, 2}, 0}}));
    CHECK(sp.get_decided_idx() == 2);
    CHECK(lmdb_storage::meta_of(committed_storage_of(sp)).decided_idx == 2);
}

// follower.rs:230-238 handle_sequence_num: a gap in the sequence numbers
// means a lost AcceptDecide. The follower goes to Recover and asks the
// leader with PrepareReq, and it does not append the entries.
TEST_CASE("a gap in the sequence numbers starts recovery")
{
    auto sp = make(1);
    REQUIRE(sp.handle({3, 1, konsens::prepare{ballot_of(1, 3), 0, ballot{}, 0}}));
    REQUIRE(sp.handle({3, 1, konsens::accept_sync{ballot_of(1, 3), {1, 1}, 0, {{}, 0}, 0}}));
    committed_messages_of(sp);
    REQUIRE(sp.handle({3, 1, konsens::accept_decide{ballot_of(1, 3), {1, 3}, 0, {make_entry(1)}, 0}}));
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
    REQUIRE(sp.handle({3, 1, konsens::accept_sync{ballot_of(1, 3), {1, 1}, 0, {{}, 0}, 0}}));
    const paxos_message ad{3, 1, konsens::accept_decide{ballot_of(1, 3), {1, 2}, 0, {make_entry(1)}, 0}};
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
// of the machine, and it is raised with the error code of the store. The
// handler writes the promise of the Prepare in the transaction of the step,
// so the write fails inside the handler. The program does not repeat the
// write, so a later commit is a wrong use, and no message of the step
// leaves.
TEST_CASE("a failed write inside a step throws write_error, and a later commit is a wrong use")
{
    auto sp = make(1, storage_with_full_map());
    try {
        static_cast<void>(sp.handle({3, 1, konsens::prepare{ballot_of(1, 3), 0, ballot{}, 0}}));
        FAIL("the handler did not throw");
    } catch (const konsens::write_error& e) {
        CHECK(e.code() == std::make_error_code(std::errc::no_space_on_device));
    }
    CHECK_THROWS_AS(static_cast<void>(batches::commit_and_sync(sp)), std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(sp.take_outgoing_messages()), std::logic_error);
}

// The same failure at the commit. The file of the storage may not grow,
// so the write of the pages at the commit fails with EFBIG, and the store
// raises it as sync_error. The Promise of the Prepare depends on the
// promise that the failed commit held, so it stays in the node, and a
// second commit is a wrong use.
TEST_CASE("a failed commit throws write_error and sends nothing")
{
    const auto directory = lmdb_storage::new_directory();
    auto sp = make(1, lmdb_storage::open_in(directory));
    REQUIRE(sp.handle({3, 1, konsens::prepare{ballot_of(1, 3), 0, ballot{}, 0}}));
    const auto file_size = std::filesystem::file_size(directory / "data.mdb");
    {
        const file_size_limit limit{file_size};
        try {
            static_cast<void>(batches::commit_and_sync(sp));
            FAIL("the commit did not throw");
        } catch (const konsens::sync_error& e) {
            CHECK(e.code() == std::make_error_code(std::errc::file_too_large));
        }
    }
    CHECK_THROWS_AS(static_cast<void>(sp.take_outgoing_messages()), std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(batches::commit_and_sync(sp)), std::logic_error);
}

// A pid of 0 means "no leader" in mod.rs:371-384, so a node with pid 0 is
// a wrong use and must fail loudly.
TEST_CASE("a pid of 0 is a wrong use")
{
    CHECK_THROWS_AS(static_cast<void>(sp_t::with({0, {1, 2}}, lmdb_storage::open_new(), first_nonce)),
                    std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(sp_t::with({1, {1, 2}}, lmdb_storage::open_new(), first_nonce)),
                    std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(sp_t::with({1, {2, 2}}, lmdb_storage::open_new(), first_nonce)),
                    std::logic_error);
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
    CHECK(lmdb_storage::meta_of(committed_storage_of(sp)).promise == ballot_of(1, 2));
    REQUIRE(sp.handle_leader(ballot_of(5, 1)));
    CHECK(sp.get_state() == std::pair{role::follower, phase::recover});
    CHECK(committed_messages_of(sp).empty());
    const auto refused = sp.append(make_entry(1));
    REQUIRE(!refused);
    CHECK(refused.error() == konsens::client_errc::recovering);
    REQUIRE(sp.handle({2, 1, konsens::recovery_response{7, ballot_of(1, 2), false, ballot{}, {}, 0, false, 0, 0, 0}}));
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
    lmdb_storage::write(s, [&](auto& t) {
        REQUIRE(t.append_entries(two));
        REQUIRE(t.set_accepted_round(ballot_of(2, 2)));
        REQUIRE(t.set_decided_index(1));
    });
    auto sp = make(1, std::move(s), 7);
    committed_messages_of(sp);
    REQUIRE(sp.handle({2, 1, konsens::recovery{9}}));
    const auto out = committed_messages_of(sp);
    REQUIRE(out.size() == 1);
    CHECK(out[0].to == 2);
    CHECK(std::get<konsens::recovery_response>(out[0].msg) ==
          konsens::recovery_response{
              9, ballot_of(3, 1), false, ballot_of(2, 2), {}, 1, true, 2, last_crc_of(two), chain_of(two, 1)});
    CHECK(sp.recovering());
    CHECK(sp.get_state() == std::pair{role::follower, phase::recover});
    CHECK(sp.get_promise() == ballot_of(3, 1));
    CHECK(sp.get_accepted_idx() == 2);
    CHECK(sp.get_decided_idx() == 1);
    CHECK(lmdb_storage::log_of(committed_storage_of(sp)) == two);
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
    lmdb_storage::write(s, [&](auto& t) {
        REQUIRE(t.append_entries(v1));
        REQUIRE(t.set_accepted_round(ballot_of(3, 3)));
        REQUIRE(t.set_decided_index(1));
    });
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
                                                  chain_of({make_entry(2)}, 1),
                                                  0}}));
    REQUIRE(sp.handle({3, 1, konsens::recovery_response{7, ballot{}, false, ballot{}, {}, 0, true, 0, 0, 0}}));
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
    REQUIRE(sp.handle(
        {2, 1, konsens::recovery_response{7, ballot_of(1, 2), true, ballot_of(1, 2), {}, 0, false, 0, 0, 0}}));
    REQUIRE(sp.handle({3, 1, konsens::recovery_response{7, ballot_of(1, 3), false, ballot{}, {}, 0, false, 0, 0, 0}}));
    CHECK(sp.recovering());
    REQUIRE(sp.handle(
        {2, 1, konsens::recovery_response{7, ballot_of(2, 2), true, ballot_of(2, 2), {}, 0, false, 0, 0, 0}}));
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
    const auto crc = last_crc_of({make_entry(1)});
    REQUIRE(c.handle({2, 3, konsens::recovery{4}}));
    auto out = committed_messages_of(c);
    REQUIRE(out.size() == 1);
    CHECK(out[0].to == 2);
    CHECK(
        std::get<konsens::recovery_response>(out[0].msg) ==
        konsens::recovery_response{4, ballot_of(1, 3), true, ballot_of(1, 3), {make_entry(1)}, 1, false, 1, crc, crc});
    REQUIRE(a.handle({2, 1, konsens::recovery{4}}));
    out = committed_messages_of(a);
    REQUIRE(out.size() == 1);
    CHECK(std::get<konsens::recovery_response>(out[0].msg) ==
          konsens::recovery_response{4, ballot_of(1, 3), false, ballot_of(1, 3), {}, 1, false, 1, crc, crc});
}

// IfVotes in HandleRecovery: a node that does not vote does not answer.
// Here the node stopped voting after a read of its storage failed inside a
// step: a GetState asks for the log from index 0, and a page of the middle
// entry does not match its checksum on the disk.
TEST_CASE("handle_recovery: a node that does not vote does not answer")
{
    const auto b = ballot_of(1, 3);
    auto sp = make(1, storage_with_flipped_middle_entry(b));
    committed_messages_of(sp);
    REQUIRE(!sp.handle({2, 1, konsens::get_state{4, b, 3, 0}}));
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
    const konsens::recovery_response primary{7,
                                             ballot_of(1, 2),
                                             true,
                                             ballot_of(1, 2),
                                             {make_entry(1)},
                                             1,
                                             false,
                                             1,
                                             last_crc_of({make_entry(1)}),
                                             last_crc_of({make_entry(1)})};
    auto old = primary;
    old.nonce = 6;
    REQUIRE(sp.handle({2, 1, old}));
    REQUIRE(sp.handle({3, 1, konsens::recovery_response{7, ballot_of(1, 2), false, ballot{}, {}, 0, false, 0, 0, 0}}));
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
    REQUIRE(sp.handle({2, 1,
                       konsens::recovery_response{7,
                                                  ballot_of(1, 2),
                                                  true,
                                                  ballot_of(1, 2),
                                                  {make_entry(1)},
                                                  1,
                                                  false,
                                                  1,
                                                  last_crc_of({make_entry(1)}),
                                                  last_crc_of({make_entry(1)})}}));
    REQUIRE(sp.handle({3, 1, konsens::recovery_response{7, ballot_of(2, 3), false, ballot{}, {}, 0, false, 0, 0, 0}}));
    CHECK(sp.recovering());
    REQUIRE(sp.handle({3, 1,
                       konsens::recovery_response{7,
                                                  ballot_of(2, 2),
                                                  true,
                                                  ballot_of(2, 2),
                                                  {make_entry(1)},
                                                  1,
                                                  false,
                                                  1,
                                                  last_crc_of({make_entry(1)}),
                                                  last_crc_of({make_entry(1)})}}));
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
    lmdb_storage::write(s, [&](auto& t) {
        REQUIRE(t.append_entries(nines));
        REQUIRE(t.set_accepted_round(ballot_of(1, 1)));
    });
    auto sp = make(1, std::move(s), 7);
    committed_messages_of(sp);
    REQUIRE(sp.handle({3, 1, konsens::recovery_response{7, ballot_of(2, 3), false, ballot{}, {}, 0, false, 0, 0, 0}}));
    REQUIRE(sp.handle({2, 1,
                       konsens::recovery_response{7,
                                                  ballot_of(2, 2),
                                                  true,
                                                  ballot_of(2, 2),
                                                  {make_entry(1)},
                                                  1,
                                                  false,
                                                  1,
                                                  last_crc_of({make_entry(1)}),
                                                  last_crc_of({make_entry(1)})}}));
    CHECK(sp.recovering());
    REQUIRE(sp.handle({3, 1,
                       konsens::recovery_response{7,
                                                  ballot_of(2, 3),
                                                  true,
                                                  ballot_of(2, 3),
                                                  {make_entry(1), make_entry(2)},
                                                  1,
                                                  false,
                                                  2,
                                                  last_crc_of({make_entry(1), make_entry(2)}),
                                                  chain_of({make_entry(1), make_entry(2)}, 1)}}));
    CHECK(!sp.recovering());
    CHECK(sp.get_state() == std::pair{role::follower, phase::recover});
    CHECK(sp.get_promise() == ballot_of(3, 1));
    CHECK(sp.get_accepted_round() == ballot_of(2, 3));
    CHECK(sp.get_accepted_idx() == 2);
    CHECK(sp.get_decided_idx() == 1);
    const auto& st = committed_storage_of(sp);
    CHECK(lmdb_storage::meta_of(st).promise == ballot_of(3, 1));
    CHECK(lmdb_storage::meta_of(st).accepted_round == ballot_of(2, 3));
    CHECK(lmdb_storage::meta_of(st).decided_idx == 1);
    CHECK(lmdb_storage::log_of(st) == std::vector{make_entry(1), make_entry(2)});
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
    REQUIRE(sp.handle(
        {2, 1, konsens::recovery_response{7, ballot_of(4, 2), true, ballot_of(4, 2), {}, 0, false, 0, 0, 0}}));
    REQUIRE(sp.handle({3, 1, konsens::recovery_response{7, ballot_of(4, 2), false, ballot{}, {}, 0, false, 0, 0, 0}}));
    CHECK(!sp.recovering());
    CHECK(sp.get_promise() == ballot_of(4, 2));
    CHECK(lmdb_storage::meta_of(committed_storage_of(sp)).promise == ballot_of(4, 2));
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
    lmdb_storage::write(s, [&](auto& t) {
        REQUIRE(t.append_entries(written));
        REQUIRE(t.set_accepted_round(ballot_of(1, 3)));
        REQUIRE(t.set_decided_index(2));
    });
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
                                                  0,
                                                  0}}));
    REQUIRE(sp.handle({3, 1, konsens::recovery_response{7, ballot_of(2, 2), false, ballot{}, {}, 0, false, 0, 0, 0}}));
    CHECK(!sp.recovering());
    CHECK(sp.get_decided_idx() == 2);
    CHECK(lmdb_storage::meta_of(committed_storage_of(sp)).decided_idx == 2);
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
    CHECK(lmdb_storage::meta_of(committed_storage_of(sp)).promise == ballot_of(5, 3));
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
    CHECK(lmdb_storage::meta_of(st).accepted_round == ballot_of(2, 2));
    CHECK(lmdb_storage::meta_of(st).decided_idx == 2);
    CHECK(lmdb_storage::meta_of(st).promise == ballot_of(4, 1));
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
    REQUIRE(
        empty.handle({2, 1, konsens::recovery_response{7, ballot_of(1, 2), false, ballot{}, {}, 0, true, 0, 5, 0}}));
    REQUIRE(
        empty.handle({3, 1, konsens::recovery_response{7, ballot_of(1, 2), false, ballot{}, {}, 0, true, 0, 6, 0}}));
    CHECK(empty.votes());
    CHECK(!empty.recovering());

    auto sp = make(1, storage_of(ballot_of(2, 2), ballot_of(2, 2), one_two, 1), 7);
    committed_messages_of(sp);
    auto changed = answer_of(7, ballot_of(2, 2), ballot_of(2, 2), one_two, 1);
    changed.last_chain_crc += 1;
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
    CHECK(fault->crc == changed.last_chain_crc);
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
    CHECK(sp.get_hardware_fault()->crc == chain_of({source_log.at(0), make_entry(9)}, 2));
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
    CHECK(differs.get_hardware_fault()->crc == chain_of({make_entry(1), make_entry(5)}, 2));

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

namespace {

std::vector<konsens::entry_state> states_of(const persistent_storage& s)
{
    const auto t = s.begin_read_only();
    REQUIRE(t.has_value());
    const auto m = t->get_meta();
    REQUIRE(m.has_value());
    std::vector<konsens::entry_state> states;
    for (std::uint64_t i = 0; i < m->log_len; ++i) {
        const auto state = t->get_entry_state(i);
        REQUIRE(state.has_value());
        REQUIRE(state->has_value());
        states.push_back(**state);
    }
    return states;
}

std::vector<konsens::entry_state> committed_below(const std::uint64_t decided_idx, const std::uint64_t log_len)
{
    std::vector<konsens::entry_state> states(log_len, konsens::entry_state::in_work);
    std::fill_n(states.begin(), decided_idx, konsens::entry_state::committed);
    return states;
}

const std::vector<entry> one_two{make_entry(1), make_entry(2)};

// A node with a stored promise restarts in recovery and answers Recovery,
// so one Recovery is a step of a running node that reads its storage.
sp_t running_node_in(const std::filesystem::path& directory)
{
    {
        auto s = lmdb_storage::open_in(directory);
        lmdb_storage::write(s, [](auto& t) {
            REQUIRE(t.append_entries(one_two));
            REQUIRE(t.set_promise(ballot_of(1, 3)));
            REQUIRE(t.set_accepted_round(ballot_of(1, 3)));
        });
    }
    auto sp = make(1, lmdb_storage::open_in(directory), 7);
    committed_messages_of(sp);
    REQUIRE(sp.handle({2, 1, konsens::recovery{4}}));
    REQUIRE(batches::commit_and_sync(sp));
    REQUIRE(sp.take_outgoing_messages().size() == 1);
    return sp;
}

void check_stops_voting(sp_t& sp, const std::error_code e)
{
    CHECK(sp.handle({2, 1, konsens::recovery{5}}) == std::unexpected(e));
    CHECK(!sp.votes());
    CHECK(committed_messages_of(sp).empty());
}

std::vector<std::byte> meta_value_in(const std::filesystem::path& directory)
{
    std::vector<std::byte> value;
    lmdb_damage::change_with_lmdb(directory, [&value](MDB_txn* txn, MDB_dbi, const MDB_dbi meta) {
        value = lmdb_damage::meta_value_of(txn, meta);
    });
    return value;
}

}

// Plan lmdb-only-source 2.1 item 5: the step check runs at the first read
// of each step, not only at the start. A disk that loses a write while the
// node runs is found at the next step, and the node stops voting.
TEST_CASE("a key lost while the node runs is found at the next step")
{
    const auto directory = lmdb_storage::new_directory();
    auto sp = running_node_in(directory);
    lmdb_damage::change_in_another_process(
        directory, [](MDB_txn* txn, MDB_dbi, const MDB_dbi meta) { lmdb_damage::delete_meta_record(txn, meta); });
    check_stops_voting(sp, make_error_code(konsens::storage_errc::missing_meta_record));
}

// Plan 2.3: the meta record holds the log length and the chain CRC of the
// last entry. A lost last entry leaves a log that ends one entry early.
TEST_CASE("a last entry lost while the node runs is found by the chain check")
{
    const auto directory = lmdb_storage::new_directory();
    auto sp = running_node_in(directory);
    lmdb_damage::change_in_another_process(directory, [](MDB_txn* txn, const MDB_dbi log, MDB_dbi) {
        lmdb_damage::delete_key(txn, log, lmdb_damage::key_of(1));
    });
    check_stops_voting(sp, make_error_code(konsens::storage_errc::missing_entry));
}

// The start-checks plan 2.4: a log that is not empty was accepted in a
// round, and no node accepts in a round above its promise. A lost promise
// leaves a meta record with an accepted round above the promise. The meta
// record of the damage comes from a storage with the same log, so its CRC
// and its chain CRC are valid.
TEST_CASE("a promise lost while the node runs is found at the next step")
{
    const auto lost = lmdb_storage::new_directory();
    {
        auto s = lmdb_storage::open_in(lost);
        lmdb_storage::write(s, [](auto& t) {
            REQUIRE(t.append_entries(one_two));
            REQUIRE(t.set_accepted_round(ballot_of(1, 3)));
        });
    }
    auto value = meta_value_in(lost);
    const auto directory = lmdb_storage::new_directory();
    auto sp = running_node_in(directory);
    lmdb_damage::change_in_another_process(directory, [&value](MDB_txn* txn, MDB_dbi, const MDB_dbi meta) {
        lmdb_damage::put_meta_value(txn, meta, value);
    });
    check_stops_voting(sp, make_error_code(konsens::paxos_errc::inconsistent_storage));
}

// Plan 2.3 item 7: the node keeps the highest values that it committed. A
// disk that gives back an older meta record, with a valid CRC, gives a
// value below that mark. A getter between two steps runs the same check
// (plan 2.1 item 3), so no value of the old record leaves the node.
TEST_CASE("a value below the mark stops the vote, and a getter between two steps runs the step check")
{
    const auto directory = lmdb_storage::new_directory();
    auto sp = make(1, lmdb_storage::open_in(directory));
    REQUIRE(sp.handle({3, 1, konsens::prepare{ballot_of(1, 3), 0, ballot{}, 0}}));
    const auto older = lmdb_storage::new_directory();
    REQUIRE(committed_storage_of(sp).copy(older));
    auto value = meta_value_in(older);
    REQUIRE(sp.handle({2, 1, konsens::prepare{ballot_of(2, 2), 0, ballot{}, 0}}));
    REQUIRE(batches::commit_and_sync(sp));
    static_cast<void>(sp.take_outgoing_messages());
    REQUIRE(sp.get_promise() == ballot_of(2, 2));
    lmdb_damage::change_in_another_process(directory, [&value](MDB_txn* txn, MDB_dbi, const MDB_dbi meta) {
        lmdb_damage::put_meta_value(txn, meta, value);
    });
    const auto inconsistent = make_error_code(konsens::paxos_errc::inconsistent_storage);
    CHECK(sp.get_promise() == std::unexpected(inconsistent));
    CHECK(sp.get_decided_idx() == std::unexpected(inconsistent));
    CHECK(sp.handle({3, 1, konsens::prepare{ballot_of(3, 3), 0, ballot{}, 0}}) == std::unexpected(inconsistent));
    CHECK(!sp.votes());
    CHECK(committed_messages_of(sp).empty());
}

// Plan 2.1 item 6: a page whose checksum does not match is a damage of the
// step check, and the node stops voting.
TEST_CASE("a page with a wrong checksum stops the vote")
{
    const auto b = ballot_of(1, 3);
    auto sp = make(1, storage_with_flipped_middle_entry(b));
    committed_messages_of(sp);
    const auto r = sp.handle({2, 1, konsens::get_state{4, b, 3, 0}});
    REQUIRE(!r);
    CHECK((r.error() == konsens::storage_errc::crc_mismatch || r.error().category() != konsens::storage_category()));
    CHECK(!sp.votes());
    CHECK(committed_messages_of(sp).empty());
}

// Plan 2.1 item 2: the messages of a step leave only after the commit of
// its transaction, because each of them depends on its writes.
TEST_CASE("no message leaves before the commit of its step")
{
    auto sp = make(1);
    REQUIRE(sp.handle({3, 1, konsens::prepare{ballot_of(1, 3), 0, ballot{}, 0}}));
    CHECK_THROWS_AS(static_cast<void>(sp.take_outgoing_messages()), std::logic_error);
    REQUIRE(batches::commit_and_sync(sp));
    CHECK(messages_of<konsens::promise>(sp.take_outgoing_messages()).size() == 1);
}

// Plan 2.1 item 7: the node keeps no copy of its Promise. A resend builds
// it again from the storage, so it carries the stored values.
TEST_CASE("a resent promise carries the stored values")
{
    auto sp = make(1);
    REQUIRE(sp.handle({3, 1, konsens::prepare{ballot_of(1, 3), 0, ballot{}, 0}}));
    REQUIRE(sp.handle({3, 1, konsens::accept_sync{ballot_of(1, 3), {1, 1}, 0, {one_two, 0}, 0}}));
    committed_messages_of(sp);
    REQUIRE(sp.handle({2, 1, konsens::prepare{ballot_of(2, 2), 0, ballot{}, 0}}));
    const auto sent = messages_of<konsens::promise>(committed_messages_of(sp));
    REQUIRE(sent.size() == 1);
    const konsens::promise stored{ballot_of(2, 2),     ballot_of(1, 3), 0, 2, konsens::log_sync{one_two, 0}, 0,
                                  last_crc_of(one_two)};
    CHECK(sent[0] == stored);
    REQUIRE(sp.resend_message_timeout());
    const auto resent = messages_of<konsens::promise>(committed_messages_of(sp));
    REQUIRE(resent.size() == 1);
    CHECK(resent[0] == stored);
}

// Plan 2.2 item 2: each step that raises the decided index sets the state
// committed on the entries below it, in the same transaction. One test for
// each writer of the decided index in sequence_paxos.
TEST_CASE("each writer of the decided index sets the state of its entries")
{
    SUBCASE("handle_majority_promises")
    {
        auto sp = make(3);
        REQUIRE(sp.handle_leader(ballot_of(1, 3)));
        committed_messages_of(sp);
        REQUIRE(sp.handle({1, 3,
                           konsens::promise{ballot_of(1, 3), ballot_of(1, 1), 1, 2, konsens::log_sync{one_two, 0},
                                            chain_of(one_two, 1), last_crc_of(one_two)}}));
        CHECK(states_of(committed_storage_of(sp)) == committed_below(1, 2));
    }
    SUBCASE("accept_sync")
    {
        auto sp = make(1);
        REQUIRE(sp.handle({3, 1, konsens::prepare{ballot_of(1, 3), 0, ballot{}, 0}}));
        REQUIRE(
            sp.handle({3, 1, konsens::accept_sync{ballot_of(1, 3), {1, 1}, 1, {one_two, 0}, chain_of(one_two, 1)}}));
        CHECK(states_of(committed_storage_of(sp)) == committed_below(1, 2));
    }
    SUBCASE("accept_decide")
    {
        auto sp = make(1);
        REQUIRE(sp.handle({3, 1, konsens::prepare{ballot_of(1, 3), 0, ballot{}, 0}}));
        REQUIRE(sp.handle({3, 1, konsens::accept_sync{ballot_of(1, 3), {1, 1}, 0, {{}, 0}, 0}}));
        REQUIRE(sp.handle({3, 1, konsens::accept_decide{ballot_of(1, 3), {1, 2}, 1, one_two, chain_of(one_two, 1)}}));
        CHECK(states_of(committed_storage_of(sp)) == committed_below(1, 2));
    }
    SUBCASE("decide")
    {
        auto sp = make(1);
        REQUIRE(sp.handle({3, 1, konsens::prepare{ballot_of(1, 3), 0, ballot{}, 0}}));
        REQUIRE(sp.handle({3, 1, konsens::accept_sync{ballot_of(1, 3), {1, 1}, 0, {one_two, 0}, 0}}));
        REQUIRE(batches::commit_and_sync(sp));
        CHECK(states_of(committed_storage_of(sp)) == committed_below(0, 2));
        REQUIRE(sp.handle({3, 1, konsens::decide{ballot_of(1, 3), {1, 2}, 2, chain_of(one_two, 2)}}));
        CHECK(states_of(committed_storage_of(sp)) == committed_below(2, 2));
    }
    SUBCASE("accepted at the leader")
    {
        auto sp = make(3);
        REQUIRE(sp.handle_leader(ballot_of(1, 3)));
        committed_messages_of(sp);
        REQUIRE(sp.handle({1, 3, konsens::promise{ballot_of(1, 3), ballot{}, 0, 0, std::nullopt, 0, 0}}));
        REQUIRE(sp.append(make_entry(1)));
        REQUIRE(sp.append(make_entry(2)));
        committed_messages_of(sp);
        REQUIRE(sp.handle({1, 3, konsens::accepted{ballot_of(1, 3), 2, last_crc_of(one_two)}}));
        CHECK(states_of(committed_storage_of(sp)) == committed_below(2, 2));
    }
    SUBCASE("complete_recovery")
    {
        persistent_storage s = storage_with_promise(ballot_of(3, 1));
        auto sp = make(1, std::move(s), 7);
        committed_messages_of(sp);
        REQUIRE(
            sp.handle({3, 1, konsens::recovery_response{7, ballot_of(2, 3), false, ballot{}, {}, 0, false, 0, 0, 0}}));
        REQUIRE(sp.handle({2, 1,
                           konsens::recovery_response{7,
                                                      ballot_of(2, 2),
                                                      true,
                                                      ballot_of(2, 2),
                                                      {make_entry(1)},
                                                      1,
                                                      false,
                                                      1,
                                                      last_crc_of({make_entry(1)}),
                                                      last_crc_of({make_entry(1)})}}));
        REQUIRE(sp.handle({3, 1,
                           konsens::recovery_response{7, ballot_of(2, 3), true, ballot_of(2, 3), one_two, 1, false, 2,
                                                      last_crc_of(one_two), chain_of(one_two, 1)}}));
        REQUIRE(!sp.recovering());
        CHECK(states_of(committed_storage_of(sp)) == committed_below(1, 2));
    }
}

namespace {

// A leader of ballot (1, 3) with the entries 1 and 2 appended, accepted by
// node 1 in its Promise, and not yet decided.
sp_t leader_with_two_entries()
{
    auto sp = make(3);
    REQUIRE(sp.handle_leader(ballot_of(1, 3)));
    committed_messages_of(sp);
    REQUIRE(sp.handle({1, 3, konsens::promise{ballot_of(1, 3), ballot{}, 0, 0, std::nullopt, 0, 0}}));
    REQUIRE(sp.append(make_entry(1)));
    REQUIRE(sp.append(make_entry(2)));
    committed_messages_of(sp);
    return sp;
}

}

// Plan lmdb-only-source 2.5 item 2: the leader counts an Accepted only when
// its chain CRC equals the chain CRC of the own log at that index. A
// follower whose log differs below the index does not help to decide.
TEST_CASE("a leader does not decide on an accepted with another chain CRC")
{
    auto sp = leader_with_two_entries();
    const auto other = chain_of({make_entry(9), make_entry(2)}, 2);
    REQUIRE(other != last_crc_of(one_two));
    REQUIRE(sp.handle({1, 3, konsens::accepted{ballot_of(1, 3), 2, other}}));
    CHECK(sp.get_decided_idx() == 0);
    CHECK(messages_of<konsens::decide>(committed_messages_of(sp)).empty());
    REQUIRE(sp.handle({1, 3, konsens::accepted{ballot_of(1, 3), 2, last_crc_of(one_two)}}));
    CHECK(sp.get_decided_idx() == 2);
    const auto decides = messages_of<konsens::decide>(committed_messages_of(sp));
    REQUIRE(decides.size() == 1);
    CHECK(decides[0].decided_chain_crc == last_crc_of(one_two));
}

// Plan 2.5 item 3 and the decision of the owner (2026-10-24 21:27): a
// follower whose chain CRC at the decided index of the leader differs has a
// damage. It stops voting with the evidence, and raises nothing.
TEST_CASE("a decide with another chain CRC stops the vote")
{
    auto sp = make(1);
    REQUIRE(sp.handle({3, 1, konsens::prepare{ballot_of(1, 3), 0, ballot{}, 0}}));
    REQUIRE(sp.handle({3, 1, konsens::accept_sync{ballot_of(1, 3), {1, 1}, 0, {one_two, 0}, 0}}));
    REQUIRE(batches::commit_and_sync(sp));
    static_cast<void>(sp.take_outgoing_messages());
    const auto other = chain_of({make_entry(9), make_entry(2)}, 2);
    const auto r = sp.handle({3, 1, konsens::decide{ballot_of(1, 3), {1, 2}, 2, other}});
    REQUIRE(!r);
    CHECK(r.error() == konsens::paxos_errc::hardware_fault);
    CHECK(!sp.votes());
    REQUIRE(sp.get_hardware_fault());
    CHECK(sp.get_hardware_fault()->index == 2);
    CHECK(sp.get_hardware_fault()->crc == last_crc_of(one_two));
    CHECK(sp.get_hardware_fault()->other_pid == 3);
    CHECK(sp.get_hardware_fault()->other_crc == other);
    CHECK(lmdb_storage::meta_of(committed_storage_of(sp)).decided_idx == 0);
}

// Plan 2.5 item 4: the leader takes the decided index of a promise only
// when the promise carries the chain CRC that the synced log of the leader
// has at that index. Else it keeps its own decided index (VerifyPromise of
// the model).
TEST_CASE("the majority of promises does not decide on another chain CRC")
{
    auto sp = make(3);
    REQUIRE(sp.handle_leader(ballot_of(1, 3)));
    committed_messages_of(sp);
    const auto other = chain_of({make_entry(9)}, 1);
    REQUIRE(sp.handle({1, 3,
                       konsens::promise{ballot_of(1, 3), ballot_of(1, 1), 1, 2, konsens::log_sync{one_two, 0}, other,
                                        last_crc_of(one_two)}}));
    CHECK(sp.get_state() == std::pair{role::leader, phase::accept});
    const auto& st = committed_storage_of(sp);
    CHECK(lmdb_storage::log_of(st) == one_two);
    CHECK(lmdb_storage::meta_of(st).decided_idx == 0);
    CHECK(states_of(st) == committed_below(0, 2));
}

// Plan 2.2 item 5: recovery compares the chain CRC of the last entry. Two
// logs with the same last entry and another entry below it have the same
// entry CRC at the last index and another chain CRC, so the restarting
// node finds the difference.
TEST_CASE("recovery compares the chain CRC, not the entry CRC")
{
    const std::vector nine_two{make_entry(9), make_entry(2)};
    REQUIRE(konsens::compute_entry_crc(1, one_two.back()) == konsens::compute_entry_crc(1, nine_two.back()));
    const auto answer_from = [](const node_id pid, const std::vector<entry>& log) {
        auto sp = make(pid, storage_of(ballot_of(1, 1), ballot_of(1, 1), log, 0), 5);
        committed_messages_of(sp);
        REQUIRE(sp.handle({3, pid, konsens::recovery{7}}));
        const auto out = messages_of<konsens::recovery_response>(committed_messages_of(sp));
        REQUIRE(out.size() == 1);
        return out[0];
    };
    const auto a = answer_from(1, one_two);
    const auto b = answer_from(2, nine_two);
    CHECK(a.last_chain_crc == last_crc_of(one_two));
    CHECK(b.last_chain_crc == last_crc_of(nine_two));
    auto sp = make(3, storage_with_promise(ballot_of(1, 1)), 7);
    committed_messages_of(sp);
    REQUIRE(sp.handle({1, 3, a}));
    const auto r = sp.handle({2, 3, b});
    REQUIRE(!r);
    CHECK(r.error() == konsens::paxos_errc::hardware_fault);
    REQUIRE(sp.get_hardware_fault());
    CHECK(sp.get_hardware_fault()->index == 2);
}

// Plan 2.3 item 6: on one connection a peer sends its ballots in order, and
// in one ballot its accepted indexes in order. A message with a lower value
// shows that the disk of the peer went back. The node refuses it and keeps
// the proof. A new connection starts with no values.
TEST_CASE("a message of a peer that stepped back is refused and reported")
{
    auto sp = make(1);
    REQUIRE(sp.handle({3, 1, konsens::prepare{ballot_of(2, 3), 0, ballot{}, 0}}));
    committed_messages_of(sp);
    REQUIRE(sp.handle({3, 1, konsens::prepare{ballot_of(1, 3), 0, ballot{}, 0}}));
    CHECK(committed_messages_of(sp).empty());
    const auto proofs = sp.take_peer_step_backs();
    REQUIRE(proofs.size() == 1);
    CHECK(proofs[0].pid == 3);
    CHECK(proofs[0].seen.n == ballot_of(2, 3));
    CHECK(proofs[0].received.n == ballot_of(1, 3));
    REQUIRE(sp.reconnected(3));
    committed_messages_of(sp);
    REQUIRE(sp.handle({3, 1, konsens::prepare{ballot_of(1, 3), 0, ballot{}, 0}}));
    CHECK(sp.take_peer_step_backs().empty());

    auto leader = leader_with_two_entries();
    REQUIRE(leader.handle({1, 3, konsens::accepted{ballot_of(1, 3), 2, last_crc_of(one_two)}}));
    committed_messages_of(leader);
    REQUIRE(leader.handle({1, 3, konsens::accepted{ballot_of(1, 3), 1, chain_of(one_two, 1)}}));
    const auto accepted_proofs = leader.take_peer_step_backs();
    REQUIRE(accepted_proofs.size() == 1);
    CHECK(accepted_proofs[0].seen.accepted_idx == 2);
    CHECK(accepted_proofs[0].received.accepted_idx == 1);
}

namespace {

constexpr std::size_t record_tail_size = 13;
constexpr std::size_t chain_crc_offset_in_tail = 4;

// The chain CRC of a record is not under its entry CRC, so a flipped bit
// there leaves an entry that reads without error and a chain that does not
// follow.
void flip_chain_crc(MDB_txn* txn, const MDB_dbi log, const std::uint64_t index)
{
    const auto key_bytes = lmdb_damage::key_of(index);
    MDB_val key{key_bytes.size(), const_cast<std::byte*>(key_bytes.data())};
    MDB_val value{};
    REQUIRE(mdb_get(txn, log, &key, &value) == MDB_SUCCESS);
    const std::span stored(static_cast<const std::byte*>(value.mv_data), value.mv_size);
    std::vector<std::byte> changed(stored.begin(), stored.end());
    changed.at(changed.size() - record_tail_size + chain_crc_offset_in_tail) ^= std::byte{1};
    MDB_val replaced{changed.size(), changed.data()};
    REQUIRE(mdb_put(txn, log, &key, &replaced, 0) == MDB_SUCCESS);
}

std::vector<entry> numbered_log(const std::size_t length, const std::uint8_t first)
{
    std::vector<entry> log;
    for (std::size_t i = 0; i < length; ++i) {
        log.push_back(make_entry(static_cast<std::uint8_t>(first + i)));
    }
    return log;
}

// A node whose meta record is lost before its start: `with` gives a node
// that stopped voting with the damage.
sp_t damaged_node_with(const std::vector<entry>& log, const std::uint64_t decided_idx)
{
    const auto directory = lmdb_storage::new_directory();
    {
        auto s = lmdb_storage::open_in(directory);
        lmdb_storage::write(s, [&](auto& t) {
            REQUIRE(t.append_entries(log));
            REQUIRE(t.set_promise(ballot_of(1, 3)));
            REQUIRE(t.set_accepted_round(ballot_of(1, 3)));
            REQUIRE(t.set_decided_index(decided_idx));
        });
    }
    lmdb_damage::change_with_lmdb(
        directory, [](MDB_txn* txn, MDB_dbi, const MDB_dbi meta) { lmdb_damage::delete_meta_record(txn, meta); });
    auto sp = make(1, lmdb_storage::open_in(directory), 7);
    REQUIRE(sp.get_error() == make_error_code(konsens::storage_errc::missing_meta_record));
    REQUIRE_FALSE(sp.votes());
    return sp;
}

konsens::repair_response probe_answer_of(const konsens::repair_request& r, const std::vector<entry>& log)
{
    std::vector<std::uint32_t> chains;
    for (const auto length : r.lengths) {
        if (length <= log.size()) {
            chains.push_back(chain_of(log, length));
        }
    }
    return {r.nonce, r.round, log.size(), chains};
}

konsens::repair_entries entries_answer_of(const konsens::get_repair_entries& g, const std::vector<entry>& log,
                                          const std::uint64_t decided_idx)
{
    return {g.nonce,
            g.from_idx,
            std::vector(std::next(log.begin(), static_cast<std::ptrdiff_t>(g.from_idx)), log.end()),
            ballot_of(2, 2),
            ballot_of(2, 2),
            decided_idx,
            last_crc_of(log)};
}

// Peer 2 answers each probe from `source` until the node asks for the
// entries. The result is the index that the node asks for.
std::uint64_t search_against(sp_t& sp, const std::vector<entry>& source)
{
    for (;;) {
        const auto out = committed_messages_of(sp);
        const auto fetches = messages_of<konsens::get_repair_entries>(out);
        if (!fetches.empty()) {
            return fetches.front().from_idx;
        }
        const auto probes = messages_of<konsens::repair_request>(out);
        REQUIRE_FALSE(probes.empty());
        CHECK(probes.front().lengths.size() <= konsens::repair_probe_count);
        REQUIRE(sp.handle({2, 1, probe_answer_of(probes.front(), source)}));
    }
}

}

// Plan repair-from-peer 2.4: the chain CRC covers the whole prefix, so the
// search finds the first index where the two logs differ, for each
// position, and across more rounds than one when the gap holds more than
// repair_probe_count lengths. The node then asks only for the entries from
// that index.
TEST_CASE("the repair search finds the first index that differs")
{
    constexpr std::size_t length = 200;
    const auto source = numbered_log(length, 0);
    for (const std::uint64_t p : std::to_array<std::uint64_t>({0, 1, 2, 63, 64, 65, 100, 128, 198, 199, 200})) {
        INFO("first differing index ", p);
        auto own = source;
        if (p < length) {
            own.at(p) = make_entry(255);
        } else {
            own.push_back(make_entry(255));
        }
        auto sp = damaged_node_with(own, 0);
        REQUIRE(sp.start_repair(9));
        CHECK(sp.repairing());
        CHECK(search_against(sp, source) == p);
    }
    auto shorter = damaged_node_with(numbered_log(50, 0), 0);
    REQUIRE(shorter.start_repair(9));
    CHECK(search_against(shorter, source) == 50);
}

// Plan repair-from-peer 2.3: the start check finds the lost meta record.
// The chain CRC at index 12 lies below the last entry, so only the scan
// finds it: the scan ends at the first entry whose chain does not follow. The search starts below it, and the repair
// takes only the entries from the first index that differs. The new meta record holds the values of the source, and the
// next step passes the step check with the mark set from them.
TEST_CASE("a repair takes only the entries that differ, and sets the mark")
{
    const auto source = numbered_log(20, 0);
    const auto directory = lmdb_storage::new_directory();
    {
        auto s = lmdb_storage::open_in(directory);
        lmdb_storage::write(s, [&](auto& t) {
            REQUIRE(t.append_entries(source));
            REQUIRE(t.set_promise(ballot_of(1, 3)));
            REQUIRE(t.set_accepted_round(ballot_of(1, 3)));
            REQUIRE(t.set_decided_index(18));
        });
    }
    lmdb_damage::change_with_lmdb(directory, [](MDB_txn* txn, const MDB_dbi log, const MDB_dbi meta) {
        flip_chain_crc(txn, log, 12);
        lmdb_damage::delete_meta_record(txn, meta);
    });
    auto sp = make(1, lmdb_storage::open_in(directory), 7);
    REQUIRE(sp.get_error() == make_error_code(konsens::storage_errc::missing_meta_record));
    REQUIRE(sp.start_repair(9));
    CHECK(search_against(sp, source) == 12);
    REQUIRE(sp.handle({2, 1, entries_answer_of({9, 12}, source, 15)}));
    CHECK_FALSE(sp.repairing());
    CHECK(sp.recovering());
    const auto out = committed_messages_of(sp);
    const auto recoveries = messages_of<konsens::recovery>(out);
    REQUIRE(recoveries.size() == 2);
    CHECK(recoveries[0].nonce == 9);
    CHECK(log_of(sp) == source);
    CHECK(sp.get_promise() == ballot_of(2, 2));
    CHECK(sp.get_accepted_round() == ballot_of(2, 2));
    CHECK(sp.get_decided_idx() == 15);
    CHECK(states_of(sp.get_internal_storage()) == committed_below(15, 20));
}

// Plan repair-from-peer 2.7: the node folds the chain CRC from its own
// verified prefix over the received entries. An answer whose entries do not
// give its last chain CRC is refused, and the node writes nothing.
TEST_CASE("a repair refuses an entry whose chain CRC does not follow")
{
    auto sp = damaged_node_with(one_two, 0);
    REQUIRE(sp.start_repair(9));
    const std::vector source{make_entry(1), make_entry(2), make_entry(3)};
    CHECK(search_against(sp, source) == 2);
    auto wrong = entries_answer_of({9, 2}, source, 2);
    wrong.entries.front() = make_entry(4);
    REQUIRE(sp.handle({2, 1, wrong}));
    CHECK(sp.repairing());
    CHECK(committed_messages_of(sp).empty());
    CHECK(sp.get_promise() == std::unexpected(make_error_code(konsens::storage_errc::missing_meta_record)));
    auto wrong_start = entries_answer_of({9, 2}, source, 2);
    wrong_start.from_idx = 1;
    REQUIRE(sp.handle({2, 1, wrong_start}));
    CHECK(sp.repairing());
    REQUIRE(sp.handle({2, 1, entries_answer_of({9, 2}, source, 2)}));
    CHECK_FALSE(sp.repairing());
    CHECK(log_of(sp) == source);
}

// Plan repair-from-peer 2.10: a peer answers a probe only when it votes,
// does not recover and does not repair. A node that repairs ignores every
// other message, and start_repair on a node without a damage is a wrong
// use.
TEST_CASE("only a peer that votes answers a probe")
{
    auto voting = make(2, storage_of(ballot{}, ballot{}, {}, 0));
    REQUIRE(voting.handle({1, 2, konsens::repair_request{9, 1, {1}}}));
    const auto answers = messages_of<konsens::repair_response>(committed_messages_of(voting));
    REQUIRE(answers.size() == 1);
    CHECK(answers[0].log_len == 0);
    CHECK(answers[0].chain_crcs.empty());
    CHECK_THROWS_AS(static_cast<void>(voting.start_repair(9)), std::logic_error);

    auto too_many = konsens::repair_request{9, 1, std::vector<std::uint64_t>(konsens::repair_probe_count + 1, 1)};
    REQUIRE(voting.handle({1, 2, too_many}));
    CHECK(committed_messages_of(voting).empty());

    auto recovering = make(2, storage_with_promise(ballot_of(1, 3)));
    committed_messages_of(recovering);
    REQUIRE(recovering.recovering());
    REQUIRE(recovering.handle({1, 2, konsens::repair_request{9, 1, {1}}}));
    CHECK(committed_messages_of(recovering).empty());

    auto repairing = damaged_node_with(one_two, 0);
    REQUIRE(repairing.start_repair(9));
    committed_messages_of(repairing);
    REQUIRE(repairing.handle({2, 1, konsens::repair_request{9, 1, {1}}}));
    REQUIRE(repairing.handle({2, 1, konsens::recovery{4}}));
    CHECK(committed_messages_of(repairing).empty());
    CHECK(repairing.append(make_entry(5)) == std::unexpected(make_error_code(konsens::client_errc::recovering)));
}

// Plan repair-from-peer 2.5 and 2.9: a node whose disk lost its meta record
// while it ran repairs from the cluster, recovers, and votes again with the
// log of the cluster.
TEST_CASE("a follower that found a damage repairs from the cluster and votes again")
{
    const auto directory = lmdb_storage::new_directory();
    auto a = make(1, lmdb_storage::open_in(directory));
    auto b = make(2);
    auto c = make(3);
    const std::array<sp_t*, 3> nodes{&a, &b, &c};
    REQUIRE(c.handle_leader(ballot_of(1, 3)));
    deliver_all(nodes);
    for (const auto& e : numbered_log(100, 0)) {
        REQUIRE(c.append(e));
    }
    deliver_all(nodes);
    lmdb_damage::change_in_another_process(
        directory, [](MDB_txn* txn, MDB_dbi, const MDB_dbi meta) { lmdb_damage::delete_meta_record(txn, meta); });
    CHECK(a.handle({3, 1, konsens::recovery{5}}) ==
          std::unexpected(make_error_code(konsens::storage_errc::missing_meta_record)));
    REQUIRE(a.start_repair(11));
    deliver_all(nodes);
    CHECK(a.votes());
    CHECK_FALSE(a.repairing());
    CHECK_FALSE(a.recovering());
    CHECK(log_of(a) == log_of(c));
    REQUIRE(c.append(make_entry(200)));
    deliver_all(nodes);
    CHECK(a.get_decided_idx() == 101);
}
