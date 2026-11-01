#include <cerrno>
#include <system_error>

#include <sys/socket.h>
#include <unistd.h>

#include "listener.hpp"

namespace konsensd {

namespace {

constexpr int listen_backlog = 64;

}

std::string_view reason_of(const listener_errc e)
{
    switch (e) {
    case listener_errc::does_not_resolve:
        return "the address does not resolve";
    case listener_errc::in_use:
        return "a process accepts on the socket file";
    case listener_errc::not_a_socket:
        return "the path names a file that is not a socket";
    case listener_errc::no_listener:
        return "the bind or the listen fails";
    }
    return "unknown";
}

std::expected<void, listener_errc> remove_a_stale_socket_file(const std::filesystem::path& file,
                                                              const socket_address& address)
{
    std::error_code e;
    const auto type = std::filesystem::symlink_status(file, e).type();
    if (type == std::filesystem::file_type::not_found) {
        return {};
    }
    if (type != std::filesystem::file_type::socket) [[unlikely]] {
        return std::unexpected(type == std::filesystem::file_type::none ? listener_errc::no_listener
                                                                        : listener_errc::not_a_socket);
    }
    const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (fd < 0) [[unlikely]] {
        return std::unexpected(listener_errc::no_listener);
    }
    const int connected = ::connect(fd, reinterpret_cast<const sockaddr*>(&address.storage), address.length);
    const int why = connected == 0 ? 0 : errno;
    ::close(fd);
    if (why != ECONNREFUSED) [[unlikely]] {
        return std::unexpected(listener_errc::in_use);
    }
    if (!std::filesystem::remove(file, e)) [[unlikely]] {
        return std::unexpected(listener_errc::no_listener);
    }
    return {};
}

std::expected<listener, listener_errc> listener_on(const std::string_view address)
{
    const auto a = resolve(address);
    if (!a) [[unlikely]] {
        return std::unexpected(listener_errc::does_not_resolve);
    }
    const bool tcp = uses_tcp(*a);
    std::filesystem::path file;
    if (!tcp) {
        file = address.substr(unix_prefix.size());
        if (const auto r = remove_a_stale_socket_file(file, *a); !r) [[unlikely]] {
            return std::unexpected(r.error());
        }
    }
    const int fd = ::socket(a->storage.ss_family, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) [[unlikely]] {
        return std::unexpected(listener_errc::no_listener);
    }
    if (tcp) {
        const int one = 1;
        ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    }
    if (::bind(fd, reinterpret_cast<const sockaddr*>(&a->storage), a->length) != 0 ||
        ::listen(fd, listen_backlog) != 0) [[unlikely]] {
        ::close(fd);
        return std::unexpected(listener_errc::no_listener);
    }
    return listener{fd, std::move(file)};
}

}
