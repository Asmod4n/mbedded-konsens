#pragma once

#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
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

inline std::optional<std::size_t> field_count_of(const entry_kind kind) noexcept
{
    switch (kind) {
    case entry_kind::normal:
    case entry_kind::stop_sign:
        return 0;
    case entry_kind::register_client:
    case entry_kind::client_request:
        return 2;
    case entry_kind::expire_session:
        return 1;
    }
    return std::nullopt;
}

inline bool entry_has_a_field_of_another_kind(const entry& e) noexcept
{
    const bool has_session = e.session != 0;
    const bool has_request_number = e.request_number != 0;
    const bool has_registration = e.client_id != 0 || e.pid != 0;
    switch (e.kind) {
    case entry_kind::normal:
    case entry_kind::stop_sign:
        return has_session || has_request_number || has_registration;
    case entry_kind::register_client:
        return has_session || has_request_number;
    case entry_kind::client_request:
        return has_registration;
    case entry_kind::expire_session:
        return has_request_number || has_registration;
    }
    return true;
}

inline void throw_if_entry_has_a_field_of_another_kind(const entry& e)
{
    if (entry_has_a_field_of_another_kind(e)) [[unlikely]] {
        throw std::logic_error("konsens: an entry has an unknown kind or a field that its kind does not carry");
    }
}

std::size_t encoded_size_of(const entry& e);
void encode_entry(const entry& e, std::span<std::byte> out) noexcept;
std::optional<entry> decode_entry(std::span<const std::byte> encoded_entry);
std::uint32_t compute_entry_crc(std::uint64_t index, std::span<const std::byte> encoded_entry) noexcept;
std::uint32_t compute_entry_crc(std::uint64_t index, const entry& e);

struct append_entries {
    std::span<const entry> entries;
};

struct append_on_prefix {
    std::uint64_t from_idx;
    std::span<const entry> entries;
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
    { cs.get_last_entry_crc() } -> std::same_as<std::expected<std::uint32_t, std::error_code>>;
};

}
