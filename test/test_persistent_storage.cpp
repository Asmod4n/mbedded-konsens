#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <spawn.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#include <crc32c/crc32c.h>
#include <sqlite3.h>

#include "konsens/persistent_storage.hpp"
#include "sqlite_damage.hpp"
#include "temporary_directory.hpp"

static_assert(konsens::storage<konsens::persistent_storage>);

namespace {

// The session that runs these tests runs as root. The open function takes
// the effective user id as a value, so that the tests can give a user id
// that the refusal of root does not hold.
constexpr uid_t non_root_user_id = 1000;
constexpr uid_t root_user_id = 0;
constexpr std::size_t max_size = std::size_t{16} << 20;
constexpr std::size_t tiny_max_size = std::size_t{64} << 10;
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
    auto opened = konsens::persistent_storage::open(directory, max_size, non_root_user_id);
    REQUIRE(opened.has_value());
    return std::move(*opened);
}

bool finds_no_value(const std::expected<std::optional<std::span<const std::byte>>, std::error_code>& found)
{
    return found.has_value() && !found->has_value();
}

template <typename Writes>
void write(konsens::persistent_storage& storage, const Writes& writes)
{
    auto t = storage.begin();
    REQUIRE(t.has_value());
    writes(*t);
    REQUIRE(std::move(*t).commit().has_value());
}

konsens::meta_record meta_of(const konsens::persistent_storage& storage)
{
    const auto t = storage.begin_read_only();
    REQUIRE(t.has_value());
    const auto m = t->get_meta();
    REQUIRE(m.has_value());
    return *m;
}

std::expected<std::vector<konsens::entry>, std::error_code> entries_of(const konsens::persistent_storage& storage,
                                                                       const std::uint64_t from, const std::uint64_t to)
{
    const auto t = storage.begin_read_only();
    REQUIRE(t.has_value());
    return t->get_entries(from, to);
}

std::optional<konsens::entry_state> state_of(const konsens::persistent_storage& storage, const std::uint64_t index)
{
    const auto t = storage.begin_read_only();
    REQUIRE(t.has_value());
    const auto state = t->get_entry_state(index);
    REQUIRE(state.has_value());
    return *state;
}

using sqlite_damage::change_with_sqlite;

void delete_entry(const std::filesystem::path& directory, const std::uint64_t index)
{
    change_with_sqlite(directory, [index](sqlite3* db) { sqlite_damage::delete_entry(db, index); });
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
        write(storage, [&](auto& t) {
            REQUIRE(t.append_entries(written));
            REQUIRE(t.set_promise(promise));
            REQUIRE(t.set_decided_index(2));
            REQUIRE(t.set_accepted_round(accepted));
        });
    }
    const auto storage = open_storage(dir.path());
    const auto m = meta_of(storage);
    CHECK(m.log_len == 3u);
    CHECK(m.promise == promise);
    CHECK(m.decided_idx == 2u);
    CHECK(m.accepted_round == accepted);
    CHECK(entries_of(storage, 0, 3) == written);
}

// A step that only applies decided entries commits without a sync. The
// consensus layer tells such a step from the others by has_log_writes, so
// every write that changes the log or the votes must count, and the writes
// of the apply must not.
TEST_CASE("only the writes of the log and of the votes count as log writes")
{
    const temporary_directory dir;
    auto storage = open_storage(dir.path());
    write(storage, [](auto& t) { REQUIRE(t.append_entries(std::vector{normal_entry("a"), normal_entry("b")})); });
    const std::array<std::function<void(konsens::persistent_storage::transaction&)>, 5> log_writes{
        [](auto& t) { REQUIRE(t.append_entries(std::vector{normal_entry("c")})); },
        [](auto& t) { REQUIRE(t.append_on_prefix(1, std::vector{normal_entry("d")})); },
        [](auto& t) { REQUIRE(t.set_promise({1, 2, 0, 1})); },
        [](auto& t) { REQUIRE(t.set_accepted_round({1, 2, 0, 1})); },
        [](auto& t) { REQUIRE(t.set_config_first_idx(1)); },
    };
    for (const auto& w : log_writes) {
        auto t = storage.begin();
        REQUIRE(t.has_value());
        CHECK_FALSE(t->has_log_writes());
        w(*t);
        CHECK(t->has_log_writes());
        REQUIRE(std::move(*t).commit().has_value());
    }
    auto t = storage.begin();
    REQUIRE(t.has_value());
    REQUIRE(t->set_decided_index(1));
    REQUIRE(t->insert_or_assign(bytes_of("k"), bytes_of("v")));
    REQUIRE(t->insert_or_assign_session(1, {1, bytes_of("r")}));
    REQUIRE(t->set_applied(1, 0));
    REQUIRE(t->erase(bytes_of("k")));
    REQUIRE(t->erase_session(1));
    CHECK(t->has_writes());
    CHECK_FALSE(t->has_log_writes());
    REQUIRE(std::move(*t).commit().has_value());
}

// A commit without a sync is a commit: the values are there for a reader at
// once, and after a clean close and a reopen. Only a power loss may undo it.
TEST_CASE("a commit without a sync is read back and survives a close and a reopen")
{
    const temporary_directory dir;
    {
        auto storage = open_storage(dir.path());
        write(storage, [](auto& t) { REQUIRE(t.append_entries(std::vector{normal_entry("a")})); });
        auto t = storage.begin();
        REQUIRE(t.has_value());
        REQUIRE(t->set_decided_index(1));
        REQUIRE(t->insert_or_assign(bytes_of("k"), bytes_of("v")));
        REQUIRE(std::move(*t).commit_without_a_sync().has_value());
        CHECK(meta_of(storage).decided_idx == 1u);
        write(storage, [](auto& w) { REQUIRE(w.append_entries(std::vector{normal_entry("b")})); });
    }
    const auto storage = open_storage(dir.path());
    CHECK(meta_of(storage).decided_idx == 1u);
    CHECK(meta_of(storage).log_len == 2u);
    const auto r = storage.begin_read_only();
    REQUIRE(r.has_value());
    const auto found = r->find(bytes_of("k"));
    REQUIRE(found.has_value());
    REQUIRE(found->has_value());
    CHECK(std::ranges::equal(**found, bytes_of("v")));
}

// A write transaction keeps its meta record in memory after the first read.
// A transaction that is rolled back must not leave its meta record for the
// next transaction, and a write must be seen by the next read in the same
// transaction.
TEST_CASE("the meta record of a rolled back transaction does not reach the next transaction")
{
    const temporary_directory dir;
    auto storage = open_storage(dir.path());
    write(storage, [](auto& t) { REQUIRE(t.append_entries(std::vector{normal_entry("a")})); });
    {
        auto t = storage.begin();
        REQUIRE(t.has_value());
        REQUIRE(t->get_meta().has_value());
        REQUIRE(t->append_entries(std::vector{normal_entry("b")}));
        REQUIRE(t->set_decided_index(2));
        const auto m = t->get_meta();
        REQUIRE(m.has_value());
        CHECK(m->log_len == 2u);
        CHECK(m->decided_idx == 2u);
    }
    auto t = storage.begin();
    REQUIRE(t.has_value());
    const auto m = t->get_meta();
    REQUIRE(m.has_value());
    CHECK(m->log_len == 1u);
    CHECK(m->decided_idx == 0u);
}

