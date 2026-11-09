#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "konsens/client.hpp"
#include "rest.hpp"

namespace {

using konsensd::problem;
using konsensd::control_request;
using konsensd::rest_method;
using konsensd::take_control_request;
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

problem control_problem_of(const std::string_view request)
{
    const auto r = take_control_request(request);
    REQUIRE(!r);
    return r.error();
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
// doc/problems.md defines one problem type of RFC 9457 section 4 for each
// client error: a client tells a retired node, which took no part in the
// call, from a node that may have proposed it.
TEST_CASE("problem_of gives the status and the problem type of each client error")
{
    using konsens::client_errc;
    const auto of = [](const client_errc e) { return konsensd::problem_of(make_error_code(e)); };
    const std::string base = "https://github.com/Asmod4n/mbedded-konsens/blob/main/doc/problems.md#";
    struct expected_problem {
        client_errc e;
        std::uint16_t status;
        bool retry_after;
        std::string fragment;
        std::string_view title;
    };
    const std::array cases{
        expected_problem{client_errc::retry, 503, true, "retry", "The node cannot take the call now"},
        expected_problem{client_errc::building, 503, true, "building", "The node builds"},
        expected_problem{client_errc::reconfiguring, 503, true, "reconfiguring", "The configuration ends"},
        expected_problem{client_errc::retired, 503, true, "retired", "The node is retired"},
        expected_problem{client_errc::repairing, 503, true, "repairing", "The node repairs its store"},
        expected_problem{client_errc::rejected, 409, false, "rejected", "The cluster rejected the write"},
        expected_problem{client_errc::stopped, 503, false, "stopped", "The node stopped"},
    };
    for (const auto& c : cases) {
        CAPTURE(c.fragment);
        const auto p = of(c.e);
        CHECK(p.status == c.status);
        CHECK(p.retry_after == c.retry_after);
        CHECK(p.type == base + c.fragment);
        CHECK(p.title == c.title);
        CHECK(!p.detail.empty());
    }
    CHECK(of(client_errc::session_expired).status == 500);
    CHECK(of(client_errc::stale_request).status == 500);
    CHECK(of(client_errc::session_expired).type.empty());
    CHECK(konsensd::problem_of(std::make_error_code(std::errc::io_error)).status == 500);
}

// A client reads the status line, the length, the type and the body.
TEST_CASE("response_of and problem_response_of write HTTP/1.1")
{
    CHECK(konsensd::response_of(204, {}, {}, false) == "HTTP/1.1 204 No Content\r\nContent-Length: 0\r\n\r\n");
    CHECK(konsensd::response_of(200, "application/octet-stream", bytes_of("ab"), true) ==
          "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nContent-Type: application/octet-stream\r\nConnection: "
          "close\r\n\r\nab");
    const std::string body = R"({"type":"about:blank","title":"Service Unavailable","status":503,"detail":"d"})";
    CHECK(konsensd::problem_response_of({503, "d", true}, false) ==
          "HTTP/1.1 503 Service Unavailable\r\nContent-Length: " + std::to_string(body.size()) +
              "\r\nContent-Type: application/problem+json\r\nRetry-After: 1\r\n\r\n" + body);
    CHECK(status_of("POST /kv/a HTTP/1.1\r\n\r\n") == 405);
    CHECK(konsensd::problem_response_of(take_request("POST /kv/a HTTP/1.1\r\n\r\n", max_body).error(), true)
              .contains("Allow: GET, PUT, DELETE\r\n"));
    CHECK(!konsensd::problem_response_of({503, "d", true}, false).contains("Allow:"));
    CHECK(konsensd::problem_response_of({503, "d", true, "https://example.org/t", "T"}, false)
              .ends_with(R"({"type":"https://example.org/t","title":"T","status":503,"detail":"d"})"));
}

// The operator allows one more repair with this request, as curl sends
// it (plan of the control socket, 3.3).
TEST_CASE("take_control_request reads POST /repair/allow-one-more")
{
    const std::string_view request = "POST /repair/allow-one-more HTTP/1.1\r\nHost: localhost\r\nAccept: */*\r\n\r\n";
    const auto r = take_control_request(request);
    REQUIRE(r);
    REQUIRE(*r);
    CHECK(**r == control_request{konsensd::allow_one_more_repair_path, request.size(), false});
    const std::string_view closing = "POST /repair/allow-one-more HTTP/1.1\r\nConnection: close\r\n\r\n";
    CHECK(take_control_request(closing)->value().closes);
    const std::string_view empty_body = "POST /repair/allow-one-more HTTP/1.1\r\nContent-Length: 0\r\n\r\n";
    CHECK(take_control_request(empty_body)->value().bytes == empty_body.size());
}

// A request that is not whole waits for more bytes, and a second request
// on the connection stays for the next call.
TEST_CASE("take_control_request waits for the whole request and takes only its bytes")
{
    const std::string first = "POST /repair/allow-one-more HTTP/1.1\r\n\r\n";
    const auto part = take_control_request(std::string_view{first}.substr(0, first.size() - 1));
    REQUIRE(part);
    CHECK(!*part);
    const auto r = take_control_request(first + first);
    REQUIRE(r);
    REQUIRE(*r);
    CHECK((*r)->bytes == first.size());
}

// Plan cluster-stop 2.10: the operator shuts the whole cluster down with
// this request on any node.
TEST_CASE("take_control_request reads POST /cluster/shutdown")
{
    const std::string_view request = "POST /cluster/shutdown HTTP/1.1\r\nHost: localhost\r\nAccept: */*\r\n\r\n";
    const auto r = take_control_request(request);
    REQUIRE(r);
    REQUIRE(*r);
    CHECK(**r == control_request{konsensd::cluster_shutdown_path, request.size(), false});
    CHECK(control_problem_of("GET /cluster/shutdown HTTP/1.1\r\n\r\n").status == 405);
    CHECK(control_problem_of("POST /cluster HTTP/1.1\r\n\r\n").status == 404);
}

// The control socket knows two targets and one method. RFC 9110 15.5.6:
// a 405 carries the methods that the target takes.
TEST_CASE("take_control_request refuses another target, another method and a body")
{
    CHECK(control_problem_of("POST /repair HTTP/1.1\r\n\r\n").status == 404);
    CHECK(control_problem_of("POST /kv/a HTTP/1.1\r\n\r\n").status == 404);
    CHECK(control_problem_of("POST /repair/allow-one-more?x=1 HTTP/1.1\r\n\r\n").status == 404);
    const auto get = control_problem_of("GET /repair/allow-one-more HTTP/1.1\r\n\r\n");
    CHECK(get.status == 405);
    CHECK(konsensd::problem_response_of(get, true).contains("Allow: POST\r\n"));
    CHECK(control_problem_of("PUT /repair/allow-one-more HTTP/1.1\r\nContent-Length: 0\r\n\r\n").status == 405);
    CHECK(control_problem_of("POST /repair/allow-one-more HTTP/1.1\r\nContent-Length: 1\r\n\r\nx").status == 413);
    CHECK(control_problem_of("POST /repair/allow-one-more HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n").status ==
          501);
    CHECK(control_problem_of("POST /repair/allow-one-more HTTP/2.0\r\n\r\n").status == 505);
}

// The three problems of the control socket have their types in
// doc/problems.md.
TEST_CASE("the problems of the control socket have their types")
{
    const std::string base = "https://github.com/Asmod4n/mbedded-konsens/blob/main/doc/problems.md#";
    CHECK(konsensd::not_waiting_for_the_operator.status == 409);
    CHECK(konsensd::not_waiting_for_the_operator.type == base + "not-waiting-for-the-operator");
    CHECK(konsensd::repair_does_not_start.status == 500);
    CHECK(konsensd::repair_does_not_start.type == base + "repair-does-not-start");
    CHECK(konsensd::cluster_did_not_shut_down.status == 503);
    CHECK(konsensd::cluster_did_not_shut_down.retry_after);
    CHECK(konsensd::cluster_did_not_shut_down.type == base + "cluster-did-not-shut-down");
}
