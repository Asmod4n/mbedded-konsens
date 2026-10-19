#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

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

std::vector<std::byte> bytes_of(const std::string_view text) {
    std::vector<std::byte> result;
    std::ranges::transform(text, std::back_inserter(result), [](const char c) { return static_cast<std::byte>(c); });
    return result;
}

konsens::entry normal_entry(const std::string_view text) {
    return {konsens::entry_kind::normal, bytes_of(text)};
}

konsens::persistent_storage open_storage(const std::filesystem::path& directory) {
    auto opened = konsens::persistent_storage::open(directory, map_size, non_root_user_id);
    REQUIRE(opened.has_value());
    return std::move(*opened);
}

}

// The durable state must survive a close and a reopen. This is what Crash in
// the model assumes, and the model does not check it.
TEST_CASE("entries and meta values survive a close and a reopen") {
    const temporary_directory dir;
    const std::vector<konsens::entry> written{normal_entry("first"), normal_entry("second"),
                                              {konsens::entry_kind::stop_sign, {}}};
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
TEST_CASE("a new storage has an empty log and zero meta values") {
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
TEST_CASE("append_on_prefix truncates the log and appends") {
    const temporary_directory dir;
    auto storage = open_storage(dir.path());
    const std::vector<konsens::storage_op> first{
        konsens::append_entries{{normal_entry("a"), normal_entry("b"), normal_entry("c"), normal_entry("d")}}};
    REQUIRE(storage.write_atomically(first).has_value());
    const std::vector<konsens::storage_op> second{
        konsens::append_on_prefix{2, {normal_entry("x"), normal_entry("y"), normal_entry("z")}}};
    REQUIRE(storage.write_atomically(second).has_value());
    CHECK(storage.get_log_len() == 5u);
    CHECK(storage.get_entries(0, 5) == std::vector<konsens::entry>{normal_entry("a"), normal_entry("b"),
                                                                    normal_entry("x"), normal_entry("y"),
                                                                    normal_entry("z")});
    CHECK(storage.get_entries(1, 3) == std::vector<konsens::entry>{normal_entry("b"), normal_entry("x")});
}

// The two storages follow one rule for get_entries, so that Sequence
// Paxos reads the same entries from each. A range that reaches past the
// log gives the stored part of it. A start at or past the end gives no
// entries. A start after the end of the range is a wrong use.
TEST_CASE("get_entries returns the stored part of the range") {
    const temporary_directory dir;
    auto storage = open_storage(dir.path());
    const std::vector<konsens::storage_op> ops{konsens::append_entries{{normal_entry("a"), normal_entry("b")}}};
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
TEST_CASE("a flipped byte in a stored entry is a CRC mismatch at its index") {
    const temporary_directory dir;
    constexpr std::string_view marked = "konsens-marked-payload";
    {
        auto storage = open_storage(dir.path());
        const std::vector<konsens::storage_op> ops{
            konsens::append_entries{{normal_entry("zero"), normal_entry(marked), normal_entry("two")}}};
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

// A full disk is an error value, and the node keeps its state. The failed
// write must leave nothing behind, because write_atomically is one
// transaction.
TEST_CASE("a full map gives no_space_on_device and writes nothing") {
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
    const std::vector<konsens::storage_op> small{konsens::append_entries{{normal_entry("fits")}}};
    CHECK(storage.write_atomically(small).has_value());
    CHECK(storage.get_log_len() == 1u);
}

// The owner decided that a start as root is refused.
TEST_CASE("open refuses the root user") {
    const temporary_directory dir;
    const auto opened = konsens::persistent_storage::open(dir.path(), map_size, root_user_id);
    REQUIRE_FALSE(opened.has_value());
    CHECK(opened.error() == std::errc::operation_not_permitted);
}

// A user gives a directory that does not exist. The error is a value that
// says so, and not a crash.
TEST_CASE("open of a missing directory gives no_such_file_or_directory") {
    const temporary_directory dir;
    const auto opened = konsens::persistent_storage::open(dir.path() / "missing", map_size, non_root_user_id);
    REQUIRE_FALSE(opened.has_value());
    CHECK(opened.error() == std::errc::no_such_file_or_directory);
}

// A storage that was moved from has no environment. A use of it is a wrong
// use, and a wrong use fails loudly and reads no memory that is not alive.
TEST_CASE("a moved-from storage throws logic_error") {
    const temporary_directory dir;
    auto storage = open_storage(dir.path());
    const auto moved = std::move(storage);
    CHECK(moved.get_log_len() == 0u);
    CHECK_THROWS_AS((void)storage.get_log_len(), std::logic_error);
}

namespace {

// One entry of each kind, each with every field that its kind carries set
// to a value that is not zero, so that a field the encoding drops or moves
// shows in the comparison.
std::vector<konsens::entry> entries_of_each_kind() {
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
TEST_CASE("an entry of each kind survives a close and a reopen") {
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
TEST_CASE("an entry with a field of another kind is a wrong use") {
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
        const std::vector<konsens::storage_op> ops{konsens::append_entries{{e}}};
        CHECK_THROWS_AS((void)storage.write_atomically(ops), std::logic_error);
        const std::vector<konsens::storage_op> on_prefix{konsens::append_on_prefix{0, {e}}};
        CHECK_THROWS_AS((void)storage.write_atomically(on_prefix), std::logic_error);
    }
    CHECK(storage.get_log_len() == 0u);
}
