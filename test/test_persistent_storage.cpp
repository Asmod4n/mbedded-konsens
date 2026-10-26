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

#include <sys/resource.h>

#include <crc32c/crc32c.h>
#include <lmdb.h>

#include "konsens/persistent_storage.hpp"
#include "lmdb_category.hpp"
#include "lmdb_checksum.hpp"
#include "lmdb_damage.hpp"
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

using lmdb_damage::change_with_lmdb;
using lmdb_damage::delete_key;
using lmdb_damage::key_of;

void delete_entry(const std::filesystem::path& directory, const std::uint64_t index)
{
    change_with_lmdb(directory,
                     [index](MDB_txn* txn, const MDB_dbi log, MDB_dbi) { delete_key(txn, log, key_of(index)); });
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

// LMDB has one writer for each environment, and a thread that begins a
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

// Plan lmdb-only-source 2.1 item 11: LMDB checks the checksum of each page
// that it reads. A byte that changes on the disk is found by LMDB before
// the CRC of the entry sees it, and the read fails. A test of LMDB alone in
// this session showed that the first read of a new transaction can give
// MDB_BAD_TXN and not MDB_BAD_CHECKSUM: mdb_cursor_open meets the bad page,
// marks the transaction as failed and returns success. So both codes are a
// failed read here. The meta record and the few entries of this test lie on
// one page, so the open reads the changed page first and fails.
TEST_CASE("a flipped byte in a stored page fails the page checksum")
{
    const temporary_directory dir;
    constexpr std::string_view marked = "konsens-marked-payload";
    {
        auto storage = open_storage(dir.path());
        const std::vector written{normal_entry("zero"), normal_entry(marked), normal_entry("two")};
        write(storage, [&](auto& t) { REQUIRE(t.append_entries(written)); });
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
    const auto opened = konsens::persistent_storage::open(dir.path(), map_size, non_root_user_id);
    REQUIRE_FALSE(opened.has_value());
    CHECK(opened.error().category() == konsens::lmdb_category());
    CHECK((opened.error().value() == MDB_BAD_CHECKSUM || opened.error().value() == MDB_BAD_TXN));
}

// A value whose page checksum is valid but whose bytes do not agree with the
// CRC of the entry must not reach the protocol as a valid entry. The error
// names the index of the entry.
TEST_CASE("a changed value with a valid page checksum is a CRC mismatch at its index")
{
    const temporary_directory dir;
    {
        auto storage = open_storage(dir.path());
        const std::vector written{normal_entry("zero"), normal_entry("one"), normal_entry("two")};
        write(storage, [&](auto& t) { REQUIRE(t.append_entries(written)); });
    }
    change_with_lmdb(dir.path(), [](MDB_txn* txn, const MDB_dbi log, MDB_dbi) {
        const auto key_bytes = key_of(1);
        MDB_val key{key_bytes.size(), const_cast<std::byte*>(key_bytes.data())};
        MDB_val data{};
        REQUIRE(mdb_get(txn, log, &key, &data) == MDB_SUCCESS);
        std::vector<std::byte> changed(static_cast<const std::byte*>(data.mv_data),
                                       static_cast<const std::byte*>(data.mv_data) + data.mv_size);
        changed.at(1) ^= std::byte{0x01};
        MDB_val replaced{changed.size(), changed.data()};
        REQUIRE(mdb_put(txn, log, &key, &replaced, 0) == MDB_SUCCESS);
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
// The log shares its database with the meta record and the client table, so
// the count of keys does not give the length of the log, and the read of the
// last entry does not see the hole. The start scan finds it.
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
    change_with_lmdb(dir.path(), [](MDB_txn* txn, MDB_dbi, const MDB_dbi meta) {
        delete_key(txn, meta, std::as_bytes(std::span(lmdb_damage::meta_key)));
    });
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
    change_with_lmdb(dir.path(), [](MDB_txn* txn, MDB_dbi, const MDB_dbi meta) {
        constexpr std::string_view name{"\0format", 7};
        const std::array<unsigned char, 8> version_4{4, 0, 0, 0, 0, 0, 0, 0};
        const auto crc = crc32c::Crc32c(version_4.data(), 4);
        std::array<unsigned char, 8> value = version_4;
        for (std::size_t i = 0; i < 4; ++i) {
            value.at(4 + i) = static_cast<unsigned char>(crc >> (8 * i));
        }
        MDB_val key{name.size(), const_cast<char*>(name.data())};
        MDB_val data{value.size(), value.data()};
        REQUIRE(mdb_put(txn, meta, &key, &data, 0) == MDB_SUCCESS);
    });
    const auto other = konsens::persistent_storage::open(dir.path(), map_size, non_root_user_id);
    REQUIRE_FALSE(other.has_value());
    CHECK(other.error() == konsens::storage_errc::unsupported_format);
    change_with_lmdb(dir.path(), [](MDB_txn* txn, MDB_dbi, const MDB_dbi meta) {
        constexpr std::string_view name{"\0format", 7};
        delete_key(txn, meta, std::as_bytes(std::span(name)));
    });
    const auto none = konsens::persistent_storage::open(dir.path(), map_size, non_root_user_id);
    REQUIRE_FALSE(none.has_value());
    CHECK(none.error() == konsens::storage_errc::unsupported_format);
}

// Recovery after a power loss compares the CRC32C of the last entry of two
// nodes, so the value must follow the record format and nothing else: the
// index in 8 bytes little endian, the kind in one byte, the fields of the
// kind in 8 bytes little endian each, then the payload. The chain CRC of an
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
         {2, 7, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0}},
        {{.kind = konsens::entry_kind::expire_session, .session = 1}, {4, 1, 0, 0, 0, 0, 0, 0, 0}},
        {{konsens::entry_kind::client_request, bytes_of("x"), 1, 2},
         {3, 1, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 'x'}},
        {{.kind = konsens::entry_kind::results, .payload = bytes_of("y"), .from_idx = 4, .to_idx = 5},
         {5, 4, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 'y'}},
        {{.kind = konsens::entry_kind::rejected, .payload = bytes_of("z"), .from_idx = 6},
         {6, 6, 0, 0, 0, 0, 0, 0, 0, 'z'}}};
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

// An entry of more than about 2000 bytes lies on LMDB overflow pages. A
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

// The recovery writes the whole log of the primary again from index 0. An
// entry that it writes below the decided index is a decided entry, so it is
// written committed.
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

// A full map is an error value of a write, which the node raises as
// write_error (test_group_commit.cpp). A transaction that ends without a
// commit after it leaves nothing behind.
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

// The decision of the owner (2026-10-21): an LMDB commit that fails is a
// failure of the machine. The commit throws sync_error with the error
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
    auto t = storage.begin();
    REQUIRE(t.has_value());
    REQUIRE(t->append_entries(large));
    const auto handler = std::signal(SIGXFSZ, SIG_IGN);
    const int limit_set = setrlimit(RLIMIT_FSIZE, &limited);
    std::optional<std::error_code> thrown;
    try {
        static_cast<void>(std::move(*t).commit());
    } catch (const konsens::sync_error& e) {
        thrown = e.code();
    }
    const int limit_reset = setrlimit(RLIMIT_FSIZE, &unlimited);
    std::signal(SIGXFSZ, handler);
    REQUIRE(limit_set == 0);
    REQUIRE(limit_reset == 0);
    REQUIRE(thrown);
    CHECK((*thrown == std::errc::file_too_large || *thrown == std::errc::io_error));
    CHECK(meta_of(storage).log_len == 0u);
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
// copy must not reach it. The copy is the plain copy of LMDB, because a
// compact copy of an environment with page checksums does not open (plan
// lmdb-only-source 2.4 item 9).
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
            {.kind = konsens::entry_kind::results,
             .payload = bytes_of("results"),
             .from_idx = 0x3132333435363738,
             .to_idx = 0x4142434445464748},
            {.kind = konsens::entry_kind::rejected, .payload = bytes_of("rejected"), .from_idx = 0x5152535455565758}};
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
        {.kind = konsens::entry_kind::normal, .from_idx = 1},
        {.kind = konsens::entry_kind::client_request, .session = 1, .request_number = 1, .to_idx = 1},
        {.kind = konsens::entry_kind::results, .session = 1, .from_idx = 1, .to_idx = 1},
        {.kind = konsens::entry_kind::rejected, .from_idx = 1, .to_idx = 1},
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
        return std::tuple{m.applied_idx, m.applied_chain_crc, m.pointer};
    };
    {
        const auto r = storage.begin_read_only();
        REQUIRE(r.has_value());
        const auto m = r->get_meta();
        REQUIRE(m.has_value());
        CHECK(applied_of(*m) == std::tuple{std::uint64_t{0}, std::uint32_t{0}, std::uint64_t{0}});
        CHECK(finds_no_value(r->find(key)));
        CHECK(r->find_session(7).value() == std::nullopt);
    }
    auto w = storage.begin();
    REQUIRE(w.has_value());
    REQUIRE(w->insert_or_assign(key, value));
    REQUIRE(w->insert_or_assign_session(7, session));
    REQUIRE(w->set_applied(9, 10, 8));
    {
        const auto r = storage.begin_read_only();
        REQUIRE(r.has_value());
        const auto m = r->get_meta();
        REQUIRE(m.has_value());
        CHECK(applied_of(*m) == std::tuple{std::uint64_t{0}, std::uint32_t{0}, std::uint64_t{0}});
        CHECK(finds_no_value(r->find(key)));
    }
    REQUIRE(std::move(*w).commit());
    const auto r = storage.begin_read_only();
    REQUIRE(r.has_value());
    const auto m = r->get_meta();
    REQUIRE(m.has_value());
    CHECK(applied_of(*m) == std::tuple{std::uint64_t{9}, std::uint32_t{10}, std::uint64_t{8}});
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

