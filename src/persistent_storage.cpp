#include "konsens/persistent_storage.hpp"

#include <algorithm>
#include <array>
#include <exception>
#include <format>
#include <iterator>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>

#include <crc32c/crc32c.h>
#include <lmdb.h>

#include "byte_order.hpp"
#include "konsens/entry_range.hpp"
#include "lmdb_category.hpp"
#include "lmdb_checksum.hpp"

namespace konsens {

namespace {

constexpr unsigned int named_database_count = 2;
constexpr mdb_mode_t file_mode = 0600;
constexpr uid_t root_user_id = 0;
constexpr unsigned int environment_flags = MDB_NOTLS | MDB_REMAP_CHUNKS;
constexpr const char* log_database_name = "log";
constexpr const char* meta_database_name = "meta";
constexpr std::string_view meta_key = "meta";
constexpr std::string_view format_key = "format";
constexpr std::uint32_t format_version = 1;
constexpr std::size_t node_storage_map_size = std::size_t{1} << 20;
constexpr unsigned int node_database_count = 1;
constexpr const char* node_database_name = "node";
constexpr std::string_view repair_key = "repair";
constexpr std::size_t repair_record_size = sizeof(repair_record::count) + sizeof(std::uint8_t);

constexpr std::size_t index_size = sizeof(std::uint64_t);
constexpr std::size_t kind_size = sizeof(entry_kind);
constexpr std::size_t crc_size = sizeof(std::uint32_t);
constexpr std::size_t state_size = sizeof(entry_state);
constexpr std::size_t entry_crc_offset = 0;
constexpr std::size_t chain_crc_offset = entry_crc_offset + crc_size;
constexpr std::size_t state_offset = chain_crc_offset + crc_size;
constexpr std::size_t state_crc_offset = state_offset + state_size;
constexpr std::size_t record_tail_size = state_crc_offset + crc_size;

constexpr std::size_t config_id_offset = 0;
constexpr std::size_t n_offset = config_id_offset + sizeof(ballot::config_id);
constexpr std::size_t priority_offset = n_offset + sizeof(ballot::n);
constexpr std::size_t pid_offset = priority_offset + sizeof(ballot::priority);
constexpr std::size_t ballot_size = pid_offset + sizeof(ballot::pid);

constexpr std::size_t promise_offset = 0;
constexpr std::size_t accepted_round_offset = promise_offset + ballot_size;
constexpr std::size_t decided_idx_offset = accepted_round_offset + ballot_size;
constexpr std::size_t log_len_offset = decided_idx_offset + sizeof(meta_record::decided_idx);
constexpr std::size_t last_chain_crc_offset = log_len_offset + sizeof(meta_record::log_len);
constexpr std::size_t meta_record_size = last_chain_crc_offset + sizeof(meta_record::last_chain_crc);

class storage_error_category final : public std::error_category {
public:
    const char* name() const noexcept override { return "konsens.storage"; }

    std::string message(const int code) const override
    {
        switch (static_cast<storage_errc>(code)) {
        case storage_errc::crc_mismatch:
            return "The CRC32C of a record does not agree with its data";
        case storage_errc::malformed_record:
            return "A record has a length or a value that the format does not allow";
        case storage_errc::missing_entry:
            return "An index of the log has no entry";
        case storage_errc::missing_meta_record:
            return "The storage has no meta record";
        case storage_errc::unsupported_format:
            return "The storage has a format that this version of the library does not read";
        }
        return "Unknown storage error";
    }
};

class lmdb_error_category final : public std::error_category {
public:
    const char* name() const noexcept override { return "lmdb"; }

