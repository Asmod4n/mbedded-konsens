#pragma once

#include <cstdint>

#include "config.hpp"

namespace konsensd {

inline constexpr int exit_stopped = 0;
inline constexpr int exit_failed = 1;

struct start_options {
    bool first_start;
    std::uint64_t nonce;
};

[[nodiscard]] int serve(const config& c, start_options options);

}
