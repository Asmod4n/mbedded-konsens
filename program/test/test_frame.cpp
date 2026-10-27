#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <string>

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
