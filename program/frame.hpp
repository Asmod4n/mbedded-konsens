#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

#include "konsens/storage.hpp"

namespace konsensd {

inline constexpr std::size_t frame_length_bytes = 4;

enum class frame_errc : std::uint8_t {
    too_long = 1,
    malformed_hello,
};

struct frame_buffer {
    std::string bytes;
    std::size_t start;
};

struct hello {
    std::string cluster_name;
    konsens::node_id pid;

    friend bool operator==(const hello&, const hello&) = default;
};

[[nodiscard]] std::string frame_of(std::string_view payload);

[[nodiscard]] std::expected<std::optional<std::string>, frame_errc> take_frame(frame_buffer& buffer,
                                                                                std::uint64_t max_frame_bytes);

[[nodiscard]] std::string encode_hello(const hello& h);

[[nodiscard]] std::expected<hello, frame_errc> decode_hello(std::string_view payload);

}