// A user opens a new directory and reads first. A new storage has an empty
// log and a meta record of zero values. The open writes that record, so
// that a storage with no meta record is a damaged storage.
TEST_CASE("a new storage has an empty log and a meta record of zero values")
{
    const temporary_directory dir;
    const auto storage = open_storage(dir.path());
    CHECK(meta_of(storage) == konsens::meta_record{});
    CHECK(entries_of(storage, 0, 10) == std::vector<konsens::entry>{});
    const auto t = storage.begin_read_only();
    REQUIRE(t.has_value());
    const auto last = t->get_last_entry();
    REQUIRE(last.has_value());
    CHECK_FALSE(last->has_value());
}

// append_on_prefix replaces the suffix of a follower log that the leader does
// not have. The entries from from_idx on must go, and the new entries follow
// the prefix.
TEST_CASE("append_on_prefix truncates the log and appends")
{
    const temporary_directory dir;
    auto storage = open_storage(dir.path());
    const std::vector four{normal_entry("a"), normal_entry("b"), normal_entry("c"), normal_entry("d")};
    write(storage, [&](auto& t) { REQUIRE(t.append_entries(four)); });
    const std::vector three{normal_entry("x"), normal_entry("y"), normal_entry("z")};
    write(storage, [&](auto& t) { REQUIRE(t.append_on_prefix(2, three)); });
    CHECK(meta_of(storage).log_len == 5u);
    CHECK(entries_of(storage, 0, 5) == std::vector<konsens::entry>{normal_entry("a"), normal_entry("b"),
                                                                   normal_entry("x"), normal_entry("y"),
                                                                   normal_entry("z")});
    CHECK(entries_of(storage, 1, 3) == std::vector<konsens::entry>{normal_entry("b"), normal_entry("x")});
}

// AppendOnPrefix with k at or beyond the log length keeps the whole log,
// as Vec::truncate does.
TEST_CASE("append_on_prefix beyond the log keeps the log")
{
    const temporary_directory dir;
    auto storage = open_storage(dir.path());
    const std::vector one{normal_entry("a")};
    const std::vector two{normal_entry("b")};
    write(storage, [&](auto& t) {
        REQUIRE(t.append_entries(one));
        REQUIRE(t.append_on_prefix(5, two));
    });
    CHECK(entries_of(storage, 0, 3) == std::vector<konsens::entry>{normal_entry("a"), normal_entry("b")});
}

// Sequence Paxos reads the entries through one rule for get_entries. A
// range that reaches past the log gives the stored part of it. A start at
// or past the end gives no entries. A start after the end of the range is a
// wrong use.
TEST_CASE("get_entries returns the stored part of the range")
{
    const temporary_directory dir;
    auto storage = open_storage(dir.path());
    const std::vector written{normal_entry("a"), normal_entry("b")};
    write(storage, [&](auto& t) { REQUIRE(t.append_entries(written)); });
    CHECK(entries_of(storage, 0, 0) == std::vector<konsens::entry>{});
    CHECK(entries_of(storage, 0, 1) == std::vector{normal_entry("a")});
    CHECK(entries_of(storage, 1, 5) == std::vector{normal_entry("b")});
    CHECK(entries_of(storage, 0, 5) == std::vector{normal_entry("a"), normal_entry("b")});
    CHECK(entries_of(storage, 2, 5) == std::vector<konsens::entry>{});
    CHECK(entries_of(storage, 3, 5) == std::vector<konsens::entry>{});
    const auto t = storage.begin_read_only();
    REQUIRE(t.has_value());
    CHECK_THROWS_AS((void)t->get_entries(1, 0), std::logic_error);
    CHECK_THROWS_AS((void)t->read_entries(1, 0), std::logic_error);
}

// Plan lmdb-only-source 2.1 item 2: every read of a step goes through its
// write transaction, so a read sees the writes of the same step before the
// commit, and a read-only transaction does not see them.
TEST_CASE("a read in a transaction sees the writes of the same transaction")
{
    const temporary_directory dir;
    auto storage = open_storage(dir.path());
    const std::vector written{normal_entry("a"), normal_entry("b")};
    auto t = storage.begin();
    REQUIRE(t.has_value());
    REQUIRE(t->append_entries(written));
    REQUIRE(t->set_promise({1, 2, 0, 3}));
    CHECK(t->get_entries(0, 2) == written);
    CHECK(t->get_meta()->log_len == 2u);
    CHECK(t->get_meta()->promise == konsens::ballot{1, 2, 0, 3});
    CHECK(meta_of(storage) == konsens::meta_record{});
    REQUIRE(std::move(*t).commit());
    CHECK(meta_of(storage).log_len == 2u);
}

// A transaction that ends without a commit leaves the storage as it was:
// this is how a node drops the writes of a step that stops the vote.
TEST_CASE("a transaction that is not committed writes nothing")
{
    const temporary_directory dir;
    auto storage = open_storage(dir.path());
    {
        auto t = storage.begin();
        REQUIRE(t.has_value());
        const std::vector written{normal_entry("a")};
        REQUIRE(t->append_entries(written));
    }
    CHECK(meta_of(storage) == konsens::meta_record{});
    CHECK(entries_of(storage, 0, 1) == std::vector<konsens::entry>{});
}

// The storage has one writer, and a thread that begins a
// second write transaction waits on itself. A second write transaction or
// a second read-only transaction of one storage is a wrong use, and so is
// a use of a transaction after its commit.
TEST_CASE("a second open transaction and a use after the commit are wrong uses")
{
    const temporary_directory dir;
    auto storage = open_storage(dir.path());
    {
        auto t = storage.begin();
        REQUIRE(t.has_value());
        CHECK_THROWS_AS((void)storage.begin(), std::logic_error);
        auto moved = std::move(*t);
        CHECK_THROWS_AS((void)t->get_meta(), std::logic_error);
        REQUIRE(std::move(moved).commit());
        CHECK_THROWS_AS((void)moved.get_meta(), std::logic_error);
        CHECK_THROWS_AS((void)std::move(moved).commit(), std::logic_error);
    }
    {
        const auto r = storage.begin_read_only();
        REQUIRE(r.has_value());
        CHECK_THROWS_AS((void)storage.begin_read_only(), std::logic_error);
    }
    CHECK(storage.begin_read_only().has_value());
}

// Plan lmdb-only-source 2.1 item 3: the read-only transaction is kept and
// reused, and it stands in the same thread as a write transaction. A reuse
// after a commit reads the new state.
TEST_CASE("a kept read-only transaction reads the state of the last commit at each use")
{
    const temporary_directory dir;
    auto storage = open_storage(dir.path());
    for (std::uint64_t step = 1; step <= 3; ++step) {
        auto t = storage.begin();
        REQUIRE(t.has_value());
        const std::vector one{normal_entry("x")};
        REQUIRE(t->append_entries(one));
        {
            const auto r = storage.begin_read_only();
            REQUIRE(r.has_value());
            CHECK(r->get_meta()->log_len == step - 1);
        }
        REQUIRE(std::move(*t).commit());
        CHECK(meta_of(storage).log_len == step);
    }
}

