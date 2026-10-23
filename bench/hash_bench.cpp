#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

#include <crc32c/crc32c.h>
#include <openssl/evp.h>

namespace {

constexpr std::array<std::size_t, 3> payload_sizes{64, 1024, 65536};
constexpr std::size_t chunk_bytes = 64;
constexpr std::size_t buffer_count = 16;
constexpr std::size_t sample_count = 31;
constexpr std::byte payload_byte{0x5a};

struct measurement {
    double ns_per_op;
    std::uint32_t folded;
};

std::uint64_t operations_for(const std::size_t bytes)
{
    if (bytes == 64) return 200000;
    if (bytes == 1024) return 20000;
    return 512;
}

template <typename F>
measurement measure(const std::size_t bytes, const std::uint64_t operations, F&& hash)
{
    const std::vector<std::vector<std::byte>> buffers(
        buffer_count, std::vector<std::byte>(bytes, payload_byte));
    std::vector<double> samples;
    samples.reserve(sample_count);
    std::uint32_t folded = 0;

    for (std::size_t sample = 0; sample < sample_count + 1; ++sample) {
        std::uint32_t result = 0;
        const auto start = std::chrono::steady_clock::now();
        for (std::uint64_t i = 0; i < operations; ++i) {
            result ^= hash(std::span<const std::byte>(buffers.at(i % buffer_count)));
        }
        const auto end = std::chrono::steady_clock::now();
        folded ^= result;
        if (sample != 0) {
            samples.push_back(std::chrono::duration<double, std::nano>(end - start).count()
                              / static_cast<double>(operations));
        }
    }

    std::ranges::sort(samples);
    return {samples.at(samples.size() / 2), folded};
}

std::uint32_t crc32c_incremental(const std::span<const std::byte> bytes)
{
    std::uint32_t crc = 0;
    for (std::size_t offset = 0; offset < bytes.size(); offset += chunk_bytes) {
        const auto count = std::min(chunk_bytes, bytes.size() - offset);
        crc = crc32c::Extend(crc, reinterpret_cast<const std::uint8_t*>(bytes.data() + offset), count);
    }
    return crc;
}

std::uint32_t sha256_incremental(EVP_MD_CTX* ctx, const std::span<const std::byte> bytes)
{
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) std::abort();
    for (std::size_t offset = 0; offset < bytes.size(); offset += chunk_bytes) {
        const auto count = std::min(chunk_bytes, bytes.size() - offset);
        if (EVP_DigestUpdate(ctx, bytes.data() + offset, count) != 1) std::abort();
    }
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int digest_bytes = 0;
    if (EVP_DigestFinal_ex(ctx, digest.data(), &digest_bytes) != 1 || digest_bytes != 32) std::abort();
    return (static_cast<std::uint32_t>(digest[0]) << 24)
         | (static_cast<std::uint32_t>(digest[1]) << 16)
         | (static_cast<std::uint32_t>(digest[2]) << 8)
         | static_cast<std::uint32_t>(digest[3]);
}

bool check_vectors(EVP_MD_CTX* ctx)
{
    constexpr std::string_view crc_input = "123456789";
    constexpr std::uint32_t crc_expected = 0xe3069283;
    if (crc32c::Crc32c(crc_input) != crc_expected) return false;

    constexpr std::string_view sha_input = "abc";
    constexpr std::array<unsigned char, 32> sha_expected{
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
        0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
        0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad};
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int digest_bytes = 0;
    return EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) == 1
        && EVP_DigestUpdate(ctx, sha_input.data(), sha_input.size()) == 1
        && EVP_DigestFinal_ex(ctx, digest.data(), &digest_bytes) == 1
        && digest_bytes == sha_expected.size()
        && std::equal(sha_expected.begin(), sha_expected.end(), digest.begin());
}

} // namespace

int main()
{
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (ctx == nullptr) {
        std::cerr << "EVP_MD_CTX_new failed\n";
        return EXIT_FAILURE;
    }
    if (!check_vectors(ctx)) {
        std::cerr << "CRC32C or SHA-256 test vector failed\n";
        EVP_MD_CTX_free(ctx);
        return EXIT_FAILURE;
    }

    std::cout << "# SHA-256 via OpenSSL EVP; both hashes process each payload in 64-byte chunks\n";
    std::cout << "# 31 measured samples after one warm-up; median ns/op; independent payloads, not chained log latency\n";
    std::cout << "bytes,operations,crc32c_ns,sha256_ns,sha256_over_crc,crc_folded,sha_folded\n";
    for (const auto bytes : payload_sizes) {
        const auto operations = operations_for(bytes);
        const auto crc = measure(bytes, operations, crc32c_incremental);
        const auto sha = measure(bytes, operations, [ctx](const auto data) {
            return sha256_incremental(ctx, data);
        });
        std::cout << std::format("{},{},{:.2f},{:.2f},{:.2f},{:08x},{:08x}\n",
                                 bytes, operations, crc.ns_per_op, sha.ns_per_op,
                                 sha.ns_per_op / crc.ns_per_op, crc.folded, sha.folded);
    }
    EVP_MD_CTX_free(ctx);
}
