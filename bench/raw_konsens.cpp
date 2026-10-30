#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

#include <sqlite3.h>

namespace {

constexpr int checkpoint_frames = 1000;
constexpr int samples = 31;
constexpr int warm_up = 2;
constexpr int per_sample = 192;
constexpr int range_rows_per_write = 31;
constexpr int point_reads_per_write = 3;
constexpr int meta_writes_per_commit = 3;
constexpr int meta_reads_per_commit = 5;
constexpr int entry_overhead = 40;
constexpr int result_bytes = 16;

int on_wal(void*, sqlite3* db, const char*, const int frames)
{
    if (frames >= checkpoint_frames) {
        sqlite3_wal_checkpoint_v2(db, nullptr, SQLITE_CHECKPOINT_PASSIVE, nullptr, nullptr);
    }
    return SQLITE_OK;
}

void check(const int rc, sqlite3* db, const char* what)
{
    if (rc != SQLITE_OK && rc != SQLITE_DONE && rc != SQLITE_ROW) {
        std::fprintf(stderr, "%s: %s\n", what, sqlite3_errmsg(db));
        std::exit(1);
    }
}

sqlite3_stmt* prepare(sqlite3* db, const char* sql)
{
    sqlite3_stmt* s = nullptr;
    check(sqlite3_prepare_v3(db, sql, -1, SQLITE_PREPARE_PERSISTENT, &s, nullptr), db, sql);
    return s;
}

void run(sqlite3* db, sqlite3_stmt* s, const char* what)
{
    int rc;
    while ((rc = sqlite3_step(s)) == SQLITE_ROW) {
    }
    check(rc, db, what);
    sqlite3_reset(s);
}

void read_rows(sqlite3* db, sqlite3_stmt* s, const int rows)
{
    int rc = SQLITE_ROW;
    for (int i = 0; i < rows && (rc = sqlite3_step(s)) == SQLITE_ROW; ++i) {
    }
    check(rc, db, "range");
    sqlite3_reset(s);
}

}

