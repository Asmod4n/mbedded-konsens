#pragma once

#include <optional>
#include <string_view>

#include <sys/types.h>

constexpr std::string_view allow_root_flag = "--allow-root-for-test";
constexpr std::string_view help_flag = "--help";
constexpr uid_t root_user_id = 0;
constexpr uid_t test_user_id = 65534;
constexpr int exit_refused_root = 2;

inline std::optional<uid_t> choose_the_user_id(const uid_t effective_user_id, const bool allow_root)
{
    if (effective_user_id != root_user_id) {
        return effective_user_id;
    }
    if (!allow_root) {
        return std::nullopt;
    }
    return test_user_id;
}
