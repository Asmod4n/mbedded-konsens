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
    struct reading;

    class read_only_transaction {
    public:
        [[nodiscard]] std::expected<meta_record, std::error_code> get_meta() const;
        [[nodiscard]] std::expected<std::vector<entry>, read_failure> read_entries(std::uint64_t from,
                                                                                   std::uint64_t to) const;
        [[nodiscard]] std::expected<std::vector<entry>, std::error_code> get_entries(std::uint64_t from,
                                                                                     std::uint64_t to) const;
        [[nodiscard]] std::expected<std::optional<last_entry>, std::error_code> get_last_entry() const;
        [[nodiscard]] std::expected<std::optional<std::uint32_t>, std::error_code>
        get_chain_crc(std::uint64_t index) const;
        [[nodiscard]] std::expected<std::optional<std::uint64_t>, std::error_code> get_config_first_idx() const;
        [[nodiscard]] std::expected<std::optional<std::span<const std::byte>>, std::error_code>
        find(std::span<const std::byte> key) const;
        [[nodiscard]] std::expected<std::optional<session_record>, std::error_code>
        find_session(std::uint64_t session) const;

        read_only_transaction(read_only_transaction&& other) noexcept;
        read_only_transaction& operator=(read_only_transaction&& other) noexcept;
        ~read_only_transaction();

    private:
        friend class persistent_storage;

        explicit read_only_transaction(reading* r) noexcept;

        reading* reading_;
    };

    class reader {
    public:
        [[nodiscard]] std::expected<read_only_transaction, std::error_code> begin_read_only() const;

        reader(reader&&) noexcept;
        reader& operator=(reader&&) noexcept;
        ~reader();

    private:
        friend class persistent_storage;

        explicit reader(std::unique_ptr<reading> r) noexcept;

        std::unique_ptr<reading> reading_;
    };

    class nested_transaction {
    public:
        [[nodiscard]] std::expected<std::optional<std::span<const std::byte>>, std::error_code>
        find(std::span<const std::byte> key) const;
        [[nodiscard]] std::expected<std::optional<session_record>, std::error_code>
        find_session(std::uint64_t session) const;
        [[nodiscard]] std::expected<void, std::error_code> insert_or_assign(std::span<const std::byte> key,
                                                                            std::span<const std::byte> value);
        [[nodiscard]] std::expected<void, std::error_code> erase(std::span<const std::byte> key);
        [[nodiscard]] std::expected<void, std::error_code> insert_or_assign_session(std::uint64_t session,
                                                                                    const session_record& record);
        [[nodiscard]] std::expected<void, std::error_code> erase_session(std::uint64_t session);

        nested_transaction(nested_transaction&& other) noexcept;
        nested_transaction& operator=(nested_transaction&& other) noexcept;
        ~nested_transaction();

    private:
        friend class persistent_storage;

        explicit nested_transaction(environment* env) noexcept;

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
        [[nodiscard]] std::expected<std::optional<std::uint32_t>, std::error_code>
        get_chain_crc(std::uint64_t index) const;
        [[nodiscard]] std::expected<std::optional<std::uint64_t>, std::error_code> get_config_first_idx() const;

        [[nodiscard]] std::expected<void, std::error_code> append_entries(std::span<const entry> entries);
        [[nodiscard]] std::expected<void, std::error_code> append_on_prefix(std::uint64_t from_idx,
                                                                            std::span<const entry> entries);
        [[nodiscard]] std::expected<void, std::error_code> set_promise(ballot promise);
        [[nodiscard]] std::expected<void, std::error_code> set_accepted_round(ballot accepted_round);
        [[nodiscard]] std::expected<void, std::error_code> set_decided_index(std::uint64_t decided_idx);
        [[nodiscard]] std::expected<void, std::error_code> set_config_first_idx(std::uint64_t config_first_idx);
        [[nodiscard]] std::expected<std::optional<std::span<const std::byte>>, std::error_code>
        find(std::span<const std::byte> key) const;
        [[nodiscard]] std::expected<std::optional<session_record>, std::error_code>
        find_session(std::uint64_t session) const;
        [[nodiscard]] std::expected<void, std::error_code> insert_or_assign(std::span<const std::byte> key,
                                                                            std::span<const std::byte> value);
        [[nodiscard]] std::expected<void, std::error_code> erase(std::span<const std::byte> key);
        [[nodiscard]] std::expected<void, std::error_code> insert_or_assign_session(std::uint64_t session,
                                                                                    const session_record& record);
        [[nodiscard]] std::expected<void, std::error_code> erase_session(std::uint64_t session);
        [[nodiscard]] std::expected<void, std::error_code>
        set_applied(std::uint64_t applied_idx, std::uint32_t applied_chain_crc);
        [[nodiscard]] std::expected<nested_transaction, std::error_code> begin_nested();
        [[nodiscard]] bool has_writes() const;
        [[nodiscard]] bool has_log_writes() const;
        [[nodiscard]] std::uint64_t count_of_entry_writes() const;
        [[nodiscard]] std::expected<void, std::error_code> commit() &&;
        [[nodiscard]] std::expected<void, std::error_code> commit_without_a_sync() &&;

        transaction(transaction&& other) noexcept;
        transaction& operator=(transaction&& other) noexcept;
        ~transaction();

    private:
        friend class persistent_storage;

        explicit transaction(environment* env) noexcept;

        environment* env_;
    };

    [[nodiscard]] static std::expected<persistent_storage, std::error_code>
    open(const std::filesystem::path& directory, std::size_t max_size, uid_t effective_user_id);

    [[nodiscard]] std::expected<transaction, std::error_code> begin();
    [[nodiscard]] std::expected<read_only_transaction, std::error_code> begin_read_only() const;
    [[nodiscard]] std::expected<reader, std::error_code> open_a_reader() const;
    [[nodiscard]] std::expected<void, std::error_code> copy(const std::filesystem::path& directory) const;
    [[nodiscard]] int wal_file_descriptor() const;
    void the_sync_failed();

    persistent_storage(persistent_storage&&) noexcept;
    persistent_storage& operator=(persistent_storage&&) noexcept;
    ~persistent_storage();

