#pragma once

#include <cerrno>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <stdlib.h>

constexpr std::string_view temporary_directory_template = "konsens-example-XXXXXX";

class temporary_directories {
public:
    temporary_directories() = default;
    temporary_directories(const temporary_directories&) = delete;
    temporary_directories& operator=(const temporary_directories&) = delete;
    ~temporary_directories()
    {
        for (const auto& p : paths_) {
            std::error_code ignored;
            std::filesystem::remove_all(p, ignored);
        }
    }

    std::filesystem::path create()
    {
        std::string name = (std::filesystem::temp_directory_path() / temporary_directory_template).string();
        if (mkdtemp(name.data()) == nullptr) [[unlikely]] {
            throw std::system_error(errno, std::generic_category(), "mkdtemp");
        }
        paths_.emplace_back(name);
        return paths_.back();
    }

private:
    std::vector<std::filesystem::path> paths_;
};
