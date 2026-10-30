#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <string>
#include <string_view>
#include <vector>

#include "konsens/client.hpp"
#include "rest.hpp"

namespace {

using konsensd::problem;
using konsensd::rest_method;
using konsensd::take_request;

constexpr std::uint64_t max_body = 16;

std::vector<std::byte> bytes_of(const std::string_view s)
{
    const auto b = std::as_bytes(std::span(s));
    return {b.begin(), b.end()};
}

std::uint16_t status_of(const std::string_view request)
{
    const auto r = take_request(request, max_body);
    REQUIRE(!r);
    return r.error().status;
}

}

// The three calls of a key, as curl sends them.
TEST_CASE("take_request reads GET, PUT and DELETE of a key")
{
    const std::string_view get = "GET /kv/a HTTP/1.1\r\nHost: x\r\n\r\n";
    auto r = take_request(get, max_body);
    REQUIRE(r);
    REQUIRE(*r);
    CHECK(**r == konsensd::rest_request{rest_method::get, bytes_of("a"), {}, get.size(), false});

    const std::string_view put = "PUT /kv/a HTTP/1.1\r\nContent-Length: 3\r\n\r\nxyz";
    r = take_request(put, max_body);
    REQUIRE(r);
    REQUIRE(*r);
    CHECK(**r == konsensd::rest_request{rest_method::put, bytes_of("a"), bytes_of("xyz"), put.size(), false});

    const std::string_view erase = "DELETE /kv/a HTTP/1.1\r\n\r\n";
    r = take_request(erase, max_body);
    REQUIRE(r);
    REQUIRE(*r);
    CHECK((*r)->method == rest_method::erase);
    CHECK((*r)->key == bytes_of("a"));
}

// TCP gives a request in pieces. Each cut before the end waits for more
// bytes; the bytes after the request stay for the next one.
TEST_CASE("take_request waits for the whole request and takes only its bytes")
{
    const std::string whole = "PUT /kv/k HTTP/1.1\r\nContent-Length: 2\r\n\r\nab";
    for (std::size_t cut = 0; cut < whole.size(); ++cut) {
        const auto r = take_request(std::string_view{whole}.substr(0, cut), max_body);
        REQUIRE(r);
        CHECK(!*r);
    }
    const std::string two = whole + "GET /kv/k HTTP/1.1\r\n\r\n";
    const auto r = take_request(two, max_body);
    REQUIRE(r);
    REQUIRE(*r);
    CHECK((*r)->bytes == whole.size());
    CHECK((*r)->body == bytes_of("ab"));
}

// A key is any bytes; the path carries them percent-encoded.
TEST_CASE("take_request decodes a percent-encoded key")
{
    const auto r = take_request("GET /kv/a%2Fb%00c HTTP/1.1\r\n\r\n", max_body);
    REQUIRE(r);
    REQUIRE(*r);
    const std::vector<std::byte> key{std::byte{'a'}, std::byte{'/'}, std::byte{'b'}, std::byte{0}, std::byte{'c'}};
    CHECK((*r)->key == key);
    CHECK(status_of("GET /kv/a%zz HTTP/1.1\r\n\r\n") == 400);
}

// Each refusal of the plan, 3.6 and 3.7, with its status.
TEST_CASE("take_request refuses a wrong request with its status")
{
    CHECK(status_of("POST /kv/a HTTP/1.1\r\n\r\n") == 405);
    CHECK(status_of("PATCH /kv/a HTTP/1.1\r\n\r\n") == 405);
    CHECK(status_of("GET /a HTTP/1.1\r\n\r\n") == 404);
    CHECK(status_of("GET /kv/ HTTP/1.1\r\n\r\n") == 404);
    CHECK(status_of("GET /kv/a/b HTTP/1.1\r\n\r\n") == 404);
    CHECK(status_of("GET /kv/a?x=1 HTTP/1.1\r\n\r\n") == 404);
    CHECK(status_of("PUT /kv/a HTTP/1.1\r\n\r\n") == 411);
    CHECK(status_of("PUT /kv/a HTTP/1.1\r\nContent-Length: 17\r\n\r\n") == 413);
    CHECK(status_of("PUT /kv/a HTTP/1.1\r\nContent-Length: x\r\n\r\n") == 400);
    CHECK(status_of("PUT /kv/a HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n") == 501);
    CHECK(status_of("GET /kv/a HTTP/2.0\r\n\r\n") == 505);
    CHECK(status_of("GET /kv/a HTTP/1.1\n\n") == 400);
    CHECK(status_of("GET /kv/a HTTP/1.1\r\nX: " + std::string(konsensd::most_request_head_bytes, 'y')) == 431);
}

