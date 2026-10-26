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
        [[nodiscard]] std::expected<std::optional<std::uint32_t>, std::error_code>
        get_chain_crc(std::uint64_t index) const;
        [[nodiscard]] std::expected<std::optional<std::span<const std::byte>>, std::error_code>
        find(std::span<const std::byte> key) const;
        [[nodiscard]] std::expected<std::optional<session_record>, std::error_code>
        find_session(std::uint64_t session) const;

        read_only_transaction(read_only_transaction&& other) noexcept;
        read_only_transaction& operator=(read_only_transaction&& other) noexcept;
        ~read_only_transaction();

    private:
        friend class persistent_storage;

        explicit read_only_transaction(environment* env) noexcept;

        environment* env_;
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

    class prepared_transaction {
    public:
        [[nodiscard]] std::expected<void, std::error_code> commit() &&;

        prepared_transaction(prepared_transaction&& other) noexcept;
        prepared_transaction& operator=(prepared_transaction&& other) noexcept;
        ~prepared_transaction();

    private:
        friend class persistent_storage;

        explicit prepared_transaction(environment* env) noexcept;

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
        [[nodiscard]] std::expected<std::optional<std::uint32_t>, std::error_code>
        get_chain_crc(std::uint64_t index) const;

        [[nodiscard]] std::expected<void, std::error_code> append_entries(std::span<const entry> entries);
        [[nodiscard]] std::expected<void, std::error_code> append_on_prefix(std::uint64_t from_idx,
                                                                            std::span<const entry> entries);
        [[nodiscard]] std::expected<void, std::error_code> set_promise(ballot promise);
        [[nodiscard]] std::expected<void, std::error_code> set_accepted_round(ballot accepted_round);
        [[nodiscard]] std::expected<void, std::error_code> set_decided_index(std::uint64_t decided_idx);
        [[nodiscard]] std::expected<void, std::error_code> write_repaired_log(std::uint64_t from_idx,
                                                                              std::span<const entry> entries,
                                                                              ballot promise, ballot accepted_round,
                                                                              std::uint64_t decided_idx);
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
        set_applied(std::uint64_t applied_idx, std::uint32_t applied_chain_crc, std::uint64_t pointer);
        [[nodiscard]] std::expected<nested_transaction, std::error_code> begin_nested();
        [[nodiscard]] bool has_writes() const;
        [[nodiscard]] std::expected<void, std::error_code> commit() &&;
        [[nodiscard]] std::expected<prepared_transaction, std::error_code> prepare() &&;

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
    [[nodiscard]] std::expected<transaction, std::error_code> begin_without_sync();
    [[nodiscard]] int file_descriptor() const;
    [[nodiscard]] std::expected<read_only_transaction, std::error_code> begin_read_only() const;
    [[nodiscard]] std::expected<void, std::error_code> copy(const std::filesystem::path& directory) const;

    persistent_storage(persistent_storage&&) noexcept;
    persistent_storage& operator=(persistent_storage&&) noexcept;
    ~persistent_storage();

private:
    explicit persistent_storage(std::unique_ptr<environment> env) noexcept;

    void throw_if_moved_from() const;
    std::expected<transaction, std::error_code> begin_writer(bool sync_outside);

    std::unique_ptr<environment> env_;
};

enum class proof_kind : std::uint8_t {
    damage = 1,
    result_crc,
    read_back,
    refused_by_a_majority,
    listed_result_differs,
    wrong_list
};

struct fault_proof {
    proof_kind kind;
    std::uint64_t index;
    ballot n;
    std::uint32_t crc;
    std::uint32_t other_crc;

    friend bool operator==(const fault_proof&, const fault_proof&) = default;
};

struct repair_record {
    std::uint64_t count;
    std::optional<std::uint64_t> open_mark;
    bool mark_covered;
    std::optional<fault_proof> proof;

    friend bool operator==(const repair_record&, const repair_record&) = default;
};

class node_storage {
    struct environment;

public:
    [[nodiscard]] static std::expected<node_storage, std::error_code> open(const std::filesystem::path& directory,
                                                                           uid_t effective_user_id);

    [[nodiscard]] std::expected<repair_record, std::error_code> get_repair_record() const;
    [[nodiscard]] std::expected<void, std::error_code> write_the_proof(const fault_proof& proof);
    [[nodiscard]] std::expected<void, std::error_code> cover_the_proof();
    [[nodiscard]] std::expected<void, std::error_code> count_a_repair(std::uint64_t open_mark);
    [[nodiscard]] std::expected<void, std::error_code> close_the_repair();
    [[nodiscard]] std::expected<void, std::error_code> allow_one_more_repair();

    node_storage(node_storage&&) noexcept;
    node_storage& operator=(node_storage&&) noexcept;
    ~node_storage();

private:
    explicit node_storage(std::unique_ptr<environment> env) noexcept;

    void throw_if_moved_from() const;
    std::expected<void, std::error_code> write_repair_record(const repair_record& r);

    std::unique_ptr<environment> env_;
};

}
