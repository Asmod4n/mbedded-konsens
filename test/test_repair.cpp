#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include "konsens/chain_hash.hpp"
#include "konsens/sequence_paxos.hpp"
#include "sqlite_damage.hpp"
#include "sqlite_storage.hpp"

namespace {

using konsens::ballot;
using konsens::entry;
using konsens::index_range;
using konsens::node_id;
using konsens::persistent_storage;
using sp_t = konsens::sequence_paxos<persistent_storage>;

constexpr std::uint64_t first_nonce = 1;
constexpr std::uint64_t log_len = 5;
constexpr std::uint64_t decided_idx = 3;

// The tail of a record, from its end: the state CRC in 4 bytes, the state
// in 1 byte, the chain hash in 32 bytes and the record CRC in 4 bytes. The
// record CRC covers the index, the encoded entry and the chain hash.
constexpr std::size_t state_crc_size = 4;
constexpr std::size_t state_size = 1;
constexpr std::size_t record_crc_size = 4;
constexpr std::size_t state_from_the_end = state_crc_size + state_size;
constexpr std::size_t chain_from_the_end = state_from_the_end + konsens::chain_hash_size;
constexpr std::size_t record_crc_from_the_end = chain_from_the_end + record_crc_size;
constexpr std::size_t tail_size = record_crc_from_the_end;

ballot ballot_of(const std::uint32_t n, const node_id pid)
{
    return {1, n, 0, pid};
}

std::vector<node_id> peers_of(const node_id pid)
{
    std::vector<node_id> peers;
    for (node_id p = 1; p <= 3; ++p) {
        if (p != pid) {
            peers.push_back(p);
        }
    }
    return peers;
}

entry entry_of(const std::uint8_t fill)
{
    return {konsens::entry_kind::normal, std::vector(8, std::byte{fill})};
}

std::vector<entry> written_entries()
{
    std::vector<entry> written;
    for (std::uint64_t i = 0; i < log_len; ++i) {
        written.push_back(entry_of(static_cast<std::uint8_t>(0x10 + i)));
    }
    return written;
}

// A log of five entries, accepted in one round, with three of them decided,
// so that the log holds both states of a record.
std::filesystem::path directory_with_a_log()
{
    const auto directory = sqlite_storage::new_directory();
    auto storage = sqlite_storage::open_in(directory);
    sqlite_storage::write(storage, [](auto& t) {
        REQUIRE(t.append_entries(written_entries()));
        REQUIRE(t.set_promise(ballot_of(1, 2)));
        REQUIRE(t.set_accepted_round(ballot_of(1, 2)));
        REQUIRE(t.set_decided_index(decided_idx));
    });
    return directory;
}

std::vector<std::vector<std::byte>> records_in(const std::filesystem::path& directory)
{
    std::vector<std::vector<std::byte>> records;
    sqlite_damage::change_with_sqlite(directory, [&records](sqlite3* db) {
        for (std::uint64_t i = 0; i < log_len; ++i) {
            records.push_back(sqlite_damage::entry_record_of(db, i));
        }
    });
    return records;
}

void flip_a_byte(const std::filesystem::path& directory, const std::uint64_t index, const std::size_t from_the_end)
{
    sqlite_damage::change_with_sqlite(directory, [index, from_the_end](sqlite3* db) {
        auto changed = sqlite_damage::entry_record_of(db, index);
        changed.at(changed.size() - from_the_end) ^= std::byte{0x01};
        sqlite_damage::put_entry_record(db, index, changed);
    });
}

void delete_the_row(const std::filesystem::path& directory, const std::uint64_t index)
{
    sqlite_damage::change_with_sqlite(directory, [index](sqlite3* db) { sqlite_damage::delete_entry(db, index); });
}

sp_t node_on(const std::filesystem::path& directory)
{
    auto sp = sp_t::with({1, 1, peers_of(1), {}, konsens::start_kind::clean}, sqlite_storage::open_in(directory),
                         first_nonce);
    REQUIRE(sp.has_value());
    static_cast<void>(sp->take_outgoing_messages());
    return std::move(*sp);
}

std::vector<index_range> damaged_ranges_of(const sp_t& sp)
{
    const auto ranges = sp.find_the_damaged_ranges();
    REQUIRE(ranges.has_value());
    return *ranges;
}

}

// Plan of the repair, item 2: the record CRC covers the entry and the chain
// field together, so a mismatch does not say which of them changed. The
// local repair takes the entry from the record without the record CRC and
// keeps it only when the chain of the next record follows from it. A change
// of the chain field, of the record CRC, of the state or of the state CRC
// leaves the entry as it was, so the node writes the record again from its
// own bytes. The test changes the first record, a record in the middle, the
// last decided record, the first record in work and the last record, so that
// both states and both ends of the log are checked.
TEST_CASE("a flipped chain field, record CRC, state or state CRC is repaired locally")
{
    const std::vector from_the_end{chain_from_the_end, record_crc_from_the_end, state_from_the_end, state_crc_size};
    for (const auto offset : from_the_end) {
        for (std::uint64_t index = 0; index < log_len; ++index) {
            CAPTURE(offset);
            CAPTURE(index);
            const auto directory = directory_with_a_log();
            const auto before = records_in(directory);
            flip_a_byte(directory, index, offset);
            auto sp = node_on(directory);
            const auto ranges = damaged_ranges_of(sp);
            CHECK(ranges == std::vector{index_range{index, index + 1}});
            const auto left = sp.repair_locally(ranges);
            REQUIRE(left.has_value());
            CHECK(left->empty());
            CHECK(damaged_ranges_of(sp).empty());
            CHECK(records_in(directory) == before);
        }
    }
}

