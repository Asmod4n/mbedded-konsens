#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <sys/resource.h>

#include <crc32c/crc32c.h>

#include "konsens/persistent_storage.hpp"
#include "temporary_directory.hpp"

static_assert(konsens::storage<konsens::persistent_storage>);

namespace {

// The session that runs these tests runs as root. The open function takes
// the effective user id as a value, so that the tests can give a user id
// that the refusal of root does not hold.
constexpr uid_t non_root_user_id = 1000;
constexpr uid_t root_user_id = 0;
constexpr std::size_t map_size = std::size_t{16} << 20;
constexpr std::size_t tiny_map_size = std::size_t{64} << 10;
constexpr std::size_t large_entry_size = std::size_t{64} << 10;

std::vector<std::byte> bytes_of(const std::string_view text)
{
    std::vector<std::byte> result;
    std::ranges::transform(text, std::back_inserter(result), [](const char c) { return static_cast<std::byte>(c); });
    return result;
}

konsens::entry normal_entry(const std::string_view text)
{
    return {konsens::entry_kind::normal, bytes_of(text)};
}

konsens::persistent_storage open_storage(const std::filesystem::path& directory)
{
    auto opened = konsens::persistent_storage::open(directory, map_size, non_root_user_id);
    REQUIRE(opened.has_value());
    return std::move(*opened);
}

}

// The durable state must survive a close and a reopen. This is what Crash in
// the model assumes, and the model does not check it.
TEST_CASE("entries and meta values survive a close and a reopen")
{
    const temporary_directory dir;
    const std::vector<konsens::entry> written{
        normal_entry("first"), normal_entry("second"), {konsens::entry_kind::stop_sign, {}}};
    const konsens::ballot promise{1, 7, 3, 2};
    const konsens::ballot accepted{1, 6, 3, 1};
    {
        auto storage = open_storage(dir.path());
        const std::vector<konsens::storage_op> ops{konsens::append_entries{written}, konsens::set_promise{promise},
                                                   konsens::set_decided_index{2},
                                                   konsens::set_accepted_round{accepted}};
        REQUIRE(storage.write_atomically(ops).has_value());
    }
    const auto storage = open_storage(dir.path());
    CHECK(storage.get_log_len() == 3u);
    CHECK(storage.get_entries(0, 3) == written);
    CHECK(storage.get_promise() == promise);
    CHECK(storage.get_decided_idx() == 2u);
    CHECK(storage.get_accepted_round() == accepted);
}

// A user opens a new directory and reads first. A new storage has an empty
// log and the zero values of the meta data.
TEST_CASE("a new storage has an empty log and zero meta values")
{
    const temporary_directory dir;
    const auto storage = open_storage(dir.path());
    CHECK(storage.get_log_len() == 0u);
    CHECK(storage.get_entries(0, 10) == std::vector<konsens::entry>{});
    CHECK(storage.get_promise() == konsens::ballot{});
    CHECK(storage.get_decided_idx() == 0u);
    CHECK(storage.get_accepted_round() == konsens::ballot{});
}

// append_on_prefix replaces the suffix of a follower log that the leader does
// not have. The entries from from_idx on must go, and the new entries follow
// the prefix.
TEST_CASE("append_on_prefix truncates the log and appends")
{
    const temporary_directory dir;
    auto storage = open_storage(dir.path());
    const std::vector four{normal_entry("a"), normal_entry("b"), normal_entry("c"), normal_entry("d")};
    const std::vector<konsens::storage_op> first{konsens::append_entries{four}};
    REQUIRE(storage.write_atomically(first).has_value());
    const std::vector three{normal_entry("x"), normal_entry("y"), normal_entry("z")};
    const std::vector<konsens::storage_op> second{konsens::append_on_prefix{2, three}};
    REQUIRE(storage.write_atomically(second).has_value());
    CHECK(storage.get_log_len() == 5u);
    CHECK(storage.get_entries(0, 5) == std::vector<konsens::entry>{normal_entry("a"), normal_entry("b"),
                                                                   normal_entry("x"), normal_entry("y"),
                                                                   normal_entry("z")});
    CHECK(storage.get_entries(1, 3) == std::vector<konsens::entry>{normal_entry("b"), normal_entry("x")});
}

