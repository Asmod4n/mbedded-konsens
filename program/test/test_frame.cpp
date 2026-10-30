#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>

#include <sys/un.h>

#include "address.hpp"
#include "frame.hpp"

using konsensd::frame_buffer;
using konsensd::frame_errc;

// A frame that arrives whole gives its payload, and the buffer is empty
// after it.
TEST_CASE("a frame round trip")
{
    frame_buffer b{konsensd::frame_of("abc") + konsensd::frame_of(""), 0};
    const auto first = konsensd::take_frame(b, 16);
    REQUIRE(first);
    REQUIRE(*first);
    CHECK(**first == "abc");
    const auto second = konsensd::take_frame(b, 16);
    REQUIRE(second);
    REQUIRE(*second);
    CHECK(second->value().empty());
    const auto none = konsensd::take_frame(b, 16);
    REQUIRE(none);
    CHECK(!*none);
}

// TCP cuts a stream at any byte, also inside the length.
TEST_CASE("a frame cut at each byte waits for the rest")
{
    const std::string whole = konsensd::frame_of("hello world");
    for (std::size_t cut = 0; cut < whole.size(); ++cut) {
        frame_buffer b{whole.substr(0, cut), 0};
        const auto early = konsensd::take_frame(b, 64);
        REQUIRE(early);
        CHECK(!*early);
        b.bytes.append(whole.substr(cut));
        const auto late = konsensd::take_frame(b, 64);
        REQUIRE(late);
        REQUIRE(*late);
        CHECK(**late == "hello world");
    }
}

// The length is read before any byte of the payload is kept, so a peer
// cannot make the node reserve memory for a frame above the bound.
TEST_CASE("a frame above the bound is refused")
{
    frame_buffer b{konsensd::frame_of(std::string(17, 'x')), 0};
    const auto r = konsensd::take_frame(b, 16);
    REQUIRE(!r);
    CHECK(r.error() == frame_errc::too_long);
    frame_buffer head{konsensd::frame_of(std::string(17, 'x')).substr(0, konsensd::frame_length_bytes), 0};
    const auto early = konsensd::take_frame(head, 16);
    REQUIRE(!early);
    CHECK(early.error() == frame_errc::too_long);
}

// The hello names the cluster and the sender; anything else is refused.
TEST_CASE("a hello round trip, and a malformed hello")
{
    const konsensd::hello h{"home", 3};
    const auto back = konsensd::decode_hello(konsensd::encode_hello(h));
    REQUIRE(back);
    CHECK(*back == h);
    CHECK(konsensd::decode_hello("x").error() == frame_errc::malformed_hello);
    CHECK(konsensd::decode_hello(konsensd::encode_hello(h) + "x").error() == frame_errc::malformed_hello);
}

// The forms of an address in the config: a name or IPv4 with a port, and
// IPv6 in brackets.
TEST_CASE("an address splits into host and port")
{
    using konsensd::address_errc;
    using konsensd::host_and_port;
    CHECK(konsensd::split_host_and_port("192.0.2.2:7151") == host_and_port{"192.0.2.2", "7151"});
    CHECK(konsensd::split_host_and_port("[::]:7151") == host_and_port{"::", "7151"});
    CHECK(konsensd::split_host_and_port("node-1.example:80") == host_and_port{"node-1.example", "80"});
    for (const auto* bad : {"7151", "::1:7151", "[::1]7151", "[::1", "host:", ":80", "host:65536", "host:8x"}) {
        CHECK(konsensd::split_host_and_port(bad).error() == address_errc::malformed);
    }
    CHECK(konsensd::resolve("127.0.0.1:7151").has_value());
    CHECK(konsensd::resolve("[::1]:7151").has_value());
}

// Each form of an address that resolve takes: the family that it gives,
// and whether the socket speaks TCP.
TEST_CASE("each form of an address resolves to its family")
{
    const auto v4 = konsensd::resolve("127.0.0.1:7151");
    REQUIRE(v4);
    CHECK(v4->storage.ss_family == AF_INET);
    CHECK(konsensd::uses_tcp(*v4));
    const auto name = konsensd::resolve("localhost:7151");
    REQUIRE(name);
    CHECK(konsensd::uses_tcp(*name));
    const auto v6 = konsensd::resolve("[::1]:7151");
    REQUIRE(v6);
    CHECK(v6->storage.ss_family == AF_INET6);
    CHECK(konsensd::uses_tcp(*v6));
    const auto un = konsensd::resolve("unix:/run/konsensd/peer.sock");
    REQUIRE(un);
    CHECK(un->storage.ss_family == AF_UNIX);
    CHECK(!konsensd::uses_tcp(*un));
}

namespace {

std::string path_in(const konsensd::socket_address& a)
{
    sockaddr_un u{};
    std::memcpy(&u, &a.storage, sizeof u);
    return std::string{u.sun_path};
}

}

// unix(7) gives the length of a path address as the offset of sun_path,
// the bytes of the path and its NUL. A relative path stays relative, as
// bind resolves it against the working directory. The prefix is read
// before the split, so "unix:7151" names a file and not a port.
TEST_CASE("a unix: address names a socket file")
{
    for (const std::string path : {"/run/konsensd/peer.sock", "peer.sock", "7151"}) {
        const auto a = konsensd::resolve("unix:" + path);
        REQUIRE(a);
        CHECK(a->storage.ss_family == AF_UNIX);
        CHECK(path_in(*a) == path);
        CHECK(a->length == offsetof(sockaddr_un, sun_path) + path.size() + 1);
    }
}

// sun_path holds 108 bytes, so a path of 107 bytes and its NUL fit, and
// 108 bytes do not. An empty path names no file. A path that starts
// with a NUL names the abstract namespace of Linux, which has no file
// permissions, and a NUL inside cuts the path. TOML can carry a NUL in
// a string, so the check is needed.
TEST_CASE("a unix: address outside the bounds is malformed")
{
    using konsensd::address_errc;
    CHECK(konsensd::most_unix_path_bytes == 107);
    const auto longest = konsensd::resolve("unix:/" + std::string(konsensd::most_unix_path_bytes - 1, 'x'));
    REQUIRE(longest);
    CHECK(longest->length == sizeof(sockaddr_un));
    CHECK(konsensd::resolve("unix:/" + std::string(konsensd::most_unix_path_bytes, 'x')).error() ==
          address_errc::malformed);
    CHECK(konsensd::resolve("unix:").error() == address_errc::malformed);
    using namespace std::string_view_literals;
    CHECK(konsensd::resolve("unix:\0peer"sv).error() == address_errc::malformed);
    CHECK(konsensd::resolve("unix:/run/\0peer"sv).error() == address_errc::malformed);
}
