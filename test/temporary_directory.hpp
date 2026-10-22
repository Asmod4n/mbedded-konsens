#pragma once

#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>

// Each test that opens an LMDB environment needs a directory of its own,
// and the directory must go when the test ends, also when a check fails.
class temporary_directory {
public:
    temporary_directory()
    {
        std::string name = (std::filesystem::temp_directory_path() / "konsens-test-XXXXXX").string();
        if (mkdtemp(name.data()) == nullptr) [[unlikely]] {
            throw std::system_error(errno, std::generic_category(), "mkdtemp");
        }
        path_ = name;
    }
    temporary_directory(const temporary_directory&) = delete;
    temporary_directory& operator=(const temporary_directory&) = delete;
    ~temporary_directory() { std::filesystem::remove_all(path_); }

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};
