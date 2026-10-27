#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <memory>
#include <stdexcept>

#include <cbor/item_size.hpp>
#include <cbor/structs.hpp>

#include "frame.hpp"

namespace konsensd {

namespace {

using length_type = std::uint32_t;
static_assert(sizeof(length_type) == frame_length_bytes);

std::array<char, frame_length_bytes> big_endian_of(const length_type n)
{
    const length_type big = std::endian::native == std::endian::big ? n : std::byteswap(n);
    return std::bit_cast<std::array<char, frame_length_bytes>>(big);
}

length_type length_in(const std::string_view head)
{
    std::array<char, frame_length_bytes> b{};
    std::ranges::copy(head.substr(0, frame_length_bytes), b.begin());
    const auto big = std::bit_cast<length_type>(b);
    return std::endian::native == std::endian::big ? big : std::byteswap(big);
}

}

std::string frame_of(const std::string_view payload)
{
    if (payload.size() > std::numeric_limits<length_type>::max()) [[unlikely]] {
        throw std::logic_error("konsensd: a frame holds at most 2^32 - 1 bytes");
    }
    std::string out;
    out.reserve(frame_length_bytes + payload.size());
    out.append(std::string_view{big_endian_of(static_cast<length_type>(payload.size()))});
    out.append(payload);
    return out;
}

std::expected<std::optional<std::string>, frame_errc> take_frame(frame_buffer& buffer, const std::uint64_t max_frame_bytes)
{
    const std::string_view rest = std::string_view{buffer.bytes}.substr(buffer.start);
    if (rest.size() < frame_length_bytes) {
        buffer.bytes.erase(0, buffer.start);
        buffer.start = 0;
        return std::nullopt;
    }
    const length_type length = length_in(rest);
    if (length > max_frame_bytes) [[unlikely]] {
        return std::unexpected(frame_errc::too_long);
    }
    if (rest.size() - frame_length_bytes < length) {
        buffer.bytes.erase(0, buffer.start);
        buffer.start = 0;
        return std::nullopt;
    }
    std::string frame{rest.substr(frame_length_bytes, length)};
    buffer.start += frame_length_bytes + length;
    return frame;
}

std::string encode_hello(const hello& h)
{
    const auto out = cbor::structs<hello>::encode(h);
    if (!out) [[unlikely]] {
        throw std::logic_error("konsensd: a hello does not encode");
    }
    return *out;
}

std::expected<hello, frame_errc> decode_hello(const std::string_view payload)
{
    const auto owner = std::make_shared<const std::string>(payload);
    const auto size = cbor::item_size(*owner);
    if (!size || *size != owner->size()) [[unlikely]] {
        return std::unexpected(frame_errc::malformed_hello);
    }
    const auto decoded = cbor::structs<hello>::decode(owner);
    if (!decoded) [[unlikely]] {
        return std::unexpected(frame_errc::malformed_hello);
    }
    return **decoded;
}

}
