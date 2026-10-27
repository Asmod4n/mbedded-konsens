#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <string>
#include <system_error>
#include <type_traits>
#include <variant>
#include <vector>

#include <cbor/ref.hpp>

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

template <typename Paxos, typename Heartbeat>
struct received_msg_of;

template <typename... Paxos, typename... Heartbeat>
struct received_msg_of<std::variant<Paxos...>, std::variant<Heartbeat...>> {
    using type = std::variant<cbor::ref<Paxos>..., cbor::ref<Heartbeat>...>;
};

using received_msg = received_msg_of<paxos_msg, heartbeat_msg>::type;

struct received {
    node_id from;
    node_id to;
    received_msg msg;
};

std::uint8_t kind_of(const message& m) noexcept;
std::uint8_t kind_of(const received& r) noexcept;

[[nodiscard]] std::expected<std::string, std::error_code> encode(const message& m);
[[nodiscard]] std::expected<received, std::error_code> decode(std::string bytes);

[[nodiscard]] std::expected<std::vector<std::byte>, std::error_code> encode(const cluster_config& c);
[[nodiscard]] std::expected<cluster_config, std::error_code> decode_cluster_config(std::span<const std::byte> payload);

}

template <>
struct std::is_error_code_enum<konsens::wire_errc> : std::true_type {};
