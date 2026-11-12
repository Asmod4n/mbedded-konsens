#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include <sqlite3.h>

#include "io_error_vfs.hpp"
#include "konsens/persistent_storage.hpp"
#include "sqlite_storage.hpp"
#include "temporary_directory.hpp"

// I/O error testing in the form of SQLite (plan doc/plan-io-error-testing.md,
// owner card 2026-11-12 22:41 UTC). Each scenario runs once for each N from 1
// until a run meets no failure. The failure is the N-th I/O operation of the
// files of the storage. After each run the test turns the failure off, opens
// the storage again and checks it: SQLite finds no damage, every confirmed
// entry is there, and nothing is there that was not attempted.
namespace {

using sqlite_storage::max_size;
using sqlite_storage::non_root_user_id;

constexpr std::size_t large_entry_size = std::size_t{64} << 10;
constexpr int commits_before_the_failure = 55;
constexpr int commits_across_the_checkpoint = 8;
constexpr int sweep_limit = 100000;

io_error_vfs::registered_vfs& vfs()
{
    static io_error_vfs::registered_vfs registered;
    return registered;
}

enum class failure_mode { once, persist, diskfull };

// How a run ended. The rules (RULES.md "An error goes to whoever can repair
// it") give each failure one end: a failed sync is sync_error, a failed write
// is write_error, a failed read is read_error or write_error inside a write,
// a full disk is a value with no_space_on_device.
struct ending {
    bool sync_error = false;
    bool write_error = false;
    bool read_error = false;
    std::optional<std::error_code> value;
};

struct log_record {
    std::vector<konsens::entry> attempted;
    std::size_t confirmed = 0;
};

konsens::entry make_entry(const std::size_t i, const std::size_t size)
{
    return {konsens::entry_kind::normal, std::vector<std::byte>(size, static_cast<std::byte>(i % 251))};
}

// Each step that returns a value ends the run at the first error value, as a
// caller does.
using scenario = std::function<void(const std::filesystem::path&, log_record&, ending&)>;

bool append_and_commit(konsens::persistent_storage& storage, const konsens::entry& e, log_record& log, ending& end)
{
    auto t = storage.begin();
    if (!t) {
        end.value = t.error();
        return false;
    }
    if (auto a = t->append_entries(std::vector{e}); !a) {
        end.value = a.error();
        return false;
    }
    log.attempted.push_back(e);
    if (auto c = std::move(*t).commit(); !c) {
        end.value = c.error();
        return false;
    }
    log.confirmed = log.attempted.size();
    return true;
}

bool read_back(const konsens::persistent_storage& storage, const log_record& log, ending& end)
{
    auto t = storage.begin_read_only();
    if (!t) {
        end.value = t.error();
        return false;
    }
    auto got = t->get_entries(0, log.confirmed);
    if (!got) {
        end.value = got.error();
        return false;
    }
    CHECK(*got == std::vector(log.attempted.begin(), std::next(log.attempted.begin(), static_cast<std::ptrdiff_t>(log.confirmed))));
    return true;
}

void run(const scenario& s, const std::filesystem::path& directory, log_record& log, ending& end)
{
    try {
        s(directory, log, end);
    } catch (const konsens::sync_error&) {
        end.sync_error = true;
    } catch (const konsens::write_error&) {
        end.write_error = true;
    } catch (const konsens::read_error&) {
        end.read_error = true;
    }
}

void check_the_storage_after(const std::filesystem::path& directory, const log_record& log)
{
    vfs().disarm();
    auto opened = konsens::persistent_storage::open(directory, max_size, non_root_user_id);
    if (!opened) {
        CHECK(log.confirmed == 0);
        return;
    }
    {
        const auto t = opened->begin_read_only();
        REQUIRE(t.has_value());
        const auto m = t->get_meta();
        REQUIRE(m.has_value());
        CHECK(m->log_len >= log.confirmed);
        CHECK(m->log_len <= log.attempted.size());
        const auto present = t->get_entries(0, m->log_len);
        REQUIRE(present.has_value());
        CHECK(*present == std::vector(log.attempted.begin(),
                                      std::next(log.attempted.begin(), static_cast<std::ptrdiff_t>(m->log_len))));
    }
    sqlite3* raw = nullptr;
    const auto file = (directory / "konsens.db").string();
    REQUIRE(sqlite3_open_v2(file.c_str(), &raw, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK);
    sqlite3_stmt* s = nullptr;
    REQUIRE(sqlite3_prepare_v2(raw, "PRAGMA integrity_check", -1, &s, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_step(s) == SQLITE_ROW);
    CHECK(std::string{reinterpret_cast<const char*>(sqlite3_column_text(s, 0))} == "ok");
    sqlite3_finalize(s);
    sqlite3_close_v2(raw);
}

// The end must match the first failed operation. A failed sync that ends in
// no raise is a sync that the program does not know of.
void check_the_ending(const int first_hit_code, const bool before_the_wal, const ending& end)
{
    INFO("first failed operation: " << first_hit_code);
    INFO("ending: sync_error " << end.sync_error << ", write_error " << end.write_error << ", read_error "
                               << end.read_error << ", value " << (end.value ? end.value->message() : "none"));
    const int raised = int{end.sync_error} + int{end.write_error} + int{end.read_error};
    CHECK(raised <= 1);
    // A new store starts in the rollback journal mode, and its first
    // transaction switches it to WAL. When a sync of that transaction fails,
    // SQLite 3.53.4 rolls it back from the journal and reports no error to
    // the caller and none to its log: the library cannot see it. The store
    // holds no data at that point, and the check after the run finds it
    // whole.
    if (first_hit_code == SQLITE_IOERR_FSYNC && before_the_wal) {
        MESSAGE("a failed sync before the switch to WAL reached no caller");
    } else if (first_hit_code == SQLITE_IOERR_FSYNC) {
        CHECK(end.sync_error);
    } else {
        CHECK_FALSE(end.sync_error);
    }
    if (first_hit_code == SQLITE_FULL && end.value) {
        CHECK(*end.value == std::errc::no_space_on_device);
    }
}

struct sweep_result {
    int runs = 0;
    int ended_early = 0;
};

sweep_result sweep(const failure_mode mode, const std::function<void(const std::filesystem::path&, log_record&)>& prepare,
                   const scenario& s)
{
    sweep_result r;
    for (int n = 1; n < sweep_limit; ++n) {
        const temporary_directory removed_after_the_run;
        const auto& directory = removed_after_the_run.path();
        log_record log;
        vfs().watch({directory});
        vfs().disarm();
        prepare(directory, log);
        vfs().arm(mode == failure_mode::diskfull ? 0 : n, mode == failure_mode::persist,
                  mode == failure_mode::diskfull ? n : 0);
        ending end;
        run(s, directory, log, end);
        const int hits = vfs().count().io_error_hit;
        const int first_hit_code = vfs().count().first_hit_code;
        const bool before_the_wal = vfs().count().first_hit_before_the_wal;
        vfs().disarm();
        r.runs = n;
        if (hits == 0) {
            CHECK_FALSE(end.value.has_value());
            CHECK(end.sync_error == false);
            CHECK(end.write_error == false);
            CHECK(end.read_error == false);
            check_the_storage_after(directory, log);
            return r;
        }
        INFO("N = " << n);
        check_the_ending(first_hit_code, before_the_wal, end);
        if (end.value || end.sync_error || end.write_error || end.read_error) {
            r.ended_early += 1;
        }
        check_the_storage_after(directory, log);
    }
    FAIL("the sweep found no end");
    return r;
}

void nothing(const std::filesystem::path&, log_record&) {}

void two_entries_before(const std::filesystem::path& directory, log_record& log)
{
    auto storage = sqlite_storage::open_in(directory);
    ending end;
    for (std::size_t i = 0; i < 2; ++i) {
        REQUIRE(append_and_commit(storage, make_entry(i, 64), log, end));
    }
}

void open_append_commit_read(const std::filesystem::path& directory, log_record& log, ending& end)
{
    auto opened = konsens::persistent_storage::open(directory, max_size, non_root_user_id);
    if (!opened) {
        end.value = opened.error();
        return;
    }
    const auto first = log.attempted.size();
    for (std::size_t i = first; i < first + 2; ++i) {
        if (!append_and_commit(*opened, make_entry(i, 64), log, end)) {
            return;
        }
    }
    read_back(*opened, log, end);
}

// The storage runs a checkpoint at the begin of a write once the WAL holds
// 1000 frames. Large entries fill most of the WAL before the failure is
// armed, inside the run, and the armed commits cross the 1000 frames, so that
// the checkpoint of the same open storage meets the failure.
void checkpoint_after_large_entries(const std::filesystem::path& directory, log_record& log, ending& end)
{
    const int pending = vfs().count().io_error_pending;
    const int diskfull = vfs().count().diskfull_pending;
    const bool persist = vfs().count().io_error_persist;
    vfs().disarm();
    auto opened = konsens::persistent_storage::open(directory, max_size, non_root_user_id);
    REQUIRE(opened.has_value());
    for (int i = 0; i < commits_before_the_failure; ++i) {
        REQUIRE(append_and_commit(*opened, make_entry(log.attempted.size(), large_entry_size), log, end));
    }
    vfs().arm(pending, persist, diskfull);
    for (int i = 0; i < commits_across_the_checkpoint; ++i) {
        if (!append_and_commit(*opened, make_entry(log.attempted.size(), large_entry_size), log, end)) {
            return;
        }
    }
    read_back(*opened, log, end);
}

void node_storage_writes(const std::filesystem::path& directory, log_record&, ending& end)
{
    auto opened = konsens::node_storage::open(directory, non_root_user_id);
    if (!opened) {
        end.value = opened.error();
        return;
    }
    if (auto w = opened->write_the_first_start_record({konsens::node_id{1}}); !w) {
        end.value = w.error();
        return;
    }
    if (auto w = opened->write_the_repair_count(1); !w) {
        end.value = w.error();
        return;
    }
    if (auto c = opened->get_repair_count(); !c) {
        end.value = c.error();
        return;
    }
}

constexpr std::array modes{failure_mode::once, failure_mode::persist, failure_mode::diskfull};

}

TEST_CASE("an I/O error while a new storage opens, commits and reads ends as the rules say and keeps the storage whole")
{
    for (const auto mode : modes) {
        const auto r = sweep(mode, nothing, open_append_commit_read);
        MESSAGE("mode " << static_cast<int>(mode) << ": " << r.runs << " runs, " << r.ended_early << " ended early");
    }
}

TEST_CASE("an I/O error on a storage that holds entries ends as the rules say and keeps every confirmed entry")
{
    for (const auto mode : modes) {
        const auto r = sweep(mode, two_entries_before, open_append_commit_read);
        MESSAGE("mode " << static_cast<int>(mode) << ": " << r.runs << " runs, " << r.ended_early << " ended early");
    }
}

TEST_CASE("an I/O error in a checkpoint ends as the rules say and keeps every confirmed entry")
{
    for (const auto mode : modes) {
        const auto r = sweep(mode, nothing, checkpoint_after_large_entries);
        MESSAGE("mode " << static_cast<int>(mode) << ": " << r.runs << " runs, " << r.ended_early << " ended early");
    }
}

TEST_CASE("an I/O error in the node storage ends as the rules say")
{
    for (const auto mode : modes) {
        const auto r = sweep(mode, nothing, node_storage_writes);
        MESSAGE("mode " << static_cast<int>(mode) << ": " << r.runs << " runs, " << r.ended_early << " ended early");
    }
}