// SQLite keeps no checksum of a page. A byte that changes on the disk
// inside a record is read without an error by SQLite, and only the CRC of
// the entry finds it. The read names the index of the entry. The test moves
// the WAL into the database file first, so that the flipped byte stands in
// the file that SQLite reads.
TEST_CASE("a flipped byte in a stored record is a CRC mismatch at its index")
{
    const temporary_directory dir;
    constexpr std::string_view marked = "konsens-marked-payload";
    {
        auto storage = open_storage(dir.path());
        const std::vector written{normal_entry("zero"), normal_entry(marked), normal_entry("two")};
        write(storage, [&](auto& t) { REQUIRE(t.append_entries(written)); });
    }
    sqlite_damage::move_the_wal_into_the_file(dir.path());
    const auto data_file = dir.path() / sqlite_damage::store_file_name;
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
    REQUIRE(flipped == 1);
    {
        std::ofstream out{data_file, std::ios::binary | std::ios::trunc};
        out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    }
    const auto storage = open_storage(dir.path());
    const auto t = storage.begin_read_only();
    REQUIRE(t.has_value());
    const auto read = t->read_entries(0, 3);
    REQUIRE_FALSE(read.has_value());
    CHECK(read.error().code == konsens::storage_errc::crc_mismatch);
    CHECK(read.error().index == 1u);
}

// Plan sqlite-storage 8.4: a damaged page of the B-tree gives SQLITE_CORRUPT,
// which the storage gives as storage_errc::corrupt, and the program does not
// crash. The first byte of a B-tree page is its type (fileformat2.html,
// section 1.6). The test sets it to a value that no page type has.
TEST_CASE("a damaged page of the log gives corrupt and no crash")
{
    const temporary_directory dir;
    {
        auto storage = open_storage(dir.path());
        const std::vector written{normal_entry("zero"), normal_entry("one"), normal_entry("two")};
        write(storage, [&](auto& t) { REQUIRE(t.append_entries(written)); });
    }
    sqlite_damage::move_the_wal_into_the_file(dir.path());
    const auto [page_size, log_root_page] = sqlite_damage::page_size_and_root_page_of(dir.path(), "log");
    const auto data_file = dir.path() / sqlite_damage::store_file_name;
    {
        std::fstream file{data_file, std::ios::binary | std::ios::in | std::ios::out};
        file.seekp(static_cast<std::streamoff>((log_root_page - 1) * page_size));
        constexpr char no_page_type = static_cast<char>(0xff);
        file.put(no_page_type);
    }
    const auto storage = open_storage(dir.path());
    const auto t = storage.begin_read_only();
    REQUIRE(t.has_value());
    CHECK(t->get_entries(0, 3) == std::unexpected(make_error_code(konsens::storage_errc::corrupt)));
    CHECK(t->get_last_entry() == std::unexpected(make_error_code(konsens::storage_errc::corrupt)));
}

// A value whose bytes do not agree with the CRC of the entry must not reach
// the protocol as a valid entry. The error names the index of the entry.
TEST_CASE("a changed value is a CRC mismatch at its index")
{
    const temporary_directory dir;
    {
        auto storage = open_storage(dir.path());
        const std::vector written{normal_entry("zero"), normal_entry("one"), normal_entry("two")};
        write(storage, [&](auto& t) { REQUIRE(t.append_entries(written)); });
    }
    change_with_sqlite(dir.path(), [](sqlite3* db) {
        auto changed = sqlite_damage::entry_record_of(db, 1);
        changed.at(1) ^= std::byte{0x01};
        sqlite_damage::put_entry_record(db, 1, changed);
    });
    const auto storage = open_storage(dir.path());
    const auto t = storage.begin_read_only();
    REQUIRE(t.has_value());
    const auto read = t->read_entries(0, 3);
    REQUIRE_FALSE(read.has_value());
    CHECK(read.error().code == konsens::storage_errc::crc_mismatch);
    CHECK(read.error().index == 1u);
    CHECK(read.error().stored_crc != read.error().computed_crc);
    CHECK(read.error().message().find("index 1") != std::string::npos);
    CHECK(t->get_entries(0, 1) == std::vector<konsens::entry>{normal_entry("zero")});
}

// The start-checks plan, item 2.2: a key that is missing in the middle of a
// range is not a shorter log. The read names the missing index as an error.
// The read of the last entry does not see the hole. The start scan finds
// it.
TEST_CASE("a missing key in the middle of a range is missing_entry")
{
    const temporary_directory dir;
    {
        auto storage = open_storage(dir.path());
        const std::vector written{normal_entry("a"), normal_entry("b"), normal_entry("c"), normal_entry("d")};
        write(storage, [&](auto& t) { REQUIRE(t.append_entries(written)); });
    }
    delete_entry(dir.path(), 1);
    const auto storage = open_storage(dir.path());
    const auto t = storage.begin_read_only();
    REQUIRE(t.has_value());
    CHECK(t->get_entries(0, 1) == std::vector{normal_entry("a")});
    const auto across = t->read_entries(0, 4);
    REQUIRE_FALSE(across.has_value());
    CHECK(across.error().code == konsens::storage_errc::missing_entry);
    CHECK(across.error().index == 1u);
    CHECK(t->get_entries(1, 2) == std::unexpected(make_error_code(konsens::storage_errc::missing_entry)));
    CHECK(t->get_entries(0, 2) == std::unexpected(make_error_code(konsens::storage_errc::missing_entry)));
    const auto last = t->get_last_entry();
    REQUIRE(last.has_value());
    REQUIRE(last->has_value());
    CHECK((*last)->index == 3u);
}

// Plan lmdb-only-source 2.3 item 2: the meta record exists from the open on.
// A storage that lost it is damaged, and the read says so.
TEST_CASE("a storage that lost its meta record gives missing_meta_record")
{
    const temporary_directory dir;
    {
        auto storage = open_storage(dir.path());
        const std::vector written{normal_entry("a")};
        write(storage, [&](auto& t) { REQUIRE(t.append_entries(written)); });
    }
    change_with_sqlite(dir.path(), [](sqlite3* db) { sqlite_damage::delete_meta_record(db); });
    auto storage = open_storage(dir.path());
    {
        const auto t = storage.begin_read_only();
        REQUIRE(t.has_value());
        CHECK(t->get_meta() == std::unexpected(make_error_code(konsens::storage_errc::missing_meta_record)));
    }
    auto t = storage.begin();
    REQUIRE(t.has_value());
    CHECK(t->set_promise({1, 1, 0, 1}) == std::unexpected(make_error_code(konsens::storage_errc::missing_meta_record)));
}

// Plan lmdb-only-source 2.9: the format of the files changed, and no store
// of an older format exists outside the tests. The open refuses a format
// that it does not know, and a store with data and no format.
TEST_CASE("open refuses another format and a store with data and no format")
{
    const temporary_directory dir;
    {
        auto storage = open_storage(dir.path());
        const std::vector written{normal_entry("a")};
        write(storage, [&](auto& t) { REQUIRE(t.append_entries(written)); });
    }
    change_with_sqlite(dir.path(), [](sqlite3* db) {
        const std::array<unsigned char, 8> version_4{4, 0, 0, 0, 0, 0, 0, 0};
        const auto crc = crc32c::Crc32c(version_4.data(), 4);
        std::vector<std::byte> value(version_4.size());
        std::ranges::transform(version_4, value.begin(), [](const unsigned char c) { return std::byte{c}; });
        for (std::size_t i = 0; i < 4; ++i) {
            value.at(4 + i) = static_cast<std::byte>(crc >> (8 * i));
        }
        sqlite_damage::put_record(db, "format", value);
    });
    const auto other = konsens::persistent_storage::open(dir.path(), max_size, non_root_user_id);
    REQUIRE_FALSE(other.has_value());
    CHECK(other.error() == konsens::storage_errc::unsupported_format);
    change_with_sqlite(dir.path(), [](sqlite3* db) { sqlite_damage::delete_record(db, "format"); });
    const auto none = konsens::persistent_storage::open(dir.path(), max_size, non_root_user_id);
    REQUIRE_FALSE(none.has_value());
    CHECK(none.error() == konsens::storage_errc::unsupported_format);
}

