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
constexpr std::uint16_t status_length_required = 411;
constexpr std::uint16_t status_content_too_large = 413;
constexpr std::uint16_t status_request_header_fields_too_large = 431;
constexpr std::uint16_t status_not_implemented = 501;
constexpr std::uint16_t status_http_version_not_supported = 505;

constexpr std::string_view problem_type_retry =
    "https://github.com/Asmod4n/mbedded-konsens/blob/main/doc/problems.md#retry";
constexpr std::string_view problem_type_building =
    "https://github.com/Asmod4n/mbedded-konsens/blob/main/doc/problems.md#building";
constexpr std::string_view problem_type_reconfiguring =
    "https://github.com/Asmod4n/mbedded-konsens/blob/main/doc/problems.md#reconfiguring";
constexpr std::string_view problem_type_retired =
    "https://github.com/Asmod4n/mbedded-konsens/blob/main/doc/problems.md#retired";
constexpr std::string_view problem_type_repairing =
    "https://github.com/Asmod4n/mbedded-konsens/blob/main/doc/problems.md#repairing";
constexpr std::string_view problem_type_rejected =
    "https://github.com/Asmod4n/mbedded-konsens/blob/main/doc/problems.md#rejected";
constexpr std::string_view problem_type_stopped =
    "https://github.com/Asmod4n/mbedded-konsens/blob/main/doc/problems.md#stopped";

constexpr std::string_view http_1_1 = "HTTP/1.1";
constexpr std::string_view http_1_0 = "HTTP/1.0";

constexpr std::string_view allow_of_a_key = "GET, PUT, DELETE";
constexpr std::string_view allow_of_a_control_target = "POST";

std::unexpected<problem> refused(const std::uint16_t status, const std::string_view detail)
{
    return std::unexpected(problem{status, detail, false});
}

std::unexpected<problem> refused_method(const std::string_view detail, const std::string_view allow)
{
    return std::unexpected(problem{status_method_not_allowed, detail, false, {}, {}, allow});
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

struct request_head {
    http1::Request request;
    std::optional<std::uint64_t> content_length;
};

std::expected<std::optional<request_head>, problem> head_of(const std::string_view bytes,
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
    std::optional<std::uint64_t> length;
    if (const auto field = http1::field_value_of(*r, "Content-Length"); !field.empty()) {
        const auto parsed = http::parse_content_length(field);
        if (!parsed) [[unlikely]] {
            return refused(status_bad_request, "the Content-Length does not parse");
        }
        length = *parsed;
    }
    if (length.value_or(0) > max_body_bytes) [[unlikely]] {
        return refused(status_content_too_large, "the body is too large");
    }
    if (bytes.size() - r->bytes < length.value_or(0)) {
        return std::nullopt;
    }
    return request_head{*r, length};
}

}

std::expected<std::optional<rest_request>, problem> take_request(const std::string_view bytes,
                                                                 const std::uint64_t max_body_bytes)
{
    const auto h = head_of(bytes, max_body_bytes);
    if (!h) [[unlikely]] {
        return std::unexpected(h.error());
    }
    if (!*h) {
        return std::nullopt;
    }
    const auto& r = (*h)->request;
    const std::string_view target = r.request_line.request_target;
    if (!target.starts_with(key_value_path) || target.size() == key_value_path.size() ||
        target.find_first_of("/?#", key_value_path.size()) != std::string_view::npos) [[unlikely]] {
        return refused(status_not_found, "the path is /kv/ and one percent-encoded key");
    }
    const auto method = rest_method_of(r.request_line.method);
    if (!method) [[unlikely]] {
        return refused_method("a key takes GET, PUT and DELETE", allow_of_a_key);
    }
    const auto key = http::percent_decode(target.substr(key_value_path.size()));
    if (!key) [[unlikely]] {
        return refused(status_bad_request, "the key is not percent-encoded");
    }
    if (*method == rest_method::put && !(*h)->content_length) [[unlikely]] {
        return refused(status_length_required, "a PUT needs Content-Length");
    }
    const auto length = (*h)->content_length.value_or(0);
    return rest_request{*method, bytes_of(*key), bytes_of(bytes.substr(r.bytes, length)), r.bytes + length,
                        asks_to_close(r)};
}

