#include "konsens/persistent_storage.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <cstdlib>
#include <format>
#include <iostream>
#include <iterator>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <variant>

#include <lmdb.h>

#include "konsens/crc32c.hpp"
#include "konsens/entry_range.hpp"

namespace konsens {

namespace {

constexpr unsigned int named_database_count = 2;
constexpr mdb_mode_t file_mode = 0600;
constexpr uid_t root_user_id = 0;
constexpr const char* log_database_name = "log";
constexpr const char* meta_database_name = "meta";
constexpr std::string_view promise_key = "promise";
constexpr std::string_view decided_idx_key = "decided_idx";
constexpr std::string_view accepted_round_key = "accepted_round";

constexpr std::size_t index_size = sizeof(std::uint64_t);
constexpr std::size_t kind_size = sizeof(entry_kind);
constexpr std::size_t crc_size = sizeof(std::uint32_t);
constexpr std::size_t field_size = sizeof(std::uint64_t);
constexpr std::size_t max_field_count = 2;

using entry_fields = std::array<std::uint64_t, max_field_count>;

constexpr std::size_t config_id_offset = 0;
constexpr std::size_t n_offset = config_id_offset + sizeof(ballot::config_id);
constexpr std::size_t priority_offset = n_offset + sizeof(ballot::n);
constexpr std::size_t pid_offset = priority_offset + sizeof(ballot::priority);
constexpr std::size_t ballot_size = pid_offset + sizeof(ballot::pid);

class storage_error_category final : public std::error_category {
public:
    const char* name() const noexcept override { return "konsens.storage"; }

    std::string message(const int code) const override {
        switch (static_cast<storage_errc>(code)) {
        case storage_errc::crc_mismatch:
            return "The CRC32C of a record does not agree with its data";
        case storage_errc::malformed_record:
            return "A record has a length or a value that the format does not allow";
        }
        return "Unknown storage error";
    }
};

class lmdb_error_category final : public std::error_category {
public:
    const char* name() const noexcept override { return "lmdb"; }