    std::string message(const int code) const override { return mdb_strerror(code); }
};

struct env_close {
    void operator()(MDB_env* env) const noexcept { mdb_env_close(env); }
};

struct transaction_abort {
    void operator()(MDB_txn* txn) const noexcept { mdb_txn_abort(txn); }
};

using mdb_txn = std::unique_ptr<MDB_txn, transaction_abort>;

struct cursor_close {
    void operator()(MDB_cursor* cursor) const noexcept { mdb_cursor_close(cursor); }
};

using cursor = std::unique_ptr<MDB_cursor, cursor_close>;

struct databases {
    MDB_txn* txn;
    MDB_dbi log_dbi;
    MDB_dbi meta_dbi;
};

std::error_code to_error_code(const int lmdb_status) noexcept
{
    if (lmdb_status == MDB_MAP_FULL) {
        return std::make_error_code(std::errc::no_space_on_device);
    }
    if (lmdb_status > 0) {
        return {lmdb_status, std::generic_category()};
    }
    return {lmdb_status, lmdb_category()};
}

MDB_val value_of(const std::span<const std::byte> bytes) noexcept
{
    return MDB_val{bytes.size(), const_cast<std::byte*>(bytes.data())};
}

MDB_val value_of(const std::string_view text) noexcept
{
    return MDB_val{text.size(), const_cast<char*>(text.data())};
}

std::span<const std::byte> as_bytes(const MDB_val& value) noexcept
{
    return {static_cast<const std::byte*>(value.mv_data), value.mv_size};
}

std::span<std::byte> as_writable_bytes(const MDB_val& value) noexcept
{
    return {static_cast<std::byte*>(value.mv_data), value.mv_size};
}

std::uint32_t crc_of(const std::span<const std::byte> bytes) noexcept
{
    return crc32c::Crc32c(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
}

std::uint32_t compute_state_crc(const std::uint64_t index, const entry_state state) noexcept
{
    std::array<std::byte, index_size + state_size> bytes{};
    std::ranges::copy(to_little_endian(index), bytes.begin());
    bytes.back() = std::byte{std::to_underlying(state)};
    return crc_of(bytes);
}

std::expected<mdb_txn, std::error_code> begin_transaction(MDB_env* env, const unsigned int flags)
{
    MDB_txn* txn = nullptr;
    const int status = mdb_txn_begin(env, nullptr, flags, &txn);
    if (status != MDB_SUCCESS) [[unlikely]] {
        return std::unexpected(to_error_code(status));
    }
    return mdb_txn{txn};
}

std::expected<void, std::error_code> commit_or_throw(mdb_txn txn)
{
    const int status = mdb_txn_commit(txn.release());
    if (status == MDB_MAP_FULL) [[unlikely]] {
        return std::unexpected(to_error_code(status));
    }
    if (status != MDB_SUCCESS) [[unlikely]] {
        throw sync_error(to_error_code(status), "konsens: mdb_txn_commit failed");
    }
    return {};
}

std::array<std::byte, ballot_size> encode_ballot(const ballot& b) noexcept
{
    std::array<std::byte, ballot_size> out{};
    auto next = std::ranges::copy(to_little_endian(b.config_id), out.begin()).out;
    next = std::ranges::copy(to_little_endian(b.n), next).out;
    next = std::ranges::copy(to_little_endian(b.priority), next).out;
    std::ranges::copy(to_little_endian(b.pid), next);
    return out;
}

ballot decode_ballot(const std::span<const std::byte, ballot_size> bytes) noexcept
{
    return ballot{
        from_little_endian<std::uint32_t>(bytes.subspan<config_id_offset, sizeof(ballot::config_id)>()),
        from_little_endian<std::uint32_t>(bytes.subspan<n_offset, sizeof(ballot::n)>()),
        from_little_endian<std::uint32_t>(bytes.subspan<priority_offset, sizeof(ballot::priority)>()),
        from_little_endian<std::uint64_t>(bytes.subspan<pid_offset, sizeof(ballot::pid)>()),
    };
}

std::array<std::byte, meta_record_size> encode_meta(const meta_record& m) noexcept
{
    std::array<std::byte, meta_record_size> out{};
    auto next = std::ranges::copy(encode_ballot(m.promise), out.begin()).out;
    next = std::ranges::copy(encode_ballot(m.accepted_round), next).out;
    next = std::ranges::copy(to_little_endian(m.decided_idx), next).out;
    next = std::ranges::copy(to_little_endian(m.log_len), next).out;
    std::ranges::copy(to_little_endian(m.last_chain_crc), next);
    return out;
}

meta_record decode_meta(const std::span<const std::byte, meta_record_size> bytes) noexcept
{
    return meta_record{
        decode_ballot(bytes.subspan<promise_offset, ballot_size>()),
        decode_ballot(bytes.subspan<accepted_round_offset, ballot_size>()),
        from_little_endian<std::uint64_t>(bytes.subspan<decided_idx_offset, sizeof(meta_record::decided_idx)>()),
        from_little_endian<std::uint64_t>(bytes.subspan<log_len_offset, sizeof(meta_record::log_len)>()),
        from_little_endian<std::uint32_t>(bytes.subspan<last_chain_crc_offset, sizeof(meta_record::last_chain_crc)>()),
    };
}

struct stored_record {
    std::span<const std::byte> encoded;
    std::uint32_t entry_crc;
    std::uint32_t chain_crc;
    entry_state state;
};

std::expected<stored_record, read_failure> check_record(const std::uint64_t index,
                                                        const std::span<const std::byte> value)
{
    if (value.size() < kind_size + record_tail_size) [[unlikely]] {
        return std::unexpected(read_failure{storage_errc::malformed_record, index, 0, 0});
    }
    const auto encoded = value.first(value.size() - record_tail_size);
    const auto tail = value.last<record_tail_size>();
    const auto stored = from_little_endian<std::uint32_t>(tail.subspan<entry_crc_offset, crc_size>());
    const auto computed = compute_entry_crc(index, encoded);
    if (stored != computed) [[unlikely]] {
        return std::unexpected(read_failure{storage_errc::crc_mismatch, index, stored, computed});
    }
    const auto chain = from_little_endian<std::uint32_t>(tail.subspan<chain_crc_offset, crc_size>());
    const auto state_byte = std::to_integer<std::uint8_t>(tail[state_offset]);
    if (state_byte > std::to_underlying(entry_state::committed)) [[unlikely]] {
        return std::unexpected(read_failure{storage_errc::malformed_record, index, 0, 0});
    }
    const auto state = static_cast<entry_state>(state_byte);
    const auto stored_state_crc = from_little_endian<std::uint32_t>(tail.subspan<state_crc_offset, crc_size>());
    const auto computed_state_crc = compute_state_crc(index, state);
    if (stored_state_crc != computed_state_crc) [[unlikely]] {
        return std::unexpected(read_failure{storage_errc::crc_mismatch, index, stored_state_crc, computed_state_crc});
    }
    return stored_record{encoded, stored, chain, state};
}

void write_state(const std::uint64_t index, const entry_state state, const std::span<std::byte> record) noexcept
{
    const auto tail = record.last<record_tail_size>();
    tail[state_offset] = std::byte{std::to_underlying(state)};
    std::ranges::copy(to_little_endian(compute_state_crc(index, state)), tail.subspan<state_crc_offset>().begin());
}

std::uint32_t encode_record(const std::uint64_t index, const entry& e, const std::uint32_t previous_chain_crc,
                            const entry_state state, const std::span<std::byte> out) noexcept
{
    const auto encoded = out.first(out.size() - record_tail_size);
    encode_entry(e, encoded);
    const auto chain = compute_chain_crc(previous_chain_crc, index, encoded);
    const auto tail = out.last<record_tail_size>();
    std::ranges::copy(to_little_endian(compute_entry_crc(index, encoded)), tail.subspan<entry_crc_offset>().begin());
    std::ranges::copy(to_little_endian(chain), tail.subspan<chain_crc_offset>().begin());
    write_state(index, state, out);
    return chain;
}

std::uint64_t index_of(const MDB_val& key) noexcept
{
    return from_big_endian<std::uint64_t>(as_bytes(key).first<index_size>());
}

std::expected<std::uint64_t, std::error_code> read_entry_count(MDB_txn* txn, const MDB_dbi dbi)
{
    MDB_stat stat{};
    const int status = mdb_stat(txn, dbi, &stat);
    if (status != MDB_SUCCESS) [[unlikely]] {
        return std::unexpected(to_error_code(status));
    }
    return stat.ms_entries;
}

std::expected<std::optional<std::span<const std::byte>>, std::error_code>
get_checked(MDB_txn* txn, const MDB_dbi dbi, const std::string_view name, const std::size_t body_size)
{
    MDB_val key = value_of(name);
    MDB_val data{};
    const int status = mdb_get(txn, dbi, &key, &data);
    if (status == MDB_NOTFOUND) {
        return std::nullopt;
    }
    if (status != MDB_SUCCESS) [[unlikely]] {
        return std::unexpected(to_error_code(status));
    }
    const auto value = as_bytes(data);
    if (value.size() != body_size + crc_size) [[unlikely]] {
        return std::unexpected(make_error_code(storage_errc::malformed_record));
    }
    const auto body = value.first(body_size);
    if (from_little_endian<std::uint32_t>(value.last<crc_size>()) != crc_of(body)) [[unlikely]] {
        return std::unexpected(make_error_code(storage_errc::crc_mismatch));
    }
    return body;
}

std::expected<void, std::error_code> put_checked(MDB_txn* txn, const MDB_dbi dbi, const std::string_view name,
                                                 const std::span<const std::byte> body)
{
    MDB_val key = value_of(name);
    MDB_val data{body.size() + crc_size, nullptr};
    const int status = mdb_put(txn, dbi, &key, &data, MDB_RESERVE);
    if (status != MDB_SUCCESS) [[unlikely]] {
        return std::unexpected(to_error_code(status));
    }
    const auto out = as_writable_bytes(data);
    std::ranges::copy(body, out.begin());
    std::ranges::copy(to_little_endian(crc_of(body)), out.last(crc_size).begin());
    return {};
}

std::expected<cursor, std::error_code> open_cursor(MDB_txn* txn, const MDB_dbi dbi)
{
    MDB_cursor* raw_cursor = nullptr;
    const int opened = mdb_cursor_open(txn, dbi, &raw_cursor);
    if (opened != MDB_SUCCESS) [[unlikely]] {
        return std::unexpected(to_error_code(opened));
    }
    return cursor{raw_cursor};
}

std::expected<meta_record, std::error_code> read_meta(const databases& d)
{
    const auto body = get_checked(d.txn, d.meta_dbi, meta_key, meta_record_size);
    if (!body) [[unlikely]] {
        return std::unexpected(body.error());
    }
    if (!body->has_value()) [[unlikely]] {
        return std::unexpected(make_error_code(storage_errc::missing_meta_record));
    }
    return decode_meta(body->value().first<meta_record_size>());
}

std::expected<void, std::error_code> write_meta(const databases& d, const meta_record& m)
{
    return put_checked(d.txn, d.meta_dbi, meta_key, encode_meta(m));
}

std::expected<std::vector<entry>, read_failure> read_entries_in(const databases& d, const std::uint64_t from,
                                                                const std::uint64_t to)
{
    throw_if_from_after_to(from, to);
    const auto c = open_cursor(d.txn, d.log_dbi);
    if (!c) [[unlikely]] {
        return std::unexpected(read_failure{c.error(), from, 0, 0});
    }
    std::vector<entry> entries;
    const auto from_key = to_big_endian(from);
    MDB_val key = value_of(from_key);
    MDB_val data{};
    for (int status = mdb_cursor_get(c->get(), &key, &data, MDB_SET_RANGE); status != MDB_NOTFOUND;
         status = mdb_cursor_get(c->get(), &key, &data, MDB_NEXT)) {
        const auto expected_index = from + entries.size();
        if (status != MDB_SUCCESS) [[unlikely]] {
            return std::unexpected(read_failure{to_error_code(status), expected_index, 0, 0});
        }
        if (key.mv_size != index_size) [[unlikely]] {
            return std::unexpected(read_failure{storage_errc::malformed_record, expected_index, 0, 0});
        }
        const auto index = index_of(key);
        if (index >= to) {
            if (expected_index != to) [[unlikely]] {
                return std::unexpected(read_failure{storage_errc::missing_entry, expected_index, 0, 0});
            }
            break;
        }
        if (index != expected_index) [[unlikely]] {
            return std::unexpected(read_failure{storage_errc::missing_entry, expected_index, 0, 0});
        }
        const auto record = check_record(index, as_bytes(data));
        if (!record) [[unlikely]] {
            return std::unexpected(record.error());
        }
        auto decoded = decode_entry(record->encoded);
        if (!decoded) [[unlikely]] {
            return std::unexpected(read_failure{storage_errc::malformed_record, index, 0, 0});
        }
        entries.push_back(std::move(*decoded));
    }
    return entries;
}

std::expected<std::optional<last_entry>, std::error_code> read_last_entry(const databases& d)
{
    const auto c = open_cursor(d.txn, d.log_dbi);
    if (!c) [[unlikely]] {
        return std::unexpected(c.error());
    }
    MDB_val key{};
    MDB_val data{};
    const int status = mdb_cursor_get(c->get(), &key, &data, MDB_LAST);
    if (status == MDB_NOTFOUND) {
        return std::nullopt;
    }
    if (status != MDB_SUCCESS) [[unlikely]] {
        return std::unexpected(to_error_code(status));
    }
    if (key.mv_size != index_size) [[unlikely]] {
        return std::unexpected(make_error_code(storage_errc::malformed_record));
    }
    const auto index = index_of(key);
    const auto count = read_entry_count(d.txn, d.log_dbi);
    if (!count) [[unlikely]] {
        return std::unexpected(count.error());
    }
    if (*count != index + 1) [[unlikely]] {
        return std::unexpected(make_error_code(storage_errc::missing_entry));
    }
    const auto record = check_record(index, as_bytes(data));
    if (!record) [[unlikely]] {
        return std::unexpected(record.error().code);
    }
    return last_entry{index, record->entry_crc, record->chain_crc};
}

std::expected<std::optional<stored_record>, std::error_code> read_record(const databases& d, const std::uint64_t index)
{
    const auto key_bytes = to_big_endian(index);
    MDB_val key = value_of(key_bytes);
    MDB_val data{};
    const int status = mdb_get(d.txn, d.log_dbi, &key, &data);
    if (status == MDB_NOTFOUND) {
        return std::nullopt;
    }
    if (status != MDB_SUCCESS) [[unlikely]] {
        return std::unexpected(to_error_code(status));
    }
    const auto record = check_record(index, as_bytes(data));
    if (!record) [[unlikely]] {
        return std::unexpected(record.error().code);
    }
    return *record;
}

std::expected<std::optional<entry_state>, std::error_code> read_entry_state(const databases& d,
                                                                            const std::uint64_t index)
{
    return read_record(d, index).transform([](const std::optional<stored_record>& r) {
        return r.transform([](const stored_record& x) { return x.state; });
    });
}

std::expected<std::optional<std::uint32_t>, std::error_code> read_chain_crc(const databases& d,
                                                                            const std::uint64_t index)
{
    return read_record(d, index).transform([](const std::optional<stored_record>& r) {
        return r.transform([](const stored_record& x) { return x.chain_crc; });
    });
}

std::expected<void, std::error_code> put_entries(const databases& d, meta_record& m,
                                                 const std::span<const entry> entries)
{
    for (const auto& e : entries) {
        const auto index = m.log_len;
        const auto key_bytes = to_big_endian(index);
        MDB_val key = value_of(key_bytes);
        MDB_val data{encoded_size_of(e) + record_tail_size, nullptr};
        const int status = mdb_put(d.txn, d.log_dbi, &key, &data, MDB_APPEND | MDB_RESERVE);
        if (status != MDB_SUCCESS) [[unlikely]] {
            return std::unexpected(to_error_code(status));
        }
        const auto state = index < m.decided_idx ? entry_state::committed : entry_state::in_work;
        m.last_chain_crc = encode_record(index, e, m.last_chain_crc, state, as_writable_bytes(data));
        m.log_len = index + 1;
    }
    return {};
}

std::expected<void, std::error_code> delete_entries_from(const databases& d, const std::uint64_t from)
{
    const auto c = open_cursor(d.txn, d.log_dbi);
    if (!c) [[unlikely]] {
        return std::unexpected(c.error());
    }
    const auto key_bytes = to_big_endian(from);
    MDB_val key = value_of(key_bytes);
    MDB_val data{};
    int status = mdb_cursor_get(c->get(), &key, &data, MDB_SET_RANGE);
    while (status == MDB_SUCCESS) {
        const int deleted = mdb_cursor_del(c->get(), 0);
        if (deleted != MDB_SUCCESS) [[unlikely]] {
            return std::unexpected(to_error_code(deleted));
        }
        key = value_of(key_bytes);
        status = mdb_cursor_get(c->get(), &key, &data, MDB_SET_RANGE);
    }
    if (status != MDB_NOTFOUND) [[unlikely]] {
        return std::unexpected(to_error_code(status));
    }
    return {};
}

std::expected<void, std::error_code> delete_entries(const databases& d, const std::uint64_t from,
                                                    const std::uint64_t to)
{
    for (const auto index : std::views::iota(from, to)) {
        const auto key_bytes = to_big_endian(index);
        MDB_val key = value_of(key_bytes);
        const int status = mdb_del(d.txn, d.log_dbi, &key, nullptr);
        if (status == MDB_NOTFOUND) [[unlikely]] {
            return std::unexpected(make_error_code(storage_errc::missing_entry));
        }
        if (status != MDB_SUCCESS) [[unlikely]] {
            return std::unexpected(to_error_code(status));
        }
    }
    return {};
}

std::expected<std::uint32_t, std::error_code> chain_crc_before(const databases& d, const std::uint64_t index)
{
    if (index == 0) {
        return 0;
    }
    const auto record = read_record(d, index - 1);
    if (!record) [[unlikely]] {
        return std::unexpected(record.error());
    }
    if (!record->has_value()) [[unlikely]] {
        return std::unexpected(make_error_code(storage_errc::missing_entry));
    }
    return record->value().chain_crc;
}

std::expected<void, std::error_code> set_entry_states(const databases& d, std::vector<std::byte>& rewritten,
                                                      const std::uint64_t from, const std::uint64_t to,
                                                      const std::uint64_t decided_idx)
{
    const auto c = open_cursor(d.txn, d.log_dbi);
    if (!c) [[unlikely]] {
        return std::unexpected(c.error());
    }
    for (const auto index : std::views::iota(from, to)) {
        const auto key_bytes = to_big_endian(index);
        MDB_val key = value_of(key_bytes);
        MDB_val data{};
        const int found = mdb_cursor_get(c->get(), &key, &data, MDB_SET_KEY);
        if (found == MDB_NOTFOUND) [[unlikely]] {
            return std::unexpected(make_error_code(storage_errc::missing_entry));
        }
        if (found != MDB_SUCCESS) [[unlikely]] {
            return std::unexpected(to_error_code(found));
        }
        const auto record = check_record(index, as_bytes(data));
        if (!record) [[unlikely]] {
            return std::unexpected(record.error().code);
        }
        const auto state = index < decided_idx ? entry_state::committed : entry_state::in_work;
        if (record->state == state) {
            continue;
        }
        const auto stored = as_bytes(data);
        rewritten.assign(stored.begin(), stored.end());
        write_state(index, state, rewritten);
        MDB_val replaced = value_of(std::span<const std::byte>(rewritten));
        const int put = mdb_cursor_put(c->get(), &key, &replaced, MDB_CURRENT);
        if (put != MDB_SUCCESS) [[unlikely]] {
            return std::unexpected(to_error_code(put));
        }
    }
    return {};
}

void throw_if_no_environment(const void* const env)
{
    if (env == nullptr) [[unlikely]] {
        throw std::logic_error("konsens: the transaction was moved from, committed or aborted");
    }
}

void throw_if_an_entry_has_a_field_of_another_kind(const std::span<const entry> entries)
{
    for (const auto& e : entries) {
        throw_if_entry_has_a_field_of_another_kind(e);
    }
}

std::expected<MDB_dbi, std::error_code> open_database(MDB_txn* txn, const char* name)
{
    MDB_dbi dbi = 0;
    const int opened = mdb_dbi_open(txn, name, MDB_CREATE, &dbi);
    if (opened != MDB_SUCCESS) [[unlikely]] {
        return std::unexpected(to_error_code(opened));
    }
    return dbi;
}

std::expected<void, std::error_code> check_or_write_format(const databases& d)
{
    const auto format = get_checked(d.txn, d.meta_dbi, format_key, sizeof format_version);
    if (!format) [[unlikely]] {
        return std::unexpected(format.error());
    }
    if (format->has_value()) {
        if (from_little_endian<std::uint32_t>(format->value().first<sizeof format_version>()) != format_version)
            [[unlikely]] {
            return std::unexpected(make_error_code(storage_errc::unsupported_format));
        }
        return {};
    }
    const auto log_count = read_entry_count(d.txn, d.log_dbi);
    if (!log_count) [[unlikely]] {
        return std::unexpected(log_count.error());
    }
    const auto meta_count = read_entry_count(d.txn, d.meta_dbi);
    if (!meta_count) [[unlikely]] {
        return std::unexpected(meta_count.error());
    }
    if (*log_count != 0 || *meta_count != 0) [[unlikely]] {
        return std::unexpected(make_error_code(storage_errc::unsupported_format));
    }
    if (auto put = put_checked(d.txn, d.meta_dbi, format_key, to_little_endian(format_version)); !put) [[unlikely]] {
        return put;
    }
    return write_meta(d, meta_record{});
}

}

struct persistent_storage::environment {
    std::unique_ptr<MDB_env, env_close> handle;
    MDB_dbi log_dbi;
    MDB_dbi meta_dbi;
    mdb_txn writer{};
    mdb_txn reader{};
    bool reader_in_use{};
    std::vector<std::byte> rewritten{};