// The vote and the build compare the chain CRC of an entry of two nodes,
// so the value must follow the record format and nothing else: the
// index in 8 bytes little endian, the kind in one byte, the fields of the
// kind in 8 bytes little endian each, then the payload. A request kind
// carries its result between its fields and its payload: one byte that is 1
// for a rejected task, the result CRC in 4 bytes, the count of the nodes out
// and each of their ids in 8 bytes, the size of the result in 8 bytes and the
// result. A request in work has an empty result. The chain CRC of an
// entry is the CRC32C over the chain CRC of the entry before it in 4 bytes
// little endian (0 for the first entry), the index in 8 bytes little
// endian, and the same encoded entry. Each kind is tried, and the expected
// values are computed from records written by hand from that format, not
// from the encoder under test.
TEST_CASE("the entry CRC and the chain CRC of each kind follow the record format")
{
    const temporary_directory dir;
    auto storage = open_storage(dir.path());
    struct kind_and_record {
        konsens::entry e;
        std::vector<unsigned char> encoded;
    };
    const std::vector<kind_and_record> each_kind{
        {normal_entry("first"), {0, 'f', 'i', 'r', 's', 't'}},
        {{konsens::entry_kind::stop_sign, {}}, {1}},
        {{.kind = konsens::entry_kind::register_client, .client_id = 7, .pid = 3},
         {2, 7, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
        {{.kind = konsens::entry_kind::expire_session, .session = 1},
         {4, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
        {{konsens::entry_kind::client_request, bytes_of("x"), 1, 2},
         {3, 1, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 'x'}},
        {{.kind = konsens::entry_kind::client_request,
          .payload = bytes_of("x"),
          .session = 1,
          .request_number = 2,
          .result = bytes_of("r"),
          .result_crc = 0x01020304,
          .out = {5}},
         {3, 1, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 4, 3, 2, 1, 1, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'r', 'x'}},
        {{.kind = konsens::entry_kind::expire_session, .session = 1, .result = bytes_of("a"), .rejected = true},
         {4, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'a'}}};
    std::uint32_t chain = 0;
    for (std::uint64_t index = 0; const auto& [e, encoded] : each_kind) {
        const std::vector<konsens::entry> one{e};
        write(storage, [&](auto& t) { REQUIRE(t.append_entries(one)); });
        std::vector<unsigned char> entry_record{static_cast<unsigned char>(index), 0, 0, 0, 0, 0, 0, 0};
        entry_record.insert(entry_record.end(), encoded.begin(), encoded.end());
        std::vector<unsigned char> chain_record;
        for (std::size_t i = 0; i < 4; ++i) {
            chain_record.push_back(static_cast<unsigned char>(chain >> (8 * i)));
        }
        chain_record.insert(chain_record.end(), entry_record.begin(), entry_record.end());
        const auto entry_crc = crc32c::Crc32c(entry_record.data(), entry_record.size());
        chain = crc32c::Crc32c(chain_record.data(), chain_record.size());
        const auto t = storage.begin_read_only();
        REQUIRE(t.has_value());
        const auto last = t->get_last_entry();
        REQUIRE(last.has_value());
        REQUIRE(last->has_value());
        CHECK(last->value().index == index);
        CHECK(last->value().entry_crc == entry_crc);
        CHECK(last->value().chain_crc == chain);
        CHECK(t->get_meta()->last_chain_crc == chain);
        CHECK(konsens::compute_entry_crc(index, e) == entry_crc);
        ++index;
    }
}

// Plan lmdb-only-source 2.3 item 1: after a cut, the chain continues from
// the entry before the cut, so two logs with the same entries have the same
// chain CRC whatever was cut before.
TEST_CASE("the chain CRC after a cut is the chain CRC of the same log written at once")
{
    const temporary_directory cut_dir;
    const temporary_directory once_dir;
    auto cut = open_storage(cut_dir.path());
    auto once = open_storage(once_dir.path());
    const std::vector four{normal_entry("a"), normal_entry("b"), normal_entry("c"), normal_entry("d")};
    const std::vector two{normal_entry("x"), normal_entry("y")};
    write(cut, [&](auto& t) {
        REQUIRE(t.append_entries(four));
        REQUIRE(t.append_on_prefix(1, two));
    });
    const std::vector same{normal_entry("a"), normal_entry("x"), normal_entry("y")};
    write(once, [&](auto& t) { REQUIRE(t.append_entries(same)); });
    CHECK(meta_of(cut) == meta_of(once));
    write(cut, [&](auto& t) { REQUIRE(t.append_on_prefix(0, {})); });
    CHECK(meta_of(cut) == konsens::meta_record{});
}

// Plan lmdb-only-source 2.2 item 2: each raise of the decided index from o
// to d sets the state committed on the entries o to d - 1, and the entries
// from d on stay in work. A decided index above the log length is
// missing_entry (the start-checks plan, item 2.5).
TEST_CASE("set_decided_index commits the entries below it and nothing above")
{
    const temporary_directory dir;
    auto storage = open_storage(dir.path());
    const std::vector four{normal_entry("a"), normal_entry("b"), normal_entry("c"), normal_entry("d")};
    write(storage, [&](auto& t) {
        REQUIRE(t.append_entries(four));
        REQUIRE(t.set_decided_index(1));
    });
    CHECK(state_of(storage, 0) == konsens::entry_state::committed);
    CHECK(state_of(storage, 1) == konsens::entry_state::in_work);
    write(storage, [&](auto& t) { REQUIRE(t.set_decided_index(3)); });
    CHECK(state_of(storage, 0) == konsens::entry_state::committed);
    CHECK(state_of(storage, 1) == konsens::entry_state::committed);
    CHECK(state_of(storage, 2) == konsens::entry_state::committed);
    CHECK(state_of(storage, 3) == konsens::entry_state::in_work);
    CHECK(state_of(storage, 4) == std::nullopt);
    CHECK(entries_of(storage, 0, 4) == four);
    auto t = storage.begin();
    REQUIRE(t.has_value());
    CHECK(t->set_decided_index(5) == std::unexpected(make_error_code(konsens::storage_errc::missing_entry)));
}

// An entry of more than about 2000 bytes lay on LMDB overflow pages. A
// raise of the decided index over two or more such entries in one
// transaction once wrote each new state under a key that pointed into a page
// of the previous put, so the next entry was lost or failed its CRC.
TEST_CASE("a raise of the decided index over large entries keeps every entry")
{
    const temporary_directory dir;
    auto storage = open_storage(dir.path());
    const std::string large(70000, 'x');
    const std::vector entries{normal_entry(large), normal_entry(large + "b"), normal_entry(large + "c"),
                              normal_entry(large + "d"), normal_entry(std::string(2100, 'e'))};
    write(storage, [&](auto& t) { REQUIRE(t.append_entries(entries)); });
    write(storage, [&](auto& t) { REQUIRE(t.set_decided_index(5)); });
    for (const auto index : std::views::iota(std::uint64_t{0}, std::uint64_t{5})) {
        CHECK(state_of(storage, index) == konsens::entry_state::committed);
    }
    CHECK(entries_of(storage, 0, 5) == entries);
}

// append_on_prefix can write the log again from index 0. An entry that it
// writes below the decided index is a decided entry, so it is written
// committed.
TEST_CASE("an entry written below the decided index is committed")
{
    const temporary_directory dir;
    auto storage = open_storage(dir.path());
    const std::vector three{normal_entry("a"), normal_entry("b"), normal_entry("c")};
    write(storage, [&](auto& t) {
        REQUIRE(t.append_entries(three));
        REQUIRE(t.set_decided_index(2));
        REQUIRE(t.append_on_prefix(0, three));
    });
    CHECK(state_of(storage, 0) == konsens::entry_state::committed);
    CHECK(state_of(storage, 1) == konsens::entry_state::committed);
    CHECK(state_of(storage, 2) == konsens::entry_state::in_work);
}

// A full store is an error value of a write, which the node raises as
// write_error (test_group_commit.cpp). SQLite gives SQLITE_FULL when a
// write needs more pages than max_page_count allows. A transaction that
// ends without a commit after it leaves nothing behind.
TEST_CASE("a full store gives no_space_on_device and writes nothing")
{
    const temporary_directory dir;
    auto opened = konsens::persistent_storage::open(dir.path(), tiny_max_size, non_root_user_id);
    REQUIRE(opened.has_value());
    auto& storage = *opened;
    std::vector<konsens::entry> large;
    for (int i = 0; i < 64; ++i) {
        large.push_back({konsens::entry_kind::normal, std::vector<std::byte>(4096, std::byte{0x5a})});
    }
    {
        auto t = storage.begin();
        REQUIRE(t.has_value());
        const auto written = t->append_entries(large);
        REQUIRE_FALSE(written.has_value());
        CHECK(written.error() == std::errc::no_space_on_device);
    }
    CHECK(meta_of(storage).log_len == 0u);
    const std::vector fits{normal_entry("fits")};
    write(storage, [&](auto& t) { REQUIRE(t.append_entries(fits)); });
    CHECK(meta_of(storage).log_len == 1u);
}

// Plan sqlite-storage 6.1: a write to the disk that fails is a failure of
// the machine. The commit throws write_error with the error code, and
// nothing in the library catches it. A limit on the size of a file makes
// the commit fail, because the pages of a transaction reach the WAL only
// at its commit. The limit holds this test process, and the test sets it
// back before it checks anything. The store that opens again after the
// failure holds nothing of the transaction.
TEST_CASE("a failed write of a commit throws write_error and writes nothing")
{
    const temporary_directory dir;
    {
        auto storage = open_storage(dir.path());
        rlimit unlimited{};
        REQUIRE(getrlimit(RLIMIT_FSIZE, &unlimited) == 0);
        rlimit limited = unlimited;
        limited.rlim_cur = std::filesystem::file_size(dir.path() / "konsens.db-wal");
        const std::vector large{
            konsens::entry{konsens::entry_kind::normal, std::vector<std::byte>(large_entry_size, std::byte{0x5a})}};
        auto t = storage.begin();
        REQUIRE(t.has_value());
        REQUIRE(t->append_entries(large));
        const auto handler = std::signal(SIGXFSZ, SIG_IGN);
        const int limit_set = setrlimit(RLIMIT_FSIZE, &limited);
        std::optional<std::error_code> thrown;
        try {
            static_cast<void>(std::move(*t).commit());
        } catch (const konsens::write_error& e) {
            thrown = e.code();
        }
        const int limit_reset = setrlimit(RLIMIT_FSIZE, &unlimited);
        std::signal(SIGXFSZ, handler);
        REQUIRE(limit_set == 0);
        REQUIRE(limit_reset == 0);
        REQUIRE(thrown);
        CHECK(*thrown == std::errc::file_too_large);
    }
    const auto storage = open_storage(dir.path());
    CHECK(meta_of(storage).log_len == 0u);
}

// The owner decided that a start as root is refused.
TEST_CASE("open refuses the root user")
{
    const temporary_directory dir;
    const auto opened = konsens::persistent_storage::open(dir.path(), max_size, root_user_id);
    REQUIRE_FALSE(opened.has_value());
    CHECK(opened.error() == std::errc::operation_not_permitted);
}

// A user gives a directory that does not exist. The error is a value that
// says so, and not a crash.
TEST_CASE("open of a missing directory gives no_such_file_or_directory")
{
    const temporary_directory dir;
    const auto opened = konsens::persistent_storage::open(dir.path() / "missing", max_size, non_root_user_id);
    REQUIRE_FALSE(opened.has_value());
    CHECK(opened.error() == std::errc::no_such_file_or_directory);
}

// A rollback in the simulation restores a copy that was made while the node
// ran. The copy must hold the state of the last commit, and a write after the
// copy must not reach it. The copy is VACUUM INTO on the reader, which
// copies the state of its read transaction.
TEST_CASE("copy keeps the state of the last commit and not a later write")
{
    const temporary_directory dir;
    const temporary_directory copied;
    const std::vector<konsens::entry> written{normal_entry("first"), normal_entry("second")};
    const konsens::ballot promise{1, 7, 3, 2};
    {
        auto storage = open_storage(dir.path());
        write(storage, [&](auto& t) {
            REQUIRE(t.append_entries(written));
            REQUIRE(t.set_promise(promise));
            REQUIRE(t.set_decided_index(1));
        });
        REQUIRE(storage.copy(copied.path()).has_value());
        const std::vector<konsens::entry> later_entries{normal_entry("later")};
        write(storage, [&](auto& t) {
            REQUIRE(t.append_entries(later_entries));
            REQUIRE(t.set_decided_index(3));
        });
    }
    const auto storage = open_storage(copied.path());
    const auto m = meta_of(storage);
    CHECK(m.log_len == 2u);
    CHECK(entries_of(storage, 0, 3) == written);
    CHECK(m.promise == promise);
    CHECK(m.decided_idx == 1u);
}

// A user copies twice into the same directory. The storage does not
// overwrite a copy, and the second copy is an error value, not a crash.
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
    CHECK(meta_of(moved).log_len == 0u);
    CHECK_THROWS_AS((void)storage.begin(), std::logic_error);
    CHECK_THROWS_AS((void)storage.begin_read_only(), std::logic_error);
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
            {.kind = konsens::entry_kind::expire_session, .session = 0x2122232425262728},
            {.kind = konsens::entry_kind::client_request,
             .payload = bytes_of("done"),
             .session = 3,
             .request_number = 0x3132333435363738,
             .result = bytes_of("result"),
             .result_crc = 0x41424344,
             .out = {1, 0x5152535455565758}},
            {.kind = konsens::entry_kind::register_client,
             .client_id = 4,
             .pid = 5,
             .result = bytes_of("answers"),
             .result_crc = 6,
             .rejected = true}};
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
        write(storage, [&](auto& t) { REQUIRE(t.append_entries(written)); });
    }
    const auto storage = open_storage(dir.path());
    CHECK(meta_of(storage).log_len == written.size());
    CHECK(entries_of(storage, 0, written.size()) == written);
    CHECK(entries_of(storage, 2, 4) == std::vector<konsens::entry>{written[2], written[3]});
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
        {.kind = konsens::entry_kind::normal, .result = bytes_of("r")},
        {.kind = konsens::entry_kind::normal, .result_crc = 1},
        {.kind = konsens::entry_kind::stop_sign, .out = {1}},
        {.kind = konsens::entry_kind::stop_sign, .rejected = true},
        {static_cast<konsens::entry_kind>(0xff), {}},
    };
    for (const auto& e : wrong) {
        const std::vector<konsens::entry> with_a_good_one{normal_entry("good"), e};
        auto t = storage.begin();
        REQUIRE(t.has_value());
        CHECK_THROWS_AS((void)t->append_entries(with_a_good_one), std::logic_error);
        CHECK_THROWS_AS((void)t->append_on_prefix(0, with_a_good_one), std::logic_error);
        CHECK(t->get_meta()->log_len == 0u);
    }
    CHECK(meta_of(storage).log_len == 0u);
}

