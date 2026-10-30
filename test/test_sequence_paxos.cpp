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
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

#include "batches.hpp"
#include "konsens/sequence_paxos.hpp"
#include "sqlite_damage.hpp"
#include "sqlite_storage.hpp"
#include <sys/resource.h>

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

// Plan clean-stop 2.3: a first start needs a store without a promise. Many
// tests here start a node on a store with a promise, so the helper starts
// such a store as a clean start and drops its PrepareReq. The tests then
// send the messages that they check.
sp_t make(node_id pid, persistent_storage s = sqlite_storage::open_new())
{
    const auto promise = s.begin_read_only().value().get_meta().value().promise;
    if (promise == ballot{}) {
        return *sp_t::with({1, pid, peers_of(pid)}, std::move(s), first_nonce);
    }
    auto sp = *sp_t::with({1, pid, peers_of(pid), {}, konsens::start_kind::clean}, std::move(s), first_nonce);
    static_cast<void>(sp.take_outgoing_messages());
    return sp;
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
    auto s = sqlite_storage::open_new();
    sqlite_storage::write(s, [&](auto& t) { REQUIRE(t.set_promise(b)); });
    return s;
}

void deliver_among(const std::array<sp_t*, 3> nodes, const std::array<bool, 3> connected)
{
    for (bool more = true; more;) {
        more = false;
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            for (auto& m : committed_messages_of(*nodes[i])) {
                more = true;
                if (connected[i] && connected[m.to - 1]) {
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

// A storage with no room for one more page. open takes the smallest bound
// in which SQLite can create the tables of the storage, each try in a new
// directory, because a try that fails leaves a file. So the next write
// that needs a new page gives SQLITE_FULL, which the storage gives as
// no_space_on_device. The fault comes from SQLite and not from a wrapper.
persistent_storage storage_at_its_max_size()
{
    const auto page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    for (std::size_t pages = 1; pages <= largest_map_pages; ++pages) {
        auto opened =
            persistent_storage::open(sqlite_storage::new_directory(), pages * page, sqlite_storage::non_root_user_id);
        if (opened) {
            return std::move(*opened);
        }
    }
    FAIL("no bound of up to largest_map_pages pages holds the tables");
    return sqlite_storage::open_new();
}

constexpr std::size_t large_entry_size = 3000;

entry large_entry_of(const std::uint8_t fill)
{
    return {konsens::entry_kind::normal, std::vector(large_entry_size, std::byte{fill})};
}

// A storage of three large entries, accepted in round b, whose middle entry
// has a flipped byte on the disk. The step check at the start reads the
// meta record and the last entry, and does not check the middle entry. The
// first read of the middle entry fails. The test moves the WAL into the
// database file first, so that the flipped byte stands in the file that
// SQLite reads. The fault is in that file and not in a wrapper.
persistent_storage storage_with_flipped_middle_entry(const ballot b)
{
    constexpr std::uint8_t middle_fill = 0x5a;
    const auto directory = sqlite_storage::new_directory();
    {
        auto storage = sqlite_storage::open_in(directory);
        const std::vector written{large_entry_of(0x11), large_entry_of(middle_fill), large_entry_of(0x22)};
        sqlite_storage::write(storage, [&](auto& t) {
            REQUIRE(t.append_entries(written));
            REQUIRE(t.set_promise(b));
            REQUIRE(t.set_accepted_round(b));
        });
    }
    sqlite_damage::move_the_wal_into_the_file(directory);
    const auto data_file = directory / sqlite_damage::store_file_name;
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
    return sqlite_storage::open_in(directory);
}

}

// mod.rs:53-128 `with`: a node with no promise starts as (Follower, None)
// and sends nothing. A node that sent PrepareReq at the first start would
// look like a node in Phase::Recover.
TEST_CASE("a new node is a follower in phase none")
{
    auto sp = make(1);
    CHECK(sp.get_state() == std::pair{role::follower, phase::none});
    CHECK(committed_messages_of(sp).empty());
}

// mod.rs:53-128 `with`: a node that finds a promise in storage restarts in
// Phase::Recover and sends PrepareReq, the fail-recovery of Omni-Paxos.
// Plan clean-stop 2.3: this holds for a clean start only. A start without a
// clean stop retires the node.
TEST_CASE("a clean start is a follower in recover and sends prepare_req")
{
    auto sp = *sp_t::with({1, 1, peers_of(1), {}, konsens::start_kind::clean},
                          storage_with_promise(ballot_of(1, 2)), 7);
    CHECK(sp.get_state() == std::pair{role::follower, phase::recover});
    CHECK(sp.votes());
    const auto out = committed_messages_of(sp);
    const auto rs = messages_of<konsens::prepare_req>(out);
    REQUIRE(rs.size() == 2);
    CHECK(rs[0].n == ballot_of(1, 2));
}

// Plan clean-stop 2.3: a first start on a store that holds a promise would
// let the node promise twice, so the caller that states it uses the library
// wrongly. A build start on a store that is no build, and the reverse, are
// wrong uses too.
TEST_CASE("a first start with a promise and a build start without a build are wrong uses")
{
    CHECK_THROWS_AS(static_cast<void>(sp_t::with({1, 1, peers_of(1)}, storage_with_promise(ballot_of(1, 2)), 7)),
                    std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(sp_t::with({1, 1, peers_of(1), {2}, konsens::start_kind::build},
                                                 sqlite_storage::open_new(), 7)),
                    std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(sp_t::with({2, 1, peers_of(1), {2}}, sqlite_storage::open_new(), 7)),
                    std::logic_error);
}

// Plan clean-stop 2.4 and fact 4: a retired node votes never. It sends
// nothing at the start, drops each message, refuses each call, and sends
// nothing at a timeout.
TEST_CASE("a retired node sends nothing, drops each message and refuses each call")
{
    auto sp = *sp_t::with({1, 1, peers_of(1), {}, konsens::start_kind::retired},
                          storage_with_promise(ballot_of(1, 2)), 7);
    CHECK(sp.retired());
    CHECK(!sp.votes());
    CHECK(committed_messages_of(sp).empty());
    REQUIRE(sp.handle({2, 1, konsens::prepare{ballot_of(2, 2), 0, ballot{}, 0}}));
    REQUIRE(sp.handle({2, 1, konsens::prepare_req{ballot_of(2, 2)}}));
    REQUIRE(sp.resend_message_timeout());
    CHECK(committed_messages_of(sp).empty());
    CHECK(sp.refuse_if_retired().error() == konsens::client_errc::retired);
}

// leader.rs:16-60 handle_leader: the elected node writes its promise and
// sends Prepare to each peer. A lower ballot changes nothing.
TEST_CASE("handle_leader with the own ballot sends prepare")
{
    auto sp = make(3);
    REQUIRE(sp.handle_leader(ballot_of(1, 3)));
    CHECK(sp.get_state() == std::pair{role::leader, phase::prepare});
    CHECK(sqlite_storage::meta_of(committed_storage_of(sp)).promise == ballot_of(1, 3));
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
    CHECK(sqlite_storage::meta_of(committed_storage_of(sp)).promise == ballot_of(1, 3));
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
// the leader takes phase Accept and sends AcceptSync to each promised
// follower. The buffered proposals wait for the end of the step, so that
// the node can make each request a task first (plan-one-environment 2 item
// 2); sequence_paxos alone appends them then and sends them in AcceptDecide.
TEST_CASE("a majority of promises moves the leader to accept")
{
    auto sp = make(3);
    REQUIRE(sp.handle_leader(ballot_of(1, 3)));
    REQUIRE(sp.append(make_entry(7)));
    committed_messages_of(sp);
    REQUIRE(sp.handle({1, 3, konsens::promise{ballot_of(1, 3), ballot{}, 0, 0, std::nullopt, 0, 0}}));
    CHECK(sp.get_state() == std::pair{role::leader, phase::accept});
    const auto out = committed_messages_of(sp);
    CHECK(sp.get_accepted_idx() == 1);
    CHECK(sqlite_storage::meta_of(committed_storage_of(sp)).accepted_round == ballot_of(1, 3));
    const auto syncs = messages_of<konsens::accept_sync>(out);
    REQUIRE(syncs.size() == 1);
    CHECK(syncs[0].log_sync.suffix.empty());
    CHECK(syncs[0].seq_num == konsens::sequence_number{1, 1});
    const auto decides = messages_of<konsens::accept_decide>(out);
    REQUIRE(decides.size() == 1);
    CHECK(decides[0].entries == std::vector{make_entry(7)});
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
    CHECK(sqlite_storage::meta_of(committed_storage_of(sp)).decided_idx == 2);
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
// handler writes the entries of the AcceptSync in the transaction of the
// step, and they need new pages, so the write fails inside the handler.
// SQLite changes the meta record in its page, so the Prepare before it
// needs no new page. The program does not repeat the write, so a later
// commit is a wrong use, and no message of the step leaves.
TEST_CASE("a failed write inside a step throws write_error, and a later commit is a wrong use")
{
    auto sp = make(1, storage_at_its_max_size());
    REQUIRE(sp.handle({3, 1, konsens::prepare{ballot_of(1, 3), 0, ballot{}, 0}}));
    committed_messages_of(sp);
    const std::vector large{large_entry_of(0x11), large_entry_of(0x22), large_entry_of(0x33)};
    try {
        static_cast<void>(sp.handle({3, 1, konsens::accept_sync{ballot_of(1, 3), {1, 1}, 0, {large, 0}, 0}}));
        FAIL("the handler did not throw");
    } catch (const konsens::write_error& e) {
        CHECK(e.code() == std::make_error_code(std::errc::no_space_on_device));
    }
    CHECK_THROWS_AS(static_cast<void>(sp.commit()), std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(sp.take_outgoing_messages()), std::logic_error);
}

// The same failure at the commit. The WAL of the storage may not grow, so
// the write of the pages at the commit fails with EFBIG, and the store
// raises it as write_error. The Promise of the Prepare depends on the
// promise that the failed commit held, so it stays in the node, and a
// second commit is a wrong use.
TEST_CASE("a failed commit throws write_error and sends nothing")
{
    const auto directory = sqlite_storage::new_directory();
    auto sp = make(1, sqlite_storage::open_in(directory));
    REQUIRE(sp.handle({3, 1, konsens::prepare{ballot_of(1, 3), 0, ballot{}, 0}}));
    const auto file_size = std::filesystem::file_size(directory / "konsens.db-wal");
    {
        const file_size_limit limit{file_size};
        try {
            static_cast<void>(sp.commit());
            FAIL("the commit did not throw");
        } catch (const konsens::write_error& e) {
            CHECK(e.code() == std::make_error_code(std::errc::file_too_large));
        }
    }
    CHECK_THROWS_AS(static_cast<void>(sp.take_outgoing_messages()), std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(sp.commit()), std::logic_error);
}

// A pid of 0 means "no leader" in mod.rs:371-384, so a node with pid 0 is
// a wrong use and must fail loudly.
TEST_CASE("a pid of 0 is a wrong use")
{
    CHECK_THROWS_AS(static_cast<void>(sp_t::with({1, 0, {1, 2}}, sqlite_storage::open_new(), first_nonce)),
                    std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(sp_t::with({1, 1, {1, 2}}, sqlite_storage::open_new(), first_nonce)),
                    std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(sp_t::with({1, 1, {2, 2}}, sqlite_storage::open_new(), first_nonce)),
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

// F5 of plan v0.1: a first start finds the default promise, sends nothing
// and takes a proposal at once. Each node of a new cluster starts with no
// promise, so a node that waited for its peers would wait forever.
TEST_CASE("a first start does not recover")
{
    auto sp = make(1);
    CHECK(sp.refuse_if_building());
    CHECK(committed_messages_of(sp).empty());
    REQUIRE(sp.append(make_entry(1)));
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

// A node with a stored promise starts clean in Phase::Recover. A PrepareReq
// of a peer is a step that reads the storage and sends no answer, so one
// PrepareReq makes a running node that read its storage.
sp_t running_node_in(const std::filesystem::path& directory)
{
    {
        auto s = sqlite_storage::open_in(directory);
        sqlite_storage::write(s, [](auto& t) {
            REQUIRE(t.append_entries(one_two));
            REQUIRE(t.set_promise(ballot_of(1, 3)));
            REQUIRE(t.set_accepted_round(ballot_of(1, 3)));
        });
    }
    auto sp = make(1, sqlite_storage::open_in(directory));
    committed_messages_of(sp);
    REQUIRE(sp.handle({2, 1, konsens::prepare_req{ballot_of(1, 3)}}));
    REQUIRE(sp.commit());
    REQUIRE(sp.take_outgoing_messages().empty());
    return sp;
}

void check_stops_voting(sp_t& sp, const std::error_code e)
{
    CHECK(sp.handle({2, 1, konsens::prepare_req{ballot_of(1, 3)}}) == std::unexpected(e));
    CHECK(!sp.votes());
    CHECK(committed_messages_of(sp).empty());
}

std::vector<std::byte> meta_value_in(const std::filesystem::path& directory)
{
    std::vector<std::byte> value;
    sqlite_damage::change_with_sqlite(directory, [&value](sqlite3* db) { value = sqlite_damage::meta_value_of(db); });
    return value;
}

}

// Plan lmdb-only-source 2.1 item 5: the step check runs at the first read
// of each step, not only at the start. A disk that loses a write while the
// node runs is found at the next step, and the node stops voting.
TEST_CASE("a key lost while the node runs is found at the next step")
{
    const auto directory = sqlite_storage::new_directory();
    auto sp = running_node_in(directory);
    sqlite_damage::change_with_sqlite(directory, [](sqlite3* db) { sqlite_damage::delete_meta_record(db); });
    check_stops_voting(sp, make_error_code(konsens::storage_errc::missing_meta_record));
}

// Plan 2.3: the meta record holds the log length and the chain CRC of the
// last entry. A lost last entry leaves a log that ends one entry early.
TEST_CASE("a last entry lost while the node runs is found by the chain check")
{
    const auto directory = sqlite_storage::new_directory();
    auto sp = running_node_in(directory);
    sqlite_damage::change_with_sqlite(directory, [](sqlite3* db) { sqlite_damage::delete_entry(db, 1); });
    check_stops_voting(sp, make_error_code(konsens::storage_errc::missing_entry));
}

// The start-checks plan 2.4: a log that is not empty was accepted in a
// round, and no node accepts in a round above its promise. A lost promise
// leaves a meta record with an accepted round above the promise. The meta
// record of the damage comes from a storage with the same log, so its CRC
// and its chain CRC are valid.
TEST_CASE("a promise lost while the node runs is found at the next step")
{
    const auto lost = sqlite_storage::new_directory();
    {
        auto s = sqlite_storage::open_in(lost);
        sqlite_storage::write(s, [](auto& t) {
            REQUIRE(t.append_entries(one_two));
            REQUIRE(t.set_accepted_round(ballot_of(1, 3)));
        });
    }
    auto value = meta_value_in(lost);
    const auto directory = sqlite_storage::new_directory();
    auto sp = running_node_in(directory);
    sqlite_damage::change_with_sqlite(directory, [&value](sqlite3* db) { sqlite_damage::put_meta_value(db, value); });
    check_stops_voting(sp, make_error_code(konsens::paxos_errc::inconsistent_storage));
}

// Plan 2.3 item 7: the node keeps the highest values that it committed. A
// disk that gives back an older meta record, with a valid CRC, gives a
// value below that mark. A getter between two steps runs the same check
// (plan 2.1 item 3), so no value of the old record leaves the node.
TEST_CASE("a value below the mark stops the vote, and a getter between two steps runs the step check")
{
    const auto directory = sqlite_storage::new_directory();
    auto sp = make(1, sqlite_storage::open_in(directory));
    REQUIRE(sp.handle({3, 1, konsens::prepare{ballot_of(1, 3), 0, ballot{}, 0}}));
    const auto older = sqlite_storage::new_directory();
    REQUIRE(committed_storage_of(sp).copy(older));
    auto value = meta_value_in(older);
    REQUIRE(sp.handle({2, 1, konsens::prepare{ballot_of(2, 2), 0, ballot{}, 0}}));
    REQUIRE(sp.commit());
    static_cast<void>(sp.take_outgoing_messages());
    REQUIRE(sp.get_promise() == ballot_of(2, 2));
    sqlite_damage::change_with_sqlite(directory, [&value](sqlite3* db) { sqlite_damage::put_meta_value(db, value); });
    const auto inconsistent = make_error_code(konsens::paxos_errc::inconsistent_storage);
    CHECK(sp.get_promise() == std::unexpected(inconsistent));
    CHECK(sp.get_decided_idx() == std::unexpected(inconsistent));
    CHECK(sp.handle({3, 1, konsens::prepare{ballot_of(3, 3), 0, ballot{}, 0}}) == std::unexpected(inconsistent));
    CHECK(!sp.votes());
    CHECK(committed_messages_of(sp).empty());
}

// Plan 2.1 item 6: an entry whose CRC does not match is a damage of the
// step check, and the node stops voting. The start states all three entries
// as verified, and the Prepare carries no accepted round, so the Promise
// carries the log from index 0 and its step reads the middle entry.
TEST_CASE("an entry with a wrong CRC stops the vote")
{
    auto sp = *sp_t::with({1, 1, peers_of(1), {}, konsens::start_kind::clean, 3},
                          storage_with_flipped_middle_entry(ballot_of(1, 3)), first_nonce);
    committed_messages_of(sp);
    const auto r = sp.handle({2, 1, konsens::prepare{ballot_of(2, 2), 0, ballot{}, 0}});
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
    REQUIRE(sp.commit());
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
        REQUIRE(sp.commit());
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
    SUBCASE("handle_build_response")
    {
        const entry stop{konsens::entry_kind::stop_sign,
                         konsens::encode(konsens::cluster_config{2, {1, 2, 3}}).value()};
        const std::vector log{make_entry(1), stop};
        auto sp =
            *sp_t::with({2, 1, peers_of(1), {2}, konsens::start_kind::build}, sqlite_storage::open_new(), first_nonce);
        committed_messages_of(sp);
        REQUIRE(sp.handle({2, 1, konsens::build_response{first_nonce, 0, log, last_crc_of(log), 2, last_crc_of(log)}}));
        REQUIRE(!sp.building());
        CHECK(states_of(committed_storage_of(sp)) == committed_below(2, 2));
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
    REQUIRE(sp.commit());
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
    CHECK(sqlite_storage::meta_of(committed_storage_of(sp)).decided_idx == 0);
}

// Each error value has a message of its own, because a log or an operator
// reads the message and not the number.
TEST_CASE("each paxos error has its own message")
{
    const std::array codes{konsens::paxos_errc::hardware_fault, konsens::paxos_errc::inconsistent_storage,
                           konsens::paxos_errc::state_differs_from_log, konsens::paxos_errc::result_differs};
    std::vector<std::string> messages;
    for (const auto c : codes) {
        const std::error_code e = c;
        CHECK(e.category().name() == std::string_view{"konsens.paxos"});
        messages.push_back(e.message());
    }
    std::ranges::sort(messages);
    CHECK(std::ranges::adjacent_find(messages) == messages.end());
    CHECK(std::ranges::find(messages, "Unknown paxos error") == messages.end());
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
    CHECK(sqlite_storage::log_of(st) == one_two);
    CHECK(sqlite_storage::meta_of(st).decided_idx == 0);
    CHECK(states_of(st) == committed_below(0, 2));
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

entry stop_sign_entry()
{
    return {konsens::entry_kind::stop_sign, {}};
}

}

// Plan reconfiguration 2.2 and 2.4: the StopSign is the last entry of a
// configuration. After the leader holds one, it refuses a local append
// and cuts a forwarded entry, as leader.rs:264-274 drops the proposals.
TEST_CASE("a leader appends no entry after its StopSign")
{
    auto a = make(1);
    auto b = make(2);
    auto c = make(3);
    const std::array<sp_t*, 3> nodes{&a, &b, &c};
    REQUIRE(c.handle_leader(ballot_of(1, 3)));
    deliver_all(nodes);
    REQUIRE(c.append(make_entry(1)));
    REQUIRE(c.append(stop_sign_entry()));
    CHECK(c.append(make_entry(2)).error() == konsens::client_errc::reconfiguring);
    deliver_all(nodes);
    REQUIRE(a.append(make_entry(3)));
    deliver_all(nodes);
    for (auto* n : nodes) {
        CHECK(decided_of(*n) == std::vector{make_entry(1), stop_sign_entry()});
    }
    CHECK(c.append(make_entry(4)).error() == konsens::client_errc::reconfiguring);
}

// A leader in its prepare phase buffers what a follower forwards. At the
// flush it keeps the entries up to the first StopSign and cuts the rest.
TEST_CASE("a leader cuts the forwarded entries after a StopSign at the flush")
{
    auto a = make(1);
    auto b = make(2);
    auto c = make(3);
    const std::array<sp_t*, 3> nodes{&a, &b, &c};
    REQUIRE(c.handle_leader(ballot_of(1, 3)));
    REQUIRE(c.handle(
        {1, 3, konsens::proposal_forward{{make_entry(1), stop_sign_entry(), make_entry(2), stop_sign_entry()}}}));
    deliver_all(nodes);
    for (auto* n : nodes) {
        CHECK(decided_of(*n) == std::vector{make_entry(1), stop_sign_entry()});
    }
}

// A new leader reads the end of its synced log. A StopSign there ends the
// configuration for the new leader too.
TEST_CASE("a new leader that syncs a StopSign appends nothing after it")
{
    auto a = make(1);
    auto b = make(2);
    auto c = make(3);
    const std::array<sp_t*, 3> nodes{&a, &b, &c};
    REQUIRE(c.handle_leader(ballot_of(1, 3)));
    deliver_all(nodes);
    REQUIRE(c.append(stop_sign_entry()));
    deliver_all(nodes);
    REQUIRE(b.handle_leader(ballot_of(2, 2)));
    deliver_all(nodes);
    REQUIRE(b.get_state() == std::pair{role::leader, phase::accept});
    CHECK(b.append(make_entry(1)).error() == konsens::client_errc::reconfiguring);
    REQUIRE(a.append(make_entry(2)));
    deliver_all(nodes);
    for (auto* n : nodes) {
        CHECK(decided_of(*n) == std::vector{stop_sign_entry()});
    }
}

// Plan reconfiguration 2.5: a StopSign that no majority accepted is an
// entry like any other. A new leader without it removes it by its accept
// sync, and the configuration goes on.
TEST_CASE("a StopSign that is not decided is removed by a new leader")
{
    auto a = make(1);
    auto b = make(2);
    auto c = make(3);
    const std::array<sp_t*, 3> nodes{&a, &b, &c};
    REQUIRE(c.handle_leader(ballot_of(1, 3)));
    deliver_all(nodes);
    REQUIRE(c.append(stop_sign_entry()));
    deliver_among(nodes, {false, false, true});
    REQUIRE(b.handle_leader(ballot_of(2, 2)));
    deliver_among(nodes, {true, true, false});
    REQUIRE(b.get_state() == std::pair{role::leader, phase::accept});
    REQUIRE(b.append(make_entry(1)));
    REQUIRE(c.reconnected(2));
    deliver_all(nodes);
    for (auto* n : nodes) {
        CHECK(decided_of(*n) == std::vector{make_entry(1)});
    }
}

namespace {

sp_t make_synced_by_the_caller(const node_id pid)
{
    return *sp_t::with({1, pid, peers_of(pid), {}, konsens::start_kind::first, 0, true}, sqlite_storage::open_new(),
                       first_nonce);
}

void report_the_sync(sp_t& sp)
{
    if (sp.get_last_sync_number() > sp.get_synced_sync_number()) {
        REQUIRE(sp.the_log_is_synced(sp.get_last_sync_number()));
        static_cast<void>(sp.commit());
    }
}

}

// R1 of section 15 of doc/plan-write-path.md: the synced length of the log
// counts only the prefix that is durable and still the same. Node 3 syncs
// its entry 1 of round 1, and node 2 then accepts entry 2 at the same index
// in round 2. When node 3 leads again, it adopts entry 2 over its synced
// entry 1. With node 1 alone beside it, it may decide only after its own
// sync of entry 2; a synced length that kept the old entry 1 would let it
// count its cache as a vote.
TEST_CASE("a new leader that adopts a suffix over synced entries decides only after its sync")
{
    auto a = make_synced_by_the_caller(1);
    auto b = make_synced_by_the_caller(2);
    auto c = make_synced_by_the_caller(3);
    const std::array<sp_t*, 3> nodes{&a, &b, &c};
    REQUIRE(c.handle_leader(ballot_of(1, 3)));
    deliver_all(nodes);
    REQUIRE(c.append(make_entry(1)));
    deliver_among(nodes, {false, false, true});
    report_the_sync(c);
    CHECK(c.get_synced_len() == 1);
    CHECK(c.get_decided_idx().value() == 0);
    REQUIRE(b.handle_leader(ballot_of(2, 2)));
    deliver_among(nodes, {true, true, false});
    REQUIRE(b.get_state() == std::pair{role::leader, phase::accept});
    REQUIRE(b.append(make_entry(2)));
    deliver_among(nodes, {true, true, false});
    report_the_sync(a);
    REQUIRE(c.handle_leader(ballot_of(3, 3)));
    deliver_among(nodes, {true, false, true});
    REQUIRE(c.get_state() == std::pair{role::leader, phase::accept});
    CHECK(c.get_synced_len() == 0);
    CHECK(c.get_decided_idx().value() == 0);
    report_the_sync(c);
    deliver_among(nodes, {true, false, true});
    CHECK(c.get_decided_idx().value() == 1);
    CHECK(decided_of(c) == std::vector{make_entry(2)});
}

// R2: a sync report may make several indexes chosen at once; the leader
// decides the highest, the majority-th largest accepted index, and not
// only the index of the last Accepted.
TEST_CASE("a sync report decides the highest chosen index")
{
    auto a = make_synced_by_the_caller(1);
    auto b = make_synced_by_the_caller(2);
    auto c = make_synced_by_the_caller(3);
    const std::array<sp_t*, 3> nodes{&a, &b, &c};
    REQUIRE(c.handle_leader(ballot_of(1, 3)));
    deliver_among(nodes, {true, false, true});
    report_the_sync(c);
    deliver_among(nodes, {true, false, true});
    REQUIRE(c.append(make_entry(1)));
    deliver_among(nodes, {true, false, true});
    REQUIRE(c.append(make_entry(2)));
    deliver_among(nodes, {true, false, true});
    CHECK(c.get_decided_idx().value() == 0);
    report_the_sync(c);
    CHECK(c.get_decided_idx().value() == 2);
}

// With sync_by_the_caller off a step with log writes takes no sync number:
// the commit syncs, as before.
TEST_CASE("without sync_by_the_caller no step takes a sync number")
{
    auto a = make(1);
    auto b = make(2);
    auto c = make(3);
    const std::array<sp_t*, 3> nodes{&a, &b, &c};
    REQUIRE(c.handle_leader(ballot_of(1, 3)));
    deliver_all(nodes);
    REQUIRE(c.append(make_entry(1)));
    deliver_all(nodes);
    for (auto* n : nodes) {
        CHECK(n->get_last_sync_number() == 0);
    }
    CHECK(c.get_decided_idx().value() == 1);
}
