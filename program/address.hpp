#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <system_error>

#include <sys/socket.h>
#include <sys/un.h>

namespace konsensd {

constexpr std::string_view unix_prefix = "unix:";
constexpr std::size_t most_unix_path_bytes = sizeof(sockaddr_un::sun_path) - 1;

struct host_and_port {
    std::string host;
    std::string port;

    friend bool operator==(const host_and_port&, const host_and_port&) = default;
};

struct socket_address {
    sockaddr_storage storage;
    socklen_t length;
};

enum class address_errc : std::uint8_t {
    malformed = 1,
    not_found,
};

[[nodiscard]] std::expected<host_and_port, address_errc> split_host_and_port(std::string_view address);

[[nodiscard]] std::expected<socket_address, address_errc> resolve(std::string_view address);

[[nodiscard]] bool uses_tcp(const socket_address& address);

}
