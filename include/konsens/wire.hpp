#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <string>
#include <system_error>
#include <type_traits>
#include <variant>

#include "konsens/message.hpp"

namespace konsens {

enum class wire_errc {
    extraneous_data = 1,
    method_not_allowed,
    unknown_kind,
    unknown_entry_kind,
    entry_field_of_another_kind
};

const std::error_category& wire_category() noexcept;
std::error_code make_error_code(wire_errc code) noexcept;

inline constexpr std::uint8_t post = 2;

inline constexpr std::size_t paxos_kind_count = std::variant_size_v<paxos_msg>;
inline constexpr std::size_t kind_count = paxos_kind_count + std::variant_size_v<heartbeat_msg>;

static_assert(kind_count <= std::size_t{std::numeric_limits<std::uint8_t>::max()} + 1);

std::uint8_t kind_of(const message& m) noexcept;

[[nodiscard]] std::expected<std::string, std::error_code> encode(const message& m);
[[nodiscard]] std::expected<message, std::error_code> decode(std::string bytes);

}

template <>
struct std::is_error_code_enum<konsens::wire_errc> : std::true_type {};