// The applied point stands in the meta record. A repair that finds no meta
// record does not know which entries the state holds, so it deletes the
// state and the client table and sets the applied point to the start. The
// next start applies the decided entries again.
TEST_CASE("a repair without a meta record deletes the state and the client table")
{
    const temporary_directory dir;
    const std::array key{std::byte{1}};
    {
        auto storage = open_storage(dir.path());
        const std::vector written{normal_entry("a"), normal_entry("b")};
        write(storage, [&](auto& t) {
            REQUIRE(t.append_entries(written));
            REQUIRE(t.insert_or_assign(key, key));
            REQUIRE(t.insert_or_assign_session(2, {1, {}}));
            REQUIRE(t.set_decided_index(2));
            REQUIRE(t.set_applied(2, 9, 1));
        });
    }
    change_with_lmdb(dir.path(), [](MDB_txn* txn, MDB_dbi, const MDB_dbi meta) {
        lmdb_damage::delete_meta_record(txn, meta);
    });
    auto storage = open_storage(dir.path());
    const std::vector repaired{normal_entry("a"), normal_entry("b")};
    write(storage, [&](auto& t) { REQUIRE(t.write_repaired_log(0, repaired, {1, 1, 0, 1}, {1, 1, 0, 1}, 2)); });
    const auto r = storage.begin_read_only();
    REQUIRE(r.has_value());
    const auto m = r->get_meta();
    REQUIRE(m.has_value());
    CHECK(m->log_len == 2u);
    CHECK(m->applied_idx == 0u);
    CHECK(m->pointer == 0u);
    CHECK(finds_no_value(r->find(key)));
    CHECK(r->find_session(2).value() == std::nullopt);
}

