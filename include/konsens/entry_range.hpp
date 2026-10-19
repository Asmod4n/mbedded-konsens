#pragma once

#include <cstdint>
#include <stdexcept>

namespace konsens {

inline void throw_if_from_after_to(const std::uint64_t from, const std::uint64_t to)
{
    if (from > to) [[unlikely]] {
        throw std::logic_error("konsens: get_entries takes from <= to");
    }
}

}
