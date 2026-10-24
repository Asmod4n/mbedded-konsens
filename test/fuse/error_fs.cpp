#include <fuse.h>

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
// I/O as the target "error" of the device mapper does: each fsync with
// error_fsync, and each read of data.mdb that touches the range from
// error_reads_offset over error_reads_length bytes. The tests run LMDB on
// it, so that a fault comes from a file system and not from a wrapper of
// the storage. It writes one byte to ready_fd when the kernel has sent
// FUSE_INIT, so that the test waits for the mount without a loop.

namespace {

struct options {
    char* backing = nullptr;
    int error_fsync = 0;
    std::uint64_t error_reads_offset = 0;
    std::uint64_t error_reads_length = 0;
    int ready_fd = -1;
};

const fuse_opt specs[] = {{"backing=%s", offsetof(options, backing), 0},
                          {"error_fsync", offsetof(options, error_fsync), 1},
                          {"error_reads_offset=%lu", offsetof(options, error_reads_offset), 0},
                          {"error_reads_length=%lu", offsetof(options, error_reads_length), 0},
                          {"ready_fd=%d", offsetof(options, ready_fd), 0},
                          FUSE_OPT_END};

constexpr std::string_view data_file = "/data.mdb";

const options& options_of_mount()
{
    return *static_cast<const options*>(fuse_get_context()->private_data);
}

std::string backing_path(const char* path)
{
    return std::string(options_of_mount().backing) + path;
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
    const int fd = options_of_mount().ready_fd;
    if (write(fd, &ready, 1) != 1 || close(fd) != 0) [[unlikely]] {
        fuse_exit(fuse_get_context()->fuse);
    }
    return fuse_get_context()->private_data;
}

int fs_getattr(const char* path, struct stat* st, fuse_file_info* fi)
{
    return status_of(fi != nullptr ? fstat(fd_of(fi), st) : lstat(backing_path(path).c_str(), st));
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
    const auto& o = options_of_mount();
    const auto first = static_cast<std::uint64_t>(offset);
    return path == data_file && o.error_reads_length > 0 && first < o.error_reads_offset + o.error_reads_length &&
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

int fs_write(const char*, const char* buf, const std::size_t size, const off_t offset, fuse_file_info* fi)
{
    const ssize_t r = pwrite(fd_of(fi), buf, size, offset);
    return r == -1 ? -errno : static_cast<int>(r);
}

int fs_fsync(const char*, const int datasync, fuse_file_info* fi)
{
    if (options_of_mount().error_fsync != 0) {
        return -EIO;
    }
    return status_of(datasync != 0 ? fdatasync(fd_of(fi)) : fsync(fd_of(fi)));
}

int fs_release(const char*, fuse_file_info* fi)
{
    return status_of(close(fd_of(fi)));
}

int fs_truncate(const char* path, const off_t size, fuse_file_info* fi)
{
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
    options opts;
    if (fuse_opt_parse(&args, &opts, specs, nullptr) == -1 || opts.backing == nullptr || opts.ready_fd < 0) {
        return 2;
    }
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
    const int status = fuse_main(args.argc, args.argv, &ops, &opts);
    fuse_opt_free_args(&args);
    std::free(opts.backing);
    return status;
}
