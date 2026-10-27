#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <doctest/doctest.h>
#include <sqlite3.h>

// A test damages a storage as a disk would: it opens the database file with
// SQLite itself and changes a record, so that SQLite reads every page
// without an error and only the checks of the library can find the damage.
// SQLite lets a second connection of the same process change the file, and
// a running node sees the change at its next transaction, as it sees a
// write of another process.
namespace sqlite_damage {

constexpr std::string_view store_file_name = "konsens.db";
constexpr std::string_view wal_file_name = "konsens.db-wal";

inline void change_with_sqlite(const std::filesystem::path& directory, const std::function<void(sqlite3*)>& f)
{
    sqlite3* db = nullptr;
    const auto file = (directory / store_file_name).string();
    REQUIRE(sqlite3_open_v2(file.c_str(), &db, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_exec(db, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK);
    f(db);
    REQUIRE(sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

inline void run(sqlite3* db, const std::string_view sql, const std::function<void(sqlite3_stmt*)>& bind)
{
    sqlite3_stmt* s = nullptr;
    REQUIRE(sqlite3_prepare_v2(db, sql.data(), static_cast<int>(sql.size()), &s, nullptr) == SQLITE_OK);
    bind(s);
    REQUIRE(sqlite3_step(s) == SQLITE_DONE);
    REQUIRE(sqlite3_finalize(s) == SQLITE_OK);
}

inline std::vector<std::byte> select_bytes(sqlite3* db, const std::string_view sql,
                                           const std::function<void(sqlite3_stmt*)>& bind)
{
    sqlite3_stmt* s = nullptr;
    REQUIRE(sqlite3_prepare_v2(db, sql.data(), static_cast<int>(sql.size()), &s, nullptr) == SQLITE_OK);
    bind(s);
    REQUIRE(sqlite3_step(s) == SQLITE_ROW);
    const std::span bytes(static_cast<const std::byte*>(sqlite3_column_blob(s, 0)),
                          static_cast<std::size_t>(sqlite3_column_bytes(s, 0)));
    std::vector<std::byte> value(bytes.begin(), bytes.end());
    REQUIRE(sqlite3_finalize(s) == SQLITE_OK);
    return value;
}

// A copy of a file holds only what the database file holds. A checkpoint
// with TRUNCATE copies every frame of the WAL into the database file and
// sets the WAL to zero bytes, so that a test can change the file itself.
// A connection opens the WAL only at its first read, and a checkpoint
// before that read finds no WAL and returns SQLITE_OK.
inline void move_the_wal_into_the_file(const std::filesystem::path& directory)
{
    sqlite3* db = nullptr;
    const auto file = (directory / store_file_name).string();
    REQUIRE(sqlite3_open_v2(file.c_str(), &db, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_exec(db, "SELECT count(*) FROM sqlite_schema", nullptr, nullptr, nullptr) == SQLITE_OK);
    int log_frames = -1;
    int moved_frames = -1;
    REQUIRE(sqlite3_wal_checkpoint_v2(db, nullptr, SQLITE_CHECKPOINT_TRUNCATE, &log_frames, &moved_frames) ==
            SQLITE_OK);
    REQUIRE(log_frames >= 0);
    REQUIRE(log_frames == moved_frames);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

inline std::int64_t integer_of(sqlite3* db, const std::string_view sql,
                               const std::function<void(sqlite3_stmt*)>& bind)
{
    sqlite3_stmt* s = nullptr;
    REQUIRE(sqlite3_prepare_v2(db, sql.data(), static_cast<int>(sql.size()), &s, nullptr) == SQLITE_OK);
    bind(s);
    REQUIRE(sqlite3_step(s) == SQLITE_ROW);
    const auto value = sqlite3_column_int64(s, 0);
    REQUIRE(sqlite3_finalize(s) == SQLITE_OK);
    return value;
}

// The root page of a table is in the schema table (fileformat2.html,
// section 2.6). Page 1 starts at byte 0 of the file.
inline std::pair<std::int64_t, std::int64_t> page_size_and_root_page_of(const std::filesystem::path& directory,
                                                                       const std::string_view table)
{
    sqlite3* db = nullptr;
    const auto file = (directory / store_file_name).string();
    REQUIRE(sqlite3_open_v2(file.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK);
    const auto page_size = integer_of(db, "PRAGMA page_size", [](sqlite3_stmt*) {});
    const auto root_page =
        integer_of(db, "SELECT rootpage FROM sqlite_schema WHERE type = 'table' AND name = ?1", [table](sqlite3_stmt* s) {
            REQUIRE(sqlite3_bind_text(s, 1, table.data(), static_cast<int>(table.size()), SQLITE_TRANSIENT) ==
                    SQLITE_OK);
        });
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
    return {page_size, root_page};
}

inline void put_record(sqlite3* db, const std::string_view name, const std::vector<std::byte>& bytes)
{
    run(db, "UPDATE meta SET record = ?2 WHERE name = ?1", [name, &bytes](sqlite3_stmt* s) {
        REQUIRE(sqlite3_bind_text(s, 1, name.data(), static_cast<int>(name.size()), SQLITE_TRANSIENT) == SQLITE_OK);
        REQUIRE(sqlite3_bind_blob64(s, 2, bytes.data(), bytes.size(), SQLITE_TRANSIENT) == SQLITE_OK);
    });
    REQUIRE(sqlite3_changes(db) == 1);
}

inline void delete_record(sqlite3* db, const std::string_view name)
{
    run(db, "DELETE FROM meta WHERE name = ?1", [name](sqlite3_stmt* s) {
        REQUIRE(sqlite3_bind_text(s, 1, name.data(), static_cast<int>(name.size()), SQLITE_TRANSIENT) == SQLITE_OK);
    });
    REQUIRE(sqlite3_changes(db) == 1);
}

inline void delete_meta_record(sqlite3* db)
{
    delete_record(db, "meta");
}

inline std::vector<std::byte> meta_value_of(sqlite3* db)
{
    return select_bytes(db, "SELECT record FROM meta WHERE name = 'meta'", [](sqlite3_stmt*) {});
}

inline void put_meta_value(sqlite3* db, const std::vector<std::byte>& bytes)
{
    put_record(db, "meta", bytes);
}

inline void delete_entry(sqlite3* db, const std::uint64_t index)
{
    run(db, "DELETE FROM log WHERE idx = ?1", [index](sqlite3_stmt* s) {
        REQUIRE(sqlite3_bind_int64(s, 1, static_cast<sqlite3_int64>(index)) == SQLITE_OK);
    });
    REQUIRE(sqlite3_changes(db) == 1);
}

inline void delete_the_last_entry(sqlite3* db)
{
    run(db, "DELETE FROM log WHERE idx = (SELECT max(idx) FROM log)", [](sqlite3_stmt*) {});
    REQUIRE(sqlite3_changes(db) == 1);
}

inline std::vector<std::byte> entry_record_of(sqlite3* db, const std::uint64_t index)
{
    return select_bytes(db, "SELECT record FROM log WHERE idx = ?1", [index](sqlite3_stmt* s) {
        REQUIRE(sqlite3_bind_int64(s, 1, static_cast<sqlite3_int64>(index)) == SQLITE_OK);
    });
}

inline void put_entry_record(sqlite3* db, const std::uint64_t index, const std::vector<std::byte>& bytes)
{
    run(db, "UPDATE log SET record = ?2 WHERE idx = ?1", [index, &bytes](sqlite3_stmt* s) {
        REQUIRE(sqlite3_bind_int64(s, 1, static_cast<sqlite3_int64>(index)) == SQLITE_OK);
        REQUIRE(sqlite3_bind_blob64(s, 2, bytes.data(), bytes.size(), SQLITE_TRANSIENT) == SQLITE_OK);
    });
    REQUIRE(sqlite3_changes(db) == 1);
}

// The WAL of SQLite (file format, section 4.1): a header of 32 bytes that
// holds the page size at offset 8 in big-endian order, then frames, each a
// header of 24 bytes and one page. A test reads the file itself, because a
// connection of SQLite would run a recovery of the WAL first.
constexpr std::size_t wal_header_bytes = 32;
constexpr std::size_t wal_frame_header_bytes = 24;
constexpr std::size_t wal_page_size_offset = 8;
constexpr std::size_t wal_page_size_bytes = 4;

inline std::vector<char> wal_of(const std::filesystem::path& directory)
{
    std::ifstream in(directory / wal_file_name, std::ios::binary);
    REQUIRE(in.is_open());
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

inline std::uint64_t wal_page_size_of(const std::vector<char>& wal)
{
    REQUIRE(wal.size() >= wal_header_bytes);
    std::uint64_t size = 0;
    for (std::size_t i = 0; i < wal_page_size_bytes; ++i) {
        size = (size << 8) | static_cast<unsigned char>(wal.at(wal_page_size_offset + i));
    }
    return size;
}

inline std::uint64_t wal_frames_of(const std::filesystem::path& directory)
{
    const auto wal = wal_of(directory);
    if (wal.size() < wal_header_bytes) {
        return 0;
    }
    return (wal.size() - wal_header_bytes) / (wal_frame_header_bytes + wal_page_size_of(wal));
}

inline void flip_a_byte_of_the_wal_frame(const std::filesystem::path& directory, const std::uint64_t frame)
{
    const auto wal = wal_of(directory);
    const auto page_size = wal_page_size_of(wal);
    const auto offset = wal_header_bytes + frame * (wal_frame_header_bytes + page_size) + wal_frame_header_bytes;
    REQUIRE(offset < wal.size());
    std::fstream f(directory / wal_file_name, std::ios::binary | std::ios::in | std::ios::out);
    REQUIRE(f.is_open());
    f.seekp(static_cast<std::streamoff>(offset));
    f.put(static_cast<char>(wal.at(offset) ^ 0x01));
    f.close();
    REQUIRE_FALSE(f.fail());
}
}
