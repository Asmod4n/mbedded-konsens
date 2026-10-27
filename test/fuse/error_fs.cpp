#include <fuse.h>

#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// A passthrough file system of the high-level API of libfuse 3 that fails
// I/O as the target "error" of the device mapper does. The tests run SQLite
// on it, so that a fault comes from a file system and not from a wrapper
// of the storage. The faults:
// - error_fsync: each fsync and fdatasync returns EIO.
// - error_writes: each write of a konsens.db or of its WAL returns ENOSPC.
// - error_reads_offset and error_reads_length: each read of a konsens.db
//   or of its WAL that touches the range returns EIO.
// The first two are mount options, and a write of "error_fsync",
// "error_writes" or "none" to the file /.error_fs of the mount sets them
// while SQLite keeps its files open. The write returns after the change, so
// the next operation of the test meets the new faults. error_fs writes one
// byte to ready_fd when the kernel has sent FUSE_INIT, so that the test
// waits for the mount without a loop.

namespace {

struct options {
    char* backing = nullptr;
    int error_fsync = 0;
    int error_writes = 0;
    std::uint64_t error_reads_offset = 0;
    std::uint64_t error_reads_length = 0;
    int ready_fd = -1;
};

const fuse_opt specs[] = {{"backing=%s", offsetof(options, backing), 0},
                          {"error_fsync", offsetof(options, error_fsync), 1},
                          {"error_writes", offsetof(options, error_writes), 1},
                          {"error_reads_offset=%lu", offsetof(options, error_reads_offset), 0},
                          {"error_reads_length=%lu", offsetof(options, error_reads_length), 0},
                          {"ready_fd=%d", offsetof(options, ready_fd), 0},
                          FUSE_OPT_END};

struct mount {
    options parsed;
    std::atomic<bool> error_fsync;
    std::atomic<bool> error_writes;
};

constexpr std::string_view data_file = "/konsens.db";
constexpr std::string_view wal_file = "/konsens.db-wal";
constexpr std::string_view control_file = "/.error_fs";
constexpr std::uint64_t control_handle = UINT64_MAX;

mount& this_mount()
{
    return *static_cast<mount*>(fuse_get_context()->private_data);
}

std::string backing_path(const char* path)
{
    return std::string(this_mount().parsed.backing) + path;
}

bool is_data_file(const std::string_view path)
{
    return path.ends_with(data_file) || path.ends_with(wal_file);
}

int status_of(const int result)
{
    return result == -1 ? -errno : 0;
}

int fd_of(const fuse_file_info* fi)
{
    return static_cast<int>(fi->fh);
}

void* fs_init(fuse_conn_info*, fuse_config*)
{
    const char ready = 1;
    const int fd = this_mount().parsed.ready_fd;
    if (write(fd, &ready, 1) != 1 || close(fd) != 0) [[unlikely]] {
        fuse_exit(fuse_get_context()->fuse);
    }
    return fuse_get_context()->private_data;
}

int fs_getattr(const char* path, struct stat* st, fuse_file_info* fi)
{
    if (path == control_file) {
        *st = {};
        st->st_mode = S_IFREG | S_IWUSR;
        st->st_nlink = 1;
        return 0;
    }
    if (fi != nullptr && fi->fh != control_handle) {
        return status_of(fstat(fd_of(fi), st));
    }
    return status_of(lstat(backing_path(path).c_str(), st));
}

int fs_readdir(const char* path, void* buf, const fuse_fill_dir_t fill, off_t, fuse_file_info*, fuse_readdir_flags)
{
    DIR* dir = opendir(backing_path(path).c_str());
    if (dir == nullptr) [[unlikely]] {
        return -errno;
    }
    for (const dirent* e = readdir(dir); e != nullptr; e = readdir(dir)) {
        fill(buf, e->d_name, nullptr, 0, FUSE_FILL_DIR_DEFAULTS);
    }
    return status_of(closedir(dir));
}

int fs_open(const char* path, fuse_file_info* fi)
{
    if (path == control_file) {
        fi->fh = control_handle;
        fi->direct_io = 1;
        return 0;
    }
    const int fd = open(backing_path(path).c_str(), fi->flags);
    if (fd == -1) [[unlikely]] {
        return -errno;
    }
    fi->fh = static_cast<std::uint64_t>(fd);
    return 0;
}

int fs_create(const char* path, const mode_t mode, fuse_file_info* fi)
{
    const int fd = open(backing_path(path).c_str(), fi->flags, mode);
    if (fd == -1) [[unlikely]] {
        return -errno;
    }
    fi->fh = static_cast<std::uint64_t>(fd);
    return 0;
}

bool touches_error_range(const std::string_view path, const off_t offset, const std::size_t size)
{
    const auto& o = this_mount().parsed;
    const auto first = static_cast<std::uint64_t>(offset);
    return is_data_file(path) && o.error_reads_length > 0 && first < o.error_reads_offset + o.error_reads_length &&
           o.error_reads_offset < first + size;
}

int fs_read(const char* path, char* buf, const std::size_t size, const off_t offset, fuse_file_info* fi)
{
    if (touches_error_range(path, offset, size)) {
        return -EIO;
    }
    const ssize_t r = pread(fd_of(fi), buf, size, offset);
    return r == -1 ? -errno : static_cast<int>(r);
}

int set_faults(const std::string_view command)
{
    auto& m = this_mount();
    const auto word = command.substr(0, command.find_first_of("\n"));
    if (word == "error_fsync") {
        m.error_fsync = true;
    } else if (word == "error_writes") {
        m.error_writes = true;
    } else if (word == "none") {
        m.error_fsync = false;
        m.error_writes = false;
    } else {
        return -EINVAL;
    }
    return 0;
}

int fs_write(const char* path, const char* buf, const std::size_t size, const off_t offset, fuse_file_info* fi)
{
    if (fi->fh == control_handle) {
        const int status = set_faults({buf, size});
        return status != 0 ? status : static_cast<int>(size);
    }
    if (this_mount().error_writes && is_data_file(path)) {
        return -ENOSPC;
    }
    const ssize_t r = pwrite(fd_of(fi), buf, size, offset);
    return r == -1 ? -errno : static_cast<int>(r);
}

int fs_fsync(const char*, const int datasync, fuse_file_info* fi)
{
    if (this_mount().error_fsync) {
        return -EIO;
    }
    return status_of(datasync != 0 ? fdatasync(fd_of(fi)) : fsync(fd_of(fi)));
}

int fs_release(const char*, fuse_file_info* fi)
{
    if (fi->fh == control_handle) {
        return 0;
    }
    return status_of(close(fd_of(fi)));
}

int fs_truncate(const char* path, const off_t size, fuse_file_info* fi)
{
    if (path == control_file) {
        return 0;
    }
    return status_of(fi != nullptr ? ftruncate(fd_of(fi), size) : truncate(backing_path(path).c_str(), size));
}

int fs_unlink(const char* path)
{
    return status_of(unlink(backing_path(path).c_str()));
}

int fs_mkdir(const char* path, const mode_t mode)
{
    return status_of(mkdir(backing_path(path).c_str(), mode));
}

}

int main(int argc, char* argv[])
{
    fuse_args args = FUSE_ARGS_INIT(argc, argv);
    mount m{};
    if (fuse_opt_parse(&args, &m.parsed, specs, nullptr) == -1 || m.parsed.backing == nullptr ||
        m.parsed.ready_fd < 0) {
        return 2;
    }
    m.error_fsync = m.parsed.error_fsync != 0;
    m.error_writes = m.parsed.error_writes != 0;
    fuse_operations ops{};
    ops.init = fs_init;
    ops.getattr = fs_getattr;
    ops.readdir = fs_readdir;
    ops.open = fs_open;
    ops.create = fs_create;
    ops.read = fs_read;
    ops.write = fs_write;
    ops.fsync = fs_fsync;
    ops.release = fs_release;
    ops.truncate = fs_truncate;
    ops.unlink = fs_unlink;
    ops.mkdir = fs_mkdir;
    const int status = fuse_main(args.argc, args.argv, &ops, &m);
    fuse_opt_free_args(&args);
    std::free(m.parsed.backing);
    return status;
}
