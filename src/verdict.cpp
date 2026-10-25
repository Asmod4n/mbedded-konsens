#include "konsens/verdict.hpp"

#include <algorithm>
#include <array>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

#include <cbor/structs.hpp>
#include <crc32c/crc32c.h>

#include "byte_order.hpp"

namespace konsens {

namespace {

template <typename T>
std::vector<std::byte> encode_or_throw(const T& value)
{
    const auto encoded = cbor::structs<T>::encode(value);
    if (!encoded) [[unlikely]] {
        throw std::logic_error("konsens: a result or a list of results has no CBOR encoding");
    }
    const auto bytes = std::as_bytes(std::span(*encoded));
    return {bytes.begin(), bytes.end()};
}

template <typename T>
std::optional<T> decode_bytes(const std::span<const std::byte> encoded)
{
    auto owner = std::make_shared<const std::string>(reinterpret_cast<const char*>(encoded.data()), encoded.size());
    const std::string_view view = *owner;
    const auto decoded = cbor::structs<T>::decode(std::shared_ptr<void const>(std::move(owner)), view);
    if (!decoded) [[unlikely]] {
        return std::nullopt;
    }
    return **decoded;
}

bool holds_a_request(const std::uint64_t first_index, const std::span<const entry> entries, const std::uint64_t index)
{
    return index >= first_index && index - first_index < entries.size() &&
           entry_is_a_request(entries[index - first_index]);
}

bool holds_a_request_in(const std::uint64_t first_index, const std::span<const entry> entries,
                        const std::uint64_t from, const std::uint64_t to)
{
    for (auto index = std::max(from, first_index); index <= to; ++index) {
        if (holds_a_request(first_index, entries, index)) {
            return true;
        }
    }
    return false;
}

}

std::vector<std::byte> encode_result(const request_result& r)
{
    return encode_or_throw(r);
}

std::optional<request_result> decode_result(const std::span<const std::byte> encoded)
{
    return decode_bytes<request_result>(encoded);
}

std::uint32_t compute_result_crc(const std::uint64_t index, const std::span<const std::byte> encoded) noexcept
{
    const auto index_bytes = to_little_endian(index);
    return crc32c::Extend(crc32c::Crc32c(reinterpret_cast<const std::uint8_t*>(index_bytes.data()), index_bytes.size()),
                          reinterpret_cast<const std::uint8_t*>(encoded.data()), encoded.size());
}

std::vector<std::byte> encode_chosen_results(const std::span<const chosen_result> list)
{
    return encode_or_throw(std::vector<chosen_result>(list.begin(), list.end()));
}

std::optional<std::vector<chosen_result>> decode_chosen_results(const std::span<const std::byte> payload)
{
    return decode_bytes<std::vector<chosen_result>>(payload);
}

verdicts find_the_verdicts(const std::uint64_t pointer, const std::uint64_t first_index,
                           const std::span<const entry> entries)
{
    if (first_index == 0 || pointer >= first_index) [[unlikely]] {
        throw std::logic_error("konsens: find_the_verdicts takes a first index above the pointer");
    }
    verdicts v{pointer, {}, {}, 0};
    for (std::uint64_t k = 0; k < entries.size(); ++k) {
        const auto& e = entries[k];
        const auto m = first_index + k;
        if (e.kind == entry_kind::results && e.from_idx == v.pointer + 1 && e.to_idx < m &&
            holds_a_request_in(first_index, entries, e.from_idx, e.to_idx)) {
            const auto list = decode_chosen_results(e.payload);
            if (!list) [[unlikely]] {
                continue;
            }
            std::uint64_t last = 0;
            for (const auto& item : *list) {
                const auto q = item.chosen.index;
                if (q > last && q >= e.from_idx && q <= e.to_idx && holds_a_request(first_index, entries, q)) {
                    v.chosen.push_back(item);
                    last = q;
                }
            }
            v.pointer = e.to_idx;
        } else if (e.kind == entry_kind::rejected && e.from_idx > v.pointer && e.from_idx < m &&
                   holds_a_request(first_index, entries, e.from_idx) &&
                   !holds_a_request_in(first_index, entries, v.pointer + 1, e.from_idx - 1)) {
            v.voids.push_back(e.from_idx);
            v.pointer = e.from_idx;
        }
    }
    v.applied_idx = first_index - 1 + entries.size();
    for (std::uint64_t k = 0; k < entries.size(); ++k) {
        if (first_index + k > v.pointer && entry_is_a_request(entries[k])) {
            v.applied_idx = first_index + k - 1;
            break;
        }
    }
    return v;
}

std::uint32_t extend_base_crc(const std::uint32_t base_crc, const std::uint64_t index,
                              const std::uint32_t result_crc) noexcept
{
    std::array<std::byte, sizeof base_crc + sizeof index + sizeof result_crc> head{};
    std::ranges::copy(to_little_endian(result_crc),
                      std::ranges::copy(to_little_endian(index),
                                        std::ranges::copy(to_little_endian(base_crc), head.begin()).out)
                          .out);
    return crc32c::Crc32c(reinterpret_cast<const std::uint8_t*>(head.data()), head.size());
}

bool results_agree(const std::span<const std::byte> a, const std::span<const std::byte> b) noexcept
{
    return std::ranges::equal(a, b);
}

}
