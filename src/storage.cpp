#include "konsens/storage.hpp"

#include <algorithm>
#include <array>
#include <iterator>
#include <utility>

#include <crc32c/crc32c.h>
#include <crc32c_arm64_check.h>
#include <crc32c_sse42_check.h>

#include "byte_order.hpp"

namespace konsens {

namespace {

constexpr std::size_t kind_size = sizeof(entry_kind);
constexpr std::size_t field_size = sizeof(std::uint64_t);
constexpr std::size_t max_field_count = 2;

using entry_fields = std::array<std::uint64_t, max_field_count>;

entry_fields fields_of(const entry& e) noexcept
{
    switch (e.kind) {
    case entry_kind::register_client:
        return {e.client_id, e.pid};
    case entry_kind::client_request:
        return {e.session, e.request_number};
    case entry_kind::expire_session:
        return {e.session, 0};
    case entry_kind::normal:
    case entry_kind::stop_sign:
        break;
    }
    return {};
}

entry entry_of(const entry_kind kind, const entry_fields& fields, const std::span<const std::byte> payload)
{
    entry e{kind, std::vector<std::byte>(payload.begin(), payload.end())};
    switch (kind) {
    case entry_kind::register_client:
        e.client_id = fields[0];
        e.pid = fields[1];
        break;
    case entry_kind::client_request:
        e.session = fields[0];
        e.request_number = fields[1];
        break;
    case entry_kind::expire_session:
        e.session = fields[0];
        break;
    case entry_kind::normal:
    case entry_kind::stop_sign:
        break;
    }
    return e;
}

}

std::size_t encoded_size_of(const entry& e)
{
    throw_if_entry_has_a_field_of_another_kind(e);
    return kind_size + field_size * field_count_of(e.kind).value() + e.payload.size();
}

void encode_entry(const entry& e, const std::span<std::byte> out) noexcept
{
    out.front() = std::byte{std::to_underlying(e.kind)};
    auto next = std::next(out.begin(), kind_size);
    const auto fields = fields_of(e);
    for (const auto field : std::span(fields).first(field_count_of(e.kind).value_or(0))) {
        next = std::ranges::copy(to_little_endian(field), next).out;
    }
    std::ranges::copy(e.payload, next);
}

std::optional<entry> decode_entry(const std::span<const std::byte> encoded_entry)
{
    if (encoded_entry.size() < kind_size) [[unlikely]] {
        return std::nullopt;
    }
    const auto kind = static_cast<entry_kind>(std::to_integer<std::uint8_t>(encoded_entry.front()));
    const auto field_count = field_count_of(kind);
    if (!field_count || encoded_entry.size() < kind_size + field_size * *field_count) [[unlikely]] {
        return std::nullopt;
    }
    entry_fields fields{};
    auto rest = encoded_entry.subspan(kind_size);
    for (auto& field : std::span(fields).first(*field_count)) {
        field = from_little_endian<std::uint64_t>(rest.first<field_size>());
        rest = rest.subspan(field_size);
    }
    return entry_of(kind, fields, rest);
}

std::uint32_t compute_entry_crc(const std::uint64_t index, const std::span<const std::byte> encoded_entry) noexcept
{
    const auto index_bytes = to_little_endian(index);
    return crc32c::Extend(crc32c::Crc32c(index_bytes.data(), index_bytes.size()),
                          reinterpret_cast<const std::uint8_t*>(encoded_entry.data()), encoded_entry.size());
}

std::uint32_t compute_entry_crc(const std::uint64_t index, const entry& e)
{
    std::vector<std::byte> encoded(encoded_size_of(e));
    encode_entry(e, encoded);
    return compute_entry_crc(index, encoded);
}

std::uint32_t compute_chain_crc(const std::uint32_t previous_chain_crc, const std::uint64_t index,
                                const std::span<const std::byte> encoded_entry) noexcept
{
    std::array<std::byte, sizeof previous_chain_crc + sizeof index> head{};
    std::ranges::copy(to_little_endian(index), std::ranges::copy(to_little_endian(previous_chain_crc), head.begin()).out);
    return crc32c::Extend(crc32c::Crc32c(reinterpret_cast<const std::uint8_t*>(head.data()), head.size()),
                          reinterpret_cast<const std::uint8_t*>(encoded_entry.data()), encoded_entry.size());
}

std::uint32_t compute_chain_crc(const std::uint32_t previous_chain_crc, const std::uint64_t index, const entry& e)
{
    std::vector<std::byte> encoded(encoded_size_of(e));
    encode_entry(e, encoded);
    return compute_chain_crc(previous_chain_crc, index, encoded);
}

bool can_use_crc32c_instruction() noexcept
{
#if HAVE_SSE42 && (defined(_M_X64) || defined(__x86_64__))
    return crc32c::CanUseSse42();
#elif HAVE_ARM64_CRC32C
    return crc32c::CanUseArm64Crc32();
#else
    return false;
#endif
}

}
