#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <span>

namespace konsens {

template <std::unsigned_integral T>
std::array<std::byte, sizeof(T)> to_little_endian(const T value) noexcept
{
    const T ordered = std::endian::native == std::endian::little ? value : std::byteswap(value);
    return std::bit_cast<std::array<std::byte, sizeof(T)>>(ordered);
}

template <std::unsigned_integral T>
std::array<std::byte, sizeof(T)> to_big_endian(const T value) noexcept
{
    const T ordered = std::endian::native == std::endian::big ? value : std::byteswap(value);
    return std::bit_cast<std::array<std::byte, sizeof(T)>>(ordered);
}

template <std::unsigned_integral T>
T from_little_endian(const std::span<const std::byte, sizeof(T)> bytes) noexcept
{
    std::array<std::byte, sizeof(T)> copy{};
    std::ranges::copy(bytes, copy.begin());
    const T value = std::bit_cast<T>(copy);
    return std::endian::native == std::endian::little ? value : std::byteswap(value);
}

template <std::unsigned_integral T>
T from_big_endian(const std::span<const std::byte, sizeof(T)> bytes) noexcept
{
    std::array<std::byte, sizeof(T)> copy{};
    std::ranges::copy(bytes, copy.begin());
    const T value = std::bit_cast<T>(copy);
    return std::endian::native == std::endian::big ? value : std::byteswap(value);
}

}