private:
    explicit persistent_storage(std::unique_ptr<environment> env) noexcept;

    void throw_if_moved_from() const;

    std::unique_ptr<environment> env_;
};

enum class proof_kind : std::uint8_t {
    damage = 1,
    result_crc,
    read_back,
    refused_by_a_majority,
    listed_result_differs,
    wrong_list,
    chosen_result_differs
};

struct fault_proof {
    proof_kind kind;
    std::uint64_t index;
    ballot n;
    std::uint32_t crc;
    std::uint32_t other_crc;

    friend bool operator==(const fault_proof&, const fault_proof&) = default;
};

struct clean_stop_mark {
    node_id pid;
    std::uint32_t configuration_id;
    meta_record meta;
    std::uint64_t verified_idx;

    friend bool operator==(const clean_stop_mark&, const clean_stop_mark&) = default;
};

class node_storage {
    struct environment;

public:
    [[nodiscard]] static std::expected<node_storage, std::error_code> open(const std::filesystem::path& directory,
                                                                           uid_t effective_user_id);

    [[nodiscard]] std::expected<std::optional<fault_proof>, std::error_code> get_fault_proof() const;
    [[nodiscard]] std::expected<void, std::error_code> write_the_proof(const fault_proof& proof);
    [[nodiscard]] std::expected<std::optional<clean_stop_mark>, std::error_code> get_clean_stop_mark() const;
    [[nodiscard]] std::expected<void, std::error_code> write_the_clean_stop_mark(const clean_stop_mark& mark);
    [[nodiscard]] std::expected<void, std::error_code> delete_the_clean_stop_mark();

    node_storage(node_storage&&) noexcept;
    node_storage& operator=(node_storage&&) noexcept;
    ~node_storage();

private:
    explicit node_storage(std::unique_ptr<environment> env) noexcept;

    void throw_if_moved_from() const;

    std::unique_ptr<environment> env_;
};

}
