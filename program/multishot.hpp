#pragma once

#include <cstdint>

namespace konsensd {

enum class receive_outcome : std::uint8_t {
    bytes_and_armed,
    bytes_and_ended,
    end_of_stream,
    no_buffer,
    error,
};

enum class accept_outcome : std::uint8_t {
    descriptor_and_armed,
    descriptor_and_ended,
    error,
};

[[nodiscard]] receive_outcome receive_outcome_of(int res, std::uint32_t flags);

[[nodiscard]] accept_outcome accept_outcome_of(int res, std::uint32_t flags);

}