int main(int argc, char** argv)
{
    if (argc != 4) {
        std::fprintf(stderr, "usage: raw_konsens <file> <value bytes> <writes per commit>\n");
        return 2;
    }
    const std::string file = argv[1];
    const int size = std::atoi(argv[2]);
    const int batch = std::atoi(argv[3]);
    sqlite3* db = nullptr;
    check(sqlite3_open(file.c_str(), &db), db, "open");
    check(sqlite3_exec(db,
                       "PRAGMA journal_mode=WAL; PRAGMA synchronous=FULL; PRAGMA wal_autocheckpoint=0;"
                       "CREATE TABLE IF NOT EXISTS meta (name TEXT PRIMARY KEY, record BLOB NOT NULL) STRICT, WITHOUT ROWID;"
                       "CREATE TABLE IF NOT EXISTS log (idx INTEGER PRIMARY KEY, record BLOB NOT NULL) STRICT;"
                       "CREATE TABLE IF NOT EXISTS session (id INTEGER PRIMARY KEY, record BLOB NOT NULL) STRICT;"
                       "CREATE TABLE IF NOT EXISTS data (key BLOB PRIMARY KEY, value BLOB NOT NULL) STRICT, WITHOUT ROWID;",
                       nullptr, nullptr, nullptr),
          db, "setup");
    sqlite3_wal_hook(db, on_wal, nullptr);
    auto* begin = prepare(db, "BEGIN IMMEDIATE");
    auto* commit = prepare(db, "COMMIT");
    auto* range = prepare(db, "SELECT idx, record FROM log WHERE idx >= ?1 ORDER BY idx");
    auto* point = prepare(db, "SELECT record FROM log WHERE idx = ?1");
    auto* insert_log =
        prepare(db, "INSERT INTO log (idx, record) SELECT ?1, ?2 WHERE ?1 > (SELECT coalesce(max(idx), -1) FROM log)");
    auto* update_log = prepare(db, "UPDATE log SET record = ?2 WHERE idx = ?1");
    auto* read_session = prepare(db, "SELECT record FROM session WHERE id = ?1");
    auto* write_session = prepare(db, "INSERT INTO session (id, record) VALUES (?1, ?2) ON CONFLICT (id) DO UPDATE SET record = excluded.record");
    auto* read_data = prepare(db, "SELECT value FROM data WHERE key = ?1");
    auto* write_data = prepare(db, "INSERT INTO data (key, value) VALUES (?1, ?2) ON CONFLICT (key) DO UPDATE SET value = excluded.value");
    auto* read_meta = prepare(db, "SELECT record FROM meta WHERE name = ?1");
    auto* write_meta = prepare(db, "INSERT INTO meta (name, record) VALUES (?1, ?2) ON CONFLICT (name) DO UPDATE SET record = excluded.record");
    const char* meta_names[] = {"promise", "decided", "accepted"};
    std::mt19937_64 rng{std::random_device{}()};
    std::vector<unsigned char> entry(static_cast<std::size_t>(size + entry_overhead));
    std::vector<unsigned char> done(static_cast<std::size_t>(size + entry_overhead + result_bytes));
    std::vector<unsigned char> value(static_cast<std::size_t>(size));
    std::vector<unsigned char> meta(64);
    std::vector<unsigned char> session(64);
    long long idx = 0;
    std::vector<double> rates;
    std::vector<double> medians;
    for (int s = 0; s < warm_up + samples; ++s) {
        std::vector<double> latency;
        const auto start = std::chrono::steady_clock::now();
        for (int n = 0; n < per_sample; n += batch) {
            const auto t = std::chrono::steady_clock::now();
            run(db, begin, "begin");
            for (int m = 0; m < meta_reads_per_commit; ++m) {
                sqlite3_bind_text(read_meta, 1, meta_names[m % 3], -1, SQLITE_STATIC);
                run(db, read_meta, "read meta");
            }
            for (int i = 0; i < batch; ++i) {
                for (auto& b : value) {
                    b = static_cast<unsigned char>(rng());
                }
                std::copy(value.begin(), value.end(), entry.begin());
                std::copy(value.begin(), value.end(), done.begin());
                ++idx;
                sqlite3_bind_int64(insert_log, 1, idx);
                sqlite3_bind_blob(insert_log, 2, entry.data(), static_cast<int>(entry.size()), SQLITE_STATIC);
                run(db, insert_log, "insert log");
                sqlite3_bind_int64(range, 1, std::max(1LL, idx - range_rows_per_write));
                read_rows(db, range, range_rows_per_write);
                for (int p = 0; p < point_reads_per_write; ++p) {
                    sqlite3_bind_int64(point, 1, idx - p);
                    run(db, point, "point");
                }
                const long long sid = static_cast<long long>(rng() % 64);
                sqlite3_bind_int64(read_session, 1, sid);
                run(db, read_session, "read session");
                const std::string k = "k" + std::to_string(idx);
                sqlite3_bind_blob(read_data, 1, k.data(), static_cast<int>(k.size()), SQLITE_STATIC);
                run(db, read_data, "read data");
                sqlite3_bind_blob(write_data, 1, k.data(), static_cast<int>(k.size()), SQLITE_STATIC);
                sqlite3_bind_blob(write_data, 2, value.data(), size, SQLITE_STATIC);
                run(db, write_data, "write data");
                sqlite3_bind_int64(write_session, 1, sid);
                sqlite3_bind_blob(write_session, 2, session.data(), static_cast<int>(session.size()), SQLITE_STATIC);
                run(db, write_session, "write session");
                sqlite3_bind_int64(update_log, 1, idx);
                sqlite3_bind_blob(update_log, 2, done.data(), static_cast<int>(done.size()), SQLITE_STATIC);
                run(db, update_log, "update log");
            }
            for (int m = 0; m < meta_writes_per_commit; ++m) {
                sqlite3_bind_text(write_meta, 1, meta_names[m], -1, SQLITE_STATIC);
                sqlite3_bind_blob(write_meta, 2, meta.data(), static_cast<int>(meta.size()), SQLITE_STATIC);
                run(db, write_meta, "write meta");
            }
            run(db, commit, "commit");
            const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t).count();
            for (int i = 0; i < batch; ++i) {
                latency.push_back(us);
            }
        }
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        if (s < warm_up) {
            continue;
        }
        std::ranges::sort(latency);
        rates.push_back(static_cast<double>(latency.size()) / seconds);
        medians.push_back(latency[latency.size() / 2]);
    }
    std::ranges::sort(rates);
    std::ranges::sort(medians);
    std::printf("label=raw_konsens system=sqlite-%s batch=%d size=%d samples=%d per_sample=%d median_writes_per_s=%.0f "
                "min=%.0f max=%.0f median_latency_us=%.0f\n",
                sqlite3_libversion(), batch, size, samples, per_sample, rates[rates.size() / 2], rates.front(),
                rates.back(), medians[medians.size() / 2]);
}
