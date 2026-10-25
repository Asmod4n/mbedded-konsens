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
#include <type_traits>
#include <utility>
#include <vector>

namespace konsens {

struct ballot {
    std::uint32_t config_id;
    std::uint32_t n;
    std::uint32_t priority;
    std::uint64_t pid;

    friend auto operator<=>(const ballot&, const ballot&) = default;
};

using node_id = std::uint64_t;

enum class entry_kind : std::uint8_t {
    normal,
    stop_sign,
    register_client,
    client_request,
    expire_session,
    results,
    rejected
};

struct entry {
    entry_kind kind;
    std::vector<std::byte> payload{};
    std::uint64_t session{};
    std::uint64_t request_number{};
    std::uint64_t client_id{};
    std::uint64_t pid{};
    std::uint64_t from_idx{};
    std::uint64_t to_idx{};

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
    case entry_kind::results:
        return 2;
    case entry_kind::expire_session:
    case entry_kind::rejected:
        return 1;
    }
    return std::nullopt;
}

constexpr bool entry_is_a_request(const entry& e) noexcept
{
    return e.kind == entry_kind::register_client || e.kind == entry_kind::client_request ||
           e.kind == entry_kind::expire_session;
}

constexpr bool entry_is_a_verdict(const entry& e) noexcept
{
    return e.kind == entry_kind::results || e.kind == entry_kind::rejected;
}

inline bool entry_has_a_field_of_another_kind(const entry& e) noexcept
{
    const bool has_session = e.session != 0;
    const bool has_request_number = e.request_number != 0;
    const bool has_registration = e.client_id != 0 || e.pid != 0;
    const bool has_range = e.from_idx != 0 || e.to_idx != 0;
    const bool has_request = has_session || has_request_number || has_registration;
    switch (e.kind) {
    case entry_kind::normal:
    case entry_kind::stop_sign:
        return has_request || has_range;
    case entry_kind::register_client:
        return has_session || has_request_number || has_range;
    case entry_kind::client_request:
        return has_registration || has_range;
    case entry_kind::expire_session:
        return has_request_number || has_registration || has_range;
    case entry_kind::results:
        return has_request;
    case entry_kind::rejected:
        return has_request || e.to_idx != 0;
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
bool can_use_crc32c_instruction() noexcept;

struct meta_record {
    ballot promise;
    ballot accepted_round;
    std::uint64_t decided_idx;
    std::uint64_t log_len;
    std::uint32_t last_chain_crc;

    friend bool operator==(const meta_record&, const meta_record&) = default;
};

enum class entry_state : std::uint8_t { in_work, committed };

struct last_entry {
    std::uint64_t index;
    std::uint32_t entry_crc;
    std::uint32_t chain_crc;

    friend bool operator==(const last_entry&, const last_entry&) = default;
};

std::uint32_t compute_chain_crc(std::uint32_t previous_chain_crc, std::uint64_t index,
                                std::span<const std::byte> encoded_entry) noexcept;
std::uint32_t compute_chain_crc(std::uint32_t previous_chain_crc, std::uint64_t index, const entry& e);
std::uint32_t compute_chain_crc(std::uint32_t previous_chain_crc, std::uint64_t from_idx,
                                std::span<const entry> entries);

enum class storage_errc { crc_mismatch = 1, malformed_record, missing_entry, missing_meta_record, unsupported_format };

const std::error_category& storage_category() noexcept;
std::error_code make_error_code(storage_errc code) noexcept;

class read_error : public std::system_error {
public:
    using std::system_error::system_error;
};

class write_error : public std::system_error {
public:
    using std::system_error::system_error;
};

class sync_error : public write_error {
public:
    using write_error::write_error;
};

template <typename T>
concept storage_read = requires(const T& t, std::uint64_t from, std::uint64_t to, std::uint64_t index) {
    { t.get_meta() } -> std::same_as<std::expected<meta_record, std::error_code>>;
    { t.get_entries(from, to) } -> std::same_as<std::expected<std::vector<entry>, std::error_code>>;
    { t.get_last_entry() } -> std::same_as<std::expected<std::optional<last_entry>, std::error_code>>;
    { t.get_entry_state(index) } -> std::same_as<std::expected<std::optional<entry_state>, std::error_code>>;
    { t.get_chain_crc(index) } -> std::same_as<std::expected<std::optional<std::uint32_t>, std::error_code>>;
};

template <typename T>
concept storage_write = storage_read<T> && std::movable<T> &&
                        requires(T& t, T&& moved, std::span<const entry> entries, std::uint64_t index, ballot b) {
                            { t.append_entries(entries) } -> std::same_as<std::expected<void, std::error_code>>;
                            {
                                t.append_on_prefix(index, entries)
                            } -> std::same_as<std::expected<void, std::error_code>>;
                            { t.set_promise(b) } -> std::same_as<std::expected<void, std::error_code>>;
                            { t.set_accepted_round(b) } -> std::same_as<std::expected<void, std::error_code>>;
                            { t.set_decided_index(index) } -> std::same_as<std::expected<void, std::error_code>>;
                            {
                                t.write_repaired_log(index, entries, b, b, index)
                            } -> std::same_as<std::expected<void, std::error_code>>;
                            { std::move(moved).commit() } -> std::same_as<std::expected<void, std::error_code>>;
                        };

template <typename S>
concept storage = storage_write<typename S::transaction> && storage_read<typename S::read_only_transaction> &&
                  requires(S& s, const S& cs) {
                      { s.begin() } -> std::same_as<std::expected<typename S::transaction, std::error_code>>;
                      {
                          cs.begin_read_only()
                      } -> std::same_as<std::expected<typename S::read_only_transaction, std::error_code>>;
                  };

}

template <>
struct std::is_error_code_enum<konsens::storage_errc> : std::true_type {};
