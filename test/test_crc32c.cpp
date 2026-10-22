#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <random>
#include <ranges>
#include <span>
#include <vector>

#include <crc32c/crc32c.h>
#include <crc32c/crc32c_config.h>
#include <crc32c_arm64_check.h>
#include <crc32c_internal.h>
#include <crc32c_sse42_check.h>

#include "konsens/storage.hpp"

namespace {

// RFC 7143 appendix A.4 writes each CRC as the four bytes in the order of
// transmission, byte 0 first. Byte 0 is the least significant byte.
std::array<std::uint8_t, 4> transmitted_bytes(const std::uint32_t crc)
{
    return {static_cast<std::uint8_t>(crc), static_cast<std::uint8_t>(crc >> 8), static_cast<std::uint8_t>(crc >> 16),
            static_cast<std::uint8_t>(crc >> 24)};
}

std::vector<std::byte> bytes(const std::vector<std::uint8_t>& values)
{
    std::vector<std::byte> result;
    for (const auto v : values) {
        result.push_back(std::byte{v});
    }
    return result;
}

using rfc_crc = std::array<std::uint8_t, 4>;

// The library computes each CRC with crc32c::Crc32c or crc32c::Extend of
// Google crc32c. The tests call the same functions.
std::uint32_t crc_of(const std::span<const std::byte> data)
{
    return crc32c::Crc32c(data.data(), data.size());
}

std::uint32_t extend(const std::uint32_t crc, const std::span<const std::byte> data)
{
    return crc32c::Extend(crc, reinterpret_cast<const std::uint8_t*>(data.data()), data.size());
}

constexpr std::size_t longest_short_length = 1024;
constexpr std::size_t long_length = 65536;
constexpr std::uint32_t buffer_seed = 7143;

}

// RFC 7143 appendix A.4: 32 bytes of zeroes give the CRC aa 36 91 8a.
TEST_CASE("crc32c of 32 bytes of zeroes agrees with RFC 7143 A.4")
{
    const std::vector<std::byte> data(32, std::byte{0x00});
    CHECK(transmitted_bytes(crc_of(data)) == rfc_crc{0xaa, 0x36, 0x91, 0x8a});
}

// RFC 7143 appendix A.4: 32 bytes of ones give the CRC 43 ab a8 62.
TEST_CASE("crc32c of 32 bytes of ones agrees with RFC 7143 A.4")
{
    const std::vector<std::byte> data(32, std::byte{0xff});
    CHECK(transmitted_bytes(crc_of(data)) == rfc_crc{0x43, 0xab, 0xa8, 0x62});
}

// RFC 7143 appendix A.4: 32 incrementing bytes 00..1f give the CRC 4e 79 dd 46.
TEST_CASE("crc32c of 32 incrementing bytes agrees with RFC 7143 A.4")
{
    std::vector<std::uint8_t> values(32);
    std::iota(values.begin(), values.end(), std::uint8_t{0x00});
    CHECK(transmitted_bytes(crc_of(bytes(values))) == rfc_crc{0x4e, 0x79, 0xdd, 0x46});
}

// RFC 7143 appendix A.4: 32 decrementing bytes 1f..00 give the CRC 5c db 3f 11.
TEST_CASE("crc32c of 32 decrementing bytes agrees with RFC 7143 A.4")
{
    std::vector<std::uint8_t> values(32);
    std::iota(values.rbegin(), values.rend(), std::uint8_t{0x00});
    CHECK(transmitted_bytes(crc_of(bytes(values))) == rfc_crc{0x5c, 0xdb, 0x3f, 0x11});
}

// RFC 7143 appendix A.4: the iSCSI Read (10) command PDU gives the CRC 56 3a 96 d9.
TEST_CASE("crc32c of the iSCSI read command PDU agrees with RFC 7143 A.4")
{
    const auto pdu =
        bytes({0x01, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
               0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x18,
               0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
    REQUIRE(pdu.size() == 48);
    CHECK(transmitted_bytes(crc_of(pdu)) == rfc_crc{0x56, 0x3a, 0x96, 0xd9});
}

// The storage computes the CRC of an entry over the index, the kind and the
// payload in three calls. The CRC of the parts must agree with the CRC of the
// whole.
TEST_CASE("crc32c extended over parts agrees with crc32c of the whole")
{
    std::vector<std::uint8_t> values(48);
    std::iota(values.begin(), values.end(), std::uint8_t{0x00});
    const auto data = bytes(values);
    const std::span<const std::byte> whole{data};
    const auto in_parts = extend(extend(extend(0, whole.first(8)), whole.subspan(8, 1)), whole.subspan(9));
    CHECK(in_parts == crc_of(whole));
}

// A user tries the empty input first. The CRC of no data with the start
// value zero is zero.
TEST_CASE("crc32c of no data is zero")
{
    CHECK(crc_of({}) == 0);
}

// crc32c::Extend takes the instruction of the CPU when the CPU has it, and
// crc32c::ExtendPortable never takes it. Both must give the same CRC for
// every length, so that a store written on one CPU reads on another.
TEST_CASE("crc32c::Extend agrees with crc32c::ExtendPortable for each length")
{
    std::minstd_rand next{buffer_seed};
    std::vector<std::uint8_t> data(long_length);
    std::ranges::generate(data, [&next] { return static_cast<std::uint8_t>(next()); });
    for (std::size_t length = 0; length <= longest_short_length; ++length) {
        CHECK(crc32c::Extend(0, data.data(), length) == crc32c::ExtendPortable(0, data.data(), length));
    }
    CHECK(crc32c::Extend(0, data.data(), data.size()) == crc32c::ExtendPortable(0, data.data(), data.size()));
}

// Without the SSE4.2 form, every x86-64 CPU computes the CRC without its
// instruction. The build of Google crc32c must find the form.
TEST_CASE("the build of Google crc32c has the SSE4.2 form on x86-64")
{
#if defined(__x86_64__) || defined(_M_X64)
    CHECK(HAVE_SSE42 == 1);
#endif
}

// The program warns at start when the CPU has no CRC32C instruction. The
// answer must be the answer of the check that crc32c::Extend takes.
TEST_CASE("can_use_crc32c_instruction agrees with the check of Google crc32c")
{
#if HAVE_SSE42 && (defined(_M_X64) || defined(__x86_64__))
    CHECK(konsens::can_use_crc32c_instruction() == crc32c::CanUseSse42());
#elif HAVE_ARM64_CRC32C
    CHECK(konsens::can_use_crc32c_instruction() == crc32c::CanUseArm64Crc32());
#else
    CHECK_FALSE(konsens::can_use_crc32c_instruction());
#endif
}
