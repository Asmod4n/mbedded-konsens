#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <system_error>
#include <type_traits>
#include <vector>

#include <sys/types.h>

#include "konsens/storage.hpp"

namespace konsens {

enum class storage_errc { crc_mismatch = 1, malformed_record };

const std::error_category& storage_category() noexcept;
std::error_code make_error_code(storage_errc code) noexcept;

struct read_failure {
    std::error_code code;
    std::uint64_t index;
    std::uint32_t stored_crc;
    std::uint32_t computed_crc;

    std::string message() const;
};

class persistent_storage {
public:
    [[nodiscard]] static std::expected<persistent_storage, std::error_code>
    open(const std::filesystem::path& directory, std::size_t map_size, uid_t effective_user_id);

    [[nodiscard]] std::expected<void, std::error_code> write_atomically(std::span<const storage_op> ops);

    [[nodiscard]] std::expected<std::vector<entry>, read_failure> read_entries(std::uint64_t from,
                                                                               std::uint64_t to) const;
    [[nodiscard]] std::expected<std::vector<entry>, std::error_code> get_entries(std::uint64_t from,
                                                                                 std::uint64_t to) const;
    [[nodiscard]] std::expected<std::uint64_t, std::error_code> get_log_len() const;
    [[nodiscard]] std::expected<ballot, std::error_code> get_promise() const;
    [[nodiscard]] std::expected<std::uint64_t, std::error_code> get_decided_idx() const;
    [[nodiscard]] std::expected<ballot, std::error_code> get_accepted_round() const;
    [[nodiscard]] std::expected<std::uint32_t, std::error_code> get_last_entry_crc() const;

    persistent_storage(persistent_storage&&) noexcept;
    persistent_storage& operator=(persistent_storage&&) noexcept;
    ~persistent_storage();

private:
    struct environment;

    persistent_storage(std::unique_ptr<environment> env, unsigned int log_dbi, unsigned int meta_dbi) noexcept;

    void throw_if_moved_from() const;

    std::unique_ptr<environment> env_;
    unsigned int log_dbi_;
    unsigned int meta_dbi_;
};

}

template <>
struct std::is_error_code_enum<konsens::storage_errc> : std::true_type {};