namespace {

struct stored_state {
    std::vector<konsens::entry> log;
    konsens::meta_record meta;

    friend bool operator==(const stored_state&, const stored_state&) = default;
};

stored_state stored_state_of(const konsens::persistent_storage& s)
{
    const auto m = meta_of(s);
    const auto log = entries_of(s, 0, m.log_len);
    REQUIRE(log);
    return {*log, m};
}

using one_write = std::function<void(konsens::persistent_storage::transaction&)>;

std::vector<one_write> writes_of_one_step(const std::vector<konsens::entry>& first,
                                          const std::vector<konsens::entry>& replaced,
                                          const std::vector<konsens::entry>& last)
{
    return {[](auto& t) { REQUIRE(t.set_promise({1, 2, 0, 3})); },
            [&first](auto& t) { REQUIRE(t.append_entries(first)); },
            [](auto& t) { REQUIRE(t.set_accepted_round({1, 2, 0, 3})); },
            [](auto& t) { REQUIRE(t.set_decided_index(1)); },
            [&replaced](auto& t) { REQUIRE(t.append_on_prefix(2, replaced)); },
            [](auto& t) { REQUIRE(t.set_decided_index(3)); },
            [&last](auto& t) { REQUIRE(t.append_entries(last)); },
            [](auto& t) { REQUIRE(t.set_promise({1, 4, 0, 1})); }};
}

}