// A repair with a meta record keeps the applied point and the state.
TEST_CASE("a repair with a meta record keeps the state")
{
    const temporary_directory dir;
    const std::array key{std::byte{1}};
    auto storage = open_storage(dir.path());
    const std::vector written{normal_entry("a"), normal_entry("b")};
    write(storage, [&](auto& t) {
        REQUIRE(t.append_entries(written));
        REQUIRE(t.insert_or_assign(key, key));
        REQUIRE(t.set_decided_index(2));
        REQUIRE(t.set_applied(2, 9, 1));
    });
    write(storage, [&](auto& t) { REQUIRE(t.write_repaired_log(1, std::span(written).subspan(1), {1, 1, 0, 1}, {1, 1, 0, 1}, 2)); });
    const auto r = storage.begin_read_only();
    REQUIRE(r.has_value());
    const auto m = r->get_meta();
    REQUIRE(m.has_value());
    CHECK(m->applied_idx == 2u);
    CHECK(m->applied_chain_crc == 9u);
    CHECK(m->pointer == 1u);
    const auto found = r->find(key);
    REQUIRE(found.has_value());
    CHECK(found->has_value());
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

// LMDB forbids a use of a parent transaction while its child is open
// (lmdb.h, mdb_txn_begin). Such a use is a wrong use of the library.
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
