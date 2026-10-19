#include "konsens/crc32c.hpp"

#include <algorithm>
#include <array>
#include <limits>

namespace konsens::crc32c {

namespace {

constexpr std::uint32_t castagnoli_polynomial_reflected = 0x82F63B78u;
constexpr std::uint32_t inversion_mask = std::numeric_limits<std::uint32_t>::max();
constexpr int bits_per_byte = std::numeric_limits<unsigned char>::digits;
constexpr std::size_t byte_value_count = std::size_t{1} << bits_per_byte;

constexpr std::array<std::uint32_t, byte_value_count> table = [] {
    std::array<std::uint32_t, byte_value_count> result{};
    for (std::uint32_t value = 0; value < byte_value_count; ++value) {
        std::uint32_t remainder = value;
        for (int bit = 0; bit < bits_per_byte; ++bit) {
            remainder = (remainder & 1u) != 0 ? (remainder >> 1) ^ castagnoli_polynomial_reflected
                                              : remainder >> 1;
        }
        result[value] = remainder;
    }
    return result;
}();

static_assert(byte_value_count == std::size_t{std::numeric_limits<std::uint8_t>::max()} + 1);

}

std::uint32_t extend(const std::uint32_t crc, const std::span<const std::byte> data) noexcept {
    const auto step = [](const std::uint32_t remainder, const std::byte octet) {
        const auto position = static_cast<std::uint8_t>(remainder ^ std::to_integer<std::uint32_t>(octet));
        return table[position] ^ (remainder >> bits_per_byte);
    };
    return std::ranges::fold_left(data, crc ^ inversion_mask, step) ^ inversion_mask;
}

}