// R7 of the review of doc/plan-group-commit.md: a step writes all its writes
// in one transaction, where each handler wrote its own before. The store
// must apply the writes of one transaction in order, an append_on_prefix
// after an append_entries of the same transaction included, so that one
// step leaves the store as the same writes in one transaction each do.
TEST_CASE("the writes of one step in one transaction leave the store as one transaction for each write does")
{
    const temporary_directory batched_dir;
    const temporary_directory single_dir;
    auto batched = open_storage(batched_dir.path());
    auto single = open_storage(single_dir.path());
    const std::vector<konsens::entry> first{normal_entry("a"), normal_entry("b"), normal_entry("c")};
    const std::vector<konsens::entry> replaced{normal_entry("x"), normal_entry("y")};
    const std::vector<konsens::entry> last{normal_entry("z")};
    const auto writes = writes_of_one_step(first, replaced, last);
    write(batched, [&writes](auto& t) {
        for (const auto& w : writes) {
            w(t);
        }
    });
    for (const auto& w : writes) {
        write(single, w);
    }
    CHECK(stored_state_of(batched) == stored_state_of(single));
    CHECK(stored_state_of(batched).log ==
          std::vector{normal_entry("a"), normal_entry("b"), normal_entry("x"), normal_entry("y"), normal_entry("z")});
}

// Plan one-environment 3: the storage keeps the keys of the machine, the
// client table and the applied point beside the log. A write becomes
// visible to a read only after its commit, and an empty storage has the
// applied point of an empty log.
TEST_CASE("the storage gives back the state that a committed write wrote")
{
    const temporary_directory dir;
    auto storage = open_storage(dir.path());
    const std::array key{std::byte{1}, std::byte{2}};
    const std::array value{std::byte{3}};
    const konsens::session_record session{4, {std::byte{5}, std::byte{6}}};
    const auto applied_of = [](const konsens::meta_record& m) {
        return std::tuple{m.applied_idx, m.applied_chain_crc};
    };
    {
        const auto r = storage.begin_read_only();
        REQUIRE(r.has_value());
        const auto m = r->get_meta();
        REQUIRE(m.has_value());
        CHECK(applied_of(*m) == std::tuple{std::uint64_t{0}, std::uint32_t{0}});
        CHECK(finds_no_value(r->find(key)));
        CHECK(r->find_session(7).value() == std::nullopt);
    }
    auto w = storage.begin();
    REQUIRE(w.has_value());
    REQUIRE(w->insert_or_assign(key, value));
    REQUIRE(w->insert_or_assign_session(7, session));
    REQUIRE(w->set_applied(9, 10));
    {
        const auto r = storage.begin_read_only();
        REQUIRE(r.has_value());
        const auto m = r->get_meta();
        REQUIRE(m.has_value());
        CHECK(applied_of(*m) == std::tuple{std::uint64_t{0}, std::uint32_t{0}});
        CHECK(finds_no_value(r->find(key)));
    }
    REQUIRE(std::move(*w).commit());
    const auto r = storage.begin_read_only();
    REQUIRE(r.has_value());
    const auto m = r->get_meta();
    REQUIRE(m.has_value());
    CHECK(applied_of(*m) == std::tuple{std::uint64_t{9}, std::uint32_t{10}});
    const auto found = r->find(key);
    REQUIRE(found.has_value());
    REQUIRE(found->has_value());
    CHECK(std::ranges::equal(**found, value));
    CHECK(r->find_session(7).value() == session);
}

// Plan state-in-lmdb 1.2: an erase removes a key and a session, and an
// erase of an absent key is no error, as for std::map.
TEST_CASE("the storage erases a key and a session")
{
    const temporary_directory dir;
    auto storage = open_storage(dir.path());
    const std::array key{std::byte{1}};
    {
        auto w = storage.begin();
        REQUIRE(w.has_value());
        REQUIRE(w->insert_or_assign(key, key));
        REQUIRE(w->insert_or_assign_session(2, {1, {}}));
        REQUIRE(std::move(*w).commit());
    }
    auto w = storage.begin();
    REQUIRE(w.has_value());
    REQUIRE(w->erase(key));
    REQUIRE(w->erase(key));
    REQUIRE(w->erase_session(2));
    REQUIRE(w->erase_session(2));
    REQUIRE(std::move(*w).commit());
    const auto r = storage.begin_read_only();
    REQUIRE(r.has_value());
    CHECK(finds_no_value(r->find(key)));
    CHECK(r->find_session(2).value() == std::nullopt);
}

// Plan one-environment 4: the runs of the requests write into a nested
// transaction. It sees the writes of its parent, and its end drops its own
// writes, so the step commits only what the parent wrote.
TEST_CASE("a nested transaction sees its parent and its writes end with it")
{
    const temporary_directory dir;
    auto storage = open_storage(dir.path());
    const std::array parent_key{std::byte{1}};
    const std::array nested_key{std::byte{2}};
    auto w = storage.begin();
    REQUIRE(w.has_value());
    REQUIRE(w->insert_or_assign(parent_key, parent_key));
    {
        auto n = w->begin_nested();
        REQUIRE(n.has_value());
        const auto seen = n->find(parent_key);
        REQUIRE(seen.has_value());
        REQUIRE(seen->has_value());
        CHECK(std::ranges::equal(**seen, parent_key));
        REQUIRE(n->insert_or_assign(nested_key, nested_key));
        REQUIRE(n->insert_or_assign_session(3, {1, {}}));
        REQUIRE(n->erase(parent_key));
        CHECK(finds_no_value(n->find(parent_key)));
    }
    const auto kept = w->find(parent_key);
    REQUIRE(kept.has_value());
    CHECK(kept->has_value());
    CHECK(finds_no_value(w->find(nested_key)));
    CHECK(w->find_session(3).value() == std::nullopt);
    REQUIRE(std::move(*w).commit());
    const auto r = storage.begin_read_only();
    REQUIRE(r.has_value());
    CHECK(finds_no_value(r->find(nested_key)));
}

