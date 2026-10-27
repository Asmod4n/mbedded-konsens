#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "konsens/state_machine.hpp"

namespace konsensd {

enum class key_value_op : std::uint8_t {
    put = 1,
    erase,
};

struct key_value_write {
    key_value_op op;
    std::vector<std::byte> key;
    std::vector<std::byte> value;

    friend bool operator==(const key_value_write&, const key_value_write&) = default;
};

struct key_value_read {
    bool found;
    std::vector<std::byte> value;

    friend bool operator==(const key_value_read&, const key_value_read&) = default;
};

[[nodiscard]] std::vector<std::byte> encode_write(const key_value_write& w);
[[nodiscard]] std::optional<key_value_write> decode_write(std::span<const std::byte> payload);
[[nodiscard]] std::vector<std::byte> encode_read(const key_value_read& r);
[[nodiscard]] std::optional<key_value_read> decode_read(std::span<const std::byte> payload);

struct key_value {
    konsens::applied apply(const std::span<const std::byte> payload, const konsens::state_view auto&) const
    {
        const auto w = decode_write(payload);
        if (!w) [[unlikely]] {
            return {};
        }
        if (w->op == key_value_op::put) {
            return {{}, {{w->key, w->value}}};
        }
        return {{}, {{w->key, std::nullopt}}};
    }

    std::vector<std::byte> query(const std::span<const std::byte> key, const konsens::state_view auto& view) const
    {
        const auto found = view.find(key);
        if (!found) {
            return encode_read({false, {}});
        }
        return encode_read({true, {found->begin(), found->end()}});
    }
};

}
