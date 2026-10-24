#pragma once

#include <array>
#include <cerrno>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include <fcntl.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <doctest/doctest.h>

// A test that needs a fault of the file system under LMDB mounts error_fs
// on a backing directory. The constructor returns after the kernel has
// sent FUSE_INIT: error_fs writes one byte to a pipe then, and the
// constructor blocks on the read of that byte. error_fs runs in a process
// of its own: a FUSE server in the process that maps its files hangs at
// the first page fault.
class mounted_error_fs {
public:
    mounted_error_fs(const std::filesystem::path& backing, const std::filesystem::path& mount_point,
                     const std::string& faults)
        : mount_point_(mount_point)
    {
        std::array<int, 2> ready{};
        REQUIRE(pipe2(ready.data(), O_CLOEXEC) == 0);
        REQUIRE(fcntl(ready[1], F_SETFD, 0) == 0);
        std::string options = "backing=" + backing.string() + ",ready_fd=" + std::to_string(ready[1]);
        if (!faults.empty()) {
            options += "," + faults;
        }
        std::vector<std::string> arguments{KONSENS_ERROR_FS, "-f", mount_point.string(), "-o", options};
        std::vector<char*> argv;
        for (auto& a : arguments) {
            argv.push_back(a.data());
        }
        argv.push_back(nullptr);
        std::array<char*, 1> envp{nullptr};
        const int spawned = posix_spawn(&server_, argv.front(), nullptr, nullptr, argv.data(), envp.data());
        if (spawned != 0) {
            server_ = 0;
        }
        close(ready[1]);
        char byte = 0;
        const ssize_t got = spawned == 0 ? read(ready[0], &byte, 1) : 0;
        close(ready[0]);
        REQUIRE(spawned == 0);
        mounted_ = got == 1;
        REQUIRE(mounted_);
    }
    mounted_error_fs(const mounted_error_fs&) = delete;
    mounted_error_fs& operator=(const mounted_error_fs&) = delete;
    ~mounted_error_fs()
    {
        if (mounted_) {
            static_cast<void>(fusermount({"-u", "-z"}));
        }
        if (server_ > 0) {
            int status = 0;
            static_cast<void>(waitpid(server_, &status, 0));
        }
    }

    // The test closes each storage on the mount before it unmounts, as
    // fusermount3 refuses a mount with open files.
    void unmount()
    {
        REQUIRE(fusermount({"-u"}) == 0);
        mounted_ = false;
        int status = 0;
        REQUIRE(waitpid(server_, &status, 0) == server_);
        server_ = 0;
        REQUIRE(WIFEXITED(status));
        REQUIRE(WEXITSTATUS(status) == 0);
    }

private:
    int fusermount(std::vector<std::string> arguments) const
    {
        arguments.insert(arguments.begin(), KONSENS_FUSERMOUNT);
        arguments.push_back(mount_point_.string());
        std::vector<char*> argv;
        for (auto& a : arguments) {
            argv.push_back(a.data());
        }
        argv.push_back(nullptr);
        std::array<char*, 1> envp{nullptr};
        pid_t pid = 0;
        if (posix_spawn(&pid, argv.front(), nullptr, nullptr, argv.data(), envp.data()) != 0) {
            return -1;
        }
        int status = 0;
        if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status)) {
            return -1;
        }
        return WEXITSTATUS(status);
    }

    std::filesystem::path mount_point_;
    pid_t server_ = 0;
    bool mounted_ = false;
};