// A changed byte of the entry gives another entry, and the chain of the
// next record does not follow from it. The local repair must not write it:
// a repair never takes a copy that differs from the lost bytes.
TEST_CASE("a flipped byte of the entry is a range that the local repair leaves")
{
    for (std::uint64_t index = 0; index < log_len; ++index) {
        CAPTURE(index);
        const auto directory = directory_with_a_log();
        flip_a_byte(directory, index, tail_size + 1);
        const auto damaged = records_in(directory);
        auto sp = node_on(directory);
        const auto ranges = damaged_ranges_of(sp);
        CHECK(ranges == std::vector{index_range{index, index + 1}});
        const auto left = sp.repair_locally(ranges);
        REQUIRE(left.has_value());
        CHECK(*left == ranges);
        CHECK(records_in(directory) == damaged);
    }
}

// A missing row has no local copy. Two damages with a record that holds
// between them are two ranges, and two damages next to each other are one
// range, so that one request asks for each gap.
TEST_CASE("missing rows and damaged records are found as ranges of the log")
{
    const auto directory = directory_with_a_log();
    delete_the_row(directory, 0);
    flip_a_byte(directory, 2, tail_size + 1);
    flip_a_byte(directory, 3, chain_from_the_end);
    auto sp = node_on(directory);
    const auto ranges = damaged_ranges_of(sp);
    CHECK(ranges == std::vector{index_range{0, 1}, index_range{2, 4}});
    const auto left = sp.repair_locally(ranges);
    REQUIRE(left.has_value());
    CHECK(*left == ranges);
}

// A log without damage has no range, and the local repair writes nothing.
TEST_CASE("a log without damage has no damaged range")
{
    const auto directory = directory_with_a_log();
    const auto before = records_in(directory);
    auto sp = node_on(directory);
    CHECK(damaged_ranges_of(sp).empty());
    const auto left = sp.repair_locally({});
    REQUIRE(left.has_value());
    CHECK(left->empty());
    CHECK(records_in(directory) == before);
}

// Plan of the repair, item 1: only a damaged record or a missing row is
// repaired. Two records that both hold their CRC but whose chain does not
// follow from one to the next are not a damage that a copy can repair. The
// test writes another entry at index 2 with a record CRC that holds and a
// chain hash computed from record 1, so that the link from record 2 to
// record 3 breaks.
TEST_CASE("a break of the chain between two records that hold is inconsistent_storage")
{
    const auto directory = directory_with_a_log();
    const auto h = konsens::sha256::fetch();
    REQUIRE(h.has_value());
    sqlite_damage::change_with_sqlite(directory, [&h](sqlite3* db) {
        const auto first = sqlite_damage::entry_record_of(db, 1);
        konsens::chain_hash previous{};
        std::ranges::copy(std::span{first}.last(chain_from_the_end).first(konsens::chain_hash_size), previous.begin());
        const auto other = entry_of(0x77);
        std::vector<std::byte> record(konsens::encoded_size_of(other) + tail_size);
        const auto encoded = std::span{record}.first(record.size() - tail_size);
        konsens::encode_entry(other, encoded);
        const auto chain = h->compute_chain_hash(previous, 2, encoded);
        const auto crc = konsens::compute_record_crc(2, encoded, chain);
        const auto tail = std::span{record}.last(tail_size);
        for (std::size_t i = 0; i < record_crc_size; ++i) {
            tail[i] = static_cast<std::byte>(crc >> (8 * i));
        }
        std::ranges::copy(chain, tail.subspan(record_crc_size).begin());
        const auto old = sqlite_damage::entry_record_of(db, 2);
        std::ranges::copy(std::span{old}.last(state_from_the_end),
                          tail.subspan(record_crc_size + konsens::chain_hash_size).begin());
        sqlite_damage::put_entry_record(db, 2, record);
    });
    auto sp = node_on(directory);
    CHECK(sp.find_the_damaged_ranges() == std::unexpected(make_error_code(konsens::paxos_errc::inconsistent_storage)));
}

// A repair replaces records of the log and never makes the log longer. An
// append goes through the meta record, and a repair does not change the
// meta record, so a repair at or past the end of the log is a wrong use.
TEST_CASE("a repair that reaches past the end of the log is a wrong use")
{
    auto storage = sqlite_storage::open_in(directory_with_a_log());
    auto t = storage.begin();
    REQUIRE(t.has_value());
    const std::vector one{entry_of(0x01)};
    const std::vector two{entry_of(0x01), entry_of(0x02)};
    CHECK_THROWS_AS(static_cast<void>(t->replace_entries(log_len, one)), std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(t->replace_entries(log_len - 1, two)), std::logic_error);
    CHECK(t->replace_entries(log_len - 1, one).has_value());
}

// The local repair reads an entry without the record CRC. A missing row has
// no such entry, and a record too short for its tail has none either.
TEST_CASE("a missing row or a record shorter than its tail has no unchecked entry")
{
    const auto directory = directory_with_a_log();
    delete_the_row(directory, 1);
    sqlite_damage::change_with_sqlite(
        directory, [](sqlite3* db) { sqlite_damage::put_entry_record(db, 2, std::vector<std::byte>(tail_size)); });
    const auto storage = sqlite_storage::open_in(directory);
    const auto t = storage.begin_read_only();
    REQUIRE(t.has_value());
    CHECK(t->get_unchecked_entry(0).value() == std::optional{entry_of(0x10)});
    CHECK(t->get_unchecked_entry(1).value() == std::nullopt);
    CHECK(t->get_unchecked_entry(2).value() == std::nullopt);
}
