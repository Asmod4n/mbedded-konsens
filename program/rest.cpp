#include <algorithm>
#include <format>
#include <span>

#include "http1.hpp"

#include "konsens/client.hpp"
#include "rest.hpp"

namespace konsensd {

static_assert(most_request_head_bytes == http1::kMostRequestBytes);

namespace {

constexpr std::uint16_t status_bad_request = 400;
constexpr std::uint16_t status_method_not_allowed = 405;
constexpr std::uint16_t status_conflict = 409;
constexpr std::uint16_t status_length_required = 411;
constexpr std::uint16_t status_content_too_large = 413;
constexpr std::uint16_t status_request_header_fields_too_large = 431;
constexpr std::uint16_t status_internal_server_error = 500;
constexpr std::uint16_t status_not_implemented = 501;
constexpr std::uint16_t status_service_unavailable = 503;
constexpr std::uint16_t status_http_version_not_supported = 505;

constexpr std::string_view http_1_1 = "HTTP/1.1";
constexpr std::string_view http_1_0 = "HTTP/1.0";

std::unexpected<problem> refused(const std::uint16_t status, const std::string_view detail)
{
    return std::unexpected(problem{status, detail, false});
}

std::optional<rest_method> rest_method_of(const http::Method m)
{
    switch (m) {
    case http::Method::kGet:
        return rest_method::get;
    case http::Method::kPut:
        return rest_method::put;
    case http::Method::kDelete:
        return rest_method::erase;
    default:
        return std::nullopt;
    }
}

std::vector<std::byte> bytes_of(const std::string_view text)
{
    const auto b = std::as_bytes(std::span{text});
    return {b.begin(), b.end()};
}

bool asks_to_close(const http1::Request& r)
{
    if (r.request_line.http_version == http_1_0) {
        return !http::equal_ignoring_case(http1::field_value_of(r, "Connection"), "keep-alive");
    }
    return http::equal_ignoring_case(http1::field_value_of(r, "Connection"), "close");
}

}

std::expected<std::optional<rest_request>, problem> take_request(const std::string_view bytes,
                                                                 const std::uint64_t max_body_bytes)
{
    const auto r = http1::parse_request(bytes);
    if (!r) [[unlikely]] {
        if (r.error().problem == http1::kNotWholeYet) {
            return std::nullopt;
        }
        if (bytes.size() > most_request_head_bytes) {
            return refused(status_request_header_fields_too_large, "the request head is too long");
        }
        return refused(status_bad_request, "the request does not parse");
    }
    const auto& line = r->request_line;
    if (line.http_version != http_1_1 && line.http_version != http_1_0) [[unlikely]] {
        return refused(status_http_version_not_supported, "only HTTP/1.1 is served");
    }
    if (!http1::field_value_of(*r, "Transfer-Encoding").empty()) [[unlikely]] {
        return refused(status_not_implemented, "a body comes with Content-Length only");
    }
    const auto method = rest_method_of(line.method);
    if (!method) [[unlikely]] {
        return refused(status_method_not_allowed, "a key takes GET, PUT and DELETE");
    }
    const std::string_view target = line.request_target;
    if (!target.starts_with(key_value_path) || target.size() == key_value_path.size() ||
        target.find_first_of("/?#", key_value_path.size()) != std::string_view::npos) [[unlikely]] {
        return refused(status_not_found, "the path is /kv/ and one percent-encoded key");
    }
    const auto key = http::percent_decode(target.substr(key_value_path.size()));
    if (!key) [[unlikely]] {
        return refused(status_bad_request, "the key is not percent-encoded");
    }
    std::uint64_t length = 0;
    if (const auto field = http1::field_value_of(*r, "Content-Length"); !field.empty()) {
        const auto parsed = http::parse_content_length(field);
        if (!parsed) [[unlikely]] {
            return refused(status_bad_request, "the Content-Length does not parse");
        }
        length = *parsed;
    } else if (*method == rest_method::put) [[unlikely]] {
        return refused(status_length_required, "a PUT needs Content-Length");
    }
    if (length > max_body_bytes) [[unlikely]] {
        return refused(status_content_too_large, "the body is too large");
    }
    if (bytes.size() - r->bytes < length) {
        return std::nullopt;
    }
    return rest_request{*method, bytes_of(*key), bytes_of(bytes.substr(r->bytes, length)), r->bytes + length,
                        asks_to_close(*r)};
}

problem problem_of(const std::error_code& e)
{
    using konsens::client_errc;
    if (e == client_errc::retry || e == client_errc::building || e == client_errc::reconfiguring ||
        e == client_errc::retired) {
        return {status_service_unavailable, "the node cannot take the call now", true};
    }
    if (e == client_errc::rejected) {
        return {status_conflict, "the cluster rejected the write", false};
    }
    if (e == client_errc::key_too_large || e == client_errc::value_too_large ||
        e == client_errc::result_too_large) {
        return {status_content_too_large, "a key or a value of the write is too large", false};
    }
    if (e == client_errc::stopped) {
        return {status_service_unavailable, "the node stopped; the outcome of the write is unknown", false};
    }
    return {status_internal_server_error, "", false};
}

std::string response_of(const std::uint16_t status, const std::string_view content_type,
                        const std::span<const std::byte> body, const bool closes)
{
    std::string out = std::format("HTTP/1.1 {} {}\r\nContent-Length: {}\r\n", status, http::reason_phrase(status),
                                  body.size());
    if (!content_type.empty()) {
        out += std::format("Content-Type: {}\r\n", content_type);
    }
    if (closes) {
        out += "Connection: close\r\n";
    }
    out += "\r\n";
    const auto chars = std::span{reinterpret_cast<const char*>(body.data()), body.size()};
    out.append(chars.begin(), chars.end());
    return out;
}

std::string problem_response_of(const problem& p, const bool closes)
{
    std::string body = std::format(R"({{"type":"about:blank","title":"{}","status":{})", http::reason_phrase(p.status),
                                   p.status);
    if (!p.detail.empty()) {
        body += std::format(R"(,"detail":"{}")", p.detail);
    }
    body += "}";
    std::string out = std::format("HTTP/1.1 {} {}\r\nContent-Length: {}\r\nContent-Type: application/problem+json\r\n",
                                  p.status, http::reason_phrase(p.status), body.size());
    if (p.retry_after) {
        out += std::format("Retry-After: {}\r\n", retry_after_seconds);
    }
    if (p.status == status_method_not_allowed) {
        out += "Allow: GET, PUT, DELETE\r\n";
    }
    if (closes) {
        out += "Connection: close\r\n";
    }
    out += "\r\n";
    out += body;
    return out;
}

}