// AppendOnPrefix with k at or beyond the log length keeps the whole log,
// as Vec::truncate does.
TEST_CASE("append_on_prefix beyond the log keeps the log")
{
    const temporary_directory dir;
    auto storage = open_storage(dir.path());
    const std::vector one{normal_entry("a")};
    const std::vector two{normal_entry("b")};
    const std::vector<konsens::storage_op> ops{konsens::append_entries{one}, konsens::append_on_prefix{5, two}};
    REQUIRE(storage.write_atomically(ops).has_value());
    CHECK(storage.get_entries(0, 3) == std::vector<konsens::entry>{normal_entry("a"), normal_entry("b")});
}

// Sequence Paxos reads the entries through one rule for get_entries. A
// range that reaches past the log gives the stored part of it. A start at or past the end gives no
// entries. A start after the end of the range is a wrong use.
TEST_CASE("get_entries returns the stored part of the range")
{
    const temporary_directory dir;
    auto storage = open_storage(dir.path());
    const std::vector written{normal_entry("a"), normal_entry("b")};
    const std::vector<konsens::storage_op> ops{konsens::append_entries{written}};
    REQUIRE(storage.write_atomically(ops).has_value());
    CHECK(storage.get_entries(0, 0) == std::vector<konsens::entry>{});
    CHECK(storage.get_entries(0, 1) == std::vector{normal_entry("a")});
    CHECK(storage.get_entries(1, 5) == std::vector{normal_entry("b")});
    CHECK(storage.get_entries(0, 5) == std::vector{normal_entry("a"), normal_entry("b")});
    CHECK(storage.get_entries(2, 5) == std::vector<konsens::entry>{});
    CHECK(storage.get_entries(3, 5) == std::vector<konsens::entry>{});
    CHECK_THROWS_AS((void)storage.get_entries(1, 0), std::logic_error);
    CHECK_THROWS_AS((void)storage.read_entries(1, 0), std::logic_error);
}

// A byte that changes on the disk must not reach the protocol as a valid
// entry. The CRC32C finds it, and the error names the index of the entry.
TEST_CASE("a flipped byte in a stored entry is a CRC mismatch at its index")
{
    const temporary_directory dir;
    constexpr std::string_view marked = "konsens-marked-payload";
    {
        auto storage = open_storage(dir.path());
        const std::vector written{normal_entry("zero"), normal_entry(marked), normal_entry("two")};
        const std::vector<konsens::storage_op> ops{konsens::append_entries{written}};
        REQUIRE(storage.write_atomically(ops).has_value());
    }
    const auto data_file = dir.path() / "data.mdb";
    std::vector<char> contents;
    {
        std::ifstream in{data_file, std::ios::binary};
        contents.assign(std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{});
    }
    int flipped = 0;
    for (auto found = std::ranges::search(contents, marked); !found.empty();
         found = std::ranges::search(found.end(), contents.end(), marked.begin(), marked.end())) {
        found.front() = static_cast<char>(found.front() ^ 0x01);
        ++flipped;
    }
    REQUIRE(flipped > 0);
    {
        std::ofstream out{data_file, std::ios::binary | std::ios::trunc};
        out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    }
    const auto storage = open_storage(dir.path());
    const auto read = storage.read_entries(0, 3);
    REQUIRE_FALSE(read.has_value());
    CHECK(read.error().code == konsens::storage_errc::crc_mismatch);
    CHECK(read.error().index == 1u);
    CHECK(read.error().stored_crc != read.error().computed_crc);
    CHECK(read.error().message().find("index 1") != std::string::npos);
    const auto got = storage.get_entries(0, 3);
    REQUIRE_FALSE(got.has_value());
    CHECK(got.error() == konsens::storage_errc::crc_mismatch);
    CHECK(storage.get_entries(0, 1) == std::vector<konsens::entry>{normal_entry("zero")});
}

