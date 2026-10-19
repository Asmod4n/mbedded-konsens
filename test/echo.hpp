#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "konsens/state_machine.hpp"

// The Echo Protocol of RFC 862 as a state machine: it holds no state and
// gives each payload back. The tests need a state machine type for the
// node, and this one adds no behaviour that a test must check.
struct echo {
    std::vector<std::byte> apply(const std::span<const std::byte> payload) { return {payload.begin(), payload.end()}; }

    std::vector<std::byte> query(const std::span<const std::byte> payload) const
    {
        return {payload.begin(), payload.end()};
    }
};

static_assert(konsens::state_machine<echo>);
