#pragma once

#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stdexcept>
#include <system_error>
#include <variant>
#include <vector>

namespace konsens {

struct ballot {
    std::uint32_t config_id;
    std::uint32_t n;
    std::uint32_t priority;
    std::uint64_t pid;

    friend auto operator<=>(const ballot&, const ballot&) = default;
};

enum class entry_kind : std::uint8_t { normal, stop_sign, register_client, client_request, expire_session };

struct entry {
    entry_kind kind;
    std::vector<std::byte> payload{};
    std::uint64_t session{};
    std::uint64_t request_number{};
    std::uint64_t client_id{};
    std::uint64_t pid{};

    friend bool operator==(const entry&, const entry&) = default;
};

inline void throw_if_entry_has_a_field_of_another_kind(const entry& e)
{
    const bool has_session = e.session != 0;
    const bool has_request_number = e.request_number != 0;
    const bool has_registration = e.client_id != 0 || e.pid != 0;
    bool wrong = false;
    switch (e.kind) {
    case entry_kind::normal:
    case entry_kind::stop_sign:
        wrong = has_session || has_request_number || has_registration;
        break;
    case entry_kind::register_client:
        wrong = has_session || has_request_number;
        break;
    case entry_kind::client_request:
        wrong = has_registration;
        break;
    case entry_kind::expire_session:
        wrong = has_request_number || has_registration;
        break;
    default:
        wrong = true;
        break;
    }
    if (wrong) [[unlikely]] {
        throw std::logic_error("konsens: an entry has an unknown kind or a field that its kind does not carry");
    }
}

struct append_entries {
    std::vector<entry> entries;
};

struct append_on_prefix {
    std::uint64_t from_idx;
    std::vector<entry> entries;
};

struct set_promise {
    ballot promise;
};

struct set_decided_index {
    std::uint64_t decided_idx;
};

struct set_accepted_round {
    ballot accepted_round;
};

using storage_op = std::variant<append_entries, append_on_prefix, set_promise, set_decided_index,
                                set_accepted_round>;

template <typename S>
concept storage = requires(S& s, const S& cs, std::span<const storage_op> ops, std::uint64_t from,
                           std::uint64_t to) {
    { s.write_atomically(ops) } -> std::same_as<std::expected<void, std::error_code>>;
    { cs.get_entries(from, to) } -> std::same_as<std::expected<std::vector<entry>, std::error_code>>;
    { cs.get_log_len() } -> std::same_as<std::expected<std::uint64_t, std::error_code>>;
    { cs.get_promise() } -> std::same_as<std::expected<ballot, std::error_code>>;
    { cs.get_decided_idx() } -> std::same_as<std::expected<std::uint64_t, std::error_code>>;
    { cs.get_accepted_round() } -> std::same_as<std::expected<ballot, std::error_code>>;
};

}