// Recovery after a power loss compares the CRC32C of the last entry of two
// nodes, so the value must follow the record format and nothing else: the
// index in 8 bytes little endian, the kind in one byte, the fields of the
// kind in 8 bytes little endian each, then the payload. An empty log gives
// 0. Each kind is tried, and the expected records are written by hand from
// that format, not from the encoder under test.
TEST_CASE("the CRC32C of the last entry of each kind is the CRC32C of its record")
{
    const temporary_directory dir;
    auto storage = open_storage(dir.path());
    CHECK(storage.get_last_entry_crc() == 0u);
    struct kind_and_record {
        konsens::entry e;
        std::vector<unsigned char> record;
    };
    const std::vector<kind_and_record> each_kind{
        {normal_entry("first"), {0, 0, 0, 0, 0, 0, 0, 0, 0, 'f', 'i', 'r', 's', 't'}},
        {{konsens::entry_kind::stop_sign, {}}, {1, 0, 0, 0, 0, 0, 0, 0, 1}},
        {{.kind = konsens::entry_kind::register_client, .client_id = 7, .pid = 3},
         {2, 0, 0, 0, 0, 0, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0}},
        {{.kind = konsens::entry_kind::expire_session, .session = 1},
         {3, 0, 0, 0, 0, 0, 0, 0, 4, 1, 0, 0, 0, 0, 0, 0, 0}},
        {{konsens::entry_kind::client_request, bytes_of("x"), 1, 2},
         {4, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 'x'}}};
    std::uint64_t index = 0;
    for (const auto& [e, record] : each_kind) {
        const std::vector<konsens::entry> one{e};
        const std::vector<konsens::storage_op> ops{konsens::append_entries{one}};
        REQUIRE(storage.write_atomically(ops).has_value());
        const auto expected = crc32c::Crc32c(record.data(), record.size());
        CHECK(storage.get_last_entry_crc() == expected);
        CHECK(konsens::compute_entry_crc(index, e) == expected);
        ++index;
    }
}

// A full map is an error value of write_atomically, which the node raises
// as write_error (test_group_commit.cpp). The failed write must leave
// nothing behind, because write_atomically is one transaction.
TEST_CASE("a full map gives no_space_on_device and writes nothing")
{
    const temporary_directory dir;
    auto opened = konsens::persistent_storage::open(dir.path(), tiny_map_size, non_root_user_id);
    REQUIRE(opened.has_value());
    auto& storage = *opened;
    std::vector<konsens::entry> large;
    for (int i = 0; i < 64; ++i) {
        large.push_back({konsens::entry_kind::normal, std::vector<std::byte>(4096, std::byte{0x5a})});
    }
    const std::vector<konsens::storage_op> ops{konsens::append_entries{large}};
    const auto written = storage.write_atomically(ops);
    REQUIRE_FALSE(written.has_value());
    CHECK(written.error() == std::errc::no_space_on_device);
    CHECK(storage.get_log_len() == 0u);
    const std::vector fits{normal_entry("fits")};
    const std::vector<konsens::storage_op> small{konsens::append_entries{fits}};
    CHECK(storage.write_atomically(small).has_value());
    CHECK(storage.get_log_len() == 1u);
}

// The decision of the owner (2026-10-21): an LMDB commit that fails is a
// failure of the machine. write_atomically throws sync_error with the error
// code, and nothing in the library catches it. A limit on the size of a
// file makes the commit fail, because the pages of a write transaction
// reach the file only in mdb_txn_commit. The limit holds this test process,
// and the test sets it back before it checks anything.
TEST_CASE("a failed LMDB commit throws sync_error and writes nothing")
{
    const temporary_directory dir;
    auto storage = open_storage(dir.path());
    rlimit unlimited{};
    REQUIRE(getrlimit(RLIMIT_FSIZE, &unlimited) == 0);
    rlimit limited = unlimited;
    limited.rlim_cur = std::filesystem::file_size(dir.path() / "data.mdb");
    const std::vector large{
        konsens::entry{konsens::entry_kind::normal, std::vector<std::byte>(large_entry_size, std::byte{0x5a})}};
    const std::vector<konsens::storage_op> ops{konsens::append_entries{large}};
    const auto handler = std::signal(SIGXFSZ, SIG_IGN);
    const int limit_set = setrlimit(RLIMIT_FSIZE, &limited);
    std::optional<std::error_code> thrown;
    try {
        static_cast<void>(storage.write_atomically(ops));
    } catch (const konsens::sync_error& e) {
        thrown = e.code();
    }
    const int limit_reset = setrlimit(RLIMIT_FSIZE, &unlimited);
    std::signal(SIGXFSZ, handler);
    REQUIRE(limit_set == 0);
    REQUIRE(limit_reset == 0);
    REQUIRE(thrown);
    CHECK((*thrown == std::errc::file_too_large || *thrown == std::errc::io_error));
    CHECK(storage.get_log_len() == 0u);
}

// The owner decided that a start as root is refused.
TEST_CASE("open refuses the root user")
{
    const temporary_directory dir;
    const auto opened = konsens::persistent_storage::open(dir.path(), map_size, root_user_id);
    REQUIRE_FALSE(opened.has_value());
    CHECK(opened.error() == std::errc::operation_not_permitted);
}

