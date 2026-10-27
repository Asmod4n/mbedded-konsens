#include <charconv>
#include <cstring>
#include <memory>

#include <netdb.h>

#include "address.hpp"

namespace konsensd {

std::expected<host_and_port, address_errc> split_host_and_port(const std::string_view address)
{
    std::string_view host;
    std::string_view rest;
    if (address.starts_with('[')) {
        const auto close = address.find(']');
        if (close == std::string_view::npos) [[unlikely]] {
            return std::unexpected(address_errc::malformed);
        }
        host = address.substr(1, close - 1);
        rest = address.substr(close + 1);
    } else {
        const auto colon = address.rfind(':');
        if (colon == std::string_view::npos) [[unlikely]] {
            return std::unexpected(address_errc::malformed);
        }
        host = address.substr(0, colon);
        rest = address.substr(colon);
        if (host.find(':') != std::string_view::npos) [[unlikely]] {
            return std::unexpected(address_errc::malformed);
        }
    }
    if (!rest.starts_with(':') || host.empty()) [[unlikely]] {
        return std::unexpected(address_errc::malformed);
    }
    const std::string_view port = rest.substr(1);
    std::uint16_t number{};
    const auto [end, ec] = std::from_chars(port.data(), port.data() + port.size(), number);
    if (ec != std::errc{} || end != port.data() + port.size() || port.empty()) [[unlikely]] {
        return std::unexpected(address_errc::malformed);
    }
    return host_and_port{std::string{host}, std::string{port}};
}

std::expected<socket_address, address_errc> resolve(const std::string_view address)
{
    const auto split = split_host_and_port(address);
    if (!split) [[unlikely]] {
        return std::unexpected(split.error());
    }
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* found = nullptr;
    if (getaddrinfo(split->host.c_str(), split->port.c_str(), &hints, &found) != 0 || found == nullptr) [[unlikely]] {
        return std::unexpected(address_errc::not_found);
    }
    const std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> owned{found, &freeaddrinfo};
    socket_address out{};
    if (found->ai_addrlen > sizeof out.storage) [[unlikely]] {
        return std::unexpected(address_errc::not_found);
    }
    std::memcpy(&out.storage, found->ai_addr, found->ai_addrlen);
    out.length = found->ai_addrlen;
    return out;
}

}