// The body bound holds at the bound itself.
TEST_CASE("take_request takes a body of max_body_bytes")
{
    const std::string request = "PUT /kv/a HTTP/1.1\r\nContent-Length: 16\r\n\r\n" + std::string(max_body, 'v');
    const auto r = take_request(request, max_body);
    REQUIRE(r);
    REQUIRE(*r);
    CHECK((*r)->body.size() == max_body);
}

// RFC 9112 9.3: HTTP/1.1 keeps the connection unless "close" is sent;
// HTTP/1.0 closes unless "keep-alive" is sent.
TEST_CASE("take_request reads whether the client closes")
{
    const auto closes = [](const std::string_view request) {
        const auto r = take_request(request, max_body);
        REQUIRE(r);
        REQUIRE(*r);
        return (*r)->closes;
    };
    CHECK(!closes("GET /kv/a HTTP/1.1\r\n\r\n"));
    CHECK(closes("GET /kv/a HTTP/1.1\r\nConnection: close\r\n\r\n"));
    CHECK(closes("GET /kv/a HTTP/1.1\r\nconnection: Close\r\n\r\n"));
    CHECK(closes("GET /kv/a HTTP/1.0\r\n\r\n"));
    CHECK(!closes("GET /kv/a HTTP/1.0\r\nConnection: keep-alive\r\n\r\n"));
}

// Plan 3.7: every value of client_errc, and an error of another category.
TEST_CASE("problem_of gives the status of each client error")
{
    using konsens::client_errc;
    const auto of = [](const client_errc e) { return konsensd::problem_of(make_error_code(e)); };
    CHECK(of(client_errc::retry) == problem{503, "the node cannot take the call now", true});
    CHECK(of(client_errc::building) == problem{503, "the node cannot take the call now", true});
    CHECK(of(client_errc::reconfiguring) == problem{503, "the node cannot take the call now", true});
    CHECK(of(client_errc::retired) == problem{503, "the node cannot take the call now", true});
    CHECK(of(client_errc::rejected) == problem{409, "the cluster rejected the write", false});
    CHECK(of(client_errc::stopped) == problem{503, "the node stopped; the outcome of the write is unknown", false});
    CHECK(of(client_errc::key_too_large) == problem{413, "a key or a value of the write is too large", false});
    CHECK(of(client_errc::value_too_large) == problem{413, "a key or a value of the write is too large", false});
    CHECK(of(client_errc::result_too_large) == problem{413, "a key or a value of the write is too large", false});
    CHECK(of(client_errc::session_expired).status == 500);
    CHECK(of(client_errc::stale_request).status == 500);
    CHECK(konsensd::problem_of(std::make_error_code(std::errc::io_error)).status == 500);
}

// A client reads the status line, the length, the type and the body.
TEST_CASE("response_of and problem_response_of write HTTP/1.1")
{
    CHECK(konsensd::response_of(204, {}, {}, false) == "HTTP/1.1 204 No Content\r\nContent-Length: 0\r\n\r\n");
    CHECK(konsensd::response_of(200, "application/octet-stream", bytes_of("ab"), true) ==
          "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nContent-Type: application/octet-stream\r\nConnection: close\r\n\r\nab");
    const std::string body = R"({"type":"about:blank","title":"Service Unavailable","status":503,"detail":"d"})";
    CHECK(konsensd::problem_response_of({503, "d", true}, false) ==
          "HTTP/1.1 503 Service Unavailable\r\nContent-Length: " + std::to_string(body.size()) +
              "\r\nContent-Type: application/problem+json\r\nRetry-After: 1\r\n\r\n" + body);
    CHECK(konsensd::problem_response_of({405, "", false}, true).contains("Allow: GET, PUT, DELETE\r\n"));
}