// A savepoint holds no state of its own. A write through the parent while
// the nested transaction is open would land inside the savepoint, and its
// rollback would remove it. Such a use is a wrong use of the library.
TEST_CASE("a use of the parent while a nested transaction is open is a wrong use")
{
    const temporary_directory dir;
    auto storage = open_storage(dir.path());
    const std::array key{std::byte{1}};
    auto w = storage.begin();
    REQUIRE(w.has_value());
    auto n = w->begin_nested();
    REQUIRE(n.has_value());
    CHECK_THROWS_AS(static_cast<void>(w->find(key)), std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(w->insert_or_assign(key, key)), std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(w->get_meta()), std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(w->begin_nested()), std::logic_error);
}

// The node commits only a transaction that wrote something. A read sees
// the writes only after the commit.
TEST_CASE("a transaction has writes after its first write and shows them after its commit")
{
    const temporary_directory dir;
    auto storage = open_storage(dir.path());
    auto w = storage.begin();
    REQUIRE(w.has_value());
    CHECK_FALSE(w->has_writes());
    const std::vector written{normal_entry("a")};
    REQUIRE(w->append_entries(written));
    CHECK(w->has_writes());
    CHECK(meta_of(storage).log_len == 0u);
    REQUIRE(std::move(*w).commit());
    CHECK(meta_of(storage).log_len == 1u);
    CHECK(entries_of(storage, 0, 1) == written);
}

// Plan sqlite-storage 5.2: a reader sees one state from its begin to its
// end. A step of the writer that commits while the reader is open does not
// change what the reader reads.
TEST_CASE("a reader keeps its state while the writer commits a step")
{
    const temporary_directory dir;
    auto storage = open_storage(dir.path());
    const std::vector first{normal_entry("first")};
    write(storage, [&](auto& t) { REQUIRE(t.append_entries(first)); });
    const auto r = storage.begin_read_only();
    REQUIRE(r.has_value());
    CHECK(r->get_meta()->log_len == 1u);
    const std::vector second{normal_entry("second")};
    write(storage, [&](auto& t) { REQUIRE(t.append_entries(second)); });
    CHECK(r->get_meta()->log_len == 1u);
    CHECK(r->get_entries(0, 1) == first);
}

// Plan sqlite-storage 5.1 item 3: SQLite rolls back a whole transaction
// after SQLITE_FULL in a savepoint, and later statements would then commit
// alone. No write of the step may reach the file: not the write of the
// parent, and not the write of the nested transaction.
TEST_CASE("a full store in a nested transaction leaves no write of the step in the file")
{
    const temporary_directory dir;
    {
        auto opened = konsens::persistent_storage::open(dir.path(), tiny_max_size, non_root_user_id);
        REQUIRE(opened.has_value());
        auto& storage = *opened;
        auto t = storage.begin();
        REQUIRE(t.has_value());
        const auto parent_key = bytes_of("parent");
        REQUIRE(t->insert_or_assign(parent_key, parent_key));
        {
            auto w = t->begin_nested();
            REQUIRE(w.has_value());
            const std::vector<std::byte> large(tiny_max_size, std::byte{0x5a});
            const auto written = w->insert_or_assign(bytes_of("nested"), large);
            REQUIRE_FALSE(written.has_value());
            CHECK(written.error() == std::errc::no_space_on_device);
            const auto after = w->insert_or_assign(bytes_of("after"), parent_key);
            CHECK_FALSE(after.has_value());
        }
        const auto committed = std::move(*t).commit();
        CHECK_FALSE(committed.has_value());
    }
    const auto storage = open_storage(dir.path());
    const auto r = storage.begin_read_only();
    REQUIRE(r.has_value());
    CHECK(finds_no_value(r->find(bytes_of("parent"))));
    CHECK(finds_no_value(r->find(bytes_of("nested"))));
    CHECK(finds_no_value(r->find(bytes_of("after"))));
}

// Plan sqlite-storage 7.3: a damaged frame in the middle of the WAL drops
// its commit and every later commit without an error. The store then holds
// the older state of the commits before the frame, and that state is whole.
// The store closes without a checkpoint (plan section 13), so the frames
// stay in the WAL across the close.
TEST_CASE("a flipped byte in an early frame of the WAL gives the state before its commit")
{
    const temporary_directory dir;
    const std::vector first{normal_entry("first")};
    std::uint64_t frames_of_the_first_commit = 0;
    {
        auto storage = open_storage(dir.path());
        write(storage, [&](auto& t) { REQUIRE(t.append_entries(first)); });
        frames_of_the_first_commit = sqlite_damage::wal_frames_of(dir.path());
        for (const auto* text : {"second", "third", "fourth"}) {
            const std::vector later{normal_entry(text)};
            write(storage, [&](auto& t) { REQUIRE(t.append_entries(later)); });
        }
        REQUIRE(sqlite_damage::wal_frames_of(dir.path()) > frames_of_the_first_commit);
    }
    sqlite_damage::flip_a_byte_of_the_wal_frame(dir.path(), frames_of_the_first_commit);
    const auto storage = open_storage(dir.path());
    CHECK(meta_of(storage).log_len == 1u);
    CHECK(entries_of(storage, 0, 1) == first);
}

// Plan sqlite-storage 8.4: the files of the store and of the node storage
// hold the log and the identity of the node, and no other user reads them.
TEST_CASE("the store, its WAL and the node storage have the mode 0600")
{
    const temporary_directory dir;
    {
        auto storage = open_storage(dir.path());
        const std::vector one{normal_entry("one")};
        write(storage, [&](auto& t) { REQUIRE(t.append_entries(one)); });
        auto node = konsens::node_storage::open(dir.path(), non_root_user_id);
        REQUIRE(node.has_value());
        const auto owner_only = std::filesystem::perms::owner_read | std::filesystem::perms::owner_write;
        for (const auto* name : {"konsens.db", "konsens.db-wal", "node.db"}) {
            INFO(name);
            CHECK(std::filesystem::status(dir.path() / name).permissions() == owner_only);
        }
    }
}

