#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <cbor/schema.hpp>

#include "konsens/state_machine.hpp"
#include "konsens/storage.hpp"

namespace konsens {

struct mutation_view {
    std::span<const std::byte> key;
    std::optional<std::span<const std::byte>> value;
};

struct request_result_view {
    std::uint64_t error;
    std::span<const std::byte> reply;
    std::vector<mutation_view> mutations;
};

struct request_result {
    std::uint64_t error;
    std::vector<std::byte> reply;
    std::vector<mutation> mutations;

    friend bool operator==(const request_result&, const request_result&) = default;

    operator request_result_view() const;
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

[[nodiscard]] std::vector<std::byte> encode_result(const request_result_view& r);
[[nodiscard]] std::optional<request_result_view> decode_result(std::shared_ptr<const void> owner,
                                                               std::span<const std::byte> encoded);
[[nodiscard]] std::uint32_t compute_result_crc(std::uint64_t index, std::span<const std::byte> encoded) noexcept;
[[nodiscard]] std::vector<std::byte> encode_chosen_results(std::span<const chosen_result> list);
[[nodiscard]] std::optional<std::vector<chosen_result>> decode_chosen_results(std::span<const std::byte> payload);

[[nodiscard]] std::uint32_t extend_base_crc(std::uint32_t base_crc, std::uint64_t index,
                                            std::uint32_t result_crc) noexcept;

struct float_rule {
    std::uint32_t float_digits;
    std::optional<std::uint32_t> float_denominator;

    friend bool operator==(const float_rule&, const float_rule&) = default;
};

[[nodiscard]] bool holds_one_data_item(std::span<const std::byte> encoded);
[[nodiscard]] std::optional<std::vector<std::byte>> round_each_float(std::span<const std::byte> data_item,
                                                                    std::uint32_t float_denominator);
[[nodiscard]] bool floats_agree(double a, double b, const float_rule& rule);
[[nodiscard]] bool data_items_agree(std::span<const std::byte> a, std::span<const std::byte> b, const float_rule& rule);
[[nodiscard]] bool results_agree(std::span<const std::byte> a, std::span<const std::byte> b,
                                 const std::optional<float_rule>& rule);

}

CBOR_DEFINE_TYPE(konsens::indexed_result, cbor::tag{35674}, CBOR_MEMBER(konsens::indexed_result, index),
                 CBOR_MEMBER(konsens::indexed_result, result), CBOR_MEMBER(konsens::indexed_result, crc));