    databases of_writer() const noexcept { return {writer.get(), log_dbi, meta_dbi}; }
    databases of_reader() const noexcept { return {reader.get(), log_dbi, meta_dbi}; }
};

const std::error_category& storage_category() noexcept
{
    static const storage_error_category category;
    return category;
}

std::error_code make_error_code(const storage_errc code) noexcept
{
    return {static_cast<int>(code), storage_category()};
}

const std::error_category& lmdb_category() noexcept
{
    static const lmdb_error_category category;
    return category;
}

std::string read_failure::message() const
{
    if (code == storage_errc::crc_mismatch) {
        return std::format("The CRC32C of the log entry at index {} does not agree: stored 0x{:08x}, computed 0x{:08x}",
                           index, stored_crc, computed_crc);
    }
    return std::format("The read of the log entry at index {} failed: {}", index, code.message());
}

persistent_storage::read_only_transaction::read_only_transaction(environment* const env) noexcept : env_{env}
{
}

persistent_storage::read_only_transaction::read_only_transaction(read_only_transaction&& other) noexcept
    : env_{std::exchange(other.env_, nullptr)}
{
}

persistent_storage::read_only_transaction&
persistent_storage::read_only_transaction::operator=(read_only_transaction&& other) noexcept
{
    if (this != &other) {
        if (env_ != nullptr) {
            mdb_txn_reset(env_->reader.get());
            env_->reader_in_use = false;
        }
        env_ = std::exchange(other.env_, nullptr);
    }
    return *this;
}

persistent_storage::read_only_transaction::~read_only_transaction()
{
    if (env_ != nullptr) {
        mdb_txn_reset(env_->reader.get());
        env_->reader_in_use = false;
    }
}

std::expected<meta_record, std::error_code> persistent_storage::read_only_transaction::get_meta() const
{
    throw_if_no_environment(env_);
    return read_meta(env_->of_reader());
}

std::expected<std::vector<entry>, read_failure>
persistent_storage::read_only_transaction::read_entries(const std::uint64_t from, const std::uint64_t to) const
{
    throw_if_no_environment(env_);
    return read_entries_in(env_->of_reader(), from, to);
}

std::expected<std::vector<entry>, std::error_code>
persistent_storage::read_only_transaction::get_entries(const std::uint64_t from, const std::uint64_t to) const
{
    return read_entries(from, to).transform_error([](const read_failure& failure) { return failure.code; });
}

std::expected<std::optional<last_entry>, std::error_code>
persistent_storage::read_only_transaction::get_last_entry() const
{
    throw_if_no_environment(env_);
    return read_last_entry(env_->of_reader());
}

std::expected<std::optional<entry_state>, std::error_code>
persistent_storage::read_only_transaction::get_entry_state(const std::uint64_t index) const
{
    throw_if_no_environment(env_);
    return read_entry_state(env_->of_reader(), index);
}

std::expected<std::optional<std::uint32_t>, std::error_code>
persistent_storage::read_only_transaction::get_chain_crc(const std::uint64_t index) const
{
    throw_if_no_environment(env_);
    return read_chain_crc(env_->of_reader(), index);
}

persistent_storage::transaction::transaction(environment* const env) noexcept : env_{env}
{
}

persistent_storage::transaction::transaction(transaction&& other) noexcept : env_{std::exchange(other.env_, nullptr)}
{
}

persistent_storage::transaction& persistent_storage::transaction::operator=(transaction&& other) noexcept
{
    if (this != &other) {
        if (env_ != nullptr) {
            env_->writer.reset();
        }
        env_ = std::exchange(other.env_, nullptr);
    }
    return *this;
}

persistent_storage::transaction::~transaction()
{
    if (env_ != nullptr) {
        env_->writer.reset();
    }
}

std::expected<meta_record, std::error_code> persistent_storage::transaction::get_meta() const
{
    throw_if_no_environment(env_);
    return read_meta(env_->of_writer());
}

std::expected<std::vector<entry>, read_failure>
persistent_storage::transaction::read_entries(const std::uint64_t from, const std::uint64_t to) const
{
    throw_if_no_environment(env_);
    return read_entries_in(env_->of_writer(), from, to);
}

std::expected<std::vector<entry>, std::error_code>
persistent_storage::transaction::get_entries(const std::uint64_t from, const std::uint64_t to) const
{
    return read_entries(from, to).transform_error([](const read_failure& failure) { return failure.code; });
}

std::expected<std::optional<last_entry>, std::error_code> persistent_storage::transaction::get_last_entry() const
{
    throw_if_no_environment(env_);
    return read_last_entry(env_->of_writer());
}

std::expected<std::optional<entry_state>, std::error_code>
persistent_storage::transaction::get_entry_state(const std::uint64_t index) const
{
    throw_if_no_environment(env_);
    return read_entry_state(env_->of_writer(), index);
}

std::expected<std::optional<std::uint32_t>, std::error_code>
persistent_storage::transaction::get_chain_crc(const std::uint64_t index) const
{
    throw_if_no_environment(env_);
    return read_chain_crc(env_->of_writer(), index);
}

std::expected<void, std::error_code>
persistent_storage::transaction::append_entries(const std::span<const entry> entries)
{
    throw_if_no_environment(env_);
    throw_if_an_entry_has_a_field_of_another_kind(entries);
    const auto d = env_->of_writer();
    auto m = read_meta(d);
    if (!m) [[unlikely]] {
        return std::unexpected(m.error());
    }
    if (auto put = put_entries(d, *m, entries); !put) [[unlikely]] {
        return put;
    }
    return write_meta(d, *m);
}

std::expected<void, std::error_code>
persistent_storage::transaction::append_on_prefix(const std::uint64_t from_idx, const std::span<const entry> entries)
{
    throw_if_no_environment(env_);
    throw_if_an_entry_has_a_field_of_another_kind(entries);
    const auto d = env_->of_writer();
    auto m = read_meta(d);
    if (!m) [[unlikely]] {
        return std::unexpected(m.error());
    }
    const auto prefix_len = std::min(m->log_len, from_idx);
    if (prefix_len < m->log_len) {
        if (auto deleted = delete_entries(d, prefix_len, m->log_len); !deleted) [[unlikely]] {
            return deleted;
        }
        const auto chain = chain_crc_before(d, prefix_len);
        if (!chain) [[unlikely]] {
            return std::unexpected(chain.error());
        }
        m->log_len = prefix_len;
        m->last_chain_crc = *chain;
    }
    if (auto put = put_entries(d, *m, entries); !put) [[unlikely]] {
        return put;
    }
    return write_meta(d, *m);
}

std::expected<void, std::error_code> persistent_storage::transaction::set_promise(const ballot promise)
{
    throw_if_no_environment(env_);
    const auto d = env_->of_writer();
    auto m = read_meta(d);
    if (!m) [[unlikely]] {
        return std::unexpected(m.error());
    }
    m->promise = promise;
    return write_meta(d, *m);
}

std::expected<void, std::error_code> persistent_storage::transaction::set_accepted_round(const ballot accepted_round)
{
    throw_if_no_environment(env_);
    const auto d = env_->of_writer();
    auto m = read_meta(d);
    if (!m) [[unlikely]] {
        return std::unexpected(m.error());
    }
    m->accepted_round = accepted_round;
    return write_meta(d, *m);
}

std::expected<void, std::error_code> persistent_storage::transaction::set_decided_index(const std::uint64_t decided_idx)
{
    throw_if_no_environment(env_);
    const auto d = env_->of_writer();
    auto m = read_meta(d);
    if (!m) [[unlikely]] {
        return std::unexpected(m.error());
    }
    if (decided_idx > m->log_len) [[unlikely]] {
        return std::unexpected(make_error_code(storage_errc::missing_entry));
    }
    if (decided_idx > m->decided_idx) {
        if (auto c = set_entry_states(d, env_->rewritten, m->decided_idx, decided_idx, decided_idx); !c) [[unlikely]] {
            return c;
        }
    }
    m->decided_idx = decided_idx;
    return write_meta(d, *m);
}

std::expected<void, std::error_code>
persistent_storage::transaction::write_repaired_log(const std::uint64_t from_idx, const std::span<const entry> entries,
                                                    const ballot promise, const ballot accepted_round,
                                                    const std::uint64_t decided_idx)
{
    throw_if_no_environment(env_);
    throw_if_an_entry_has_a_field_of_another_kind(entries);
    const auto d = env_->of_writer();
    if (auto deleted = delete_entries_from(d, from_idx); !deleted) [[unlikely]] {
        return deleted;
    }
    if (auto states = set_entry_states(d, env_->rewritten, 0, from_idx, decided_idx); !states) [[unlikely]] {
        return states;
    }
    const auto chain = chain_crc_before(d, from_idx);
    if (!chain) [[unlikely]] {
        return std::unexpected(chain.error());
    }
    meta_record m{promise, accepted_round, decided_idx, from_idx, *chain};
    if (auto put = put_entries(d, m, entries); !put) [[unlikely]] {
        return put;
    }
    return write_meta(d, m);
}

std::expected<void, std::error_code> persistent_storage::transaction::commit() &&
{
    throw_if_no_environment(env_);
    auto txn = std::move(std::exchange(env_, nullptr)->writer);
    return commit_or_throw(std::move(txn));
}

persistent_storage::persistent_storage(persistent_storage&&) noexcept = default;
persistent_storage& persistent_storage::operator=(persistent_storage&&) noexcept = default;

persistent_storage::~persistent_storage()
{
    if (env_ && (env_->writer || env_->reader_in_use)) [[unlikely]] {
        std::terminate();
    }
}

persistent_storage::persistent_storage(std::unique_ptr<environment> env) noexcept : env_{std::move(env)}
{
}

void persistent_storage::throw_if_moved_from() const
{
    if (!env_) [[unlikely]] {
        throw std::logic_error("The persistent_storage was moved from and has no LMDB environment");
    }
}

std::expected<persistent_storage, std::error_code> persistent_storage::open(const std::filesystem::path& directory,
                                                                            const std::size_t map_size,
                                                                            const uid_t effective_user_id)
{
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
    const int checksum_set = mdb_env_set_checksum(env.get(), compute_page_checksum, page_checksum_size);
    if (checksum_set != MDB_SUCCESS) [[unlikely]] {
        return std::unexpected(to_error_code(checksum_set));
    }
    const int opened = mdb_env_open(env.get(), directory.c_str(), environment_flags, file_mode);
    if (opened != MDB_SUCCESS) [[unlikely]] {
        return std::unexpected(to_error_code(opened));
    }
    auto txn = begin_transaction(env.get(), 0);
    if (!txn) [[unlikely]] {
        return std::unexpected(txn.error());
    }
    const auto log_dbi = open_database(txn->get(), log_database_name);
    if (!log_dbi) [[unlikely]] {
        return std::unexpected(log_dbi.error());
    }
    const auto meta_dbi = open_database(txn->get(), meta_database_name);
    if (!meta_dbi) [[unlikely]] {
        return std::unexpected(meta_dbi.error());
    }
    if (auto format = check_or_write_format({txn->get(), *log_dbi, *meta_dbi}); !format) [[unlikely]] {
        return std::unexpected(format.error());
    }
    const auto committed = commit_or_throw(std::move(*txn));
    if (!committed) [[unlikely]] {
        return std::unexpected(committed.error());
    }
    return persistent_storage{std::make_unique<environment>(std::move(env), *log_dbi, *meta_dbi)};
}

std::expected<persistent_storage::transaction, std::error_code> persistent_storage::begin()
{
    throw_if_moved_from();
    if (env_->writer) [[unlikely]] {
        throw std::logic_error("konsens: a write transaction of this storage is open");
    }
    auto txn = begin_transaction(env_->handle.get(), 0);
    if (!txn) [[unlikely]] {
        return std::unexpected(txn.error());
    }
    env_->writer = std::move(*txn);
    return transaction{env_.get()};
}

std::expected<persistent_storage::read_only_transaction, std::error_code> persistent_storage::begin_read_only() const
{
    throw_if_moved_from();
    if (env_->reader_in_use) [[unlikely]] {
        throw std::logic_error("konsens: a read-only transaction of this storage is open");
    }
    if (env_->reader) {
        const int renewed = mdb_txn_renew(env_->reader.get());
        if (renewed != MDB_SUCCESS) [[unlikely]] {
            return std::unexpected(to_error_code(renewed));
        }
    } else {
        auto txn = begin_transaction(env_->handle.get(), MDB_RDONLY);
        if (!txn) [[unlikely]] {
            return std::unexpected(txn.error());
        }
        env_->reader = std::move(*txn);
    }
    env_->reader_in_use = true;
    return read_only_transaction{env_.get()};
}

std::expected<void, std::error_code> persistent_storage::copy(const std::filesystem::path& directory) const
{
    throw_if_moved_from();
    const int copied = mdb_env_copy2(env_->handle.get(), directory.c_str(), 0);
    if (copied != MDB_SUCCESS) [[unlikely]] {
        return std::unexpected(to_error_code(copied));
    }
    return {};
}

struct node_storage::environment {
    std::unique_ptr<MDB_env, env_close> handle;
    MDB_dbi dbi;
};

node_storage::node_storage(std::unique_ptr<environment> env) noexcept : env_{std::move(env)}
{
}

node_storage::node_storage(node_storage&&) noexcept = default;
node_storage& node_storage::operator=(node_storage&&) noexcept = default;
node_storage::~node_storage() = default;

void node_storage::throw_if_moved_from() const
{
    if (!env_) [[unlikely]] {
        throw std::logic_error("The node_storage was moved from and has no LMDB environment");
    }
}

std::expected<node_storage, std::error_code> node_storage::open(const std::filesystem::path& directory,
                                                                const uid_t effective_user_id)
{
    if (effective_user_id == root_user_id) [[unlikely]] {
        return std::unexpected(std::make_error_code(std::errc::operation_not_permitted));
    }
    MDB_env* raw_env = nullptr;
    const int created = mdb_env_create(&raw_env);
    if (created != MDB_SUCCESS) [[unlikely]] {
        return std::unexpected(to_error_code(created));
    }
    std::unique_ptr<MDB_env, env_close> env{raw_env};
    const int maxdbs_set = mdb_env_set_maxdbs(env.get(), node_database_count);
    if (maxdbs_set != MDB_SUCCESS) [[unlikely]] {
        return std::unexpected(to_error_code(maxdbs_set));
    }
    const int mapsize_set = mdb_env_set_mapsize(env.get(), node_storage_map_size);
    if (mapsize_set != MDB_SUCCESS) [[unlikely]] {
        return std::unexpected(to_error_code(mapsize_set));
    }
    const int checksum_set = mdb_env_set_checksum(env.get(), compute_page_checksum, page_checksum_size);
    if (checksum_set != MDB_SUCCESS) [[unlikely]] {
        return std::unexpected(to_error_code(checksum_set));
    }
    const int opened = mdb_env_open(env.get(), directory.c_str(), environment_flags, file_mode);
    if (opened != MDB_SUCCESS) [[unlikely]] {
        return std::unexpected(to_error_code(opened));
    }
    auto txn = begin_transaction(env.get(), 0);
    if (!txn) [[unlikely]] {
        return std::unexpected(txn.error());
    }
    const auto dbi = open_database(txn->get(), node_database_name);
    if (!dbi) [[unlikely]] {
        return std::unexpected(dbi.error());
    }
    const auto committed = commit_or_throw(std::move(*txn));
    if (!committed) [[unlikely]] {
        return std::unexpected(committed.error());
    }
    return node_storage{std::make_unique<environment>(std::move(env), *dbi)};
}

std::expected<repair_record, std::error_code> node_storage::get_repair_record() const
{
    throw_if_moved_from();
    auto txn = begin_transaction(env_->handle.get(), MDB_RDONLY);
    if (!txn) [[unlikely]] {
        return std::unexpected(txn.error());
    }
    const auto body = get_checked(txn->get(), env_->dbi, repair_key, repair_record_size);
    if (!body) [[unlikely]] {
        return std::unexpected(body.error());
    }
    if (!body->has_value()) {
        return repair_record{0, false};
    }
    const auto bytes = body->value();
    const auto open_byte = std::to_integer<std::uint8_t>(bytes.back());
    if (open_byte > 1) [[unlikely]] {
        return std::unexpected(make_error_code(storage_errc::malformed_record));
    }
    return repair_record{from_little_endian<std::uint64_t>(bytes.first<sizeof(repair_record::count)>()),
                         open_byte == 1};
}

std::expected<void, std::error_code> node_storage::write_repair_record(const repair_record& r)
{
    auto txn = begin_transaction(env_->handle.get(), 0);
    if (!txn) [[unlikely]] {
        return std::unexpected(txn.error());
    }
    std::array<std::byte, repair_record_size> body{};
    std::ranges::copy(to_little_endian(r.count), body.begin());
    body.back() = std::byte{r.open ? std::uint8_t{1} : std::uint8_t{0}};
    if (auto put = put_checked(txn->get(), env_->dbi, repair_key, body); !put) [[unlikely]] {
        return put;
    }
    return commit_or_throw(std::move(*txn));
}

std::expected<void, std::error_code> node_storage::count_a_repair()
{
    throw_if_moved_from();
    const auto r = get_repair_record();
    if (!r) [[unlikely]] {
        return std::unexpected(r.error());
    }
    return write_repair_record({r->count + 1, true});
}

std::expected<void, std::error_code> node_storage::close_the_repair()
{
    throw_if_moved_from();
    const auto r = get_repair_record();
    if (!r) [[unlikely]] {
        return std::unexpected(r.error());
    }
    return write_repair_record({r->count, false});
}

std::expected<void, std::error_code> node_storage::allow_one_more_repair()
{
    throw_if_moved_from();
    const auto r = get_repair_record();
    if (!r) [[unlikely]] {
        return std::unexpected(r.error());
    }
    return write_repair_record({0, r->open});
}

}
