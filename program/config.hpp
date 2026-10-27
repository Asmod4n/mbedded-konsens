#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "konsens/node.hpp"

namespace konsensd {

inline constexpr std::uint64_t default_election_ticks = 5;
inline constexpr std::uint64_t default_resend_ticks = 3;
inline constexpr std::uint64_t default_tick_milliseconds = 100;
inline constexpr std::uint64_t default_max_frame_bytes = std::uint64_t{1} << 30;
inline constexpr std::uint64_t default_max_body_bytes = std::uint64_t{1} << 20;

struct peer {
    konsens::node_id pid;
    std::string address;

    friend bool operator==(const peer&, const peer&) = default;
};

struct config {
    std::string cluster_name;
    std::uint32_t configuration_id;
    konsens::node_id pid;
    std::filesystem::path data_directory;
    std::string peer_address;
    std::string client_address;
    std::uint32_t leader_priority;
    std::uint64_t election_ticks;
    std::uint64_t resend_ticks;
    std::uint64_t tick_milliseconds;
    std::uint64_t max_frame_bytes;
    std::uint64_t max_body_bytes;
    std::vector<konsens::node_id> build_from;
    std::vector<peer> peers;

    friend bool operator==(const config&, const config&) = default;
};

enum class config_errc : std::uint8_t {
    syntax = 1,
    missing_key,
    wrong_type,
    out_of_range,
    unknown_key,
    duplicate_pid,
};

struct config_error {
    config_errc code;
    std::string_view key;
    std::uint32_t line;
    std::uint32_t column;

    friend bool operator==(const config_error&, const config_error&) = default;
};

[[nodiscard]] std::expected<config, config_error> parse_config(std::string_view text);

[[nodiscard]] konsens::node_config node_config_of(const config& c, bool first_start);

}