// Plan clean-stop 2.1 and 4.1: the mark lives in node.db only. A start
// reads it once and deletes it, so the deletion must survive a close. A
// copy of konsens.db is a backup, and a backup never carries the mark.
TEST_CASE("the clean-stop mark survives a close, its deletion too, and a copy has none")
{
    const temporary_directory dir;
    const temporary_directory copied;
    const konsens::clean_stop_mark mark{
        3, 2, konsens::meta_record{konsens::ballot{2, 7, 0, 3}, konsens::ballot{2, 7, 0, 3}, 4, 5, 0xabcdef01, 4, 0x1234},
        5,
    };
    {
        auto storage = open_storage(dir.path());
        auto node = konsens::node_storage::open(dir.path(), non_root_user_id);
        REQUIRE(node.has_value());
        const auto none = node->get_clean_stop_mark();
        REQUIRE(none.has_value());
        CHECK_FALSE(none->has_value());
        REQUIRE(node->write_the_clean_stop_mark(mark).has_value());
        REQUIRE(storage.copy(copied.path()).has_value());
    }
    {
        auto node = konsens::node_storage::open(dir.path(), non_root_user_id);
        REQUIRE(node.has_value());
        const auto read = node->get_clean_stop_mark();
        REQUIRE(read.has_value());
        REQUIRE(read->has_value());
        CHECK(**read == mark);
        REQUIRE(node->delete_the_clean_stop_mark().has_value());
    }
    {
        const auto node = konsens::node_storage::open(dir.path(), non_root_user_id);
        REQUIRE(node.has_value());
        const auto read = node->get_clean_stop_mark();
        REQUIRE(read.has_value());
        CHECK_FALSE(read->has_value());
    }
    const auto node = konsens::node_storage::open(copied.path(), non_root_user_id);
    REQUIRE(node.has_value());
    const auto read = node->get_clean_stop_mark();
    REQUIRE(read.has_value());
    CHECK_FALSE(read->has_value());
}

namespace {

// The shell of SQLite runs as the operator runs it (plan sqlite-storage
// 7.7): its standard input and output are files.
void run_the_sqlite_shell(const std::filesystem::path& database, const std::filesystem::path& input,
                          const std::filesystem::path& output)
{
    posix_spawn_file_actions_t actions;
    REQUIRE(posix_spawn_file_actions_init(&actions) == 0);
    REQUIRE(posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, input.c_str(), O_RDONLY, 0) == 0);
    REQUIRE(posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, output.c_str(), O_WRONLY | O_CREAT | O_TRUNC,
                                             S_IRUSR | S_IWUSR) == 0);
    std::string program = KONSENS_SQLITE3_SHELL;
    std::string file = database.string();
    std::array<char*, 3> argv{program.data(), file.data(), nullptr};
    std::array<char*, 1> envp{nullptr};
    pid_t pid = 0;
    const int spawned = posix_spawn(&pid, program.c_str(), &actions, nullptr, argv.data(), envp.data());
    posix_spawn_file_actions_destroy(&actions);
    REQUIRE(spawned == 0);
    int status = 0;
    REQUIRE(waitpid(pid, &status, 0) == pid);
    REQUIRE(WIFEXITED(status));
    CHECK(WEXITSTATUS(status) == 0);
}

std::vector<std::int64_t> root_pages_of(const std::filesystem::path& directory)
{
    std::vector<std::int64_t> roots;
    sqlite3* db = nullptr;
    const auto file = (directory / sqlite_damage::store_file_name).string();
    REQUIRE(sqlite3_open_v2(file.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK);
    sqlite3_stmt* s = nullptr;
    REQUIRE(sqlite3_prepare_v2(db, "SELECT rootpage FROM sqlite_schema", -1, &s, nullptr) == SQLITE_OK);
    while (sqlite3_step(s) == SQLITE_ROW) {
        roots.push_back(sqlite3_column_int64(s, 0));
    }
    sqlite3_finalize(s);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
    return roots;
}

}

// Plan sqlite-storage 7.7: the operator repairs a damaged store with
// .recover of the SQLite shell on a clone. Each record that .recover gives
// back must still pass its CRC, and a record on the damaged page may be
// lost. The test damages a leaf page of the log that is no root page: the
// first byte of a B-tree page is its type, 13 is a leaf of a table
// (fileformat2.html, section 1.6), and only the log has more than one page.
// Page 1 starts with the header of the file, so its first byte is no type.
TEST_CASE("recover on a damaged clone gives back records whose CRC holds")
{
    constexpr char table_leaf_page = 13;
    constexpr char no_page_type = static_cast<char>(0xff);
    constexpr std::uint64_t entry_count = 400;
    const temporary_directory dir;
    const auto clone = dir.path() / "clone";
    const auto repaired = dir.path() / "repaired";
    std::filesystem::create_directory(clone);
    std::filesystem::create_directory(repaired);
    std::vector<konsens::entry> written;
    for (std::uint64_t i = 0; i < entry_count; ++i) {
        written.push_back(normal_entry(std::string(100, static_cast<char>('a' + i % 26))));
    }
    {
        auto storage = open_storage(dir.path());
        write(storage, [&](auto& t) { REQUIRE(t.append_entries(written)); });
    }
    sqlite_damage::move_the_wal_into_the_file(dir.path());
    std::filesystem::copy_file(dir.path() / sqlite_damage::store_file_name, clone / sqlite_damage::store_file_name);
    const auto page_size = sqlite_damage::page_size_and_root_page_of(clone, "log").first;
    const auto roots = root_pages_of(clone);
    const auto data_file = clone / sqlite_damage::store_file_name;
    const auto pages = static_cast<std::int64_t>(std::filesystem::file_size(data_file)) / page_size;
    std::vector<std::int64_t> leaves;
    {
        std::ifstream in{data_file, std::ios::binary};
        for (std::int64_t page = 1; page <= pages; ++page) {
            in.seekg(static_cast<std::streamoff>((page - 1) * page_size));
            if (in.get() == table_leaf_page && !std::ranges::contains(roots, page)) {
                leaves.push_back(page);
            }
        }
    }
    REQUIRE(leaves.size() > 2);
    {
        std::fstream file{data_file, std::ios::binary | std::ios::in | std::ios::out};
        file.seekp(static_cast<std::streamoff>((leaves.at(leaves.size() / 2) - 1) * page_size));
        file.put(no_page_type);
    }
    const auto commands = dir.path() / "recover.sql";
    std::ofstream{dir.path() / "recover.in"} << ".recover\n";
    run_the_sqlite_shell(data_file, dir.path() / "recover.in", commands);
    run_the_sqlite_shell(repaired / sqlite_damage::store_file_name, commands, dir.path() / "load.out");
    const auto storage = open_storage(repaired);
    const auto r = storage.begin_read_only();
    REQUIRE(r.has_value());
    std::uint64_t kept = 0;
    for (std::uint64_t i = 0; i < entry_count; ++i) {
        const auto read = r->get_entries(i, i + 1);
        if (read.has_value()) {
            REQUIRE(read->size() == 1u);
            CHECK(read->front() == written.at(i));
            kept += 1;
        }
    }
    CHECK(kept < entry_count);
    CHECK(kept > entry_count / 2);
}

// Plan reconfiguration 2.7: config_first_idx is absent in a new store and in
// a store of the format before it, and a write keeps it across a new open.
TEST_CASE("config_first_idx is absent until a transaction sets it, and an open keeps it")
{
    const temporary_directory dir;
    {
        const auto storage = open_storage(dir.path());
        const auto t = storage.begin_read_only();
        REQUIRE(t.has_value());
        const auto first = t->get_config_first_idx();
        REQUIRE(first.has_value());
        CHECK_FALSE(first->has_value());
    }
    {
        auto storage = open_storage(dir.path());
        write(storage, [](auto& t) { REQUIRE(t.set_config_first_idx(12)); });
    }
    const auto storage = open_storage(dir.path());
    const auto t = storage.begin_read_only();
    REQUIRE(t.has_value());
    const auto first = t->get_config_first_idx();
    REQUIRE(first.has_value());
    CHECK(*first == std::optional<std::uint64_t>{12});
}
