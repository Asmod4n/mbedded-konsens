#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <vector>

#include <sys/types.h>

#include "konsens/storage.hpp"

namespace konsens {

struct read_failure {
    std::error_code code;
    std::uint64_t index;
    std::uint32_t stored_crc;
    std::uint32_t computed_crc;

    std::string message() const;
};

class persistent_storage {
    struct environment;

public:
    class read_only_transaction {
    public:
        [[nodiscard]] std::expected<meta_record, std::error_code> get_meta() const;
        [[nodiscard]] std::expected<std::vector<entry>, read_failure> read_entries(std::uint64_t from,
                                                                                   std::uint64_t to) const;
        [[nodiscard]] std::expected<std::vector<entry>, std::error_code> get_entries(std::uint64_t from,
                                                                                     std::uint64_t to) const;
        [[nodiscard]] std::expected<std::optional<last_entry>, std::error_code> get_last_entry() const;
        [[nodiscard]] std::expected<std::optional<entry_state>, std::error_code>
        get_entry_state(std::uint64_t index) const;

        read_only_transaction(read_only_transaction&& other) noexcept;
        read_only_transaction& operator=(read_only_transaction&& other) noexcept;
        ~read_only_transaction();

    private:
        friend class persistent_storage;

        explicit read_only_transaction(environment* env) noexcept;

        environment* env_;
    };

    class transaction {
    public:
        [[nodiscard]] std::expected<meta_record, std::error_code> get_meta() const;
        [[nodiscard]] std::expected<std::vector<entry>, read_failure> read_entries(std::uint64_t from,
                                                                                   std::uint64_t to) const;
        [[nodiscard]] std::expected<std::vector<entry>, std::error_code> get_entries(std::uint64_t from,
                                                                                     std::uint64_t to) const;
        [[nodiscard]] std::expected<std::optional<last_entry>, std::error_code> get_last_entry() const;
        [[nodiscard]] std::expected<std::optional<entry_state>, std::error_code>
        get_entry_state(std::uint64_t index) const;

        [[nodiscard]] std::expected<void, std::error_code> append_entries(std::span<const entry> entries);
        [[nodiscard]] std::expected<void, std::error_code> append_on_prefix(std::uint64_t from_idx,
                                                                            std::span<const entry> entries);
        [[nodiscard]] std::expected<void, std::error_code> set_promise(ballot promise);
        [[nodiscard]] std::expected<void, std::error_code> set_accepted_round(ballot accepted_round);
        [[nodiscard]] std::expected<void, std::error_code> set_decided_index(std::uint64_t decided_idx);
        [[nodiscard]] std::expected<void, std::error_code> commit() &&;

        transaction(transaction&& other) noexcept;
        transaction& operator=(transaction&& other) noexcept;
        ~transaction();

    private:
        friend class persistent_storage;

        explicit transaction(environment* env) noexcept;

        environment* env_;
    };

    [[nodiscard]] static std::expected<persistent_storage, std::error_code>
    open(const std::filesystem::path& directory, std::size_t map_size, uid_t effective_user_id);

    [[nodiscard]] std::expected<transaction, std::error_code> begin();
    [[nodiscard]] std::expected<read_only_transaction, std::error_code> begin_read_only() const;
    [[nodiscard]] std::expected<void, std::error_code> copy(const std::filesystem::path& directory) const;

    persistent_storage(persistent_storage&&) noexcept;
    persistent_storage& operator=(persistent_storage&&) noexcept;
    ~persistent_storage();

private:
    explicit persistent_storage(std::unique_ptr<environment> env) noexcept;

    void throw_if_moved_from() const;

    std::unique_ptr<environment> env_;
};

}
