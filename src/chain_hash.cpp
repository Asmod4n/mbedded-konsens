#include "konsens/chain_hash.hpp"

#include <exception>
#include <stdexcept>
#include <utility>

#include <openssl/evp.h>
#include <openssl/sha.h>

#include "byte_order.hpp"

namespace konsens {

static_assert(chain_hash_size == SHA256_DIGEST_LENGTH);

struct sha256::context {
    EVP_MD* md;
    EVP_MD_CTX* ctx;

    context(EVP_MD* const m, EVP_MD_CTX* const c) noexcept : md{m}, ctx{c} {}
    context(const context&) = delete;
    context& operator=(const context&) = delete;
    ~context()
    {
        EVP_MD_CTX_free(ctx);
        EVP_MD_free(md);
    }
};

std::expected<sha256, std::error_code> sha256::fetch()
{
    EVP_MD* const md = EVP_MD_fetch(nullptr, "SHA2-256", nullptr);
    if (md == nullptr) [[unlikely]] {
        return std::unexpected(std::make_error_code(std::errc::function_not_supported));
    }
    EVP_MD_CTX* const ctx = EVP_MD_CTX_new();
    if (ctx == nullptr) [[unlikely]] {
        EVP_MD_free(md);
        return std::unexpected(std::make_error_code(std::errc::not_enough_memory));
    }
    return sha256{std::make_unique<context>(md, ctx)};
}

chain_hash sha256::compute_chain_hash(const chain_hash& previous, const std::uint64_t index,
                                      const std::span<const std::byte> encoded_entry) const
{
    if (!context_) [[unlikely]] {
        throw std::logic_error("konsens: a sha256 was moved from");
    }
    const auto index_bytes = to_little_endian(index);
    chain_hash out{};
    unsigned int size = 0;
    if (EVP_DigestInit_ex2(context_->ctx, context_->md, nullptr) != 1 ||
        EVP_DigestUpdate(context_->ctx, previous.data(), previous.size()) != 1 ||
        EVP_DigestUpdate(context_->ctx, index_bytes.data(), index_bytes.size()) != 1 ||
        EVP_DigestUpdate(context_->ctx, encoded_entry.data(), encoded_entry.size()) != 1 ||
        EVP_DigestFinal_ex(context_->ctx, reinterpret_cast<unsigned char*>(out.data()), &size) != 1 ||
        size != out.size()) [[unlikely]] {
        std::terminate();
    }
    return out;
}

sha256::sha256(std::unique_ptr<context> c) noexcept : context_{std::move(c)}
{
}

sha256::sha256(sha256&&) noexcept = default;
sha256& sha256::operator=(sha256&&) noexcept = default;
sha256::~sha256() = default;

}
