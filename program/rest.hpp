#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace konsensd {

inline constexpr std::string_view key_value_path = "/kv/";
inline constexpr std::size_t most_request_head_bytes = 8192;
inline constexpr std::uint32_t retry_after_seconds = 1;
inline constexpr std::uint16_t status_ok = 200;
inline constexpr std::uint16_t status_no_content = 204;
inline constexpr std::uint16_t status_not_found = 404;

enum class rest_method : std::uint8_t {
    get = 1,
    put,
    erase,
};

struct rest_request {
    rest_method method;
    std::vector<std::byte> key;
    std::vector<std::byte> body;
    std::size_t bytes;
    bool closes;

    friend bool operator==(const rest_request&, const rest_request&) = default;
};

struct problem {
    std::uint16_t status;
    std::string_view detail;
    bool retry_after;

    friend bool operator==(const problem&, const problem&) = default;
};

[[nodiscard]] std::expected<std::optional<rest_request>, problem> take_request(std::string_view bytes,
                                                                               std::uint64_t max_body_bytes);

[[nodiscard]] problem problem_of(const std::error_code& e);

[[nodiscard]] std::string response_of(std::uint16_t status, std::string_view content_type,
                                      std::span<const std::byte> body, bool closes);

[[nodiscard]] std::string problem_response_of(const problem& p, bool closes);

}