// A user gives a directory that does not exist. The error is a value that
// says so, and not a crash.
TEST_CASE("open of a missing directory gives no_such_file_or_directory")
{
    const temporary_directory dir;
    const auto opened = konsens::persistent_storage::open(dir.path() / "missing", map_size, non_root_user_id);
    REQUIRE_FALSE(opened.has_value());
    CHECK(opened.error() == std::errc::no_such_file_or_directory);
}

// A rollback in the simulation restores a copy that was made while the node
// ran. The copy must hold the state of the last commit, and a write after the
// copy must not reach it.
TEST_CASE("copy keeps the state of the last commit and not a later write")
{
    const temporary_directory dir;
    const temporary_directory copied;
    const std::vector<konsens::entry> written{normal_entry("first"), normal_entry("second")};
    const konsens::ballot promise{1, 7, 3, 2};
    {
        auto storage = open_storage(dir.path());
        const std::vector<konsens::storage_op> ops{konsens::append_entries{written}, konsens::set_promise{promise},
                                                   konsens::set_decided_index{1}};
        REQUIRE(storage.write_atomically(ops).has_value());
        REQUIRE(storage.copy(copied.path()).has_value());
        const std::vector<konsens::entry> later_entries{normal_entry("later")};
        const std::vector<konsens::storage_op> later{konsens::append_entries{later_entries},
                                                     konsens::set_decided_index{3}};
        REQUIRE(storage.write_atomically(later).has_value());
    }
    const auto storage = open_storage(copied.path());
    CHECK(storage.get_log_len() == 2u);
    CHECK(storage.get_entries(0, 3) == written);
    CHECK(storage.get_promise() == promise);
    CHECK(storage.get_decided_idx() == 1u);
}

// A user copies twice into the same directory. LMDB does not overwrite a
// copy, and the second copy is an error value, not a crash.
TEST_CASE("copy into a directory that holds a copy gives file_exists")
{
    const temporary_directory dir;
    const temporary_directory copied;
    const auto storage = open_storage(dir.path());
    REQUIRE(storage.copy(copied.path()).has_value());
    const auto again = storage.copy(copied.path());
    REQUIRE_FALSE(again.has_value());
    CHECK(again.error() == std::errc::file_exists);
}

// A user gives a directory that does not exist. The error is a value that
// says so.
TEST_CASE("copy into a missing directory gives no_such_file_or_directory")
{
    const temporary_directory dir;
    const auto storage = open_storage(dir.path());
    const auto copied = storage.copy(dir.path() / "missing");
    REQUIRE_FALSE(copied.has_value());
    CHECK(copied.error() == std::errc::no_such_file_or_directory);
}

// A storage that was moved from has no environment. A use of it is a wrong
// use, and a wrong use fails loudly and reads no memory that is not alive.
TEST_CASE("a moved-from storage throws logic_error")
{
    const temporary_directory dir;
    auto storage = open_storage(dir.path());
    const auto moved = std::move(storage);
    CHECK(moved.get_log_len() == 0u);
    CHECK_THROWS_AS((void)storage.get_log_len(), std::logic_error);
    const temporary_directory copied;
    CHECK_THROWS_AS((void)storage.copy(copied.path()), std::logic_error);
}

namespace {

// One entry of each kind, each with every field that its kind carries set
// to a value that is not zero, so that a field the encoding drops or moves
// shows in the comparison.
std::vector<konsens::entry> entries_of_each_kind()
{
    return {normal_entry("normal"),
            {konsens::entry_kind::stop_sign, {}},
            {.kind = konsens::entry_kind::register_client, .client_id = 0x0102030405060708, .pid = 3},
            {konsens::entry_kind::client_request, bytes_of("request"), 1, 0x1112131415161718},
            {konsens::entry_kind::client_request, {}, 2, 1},
            {.kind = konsens::entry_kind::expire_session, .session = 0x2122232425262728}};
}

}

// The client entries carry a session, a request number, a client id and a
// pid beside the payload. Each kind must come back from the disk with the
// same fields after a close and a reopen, and the CRC must cover them.
TEST_CASE("an entry of each kind survives a close and a reopen")
{
    const temporary_directory dir;
    const auto written = entries_of_each_kind();
    {
        auto storage = open_storage(dir.path());
        const std::vector<konsens::storage_op> ops{konsens::append_entries{written}};
        REQUIRE(storage.write_atomically(ops).has_value());
    }
    const auto storage = open_storage(dir.path());
    CHECK(storage.get_log_len() == written.size());
    CHECK(storage.get_entries(0, written.size()) == written);
    CHECK(storage.get_entries(2, 4) == std::vector<konsens::entry>{written[2], written[3]});
}

