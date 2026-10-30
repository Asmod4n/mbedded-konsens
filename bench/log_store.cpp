#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

#include <sqlite3.h>
extern "C" {
#include "lmdbsrc/lmdb.h"
}

namespace {

constexpr int samples = 31;
constexpr int warm_up = 2;
constexpr int per_sample = 192;
constexpr int entry_overhead = 40;
constexpr std::size_t map_size = std::size_t{1} << 30;
constexpr int checkpoint_frames = 1000;

void must(const bool ok, const char* what)
{
    if (!ok) {
        std::fprintf(stderr, "%s failed\n", what);
        std::exit(1);
    }
}

int on_wal(void*, sqlite3* db, const char*, const int frames)
{
    if (frames >= checkpoint_frames) {
        sqlite3_wal_checkpoint_v2(db, nullptr, SQLITE_CHECKPOINT_PASSIVE, nullptr, nullptr);
    }
    return SQLITE_OK;
}

}

int main(int argc, char** argv)
{
    if (argc != 5) {
        std::fprintf(stderr, "usage: log_store lmdb|sqlite <path> <value bytes> <writes per commit>\n");
        return 2;
    }
    const std::string kind = argv[1];
    const std::string path = argv[2];
    const int size = std::atoi(argv[3]);
    const int batch = std::atoi(argv[4]);
    std::vector<unsigned char> entry(static_cast<std::size_t>(size + entry_overhead));
    std::mt19937_64 rng{std::random_device{}()};
    MDB_env* env = nullptr;
    MDB_dbi dbi = 0;
    sqlite3* db = nullptr;
    sqlite3_stmt *begin = nullptr, *commit = nullptr, *insert = nullptr;
    if (kind == "lmdb") {
        must(mdb_env_create(&env) == 0, "create");
        must(mdb_env_set_mapsize(env, map_size) == 0, "mapsize");
        must(mdb_env_open(env, path.c_str(), MDB_NOSUBDIR, 0600) == 0, "open");
        MDB_txn* t = nullptr;
        must(mdb_txn_begin(env, nullptr, 0, &t) == 0, "begin");
        must(mdb_dbi_open(t, nullptr, MDB_INTEGERKEY, &dbi) == 0, "dbi");
        must(mdb_txn_commit(t) == 0, "commit");
    } else {
        must(sqlite3_open(path.c_str(), &db) == SQLITE_OK, "open");
        must(sqlite3_exec(db, "PRAGMA journal_mode=WAL; PRAGMA synchronous=FULL; PRAGMA wal_autocheckpoint=0;"
                              "CREATE TABLE IF NOT EXISTS log (idx INTEGER PRIMARY KEY, record BLOB NOT NULL) STRICT;",
                          nullptr, nullptr, nullptr) == SQLITE_OK, "setup");
        sqlite3_wal_hook(db, on_wal, nullptr);
        must(sqlite3_prepare_v3(db, "BEGIN IMMEDIATE", -1, SQLITE_PREPARE_PERSISTENT, &begin, nullptr) == SQLITE_OK, "p");
        must(sqlite3_prepare_v3(db, "COMMIT", -1, SQLITE_PREPARE_PERSISTENT, &commit, nullptr) == SQLITE_OK, "p");
        must(sqlite3_prepare_v3(db, "INSERT INTO log (idx, record) VALUES (?1, ?2)", -1, SQLITE_PREPARE_PERSISTENT, &insert,
                                nullptr) == SQLITE_OK, "p");
    }
    std::uint64_t idx = 0;
    std::vector<double> rates;
    for (int s = 0; s < warm_up + samples; ++s) {
        const auto start = std::chrono::steady_clock::now();
        for (int n = 0; n < per_sample; n += batch) {
            if (env) {
                MDB_txn* t = nullptr;
                must(mdb_txn_begin(env, nullptr, 0, &t) == 0, "begin");
                for (int i = 0; i < batch; ++i) {
                    for (auto& b : entry) b = static_cast<unsigned char>(rng());
                    ++idx;
                    MDB_val k{sizeof idx, &idx};
                    MDB_val v{entry.size(), entry.data()};
                    must(mdb_put(t, dbi, &k, &v, MDB_APPEND) == 0, "put");
                }
                must(mdb_txn_commit(t) == 0, "commit");
            } else {
                must(sqlite3_step(begin) == SQLITE_DONE, "begin");
                sqlite3_reset(begin);
                for (int i = 0; i < batch; ++i) {
                    for (auto& b : entry) b = static_cast<unsigned char>(rng());
                    ++idx;
                    sqlite3_bind_int64(insert, 1, static_cast<sqlite3_int64>(idx));
                    sqlite3_bind_blob(insert, 2, entry.data(), static_cast<int>(entry.size()), SQLITE_STATIC);
                    must(sqlite3_step(insert) == SQLITE_DONE, "insert");
                    sqlite3_reset(insert);
                }
                must(sqlite3_step(commit) == SQLITE_DONE, "commit");
                sqlite3_reset(commit);
            }
        }
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        if (s >= warm_up) rates.push_back(per_sample / seconds);
    }
    std::ranges::sort(rates);
    std::printf("median_writes_per_s=%.0f min=%.0f max=%.0f\n", rates[rates.size() / 2], rates.front(), rates.back());
}
