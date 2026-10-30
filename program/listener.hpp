#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string_view>

#include "address.hpp"

namespace konsensd {

struct listener {
    int fd;
    std::filesystem::path socket_file;
};

enum class listener_errc : std::uint8_t {
    does_not_resolve = 1,
    in_use,
    not_a_socket,
    no_listener,
};

[[nodiscard]] std::string_view reason_of(listener_errc e);

[[nodiscard]] std::expected<void, listener_errc> remove_a_stale_socket_file(const std::filesystem::path& file,
                                                                            const socket_address& address);

[[nodiscard]] std::expected<listener, listener_errc> listener_on(std::string_view address);

}
