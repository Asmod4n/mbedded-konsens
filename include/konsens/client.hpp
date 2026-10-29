#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <system_error>
#include <type_traits>
#include <vector>

namespace konsens {

enum class client_errc { recovering = 1, session_expired, stale_request, retry, rejected, reconfiguring, retired, stopped };

class client_error_category final : public std::error_category {
public:
    const char* name() const noexcept override { return "konsens.client"; }

    std::string message(const int code) const override
    {
        switch (static_cast<client_errc>(code)) {
        case client_errc::recovering:
            return "The node builds and takes no call";
        case client_errc::session_expired:
            return "The session has expired";
        case client_errc::stale_request:
            return "The request number is below the last request number of the session";
        case client_errc::retry:
            return "The node knows no leader, or its promise changed; send the call again";
        case client_errc::rejected:
            return "No result of the request has a majority; the request changed nothing";
        case client_errc::reconfiguring:
            return "The configuration of this node ends with a StopSign; send the call to the next configuration";
        case client_errc::retired:
            return "The node started without a clean stop and votes never again; send the call to another node";
        case client_errc::stopped:
            return "The node stopped before the call was decided; the call may still be decided";
        }
        return "Unknown client error";
    }
};

inline const std::error_category& client_category() noexcept
{
    static const client_error_category category;
    return category;
}

inline std::error_code make_error_code(const client_errc code) noexcept
{
    return {static_cast<int>(code), client_category()};
}

struct client_reply {
    std::uint64_t client_id;
    std::uint64_t session;
    std::expected<std::vector<std::byte>, std::error_code> response;
};

}

template <>
struct std::is_error_code_enum<konsens::client_errc> : std::true_type {};
