#pragma once

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <filesystem>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <sqlite3.h>

// I/O error testing in the form of SQLite (https://www.sqlite.org/testing.html
// section 3.2, plan doc/plan-io-error-testing.md). A VFS over the default VFS
// fails the operation at io_error_pending, and with io_error_persist every
// operation after it. The counters keep the names of SQLite's own test build
// (src/os_common.h), which the package of the distribution does not have.
// Only the files below the watched directories fail, so that one node of a
// cluster in one process fails and the others do not. The failed operations
// and their codes are the ones that os_unix.c fails in a test build. The
// shared memory of the WAL is not failed, as there.
namespace io_error_vfs {

struct counters {
    std::atomic<int> io_error_pending{0};
    std::atomic<bool> io_error_persist{false};
    std::atomic<int> io_error_hit{0};
    std::atomic<int> diskfull_pending{0};
    std::atomic<int> operations{0};
    std::atomic<int> first_hit_code{SQLITE_OK};
    std::atomic<bool> wal_opened{false};
    std::atomic<bool> first_hit_before_the_wal{false};
};

struct watched_file {
    sqlite3_file base;
    sqlite3_file* real;
    counters* count;
    bool watched;
    int last_errno;
};

inline watched_file& file_of(sqlite3_file* f)
{
    return *reinterpret_cast<watched_file*>(f);
}

inline sqlite3_file* real_of(sqlite3_file* f)
{
    return file_of(f).real;
}

// The order of os_common.h: a persistent failure fails every operation after
// the first hit, and otherwise the pending count fails exactly one.
inline bool io_error_now(counters& c)
{
    c.operations += 1;
    if (c.io_error_persist && c.io_error_hit > 0) {
        c.io_error_hit += 1;
        return true;
    }
    if (c.io_error_pending > 0 && c.io_error_pending.fetch_sub(1) == 1) {
        c.io_error_hit += 1;
        return true;
    }
    return false;
}

inline bool diskfull_now(counters& c)
{
    if (c.diskfull_pending > 0 && c.diskfull_pending.fetch_sub(1) == 1) {
        c.io_error_hit += 1;
        return true;
    }
    return false;
}

inline int hit(counters& c, const int code)
{
    int none = SQLITE_OK;
    c.first_hit_code.compare_exchange_strong(none, code);
    return code;
}

inline int failed(sqlite3_file* f, const int code, const int system_errno)
{
    file_of(f).last_errno = system_errno;
    auto& c = *file_of(f).count;
    if (c.first_hit_code == SQLITE_OK) {
        c.first_hit_before_the_wal = !c.wal_opened;
    }
    return hit(c, code);
}

inline bool fails(sqlite3_file* f)
{
    return file_of(f).watched && io_error_now(*file_of(f).count);
}

inline int x_close(sqlite3_file* f)
{
    const int r = real_of(f)->pMethods->xClose(real_of(f));
    sqlite3_free(real_of(f));
    return r;
}

inline int x_read(sqlite3_file* f, void* buffer, const int amount, const sqlite3_int64 offset)
{
    if (fails(f)) {
        return failed(f, SQLITE_IOERR_READ, EIO);
    }
    return real_of(f)->pMethods->xRead(real_of(f), buffer, amount, offset);
}

inline int x_write(sqlite3_file* f, const void* buffer, const int amount, const sqlite3_int64 offset)
{
    if (fails(f)) {
        return failed(f, SQLITE_IOERR_WRITE, EIO);
    }
    if (file_of(f).watched && diskfull_now(*file_of(f).count)) {
        return failed(f, SQLITE_FULL, ENOSPC);
    }
    return real_of(f)->pMethods->xWrite(real_of(f), buffer, amount, offset);
}

inline int x_truncate(sqlite3_file* f, const sqlite3_int64 size)
{
    if (fails(f)) {
        return failed(f, SQLITE_IOERR_TRUNCATE, EIO);
    }
    return real_of(f)->pMethods->xTruncate(real_of(f), size);
}

inline int x_sync(sqlite3_file* f, const int flags)
{
    if (fails(f)) {
        return failed(f, SQLITE_IOERR_FSYNC, EIO);
    }
    return real_of(f)->pMethods->xSync(real_of(f), flags);
}

inline int x_file_size(sqlite3_file* f, sqlite3_int64* size)
{
    if (fails(f)) {
        return failed(f, SQLITE_IOERR_FSTAT, EIO);
    }
    return real_of(f)->pMethods->xFileSize(real_of(f), size);
}

inline int x_lock(sqlite3_file* f, const int level)
{
    return real_of(f)->pMethods->xLock(real_of(f), level);
}

inline int x_unlock(sqlite3_file* f, const int level)
{
    return real_of(f)->pMethods->xUnlock(real_of(f), level);
}

inline int x_check_reserved_lock(sqlite3_file* f, int* out)
{
    if (fails(f)) {
        return failed(f, SQLITE_IOERR_CHECKRESERVEDLOCK, EIO);
    }
    return real_of(f)->pMethods->xCheckReservedLock(real_of(f), out);
}

inline int x_file_control(sqlite3_file* f, const int op, void* argument)
{
    if (op == SQLITE_FCNTL_LAST_ERRNO && file_of(f).last_errno != 0) {
        *static_cast<int*>(argument) = file_of(f).last_errno;
        return SQLITE_OK;
    }
    return real_of(f)->pMethods->xFileControl(real_of(f), op, argument);
}

inline int x_sector_size(sqlite3_file* f)
{
    return real_of(f)->pMethods->xSectorSize(real_of(f));
}

inline int x_device_characteristics(sqlite3_file* f)
{
    return real_of(f)->pMethods->xDeviceCharacteristics(real_of(f));
}

inline int x_shm_map(sqlite3_file* f, const int region, const int size, const int extend, void volatile** out)
{
    return real_of(f)->pMethods->xShmMap(real_of(f), region, size, extend, out);
}

inline int x_shm_lock(sqlite3_file* f, const int offset, const int n, const int flags)
{
    return real_of(f)->pMethods->xShmLock(real_of(f), offset, n, flags);
}

inline void x_shm_barrier(sqlite3_file* f)
{
    real_of(f)->pMethods->xShmBarrier(real_of(f));
}

inline int x_shm_unmap(sqlite3_file* f, const int delete_flag)
{
    return real_of(f)->pMethods->xShmUnmap(real_of(f), delete_flag);
}

inline int x_fetch(sqlite3_file* f, const sqlite3_int64 offset, const int amount, void** out)
{
    return real_of(f)->pMethods->xFetch(real_of(f), offset, amount, out);
}

inline int x_unfetch(sqlite3_file* f, const sqlite3_int64 offset, void* page)
{
    return real_of(f)->pMethods->xUnfetch(real_of(f), offset, page);
}

inline constexpr sqlite3_io_methods io_methods{
    .iVersion = 3,
    .xClose = x_close,
    .xRead = x_read,
    .xWrite = x_write,
    .xTruncate = x_truncate,
    .xSync = x_sync,
    .xFileSize = x_file_size,
    .xLock = x_lock,
    .xUnlock = x_unlock,
    .xCheckReservedLock = x_check_reserved_lock,
    .xFileControl = x_file_control,
    .xSectorSize = x_sector_size,
    .xDeviceCharacteristics = x_device_characteristics,
    .xShmMap = x_shm_map,
    .xShmLock = x_shm_lock,
    .xShmBarrier = x_shm_barrier,
    .xShmUnmap = x_shm_unmap,
    .xFetch = x_fetch,
    .xUnfetch = x_unfetch,
};

struct state {
    sqlite3_vfs* base;
    std::vector<std::filesystem::path> directories;
    counters count;
};

inline state& state_of(sqlite3_vfs* vfs)
{
    return *static_cast<state*>(vfs->pAppData);
}

// A file with no name is a temporary file of SQLite and is not watched.
inline bool is_watched(const state& s, const char* name)
{
    if (name == nullptr) {
        return false;
    }
    const std::string_view file = name;
    return std::ranges::any_of(s.directories, [&](const std::filesystem::path& d) {
        const std::string prefix = d.string() + "/";
        return file.starts_with(prefix);
    });
}

inline int v_open(sqlite3_vfs* vfs, sqlite3_filename name, sqlite3_file* f, const int flags, int* out_flags)
{
    auto& s = state_of(vfs);
    auto& w = file_of(f);
    w.real = static_cast<sqlite3_file*>(sqlite3_malloc(s.base->szOsFile));
    if (w.real == nullptr) {
        f->pMethods = nullptr;
        return SQLITE_NOMEM;
    }
    std::ranges::fill(std::span{reinterpret_cast<std::byte*>(w.real), static_cast<std::size_t>(s.base->szOsFile)},
                      std::byte{0});
    w.count = &s.count;
    w.watched = is_watched(s, name);
    if (w.watched && std::string_view{name}.ends_with("-wal")) {
        s.count.wal_opened = true;
    }
    w.last_errno = 0;
    const int r = s.base->xOpen(s.base, name, w.real, flags, out_flags);
    if (r != SQLITE_OK) {
        sqlite3_free(w.real);
        f->pMethods = nullptr;
        return r;
    }
    f->pMethods = &io_methods;
    return r;
}

inline int v_delete(sqlite3_vfs* vfs, const char* name, const int sync_directory)
{
    auto& s = state_of(vfs);
    if (is_watched(s, name) && io_error_now(s.count)) {
        return hit(s.count, SQLITE_IOERR_DELETE);
    }
    return s.base->xDelete(s.base, name, sync_directory);
}

inline int v_access(sqlite3_vfs* vfs, const char* name, const int flags, int* out)
{
    auto& s = state_of(vfs);
    if (is_watched(s, name) && io_error_now(s.count)) {
        return hit(s.count, SQLITE_IOERR_ACCESS);
    }
    return s.base->xAccess(s.base, name, flags, out);
}

inline int v_full_pathname(sqlite3_vfs* vfs, const char* name, const int size, char* out)
{
    return state_of(vfs).base->xFullPathname(state_of(vfs).base, name, size, out);
}

inline int v_randomness(sqlite3_vfs* vfs, const int size, char* out)
{
    return state_of(vfs).base->xRandomness(state_of(vfs).base, size, out);
}

inline int v_sleep(sqlite3_vfs* vfs, const int microseconds)
{
    return state_of(vfs).base->xSleep(state_of(vfs).base, microseconds);
}

inline int v_current_time_int64(sqlite3_vfs* vfs, sqlite3_int64* out)
{
    return state_of(vfs).base->xCurrentTimeInt64(state_of(vfs).base, out);
}

inline int v_get_last_error(sqlite3_vfs*, int, char*)
{
    return EIO;
}

// The VFS is the default VFS of the test program while this object lives.
class registered_vfs {
public:
    registered_vfs()
    {
        state_.base = sqlite3_vfs_find(nullptr);
        if (state_.base == nullptr) [[unlikely]] {
            throw std::logic_error("io_error_vfs: SQLite has no default VFS");
        }
        vfs_.iVersion = 2;
        vfs_.szOsFile = static_cast<int>(sizeof(watched_file));
        vfs_.mxPathname = state_.base->mxPathname;
        vfs_.zName = "io_error";
        vfs_.pAppData = &state_;
        vfs_.xOpen = v_open;
        vfs_.xDelete = v_delete;
        vfs_.xAccess = v_access;
        vfs_.xFullPathname = v_full_pathname;
        vfs_.xRandomness = v_randomness;
        vfs_.xSleep = v_sleep;
        vfs_.xGetLastError = v_get_last_error;
        vfs_.xCurrentTimeInt64 = v_current_time_int64;
        if (sqlite3_vfs_register(&vfs_, 1) != SQLITE_OK) [[unlikely]] {
            throw std::logic_error("io_error_vfs: SQLite refused the VFS");
        }
    }
    registered_vfs(const registered_vfs&) = delete;
    registered_vfs& operator=(const registered_vfs&) = delete;
    ~registered_vfs() { sqlite3_vfs_unregister(&vfs_); }

    void watch(const std::vector<std::filesystem::path>& directories)
    {
        state_.directories.clear();
        state_.count.wal_opened = false;
        std::ranges::transform(directories, std::back_inserter(state_.directories),
                               [](const std::filesystem::path& d) { return std::filesystem::canonical(d); });
    }

    void arm(const int pending, const bool persist, const int diskfull)
    {
        auto& c = state_.count;
        c.io_error_hit = 0;
        c.operations = 0;
        c.first_hit_code = SQLITE_OK;
        c.first_hit_before_the_wal = false;
        c.io_error_persist = persist;
        c.diskfull_pending = diskfull;
        c.io_error_pending = pending;
    }

    void disarm() { arm(0, false, 0); }
    counters& count() { return state_.count; }

private:
    state state_{};
    sqlite3_vfs vfs_{};
};

}
