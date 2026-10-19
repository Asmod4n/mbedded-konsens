#pragma once

#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
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

enum class entry_kind : std::uint8_t { normal, stop_sign };

struct entry {
    entry_kind kind;
    std::vector<std::byte> payload;

    friend bool operator==(const entry&, const entry&) = default;
};

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
