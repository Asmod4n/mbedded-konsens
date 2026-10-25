#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "konsens/state_machine.hpp"
#include "konsens/storage.hpp"

namespace konsens {

struct request_result {
    std::uint64_t error;
    std::vector<std::byte> reply;
    std::vector<mutation> mutations;

    friend bool operator==(const request_result&, const request_result&) = default;
};

struct indexed_result {
    std::uint64_t index;
    std::vector<std::byte> result;
    std::uint32_t crc;

    friend bool operator==(const indexed_result&, const indexed_result&) = default;
};

struct chosen_result {
    indexed_result chosen;
    node_id node;
    std::vector<node_id> out;

    friend bool operator==(const chosen_result&, const chosen_result&) = default;
};

[[nodiscard]] std::vector<std::byte> encode_result(const request_result& r);
[[nodiscard]] std::optional<request_result> decode_result(std::span<const std::byte> encoded);
[[nodiscard]] std::uint32_t compute_result_crc(std::uint64_t index, std::span<const std::byte> encoded) noexcept;
[[nodiscard]] std::vector<std::byte> encode_chosen_results(std::span<const chosen_result> list);
[[nodiscard]] std::optional<std::vector<chosen_result>> decode_chosen_results(std::span<const std::byte> payload);

struct verdicts {
    std::uint64_t pointer;
    std::vector<chosen_result> chosen;
    std::vector<std::uint64_t> voids;
    std::uint64_t applied_idx;

    friend bool operator==(const verdicts&, const verdicts&) = default;
};

[[nodiscard]] verdicts find_the_verdicts(std::uint64_t pointer, std::uint64_t first_index,
                                         std::span<const entry> entries);

[[nodiscard]] std::uint32_t extend_base_crc(std::uint32_t base_crc, std::uint64_t index,
                                            std::uint32_t result_crc) noexcept;

}