// A field that the kind of an entry does not carry is not stored. Such an
// entry would come back different from the disk, so the write is a wrong
// use and writes nothing.
TEST_CASE("an entry with a field of another kind is a wrong use")
{
    const temporary_directory dir;
    auto storage = open_storage(dir.path());
    const std::vector<konsens::entry> wrong{
        {konsens::entry_kind::normal, {}, 1},
        {konsens::entry_kind::stop_sign, {}, 0, 0, 1},
        {.kind = konsens::entry_kind::register_client, .session = 1, .client_id = 1, .pid = 1},
        {.kind = konsens::entry_kind::client_request, .session = 1, .request_number = 1, .pid = 1},
        {.kind = konsens::entry_kind::expire_session, .session = 1, .request_number = 1},
        {static_cast<konsens::entry_kind>(0xff), {}},
    };
    for (const auto& e : wrong) {
        const std::span<const konsens::entry> one{&e, 1};
        const std::vector<konsens::storage_op> ops{konsens::append_entries{one}};
        CHECK_THROWS_AS((void)storage.write_atomically(ops), std::logic_error);
        const std::vector<konsens::storage_op> on_prefix{konsens::append_on_prefix{0, one}};
        CHECK_THROWS_AS((void)storage.write_atomically(on_prefix), std::logic_error);
    }
    CHECK(storage.get_log_len() == 0u);
}

namespace {

struct stored_state {
    std::vector<konsens::entry> log;
    konsens::ballot promise;
    std::uint64_t decided_idx;
    konsens::ballot accepted_round;
    std::uint32_t last_entry_crc;

    friend bool operator==(const stored_state&, const stored_state&) = default;
};

template <typename S>
stored_state stored_state_of(const S& s)
{
    const auto len = s.get_log_len();
    REQUIRE(len);
    const auto log = s.get_entries(0, *len);
    const auto promise = s.get_promise();
    const auto decided = s.get_decided_idx();
    const auto round = s.get_accepted_round();
    const auto crc = s.get_last_entry_crc();
    REQUIRE(log);
    REQUIRE(promise);
    REQUIRE(decided);
    REQUIRE(round);
    REQUIRE(crc);
    return {*log, *promise, *decided, *round, *crc};
}

std::vector<konsens::storage_op> ops_of_one_batch(const std::vector<konsens::entry>& first,
                                                  const std::vector<konsens::entry>& replaced,
                                                  const std::vector<konsens::entry>& last)
{
    return {konsens::set_promise{{1, 2, 0, 3}},
            konsens::append_entries{first},
            konsens::set_accepted_round{{1, 2, 0, 3}},
            konsens::set_decided_index{1},
            konsens::append_on_prefix{2, replaced},
            konsens::set_decided_index{3},
            konsens::append_entries{last},
            konsens::set_promise{{1, 4, 0, 1}}};
}

void check_one_batch_equals_the_ops_one_by_one(konsens::persistent_storage& batched,
                                               konsens::persistent_storage& single)
{
    const std::vector<konsens::entry> first{normal_entry("a"), normal_entry("b"), normal_entry("c")};
    const std::vector<konsens::entry> replaced{normal_entry("x"), normal_entry("y")};
    const std::vector<konsens::entry> last{normal_entry("z")};
    const auto ops = ops_of_one_batch(first, replaced, last);
    REQUIRE(batched.write_atomically(ops));
    for (const auto& op : ops) {
        REQUIRE(single.write_atomically(std::span(&op, 1)));
    }
    CHECK(stored_state_of(batched) == stored_state_of(single));
    CHECK(stored_state_of(batched).log ==
          std::vector{normal_entry("a"), normal_entry("b"), normal_entry("x"), normal_entry("y"), normal_entry("z")});
}

}

// R7 of the review of doc/plan-group-commit.md: a commit gives one
// write_atomically all ops of a batch, where each handler wrote its own ops
// before. The store must apply the ops of one call in order, an
// append_on_prefix after an append_entries of the same call included, so
// that one batch leaves the store as the same ops one by one do.
TEST_CASE("one write of the ops of a batch leaves the store as the ops one by one do")
{
    const temporary_directory batched_dir;
    const temporary_directory single_dir;
    auto batched = open_storage(batched_dir.path());
    auto single = open_storage(single_dir.path());
    check_one_batch_equals_the_ops_one_by_one(batched, single);
}
