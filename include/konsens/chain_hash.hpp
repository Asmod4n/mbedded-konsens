// The chain of the log: SHA-256 (FIPS 180-4) over the previous chain value, the index of the entry in little-endian
// order and the encoded entry. The digest comes from libcrypto of OpenSSL.
// https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.180-4.pdf
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <system_error>

namespace konsens {

inline constexpr std::size_t chain_hash_size = 32;

using chain_hash = std::array<std::byte, chain_hash_size>;

class sha256 {
public:
    [[nodiscard]] static std::expected<sha256, std::error_code> fetch();

    [[nodiscard]] chain_hash compute_chain_hash(const chain_hash& previous, std::uint64_t index,
                                                std::span<const std::byte> encoded_entry) const;

    sha256(sha256&&) noexcept;
    sha256& operator=(sha256&&) noexcept;
    ~sha256();

private:
    struct context;

    explicit sha256(std::unique_ptr<context> c) noexcept;

    std::unique_ptr<context> context_;
};

}
