#pragma once

#include <concepts>
#include <cstddef>
#include <span>
#include <vector>

namespace konsens {

template <typename M>
concept state_machine = requires(M& m, const M& cm, std::span<const std::byte> payload) {
    { m.apply(payload) } -> std::same_as<std::vector<std::byte>>;
    { cm.query(payload) } -> std::same_as<std::vector<std::byte>>;
};

}
