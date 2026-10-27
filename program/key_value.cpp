#include <algorithm>
#include <stdexcept>
#include <string>

#include <cbor/item_size.hpp>
#include <cbor/structs.hpp>

#include "key_value.hpp"

namespace konsensd {

namespace {

template <typename T>
std::vector<std::byte> encoded(const T& value)
{
    const auto out = cbor::structs<T>::encode(value);
    if (!out) [[unlikely]] {
        throw std::logic_error("konsensd: a key-value record does not encode");
    }
    const auto bytes = std::as_bytes(std::span(*out));
    return {bytes.begin(), bytes.end()};
}

template <typename T>
std::optional<T> decoded(const std::span<const std::byte> payload)
{
    std::string bytes(payload.size(), '\0');
    std::ranges::transform(payload, bytes.begin(), [](const std::byte b) { return static_cast<char>(b); });
    const auto size = cbor::item_size(bytes);
    if (!size || *size != bytes.size()) [[unlikely]] {
        return std::nullopt;
    }
    const auto d = cbor::structs<T>::decode(std::move(bytes));
    if (!d) [[unlikely]] {
        return std::nullopt;
    }
    return **d;
}

}

std::vector<std::byte> encode_write(const key_value_write& w)
{
    return encoded(w);
}

std::optional<key_value_write> decode_write(const std::span<const std::byte> payload)
{
    auto w = decoded<key_value_write>(payload);
    if (w && w->op != key_value_op::put && w->op != key_value_op::erase) [[unlikely]] {
        return std::nullopt;
    }
    return w;
}

std::vector<std::byte> encode_read(const key_value_read& r)
{
    return encoded(r);
}

std::optional<key_value_read> decode_read(const std::span<const std::byte> payload)
{
    return decoded<key_value_read>(payload);
}

}