std::expected<std::optional<control_request>, problem> take_control_request(const std::string_view bytes)
{
    const auto h = head_of(bytes, max_control_body_bytes);
    if (!h) [[unlikely]] {
        return std::unexpected(h.error());
    }
    if (!*h) {
        return std::nullopt;
    }
    const auto& r = (*h)->request;
    const auto target = r.request_line.request_target;
    if (target != allow_one_more_repair_path && target != cluster_shutdown_path) [[unlikely]] {
        return refused(status_not_found, "the control socket serves /repair/allow-one-more and /cluster/shutdown");
    }
    if (r.request_line.method != http::Method::kPost) [[unlikely]] {
        return refused_method("a target of the control socket takes POST", allow_of_a_control_target);
    }
    return control_request{target == cluster_shutdown_path ? cluster_shutdown_path : allow_one_more_repair_path,
                           r.bytes, asks_to_close(r)};
}

problem problem_of(const std::error_code& e)
{
    using konsens::client_errc;
    if (e.category() != konsens::client_category()) {
        return {status_internal_server_error, "", false};
    }
    switch (static_cast<client_errc>(e.value())) {
    case client_errc::retry:
        return {status_service_unavailable, "the node knows no leader, or its promise changed", true,
                problem_type_retry, "The node cannot take the call now"};
    case client_errc::building:
        return {status_service_unavailable, "the node builds its store from a peer", true, problem_type_building,
                "The node builds"};
    case client_errc::reconfiguring:
        return {status_service_unavailable, "the configuration of the node ends with a StopSign", true,
                problem_type_reconfiguring, "The configuration ends"};
    case client_errc::retired:
        return {status_service_unavailable, "the node started without a clean stop and took no part in the call", true,
                problem_type_retired, "The node is retired"};
    case client_errc::repairing:
        return {status_service_unavailable, "the node repairs a damage of its log", true, problem_type_repairing,
                "The node repairs its store"};
    case client_errc::rejected:
        return {status_conflict, "the cluster rejected the write", false, problem_type_rejected,
                "The cluster rejected the write"};
    case client_errc::stopped:
        return {status_service_unavailable, "the node stopped; the outcome of the write is unknown", false,
                problem_type_stopped, "The node stopped"};
    case client_errc::session_expired:
    case client_errc::stale_request:
        break;
    }
    return {status_internal_server_error, "", false};
}

std::string response_of(const std::uint16_t status, const std::string_view content_type,
                        const std::span<const std::byte> body, const bool closes)
{
    std::string out =
        std::format("HTTP/1.1 {} {}\r\nContent-Length: {}\r\n", status, http::reason_phrase(status), body.size());
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
    std::string body = std::format(R"({{"type":"{}","title":"{}","status":{})",
                                   p.type.empty() ? std::string_view{"about:blank"} : p.type,
                                   p.title.empty() ? http::reason_phrase(p.status) : p.title, p.status);
    if (!p.detail.empty()) {
        body += std::format(R"(,"detail":"{}")", p.detail);
    }
    body += "}";
    std::string out = std::format("HTTP/1.1 {} {}\r\nContent-Length: {}\r\nContent-Type: application/problem+json\r\n",
                                  p.status, http::reason_phrase(p.status), body.size());
    if (p.retry_after) {
        out += std::format("Retry-After: {}\r\n", retry_after_seconds);
    }
    if (!p.allow.empty()) {
        out += std::format("Allow: {}\r\n", p.allow);
    }
    if (closes) {
        out += "Connection: close\r\n";
    }
    out += "\r\n";
    out += body;
    return out;
}

}