    std::string message(const int code) const override { return mdb_strerror(code); }
};

std::error_code to_error_code(const int lmdb_status) noexcept {
    if (lmdb_status == MDB_MAP_FULL) {
        return std::make_error_code(std::errc::no_space_on_device);
    }
    if (lmdb_status > 0) {
        return {lmdb_status, std::generic_category()};
    }
    return {lmdb_status, lmdb_category()};
}

template <std::unsigned_integral T>
std::array<std::byte, sizeof(T)> to_little_endian(const T value) noexcept {
    const T ordered = std::endian::native == std::endian::little ? value : std::byteswap(value);
    return std::bit_cast<std::array<std::byte, sizeof(T)>>(ordered);
}

template <std::unsigned_integral T>
std::array<std::byte, sizeof(T)> to_big_endian(const T value) noexcept {
    const T ordered = std::endian::native == std::endian::big ? value : std::byteswap(value);
    return std::bit_cast<std::array<std::byte, sizeof(T)>>(ordered);
}

template <std::unsigned_integral T>
T from_little_endian(const std::span<const std::byte, sizeof(T)> bytes) noexcept {
    std::array<std::byte, sizeof(T)> copy{};
    std::ranges::copy(bytes, copy.begin());
    const T value = std::bit_cast<T>(copy);
    return std::endian::native == std::endian::little ? value : std::byteswap(value);
}

template <std::unsigned_integral T>
T from_big_endian(const std::span<const std::byte, sizeof(T)> bytes) noexcept {
    std::array<std::byte, sizeof(T)> copy{};
    std::ranges::copy(bytes, copy.begin());
    const T value = std::bit_cast<T>(copy);
    return std::endian::native == std::endian::big ? value : std::byteswap(value);
}

MDB_val value_of(const std::span<const std::byte> bytes) noexcept {
    return MDB_val{bytes.size(), const_cast<std::byte*>(bytes.data())};
}

MDB_val value_of(const std::string_view text) noexcept {
    return MDB_val{text.size(), const_cast<char*>(text.data())};
}

std::span<const std::byte> bytes_of(const MDB_val& value) noexcept {
    return {static_cast<const std::byte*>(value.mv_data), value.mv_size};
}

std::span<std::byte> writable_bytes_of(const MDB_val& value) noexcept {
    return {static_cast<std::byte*>(value.mv_data), value.mv_size};
}

struct transaction_abort {
    void operator()(MDB_txn* txn) const noexcept { mdb_txn_abort(txn); }
};

using transaction = std::unique_ptr<MDB_txn, transaction_abort>;

struct cursor_close {
    void operator()(MDB_cursor* cursor) const noexcept { mdb_cursor_close(cursor); }
};

using cursor = std::unique_ptr<MDB_cursor, cursor_close>;

std::expected<transaction, std::error_code> begin_transaction(MDB_env* env, const unsigned int flags) {
    MDB_txn* txn = nullptr;
    const int status = mdb_txn_begin(env, nullptr, flags, &txn);
    if (status != MDB_SUCCESS) [[unlikely]] {
        return std::unexpected(to_error_code(status));
    }
    return transaction{txn};
}

std::expected<void, std::error_code> commit_or_end_process(transaction txn) {
    const int status = mdb_txn_commit(txn.release());
    if (status == MDB_MAP_FULL) [[unlikely]] {
        return std::unexpected(to_error_code(status));
    }
    if (status != MDB_SUCCESS) [[unlikely]] {
        std::cerr << "mdb_txn_commit failed: " << mdb_strerror(status) << '\n';
        std::abort();
    }
    return {};
}

std::uint32_t compute_entry_crc(const std::uint64_t index, const std::span<const std::byte> kind_and_payload) noexcept {
    return crc32c::extend(crc32c::extend(0, to_little_endian(index)), kind_and_payload);
}

entry_fields fields_of(const entry& e) noexcept {
    switch (e.kind) {
    case entry_kind::register_client:
        return {e.client_id, e.pid};
    case entry_kind::client_request:
        return {e.session, e.request_number};
    case entry_kind::expire_session:
        return {e.session, 0};
    case entry_kind::normal:
    case entry_kind::stop_sign:
        break;
    }
    return {};
}

entry entry_of(const entry_kind kind, const entry_fields& fields, const std::span<const std::byte> payload) {
    entry e{kind, std::vector<std::byte>(payload.begin(), payload.end())};
    switch (kind) {
    case entry_kind::register_client:
        e.client_id = fields[0];
        e.pid = fields[1];
        break;
    case entry_kind::client_request:
        e.session = fields[0];
        e.request_number = fields[1];
        break;
    case entry_kind::expire_session:
        e.session = fields[0];
        break;
    case entry_kind::normal:
    case entry_kind::stop_sign:
        break;
    }
    return e;
}

std::size_t encoded_size_of(const entry& e) {
    throw_if_entry_has_a_field_of_another_kind(e);
    return kind_size + field_size * field_count_of(e.kind).value() + e.payload.size() + crc_size;
}

void encode_entry(const std::uint64_t index, const entry& e, const std::span<std::byte> out) noexcept {
    out.front() = std::byte{std::to_underlying(e.kind)};
    auto next = std::next(out.begin(), kind_size);
    const auto fields = fields_of(e);
    for (const auto field : std::span(fields).first(field_count_of(e.kind).value_or(0))) {
        next = std::ranges::copy(to_little_endian(field), next).out;
    }
    std::ranges::copy(e.payload, next);
    const auto crc = compute_entry_crc(index, out.first(out.size() - crc_size));
    std::ranges::copy(to_little_endian(crc), out.last(crc_size).begin());
}

std::expected<entry, read_failure> decode_entry(const std::uint64_t index, const std::span<const std::byte> value) {
    if (value.size() < kind_size + crc_size) [[unlikely]] {
        return std::unexpected(read_failure{storage_errc::malformed_record, index, 0, 0});
    }
    const auto kind_and_payload = value.first(value.size() - crc_size);
    const auto stored = from_little_endian<std::uint32_t>(value.last<crc_size>());
    const auto computed = compute_entry_crc(index, kind_and_payload);
    if (stored != computed) [[unlikely]] {
        return std::unexpected(read_failure{storage_errc::crc_mismatch, index, stored, computed});
    }
    const auto kind = static_cast<entry_kind>(std::to_integer<std::uint8_t>(kind_and_payload.front()));
    const auto field_count = field_count_of(kind);
    if (!field_count || kind_and_payload.size() < kind_size + field_size * *field_count) [[unlikely]] {
        return std::unexpected(read_failure{storage_errc::malformed_record, index, stored, computed});
    }
    entry_fields fields{};
    auto rest = kind_and_payload.subspan(kind_size);
    for (auto& field : std::span(fields).first(*field_count)) {
        field = from_little_endian<std::uint64_t>(rest.first<field_size>());
        rest = rest.subspan(field_size);
    }
    return entry_of(kind, fields, rest);
}

std::array<std::byte, ballot_size> encode_ballot(const ballot& b) noexcept {
    std::array<std::byte, ballot_size> out{};
    auto next = std::ranges::copy(to_little_endian(b.config_id), out.begin()).out;
    next = std::ranges::copy(to_little_endian(b.n), next).out;
    next = std::ranges::copy(to_little_endian(b.priority), next).out;
    std::ranges::copy(to_little_endian(b.pid), next);
    return out;
}

ballot decode_ballot(const std::span<const std::byte, ballot_size> bytes) noexcept {
    return ballot{
        from_little_endian<std::uint32_t>(bytes.subspan<config_id_offset, sizeof(ballot::config_id)>()),
        from_little_endian<std::uint32_t>(bytes.subspan<n_offset, sizeof(ballot::n)>()),
        from_little_endian<std::uint32_t>(bytes.subspan<priority_offset, sizeof(ballot::priority)>()),
        from_little_endian<std::uint64_t>(bytes.subspan<pid_offset, sizeof(ballot::pid)>()),
    };
}

struct write_target {
    MDB_txn* txn;
    MDB_dbi log_dbi;
    MDB_dbi meta_dbi;
};

std::expected<std::uint64_t, std::error_code> read_log_len(MDB_txn* txn, const MDB_dbi log_dbi) {
    MDB_stat stat{};
    const int status = mdb_stat(txn, log_dbi, &stat);
    if (status != MDB_SUCCESS) [[unlikely]] {
        return std::unexpected(to_error_code(status));
    }
    return stat.ms_entries;
}

std::expected<void, std::error_code> put_entry(const write_target& target, const std::uint64_t index, const entry& e) {
    const auto key_bytes = to_big_endian(index);
    MDB_val key = value_of(key_bytes);
    MDB_val data{encoded_size_of(e), nullptr};
    const int status = mdb_put(target.txn, target.log_dbi, &key, &data, MDB_APPEND | MDB_RESERVE);
    if (status != MDB_SUCCESS) [[unlikely]] {
        return std::unexpected(to_error_code(status));
    }
    encode_entry(index, e, writable_bytes_of(data));
    return {};
}

std::expected<std::uint64_t, std::error_code> put_entries(const write_target& target, const std::uint64_t log_len,
                                                          const std::vector<entry>& entries) {
    for (const auto& [offset, e] : std::views::enumerate(entries)) {
        const auto put = put_entry(target, log_len + static_cast<std::uint64_t>(offset), e);
        if (!put) [[unlikely]] {
            return std::unexpected(put.error());
        }
    }
    return log_len + entries.size();
}

std::expected<void, std::error_code> delete_entries(const write_target& target, const std::uint64_t from,
                                                    const std::uint64_t to) {
    for (const auto index : std::views::iota(from, to)) {
        const auto key_bytes = to_big_endian(index);
        MDB_val key = value_of(key_bytes);
        const int status = mdb_del(target.txn, target.log_dbi, &key, nullptr);
        if (status != MDB_SUCCESS) [[unlikely]] {
            return std::unexpected(to_error_code(status));
        }
    }
    return {};
}

std::expected<void, std::error_code> put_meta(const write_target& target, const std::string_view name,
                                              const std::span<const std::byte> body) {
    MDB_val key = value_of(name);
    MDB_val data{body.size() + crc_size, nullptr};
    const int status = mdb_put(target.txn, target.meta_dbi, &key, &data, MDB_RESERVE);
    if (status != MDB_SUCCESS) [[unlikely]] {
        return std::unexpected(to_error_code(status));
    }
    const auto out = writable_bytes_of(data);
    std::ranges::copy(body, out.begin());
    std::ranges::copy(to_little_endian(crc32c::extend(0, body)), out.last(crc_size).begin());
    return {};
}

std::expected<std::optional<std::span<const std::byte>>, std::error_code>
get_meta(MDB_txn* txn, const MDB_dbi meta_dbi, const std::string_view name, const std::size_t body_size) {
    MDB_val key = value_of(name);
    MDB_val data{};
    const int status = mdb_get(txn, meta_dbi, &key, &data);
    if (status == MDB_NOTFOUND) {
        return std::nullopt;
    }
    if (status != MDB_SUCCESS) [[unlikely]] {
        return std::unexpected(to_error_code(status));
    }
    const auto value = bytes_of(data);
    if (value.size() != body_size + crc_size) [[unlikely]] {
        return std::unexpected(make_error_code(storage_errc::malformed_record));
    }
    const auto body = value.first(body_size);
    if (from_little_endian<std::uint32_t>(value.last<crc_size>()) != crc32c::extend(0, body)) [[unlikely]] {
        return std::unexpected(make_error_code(storage_errc::crc_mismatch));
    }
    return body;
}

std::expected<std::uint64_t, std::error_code> apply(const write_target& target, const std::uint64_t log_len,
                                                    const append_entries& op) {
    return put_entries(target, log_len, op.entries);
}

std::expected<std::uint64_t, std::error_code> apply(const write_target& target, const std::uint64_t log_len,
                                                    const append_on_prefix& op) {
    const auto prefix_len = std::min(log_len, op.from_idx);
    const auto deleted = delete_entries(target, prefix_len, log_len);
    if (!deleted) [[unlikely]] {
        return std::unexpected(deleted.error());
    }
    return put_entries(target, prefix_len, op.entries);
}

std::expected<std::uint64_t, std::error_code> apply(const write_target& target, const std::uint64_t log_len,
                                                    const set_promise& op) {
    return put_meta(target, promise_key, encode_ballot(op.promise)).transform([log_len] { return log_len; });
}

std::expected<std::uint64_t, std::error_code> apply(const write_target& target, const std::uint64_t log_len,
                                                    const set_decided_index& op) {
    return put_meta(target, decided_idx_key, to_little_endian(op.decided_idx)).transform([log_len] {
        return log_len;
    });
}

std::expected<std::uint64_t, std::error_code> apply(const write_target& target, const std::uint64_t log_len,
                                                    const set_accepted_round& op) {
    return put_meta(target, accepted_round_key, encode_ballot(op.accepted_round)).transform([log_len] {
        return log_len;
    });
}

std::expected<ballot, std::error_code> read_ballot(MDB_env* env, const MDB_dbi meta_dbi, const std::string_view name) {
    const auto txn = begin_transaction(env, MDB_RDONLY);
    if (!txn) [[unlikely]] {
        return std::unexpected(txn.error());
    }
    const auto body = get_meta(txn->get(), meta_dbi, name, ballot_size);
    if (!body) [[unlikely]] {
        return std::unexpected(body.error());
    }
    if (!body->has_value()) {
        return ballot{};
    }
    return decode_ballot(body->value().first<ballot_size>());
}

}

const std::error_category& storage_category() noexcept {
    static const storage_error_category category;
    return category;
}

std::error_code make_error_code(const storage_errc code) noexcept {
    return {static_cast<int>(code), storage_category()};
}

const std::error_category& lmdb_category() noexcept {
    static const lmdb_error_category category;
    return category;
}

std::string read_failure::message() const {
    if (code == storage_errc::crc_mismatch) {
        return std::format("The CRC32C of the log entry at index {} does not agree: stored 0x{:08x}, computed 0x{:08x}",
                           index, stored_crc, computed_crc);
    }
    return std::format("The read of the log entry at index {} failed: {}", index, code.message());
}

void persistent_storage::env_close::operator()(MDB_env* env) const noexcept {
    mdb_env_close(env);
}

persistent_storage::persistent_storage(std::unique_ptr<MDB_env, env_close> env, const unsigned int log_dbi,
                                       const unsigned int meta_dbi) noexcept
    : env_{std::move(env)}, log_dbi_{log_dbi}, meta_dbi_{meta_dbi} {}

void persistent_storage::throw_if_moved_from() const {
    if (!env_) [[unlikely]] {
        throw std::logic_error("The persistent_storage was moved from and has no LMDB environment");
    }
}

std::expected<persistent_storage, std::error_code>
persistent_storage::open(const std::filesystem::path& directory, const std::size_t map_size,
                         const uid_t effective_user_id) {
    if (effective_user_id == root_user_id) [[unlikely]] {
        return std::unexpected(std::make_error_code(std::errc::operation_not_permitted));
    }
    MDB_env* raw_env = nullptr;
    const int created = mdb_env_create(&raw_env);
    if (created != MDB_SUCCESS) [[unlikely]] {
        return std::unexpected(to_error_code(created));
    }
    std::unique_ptr<MDB_env, env_close> env{raw_env};
    const int maxdbs_set = mdb_env_set_maxdbs(env.get(), named_database_count);
    if (maxdbs_set != MDB_SUCCESS) [[unlikely]] {
        return std::unexpected(to_error_code(maxdbs_set));
    }
    const int mapsize_set = mdb_env_set_mapsize(env.get(), map_size);
    if (mapsize_set != MDB_SUCCESS) [[unlikely]] {
        return std::unexpected(to_error_code(mapsize_set));
    }
    const int opened = mdb_env_open(env.get(), directory.c_str(), 0, file_mode);
    if (opened != MDB_SUCCESS) [[unlikely]] {
        return std::unexpected(to_error_code(opened));
    }
    auto txn = begin_transaction(env.get(), 0);
    if (!txn) [[unlikely]] {
        return std::unexpected(txn.error());
    }
    MDB_dbi log_dbi = 0;
    const int log_opened = mdb_dbi_open(txn->get(), log_database_name, MDB_CREATE, &log_dbi);
    if (log_opened != MDB_SUCCESS) [[unlikely]] {
        return std::unexpected(to_error_code(log_opened));
    }
    MDB_dbi meta_dbi = 0;
    const int meta_opened = mdb_dbi_open(txn->get(), meta_database_name, MDB_CREATE, &meta_dbi);
    if (meta_opened != MDB_SUCCESS) [[unlikely]] {
        return std::unexpected(to_error_code(meta_opened));
    }
    const auto committed = commit_or_end_process(std::move(*txn));
    if (!committed) [[unlikely]] {
        return std::unexpected(committed.error());
    }
    return persistent_storage{std::move(env), log_dbi, meta_dbi};
}

std::expected<void, std::error_code> persistent_storage::write_atomically(const std::span<const storage_op> ops) {
    throw_if_moved_from();
    auto txn = begin_transaction(env_.get(), 0);
    if (!txn) [[unlikely]] {
        return std::unexpected(txn.error());
    }
    const write_target target{txn->get(), log_dbi_, meta_dbi_};
    auto log_len = read_log_len(target.txn, log_dbi_);
    for (const auto& op : ops) {
        if (!log_len) [[unlikely]] {
            return std::unexpected(log_len.error());
        }
        log_len = std::visit([&target, len = *log_len](const auto& o) { return apply(target, len, o); }, op);
    }
    if (!log_len) [[unlikely]] {
        return std::unexpected(log_len.error());
    }
    return commit_or_end_process(std::move(*txn));
}

std::expected<std::vector<entry>, read_failure> persistent_storage::read_entries(const std::uint64_t from,
                                                                                 const std::uint64_t to) const {
    throw_if_moved_from();
    throw_if_from_after_to(from, to);
    const auto txn = begin_transaction(env_.get(), MDB_RDONLY);
    if (!txn) [[unlikely]] {
        return std::unexpected(read_failure{txn.error(), from, 0, 0});
    }
    MDB_cursor* raw_cursor = nullptr;
    const int cursor_opened = mdb_cursor_open(txn->get(), log_dbi_, &raw_cursor);
    if (cursor_opened != MDB_SUCCESS) [[unlikely]] {
        return std::unexpected(read_failure{to_error_code(cursor_opened), from, 0, 0});
    }
    const cursor c{raw_cursor};
    std::vector<entry> entries;
    const auto from_key = to_big_endian(from);
    MDB_val key = value_of(from_key);
    MDB_val data{};
    for (int status = mdb_cursor_get(c.get(), &key, &data, MDB_SET_RANGE); status != MDB_NOTFOUND;
         status = mdb_cursor_get(c.get(), &key, &data, MDB_NEXT)) {
        if (status != MDB_SUCCESS) [[unlikely]] {
            return std::unexpected(read_failure{to_error_code(status), from + entries.size(), 0, 0});
        }
        const auto key_bytes = bytes_of(key);
        if (key_bytes.size() != index_size) [[unlikely]] {
            return std::unexpected(read_failure{storage_errc::malformed_record, from + entries.size(), 0, 0});
        }
        const auto index = from_big_endian<std::uint64_t>(key_bytes.first<index_size>());
        if (index >= to) {
            break;
        }
        auto decoded = decode_entry(index, bytes_of(data));
        if (!decoded) [[unlikely]] {
            return std::unexpected(decoded.error());
        }
        entries.push_back(std::move(*decoded));
    }
    return entries;
}

std::expected<std::vector<entry>, std::error_code> persistent_storage::get_entries(const std::uint64_t from,
                                                                                   const std::uint64_t to) const {
    return read_entries(from, to).transform_error([](const read_failure& failure) { return failure.code; });
}

std::expected<std::uint64_t, std::error_code> persistent_storage::get_log_len() const {
    throw_if_moved_from();
    const auto txn = begin_transaction(env_.get(), MDB_RDONLY);
    if (!txn) [[unlikely]] {
        return std::unexpected(txn.error());
    }
    return read_log_len(txn->get(), log_dbi_);
}

std::expected<ballot, std::error_code> persistent_storage::get_promise() const {
    throw_if_moved_from();
    return read_ballot(env_.get(), meta_dbi_, promise_key);
}

std::expected<std::uint64_t, std::error_code> persistent_storage::get_decided_idx() const {
    throw_if_moved_from();
    const auto txn = begin_transaction(env_.get(), MDB_RDONLY);
    if (!txn) [[unlikely]] {
        return std::unexpected(txn.error());
    }
    const auto body = get_meta(txn->get(), meta_dbi_, decided_idx_key, sizeof(std::uint64_t));
    if (!body) [[unlikely]] {
        return std::unexpected(body.error());
    }
    if (!body->has_value()) {
        return 0;
    }
    return from_little_endian<std::uint64_t>(body->value().first<sizeof(std::uint64_t)>());
}

std::expected<ballot, std::error_code> persistent_storage::get_accepted_round() const {
    throw_if_moved_from();
    return read_ballot(env_.get(), meta_dbi_, accepted_round_key);
}

}
