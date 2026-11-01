#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include <openssl/evp.h>

#include "konsens/chain_hash.hpp"

namespace {

konsens::chain_hash chain_hash_of_hex(const std::string_view hex)
{
    konsens::chain_hash out{};
    const auto nibble = [](const char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
    for (std::size_t i = 0; i < out.size(); ++i) {
        out[i] = static_cast<std::byte>(nibble(hex[2 * i]) * 16 + nibble(hex[2 * i + 1]));
    }
    return out;
}

// The reference is the one-shot digest of libcrypto over one buffer. The
// chain step feeds three parts into a held context, so the two forms
// are independent paths through libcrypto.
konsens::chain_hash reference_sha256_of(const std::span<const std::byte> bytes)
{
    konsens::chain_hash out{};
    unsigned int size = 0;
    REQUIRE(EVP_Digest(bytes.data(), bytes.size(), reinterpret_cast<unsigned char*>(out.data()), &size,
                       EVP_sha256(), nullptr) == 1);
    REQUIRE(size == out.size());
    return out;
}

std::vector<std::byte> bytes_of(const std::string_view text)
{
    std::vector<std::byte> out(text.size());
    std::ranges::transform(text, out.begin(), [](const char c) { return static_cast<std::byte>(c); });
    return out;
}

konsens::sha256 fetched_sha256()
{
    auto h = konsens::sha256::fetch();
    REQUIRE(h.has_value());
    return std::move(*h);
}

}

// The expectations are the examples of SHA-256 that NIST publishes for
// FIPS 180-4. They check the reference that the other tests compare with.
TEST_CASE("the reference digest gives the NIST examples of SHA-256")
{
    CHECK(reference_sha256_of({}) ==
          chain_hash_of_hex("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
    CHECK(reference_sha256_of(bytes_of("abc")) ==
          chain_hash_of_hex("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
}

// The chain step is SHA-256 over the previous chain value, the index in
// little-endian order and the encoded entry, with no separator. The bytes
// are built here by hand, so a change of the order or of the byte order
// of the index fails this test.
TEST_CASE("a chain step is SHA-256 over the previous value, the index in little-endian order and the entry")
{
    auto h = fetched_sha256();
    const konsens::chain_hash previous = chain_hash_of_hex(
        "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f");
    const auto entry = bytes_of("abc");
    std::vector<std::byte> by_hand(previous.begin(), previous.end());
    for (const std::uint8_t b : std::array<std::uint8_t, 8>{0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01}) {
        by_hand.push_back(std::byte{b});
    }
    by_hand.insert(by_hand.end(), entry.begin(), entry.end());

    CHECK(h.compute_chain_hash(previous, 0x0102030405060708, entry) == reference_sha256_of(by_hand));
}

// The chain value of the empty log is 32 zero bytes. The first entry
// extends that value like any other.
TEST_CASE("the first chain step starts from 32 zero bytes")
{
    auto h = fetched_sha256();
    std::vector<std::byte> by_hand(konsens::chain_hash_size + 8);
    by_hand[konsens::chain_hash_size] = std::byte{1};

    CHECK(h.compute_chain_hash(konsens::chain_hash{}, 1, {}) == reference_sha256_of(by_hand));
}

// The context is held between steps. A step must not carry state of the
// step before it.
TEST_CASE("a chain step gives the same value when it follows another step")
{
    auto h = fetched_sha256();
    const auto entry = bytes_of("entry");
    const auto first = h.compute_chain_hash(konsens::chain_hash{}, 7, entry);
    static_cast<void>(h.compute_chain_hash(first, 8, bytes_of("other")));

    CHECK(h.compute_chain_hash(konsens::chain_hash{}, 7, entry) == first);
}

// A moved-from object holds no context. A step on it is a wrong use.
TEST_CASE("a chain step on a moved-from sha256 throws std::logic_error")
{
    auto h = fetched_sha256();
    auto other = std::move(h);
    static_cast<void>(other);

    CHECK_THROWS_AS(static_cast<void>(h.compute_chain_hash(konsens::chain_hash{}, 1, {})), std::logic_error);
}
