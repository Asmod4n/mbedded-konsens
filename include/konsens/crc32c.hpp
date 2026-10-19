#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace konsens::crc32c {

std::uint32_t extend(std::uint32_t crc, std::span<const std::byte> data) noexcept;

}
