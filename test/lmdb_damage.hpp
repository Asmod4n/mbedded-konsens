#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <span>
#include <string_view>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

#include <doctest/doctest.h>
#include <lmdb.h>

#include "lmdb_checksum.hpp"

// A test damages a storage as a disk would: it opens the files with LMDB
// itself, with the page checksum of the library, so that LMDB accepts the
// pages and only the checks of the library can find the damage.
namespace lmdb_damage {

constexpr std::size_t map_size = std::size_t{16} << 20;
constexpr std::string_view meta_key{"\0meta", 5};
constexpr std::byte log_prefix{2};

inline void change_with_lmdb(const std::filesystem::path& directory,
                             const std::function<void(MDB_txn*, MDB_dbi, MDB_dbi)>& f)
{
    MDB_env* env = nullptr;
    REQUIRE(mdb_env_create(&env) == MDB_SUCCESS);
    REQUIRE(mdb_env_set_maxdbs(env, 2) == MDB_SUCCESS);
    REQUIRE(mdb_env_set_mapsize(env, map_size) == MDB_SUCCESS);
    REQUIRE(mdb_env_set_checksum(env, konsens::compute_page_checksum, konsens::page_checksum_size) == MDB_SUCCESS);
    REQUIRE(mdb_env_open(env, directory.c_str(), MDB_NOTLS | MDB_REMAP_CHUNKS, 0600) == MDB_SUCCESS);
    MDB_txn* txn = nullptr;
    REQUIRE(mdb_txn_begin(env, nullptr, 0, &txn) == MDB_SUCCESS);
    MDB_dbi meta = 0;
    REQUIRE(mdb_dbi_open(txn, "meta", 0, &meta) == MDB_SUCCESS);
    f(txn, meta, meta);
    REQUIRE(mdb_txn_commit(txn) == MDB_SUCCESS);
    mdb_env_close(env);
}

// lmdb.h says that a process must not open one environment twice. A test
// that damages the storage of a running node makes the change in a child
// process, and the node sees it at its next transaction, as it sees a
// write of another process.
inline void change_in_another_process(const std::filesystem::path& directory,
                                      const std::function<void(MDB_txn*, MDB_dbi, MDB_dbi)>& f)
{
    const pid_t child = fork();
    REQUIRE(child != -1);
    if (child == 0) {
        try {
            change_with_lmdb(directory, f);
        } catch (...) {
            _exit(1);
        }
        _exit(0);
    }
    int status = 0;
    REQUIRE(waitpid(child, &status, 0) == child);
    REQUIRE(WIFEXITED(status));
    REQUIRE(WEXITSTATUS(status) == 0);
}

inline std::array<std::byte, 1 + sizeof(std::uint64_t)> key_of(const std::uint64_t index)
{
    std::array<std::byte, 1 + sizeof index> key{log_prefix};
    for (std::size_t i = 1; i < key.size(); ++i) {
        key.at(i) = static_cast<std::byte>(index >> (8 * (key.size() - 1 - i)));
    }
    return key;
}

inline void delete_key(MDB_txn* txn, const MDB_dbi dbi, const std::span<const std::byte> key_bytes)
{
    MDB_val key{key_bytes.size(), const_cast<std::byte*>(key_bytes.data())};
    REQUIRE(mdb_del(txn, dbi, &key, nullptr) == MDB_SUCCESS);
}

inline void delete_the_last_entry(MDB_txn* txn, const MDB_dbi dbi)
{
    MDB_cursor* c = nullptr;
    REQUIRE(mdb_cursor_open(txn, dbi, &c) == MDB_SUCCESS);
    const std::array after_the_log{static_cast<std::byte>(std::to_integer<std::uint8_t>(log_prefix) + 1u)};
    MDB_val key{after_the_log.size(), const_cast<std::byte*>(after_the_log.data())};
    MDB_val value{};
    const int found = mdb_cursor_get(c, &key, &value, MDB_SET_RANGE);
    REQUIRE((found == MDB_SUCCESS ? mdb_cursor_get(c, &key, &value, MDB_PREV)
                                  : mdb_cursor_get(c, &key, &value, MDB_LAST)) == MDB_SUCCESS);
    REQUIRE(key.mv_size == key_of(0).size());
    REQUIRE(*static_cast<const std::byte*>(key.mv_data) == log_prefix);
    REQUIRE(mdb_cursor_del(c, 0) == MDB_SUCCESS);
    mdb_cursor_close(c);
}

inline void delete_meta_record(MDB_txn* txn, const MDB_dbi meta)
{
    delete_key(txn, meta, std::as_bytes(std::span(meta_key)));
}

inline std::vector<std::byte> meta_value_of(MDB_txn* txn, const MDB_dbi meta)
{
    const auto name = std::as_bytes(std::span(meta_key));
    MDB_val key{name.size(), const_cast<std::byte*>(name.data())};
    MDB_val value{};
    REQUIRE(mdb_get(txn, meta, &key, &value) == MDB_SUCCESS);
    const std::span bytes(static_cast<const std::byte*>(value.mv_data), value.mv_size);
    return {bytes.begin(), bytes.end()};
}

inline void put_meta_value(MDB_txn* txn, const MDB_dbi meta, std::vector<std::byte> bytes)
{
    const auto name = std::as_bytes(std::span(meta_key));
    MDB_val key{name.size(), const_cast<std::byte*>(name.data())};
    MDB_val value{bytes.size(), bytes.data()};
    REQUIRE(mdb_put(txn, meta, &key, &value, 0) == MDB_SUCCESS);
}

}
