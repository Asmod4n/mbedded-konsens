#include "konsens/persistent_storage.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <format>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include <crc32c/crc32c.h>
#include <fcntl.h>
#include <sqlite3.h>
#include <unistd.h>

#include "byte_order.hpp"
#include "konsens/entry_range.hpp"

namespace konsens {

namespace {

constexpr mode_t file_mode = 0600;
constexpr uid_t root_user_id = 0;
constexpr std::string_view store_file_name = "konsens.db";
constexpr std::string_view node_file_name = "node.db";
constexpr std::string_view meta_key = "meta";
constexpr std::string_view format_key = "format";
constexpr std::string_view fault_proof_key = "fault_proof";
constexpr std::string_view clean_stop_key = "clean_stop";
constexpr std::string_view config_first_key = "config_first";
constexpr std::uint32_t format_version = 5;
constexpr int synchronous_normal = 1;
constexpr int synchronous_full = 2;
constexpr int checkpoint_frames = 1000;
constexpr std::uint64_t largest_key = std::numeric_limits<std::int64_t>::max();
constexpr int open_flags =
    SQLITE_OPEN_READWRITE | SQLITE_OPEN_URI | SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_EXRESCODE;
constexpr unsigned int prepare_flags = SQLITE_PREPARE_PERSISTENT;
constexpr std::size_t session_head_size = sizeof(session_record::request_number);

constexpr std::string_view store_schema =
    "CREATE TABLE meta (name TEXT PRIMARY KEY, record BLOB NOT NULL) STRICT, WITHOUT ROWID;"
    "CREATE TABLE log (idx INTEGER PRIMARY KEY, record BLOB NOT NULL) STRICT;"
    "CREATE TABLE session (id INTEGER PRIMARY KEY, record BLOB NOT NULL) STRICT;"
    "CREATE TABLE data (key BLOB PRIMARY KEY, value BLOB NOT NULL) STRICT, WITHOUT ROWID;";
constexpr std::string_view node_schema =
    "CREATE TABLE IF NOT EXISTS meta (name TEXT PRIMARY KEY, record BLOB NOT NULL) STRICT, WITHOUT ROWID;";

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
constexpr std::size_t applied_idx_offset = last_chain_crc_offset + sizeof(meta_record::last_chain_crc);
constexpr std::size_t applied_chain_crc_offset = applied_idx_offset + sizeof(meta_record::applied_idx);
constexpr std::size_t meta_record_size = applied_chain_crc_offset + sizeof(meta_record::applied_chain_crc);

constexpr std::size_t proof_kind_offset = 0;
constexpr std::size_t proof_index_offset = proof_kind_offset + sizeof(proof_kind);
constexpr std::size_t proof_ballot_offset = proof_index_offset + sizeof(fault_proof::index);
constexpr std::size_t proof_crc_offset = proof_ballot_offset + ballot_size;
constexpr std::size_t proof_other_crc_offset = proof_crc_offset + sizeof(fault_proof::crc);
constexpr std::size_t fault_proof_size = proof_other_crc_offset + sizeof(fault_proof::other_crc);

constexpr std::size_t mark_pid_offset = 0;
constexpr std::size_t mark_configuration_id_offset = mark_pid_offset + sizeof(clean_stop_mark::pid);
constexpr std::size_t mark_meta_offset = mark_configuration_id_offset + sizeof(clean_stop_mark::configuration_id);
constexpr std::size_t mark_verified_idx_offset = mark_meta_offset + meta_record_size;
constexpr std::size_t clean_stop_mark_size = mark_verified_idx_offset + sizeof(clean_stop_mark::verified_idx);
constexpr auto last_proof_kind = proof_kind::wrong_list;

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
        case storage_errc::corrupt:
            return "The database file of the storage is damaged";
        }
        return "Unknown storage error";
    }
};

class sqlite_error_category final : public std::error_category {
public:
    const char* name() const noexcept override { return "sqlite"; }

    std::string message(const int code) const override { return sqlite3_errstr(code); }
};

const std::error_category& sqlite_category() noexcept
{
    static const sqlite_error_category category;
    return category;
}

struct connection_close {
    void operator()(sqlite3* db) const noexcept { sqlite3_close_v2(db); }
};

using connection = std::unique_ptr<sqlite3, connection_close>;

struct statement_finalize {
    void operator()(sqlite3_stmt* s) const noexcept { sqlite3_finalize(s); }
};

using statement = std::unique_ptr<sqlite3_stmt, statement_finalize>;

bool status_is_a_failed_sync(const int status) noexcept
{
    return status == SQLITE_IOERR_FSYNC || status == SQLITE_IOERR_DIR_FSYNC;
}

bool status_is_a_wrong_use(const int status) noexcept
{
    const int primary = status & 0xff;
    return primary == SQLITE_BUSY || primary == SQLITE_MISUSE || primary == SQLITE_CONSTRAINT ||
           primary == SQLITE_LOCKED || primary == SQLITE_RANGE;
}

std::error_code to_error_code(const int status, sqlite3* db) noexcept
{
    switch (status & 0xff) {
    case SQLITE_FULL:
        return std::make_error_code(std::errc::no_space_on_device);
    case SQLITE_CORRUPT:
    case SQLITE_NOTADB:
        return make_error_code(storage_errc::corrupt);
    case SQLITE_TOOBIG:
        return std::make_error_code(std::errc::value_too_large);
    case SQLITE_IOERR:
    case SQLITE_CANTOPEN:
        if (const int system_errno = db == nullptr ? 0 : sqlite3_system_errno(db); system_errno != 0) {
            return {system_errno, std::generic_category()};
        }
        return {status, sqlite_category()};
    default:
        return {status, sqlite_category()};
    }
}

enum class access { read, write };

std::error_code error_code_or_throw(const int status, sqlite3* db, const access a)
{
    if (status_is_a_failed_sync(status)) [[unlikely]] {
        throw sync_error(to_error_code(status, db), "konsens: SQLite failed to sync the storage");
    }
    if ((status & 0xff) == SQLITE_IOERR) [[unlikely]] {
        if (a == access::write) {
            throw write_error(to_error_code(status, db), "konsens: SQLite failed to write the storage");
        }
        throw read_error(to_error_code(status, db), "konsens: SQLite failed to read the storage");
    }
    if (status_is_a_wrong_use(status)) [[unlikely]] {
        throw std::logic_error(std::format("konsens: the storage refused a statement: {}", sqlite3_errstr(status)));
    }
    return to_error_code(status, db);
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

bool key_fits_in_an_sqlite_integer(const std::uint64_t key) noexcept
{
    return key <= largest_key;
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
    next = std::ranges::copy(to_little_endian(m.last_chain_crc), next).out;
    next = std::ranges::copy(to_little_endian(m.applied_idx), next).out;
    std::ranges::copy(to_little_endian(m.applied_chain_crc), next);
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
        from_little_endian<std::uint64_t>(bytes.subspan<applied_idx_offset, sizeof(meta_record::applied_idx)>()),
        from_little_endian<std::uint32_t>(
            bytes.subspan<applied_chain_crc_offset, sizeof(meta_record::applied_chain_crc)>()),
    };
}

struct stored_record {
    std::span<const std::byte> bytes;
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
    return stored_record{value, encoded, stored, chain, state};
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

std::expected<statement, int> prepare(sqlite3* db, const std::string_view sql)
{
    sqlite3_stmt* s = nullptr;
    const int status = sqlite3_prepare_v3(db, sql.data(), static_cast<int>(sql.size()), prepare_flags, &s, nullptr);
    if (status != SQLITE_OK) [[unlikely]] {
        return std::unexpected(status);
    }
    return statement{s};
}

struct statements {
    statement begin_immediate;
    statement begin_deferred;
    statement commit;
    statement rollback;
    statement savepoint;
    statement release;
    statement rollback_to;
    statement get_record;
    statement put_record;
    statement get_entry;
    statement get_entries_from;
    statement get_last_entry;
    statement append_entry;
    statement update_entry;
    statement delete_entries_between;
    statement get_value;
    statement put_value;
    statement erase_value;
    statement get_session;
    statement put_session;
    statement erase_session;
    statement vacuum_into;
};

std::expected<statements, int> prepare_statements(sqlite3* db)
{
    statements s;
    const std::array list{
        std::pair{&s.begin_immediate, std::string_view{"BEGIN IMMEDIATE"}},
        std::pair{&s.begin_deferred, std::string_view{"BEGIN"}},
        std::pair{&s.commit, std::string_view{"COMMIT"}},
        std::pair{&s.rollback, std::string_view{"ROLLBACK"}},
        std::pair{&s.savepoint, std::string_view{"SAVEPOINT nested"}},
        std::pair{&s.release, std::string_view{"RELEASE nested"}},
        std::pair{&s.rollback_to, std::string_view{"ROLLBACK TO nested"}},
        std::pair{&s.get_record, std::string_view{"SELECT record FROM meta WHERE name = ?1"}},
        std::pair{&s.put_record,
                  std::string_view{"INSERT INTO meta (name, record) VALUES (?1, ?2) "
                                   "ON CONFLICT (name) DO UPDATE SET record = excluded.record"}},
        std::pair{&s.get_entry, std::string_view{"SELECT record FROM log WHERE idx = ?1"}},
        std::pair{&s.get_entries_from, std::string_view{"SELECT idx, record FROM log WHERE idx >= ?1 ORDER BY idx"}},
        std::pair{&s.get_last_entry, std::string_view{"SELECT idx, record FROM log ORDER BY idx DESC LIMIT 1"}},
        std::pair{&s.append_entry,
                  std::string_view{"INSERT INTO log (idx, record) SELECT ?1, ?2 "
                                   "WHERE ?1 > (SELECT coalesce(max(idx), -1) FROM log)"}},
        std::pair{&s.update_entry, std::string_view{"UPDATE log SET record = ?2 WHERE idx = ?1"}},
        std::pair{&s.delete_entries_between, std::string_view{"DELETE FROM log WHERE idx >= ?1 AND idx < ?2"}},
        std::pair{&s.get_value, std::string_view{"SELECT value FROM data WHERE key = ?1"}},
        std::pair{&s.put_value,
                  std::string_view{"INSERT INTO data (key, value) VALUES (?1, ?2) "
                                   "ON CONFLICT (key) DO UPDATE SET value = excluded.value"}},
        std::pair{&s.erase_value, std::string_view{"DELETE FROM data WHERE key = ?1"}},
        std::pair{&s.get_session, std::string_view{"SELECT record FROM session WHERE id = ?1"}},
        std::pair{&s.put_session,
                  std::string_view{"INSERT INTO session (id, record) VALUES (?1, ?2) "
                                   "ON CONFLICT (id) DO UPDATE SET record = excluded.record"}},
        std::pair{&s.erase_session, std::string_view{"DELETE FROM session WHERE id = ?1"}},
        std::pair{&s.vacuum_into, std::string_view{"VACUUM INTO ?1"}},
    };
    for (const auto& [target, sql] : list) {
        auto prepared = prepare(db, sql);
        if (!prepared) [[unlikely]] {
            return std::unexpected(prepared.error());
        }
        *target = std::move(*prepared);
    }
    return s;
}

void reset_statements(statements& s) noexcept
{
    for (auto* const st : {&s.get_record, &s.get_entry, &s.get_entries_from, &s.get_last_entry, &s.get_value,
                           &s.get_session}) {
        sqlite3_reset(st->get());
    }
}

int bind_bytes(sqlite3_stmt* s, const int position, const std::span<const std::byte> bytes) noexcept
{
    if (bytes.empty()) {
        return sqlite3_bind_zeroblob(s, position, 0);
    }
    return sqlite3_bind_blob64(s, position, bytes.data(), bytes.size(), SQLITE_STATIC);
}

int bind_text(sqlite3_stmt* s, const int position, const std::string_view text) noexcept
{
    return sqlite3_bind_text64(s, position, text.data(), text.size(), SQLITE_STATIC, SQLITE_UTF8);
}

int bind_key(sqlite3_stmt* s, const int position, const std::uint64_t key) noexcept
{
    return sqlite3_bind_int64(s, position, static_cast<sqlite3_int64>(key));
}

std::span<const std::byte> column_bytes(sqlite3_stmt* s, const int column) noexcept
{
    const auto* const data = static_cast<const std::byte*>(sqlite3_column_blob(s, column));
    return {data, static_cast<std::size_t>(sqlite3_column_bytes(s, column))};
}

std::uint64_t column_key(sqlite3_stmt* s, const int column) noexcept
{
    return static_cast<std::uint64_t>(sqlite3_column_int64(s, column));
}

struct link {
    sqlite3* db;
    statements* s;
    std::optional<std::error_code>* failure;
};

void step_once(sqlite3_stmt* s) noexcept
{
    sqlite3_reset(s);
    static_cast<void>(sqlite3_step(s));
    sqlite3_reset(s);
}

void roll_back(sqlite3* db, statements& s) noexcept
{
    reset_statements(s);
    if (sqlite3_get_autocommit(db) == 0) {
        step_once(s.rollback.get());
    }
}

void end_the_read(statements& s) noexcept
{
    reset_statements(s);
    step_once(s.commit.get());
}

void roll_back_to_the_savepoint(statements& s) noexcept
{
    reset_statements(s);
    step_once(s.rollback_to.get());
    step_once(s.release.get());
}

std::error_code fail(const link& l, const int status)
{
    if (l.failure == nullptr) {
        return error_code_or_throw(status, l.db, access::read);
    }
    roll_back(l.db, *l.s);
    *l.failure = to_error_code(status, l.db);
    return error_code_or_throw(status, l.db, access::write);
}

std::expected<void, std::error_code> refuse_after_a_failure(const link& l)
{
    if (l.failure != nullptr && *l.failure) [[unlikely]] {
        return std::unexpected(**l.failure);
    }
    return {};
}

std::expected<void, std::error_code> run(const link& l, sqlite3_stmt* s)
{
    const int status = sqlite3_step(s);
    sqlite3_reset(s);
    if (status != SQLITE_DONE) [[unlikely]] {
        return std::unexpected(fail(l, status));
    }
    return {};
}

std::expected<std::optional<std::span<const std::byte>>, std::error_code> select_one(const link& l, sqlite3_stmt* s)
{
    const int status = sqlite3_step(s);
    if (status == SQLITE_DONE) {
        sqlite3_reset(s);
        return std::nullopt;
    }
    if (status != SQLITE_ROW) [[unlikely]] {
        sqlite3_reset(s);
        return std::unexpected(fail(l, status));
    }
    return column_bytes(s, 0);
}

std::expected<std::optional<std::span<const std::byte>>, std::error_code>
get_checked(const link& l, const std::string_view name, const std::size_t body_size)
{
    sqlite3_stmt* const s = l.s->get_record.get();
    sqlite3_reset(s);
    if (const int bound = bind_text(s, 1, name); bound != SQLITE_OK) [[unlikely]] {
        return std::unexpected(fail(l, bound));
    }
    const auto value = select_one(l, s);
    if (!value || !value->has_value()) {
        return value;
    }
    const auto bytes = value->value();
    if (bytes.size() != body_size + crc_size) [[unlikely]] {
        return std::unexpected(make_error_code(storage_errc::malformed_record));
    }
    const auto body = bytes.first(body_size);
    if (from_little_endian<std::uint32_t>(bytes.last<crc_size>()) != crc_of(body)) [[unlikely]] {
        return std::unexpected(make_error_code(storage_errc::crc_mismatch));
    }
    return body;
}

template <std::size_t N>
std::expected<void, std::error_code> put_checked(const link& l, const std::string_view name,
                                                 const std::array<std::byte, N>& body)
{
    std::array<std::byte, N + crc_size> record{};
    std::ranges::copy(to_little_endian(crc_of(body)), std::ranges::copy(body, record.begin()).out);
    sqlite3_stmt* const s = l.s->put_record.get();
    sqlite3_reset(s);
    if (const int bound = bind_text(s, 1, name); bound != SQLITE_OK) [[unlikely]] {
        return std::unexpected(fail(l, bound));
    }
    if (const int bound = bind_bytes(s, 2, record); bound != SQLITE_OK) [[unlikely]] {
        return std::unexpected(fail(l, bound));
    }
    return run(l, s);
}

std::expected<meta_record, std::error_code> read_meta(const link& l)
{
    const auto body = get_checked(l, meta_key, meta_record_size);
    if (!body) [[unlikely]] {
        return std::unexpected(body.error());
    }
    if (!body->has_value()) [[unlikely]] {
        return std::unexpected(make_error_code(storage_errc::missing_meta_record));
    }
    const auto m = decode_meta(body->value().first<meta_record_size>());
    sqlite3_reset(l.s->get_record.get());
    return m;
}

std::expected<void, std::error_code> write_meta(const link& l, const meta_record& m)
{
    return put_checked(l, meta_key, encode_meta(m));
}

std::expected<std::optional<std::uint64_t>, std::error_code> read_config_first_idx(const link& l)
{
    const auto body = get_checked(l, config_first_key, sizeof(std::uint64_t));
    if (!body) [[unlikely]] {
        return std::unexpected(body.error());
    }
    if (!body->has_value()) {
        return std::nullopt;
    }
    const auto index = from_little_endian<std::uint64_t>(body->value().first<sizeof(std::uint64_t)>());
    sqlite3_reset(l.s->get_record.get());
    return index;
}

std::expected<std::vector<entry>, read_failure> read_entries_in(const link& l, const std::uint64_t from,
                                                                const std::uint64_t to)
{
    throw_if_from_after_to(from, to);
    std::vector<entry> entries;
    if (!key_fits_in_an_sqlite_integer(from)) [[unlikely]] {
        return entries;
    }
    sqlite3_stmt* const s = l.s->get_entries_from.get();
    sqlite3_reset(s);
    if (const int bound = bind_key(s, 1, from); bound != SQLITE_OK) [[unlikely]] {
        return std::unexpected(read_failure{fail(l, bound), from, 0, 0});
    }
    for (int status = sqlite3_step(s); status != SQLITE_DONE; status = sqlite3_step(s)) {
        const auto expected_index = from + entries.size();
        if (status != SQLITE_ROW) [[unlikely]] {
            sqlite3_reset(s);
            return std::unexpected(read_failure{fail(l, status), expected_index, 0, 0});
        }
        const auto index = column_key(s, 0);
        if (index >= to) {
            if (expected_index != to) [[unlikely]] {
                sqlite3_reset(s);
                return std::unexpected(read_failure{storage_errc::missing_entry, expected_index, 0, 0});
            }
            break;
        }
        if (index != expected_index) [[unlikely]] {
            sqlite3_reset(s);
            return std::unexpected(read_failure{storage_errc::missing_entry, expected_index, 0, 0});
        }
        const auto record = check_record(index, column_bytes(s, 1));
        if (!record) [[unlikely]] {
            sqlite3_reset(s);
            return std::unexpected(record.error());
        }
        auto decoded = decode_entry(record->encoded);
        if (!decoded) [[unlikely]] {
            sqlite3_reset(s);
            return std::unexpected(read_failure{storage_errc::malformed_record, index, 0, 0});
        }
        entries.push_back(std::move(*decoded));
    }
    sqlite3_reset(s);
    return entries;
}

std::expected<std::optional<last_entry>, std::error_code> read_last_entry(const link& l)
{
    sqlite3_stmt* const s = l.s->get_last_entry.get();
    sqlite3_reset(s);
    const int status = sqlite3_step(s);
    if (status == SQLITE_DONE) {
        sqlite3_reset(s);
        return std::nullopt;
    }
    if (status != SQLITE_ROW) [[unlikely]] {
        sqlite3_reset(s);
        return std::unexpected(fail(l, status));
    }
    const auto index = column_key(s, 0);
    const auto record = check_record(index, column_bytes(s, 1));
    sqlite3_reset(s);
    if (!record) [[unlikely]] {
        return std::unexpected(record.error().code);
    }
    return last_entry{index, record->entry_crc, record->chain_crc};
}

std::expected<std::optional<stored_record>, std::error_code> read_record(const link& l, const std::uint64_t index)
{
    if (!key_fits_in_an_sqlite_integer(index)) [[unlikely]] {
        return std::nullopt;
    }
    sqlite3_stmt* const s = l.s->get_entry.get();
    sqlite3_reset(s);
    if (const int bound = bind_key(s, 1, index); bound != SQLITE_OK) [[unlikely]] {
        return std::unexpected(fail(l, bound));
    }
    const auto value = select_one(l, s);
    if (!value) [[unlikely]] {
        return std::unexpected(value.error());
    }
    if (!value->has_value()) {
        return std::nullopt;
    }
    const auto record = check_record(index, value->value());
    if (!record) [[unlikely]] {
        return std::unexpected(record.error().code);
    }
    return *record;
}

std::expected<std::optional<entry_state>, std::error_code> read_entry_state(const link& l, const std::uint64_t index)
{
    auto r = read_record(l, index).transform([](const std::optional<stored_record>& found) {
        return found.transform([](const stored_record& x) { return x.state; });
    });
    sqlite3_reset(l.s->get_entry.get());
    return r;
}

std::expected<std::optional<std::uint32_t>, std::error_code> read_chain_crc(const link& l, const std::uint64_t index)
{
    auto r = read_record(l, index).transform([](const std::optional<stored_record>& found) {
        return found.transform([](const stored_record& x) { return x.chain_crc; });
    });
    sqlite3_reset(l.s->get_entry.get());
    return r;
}

std::expected<void, std::error_code> put_entries(const link& l, meta_record& m, std::vector<std::byte>& record,
                                                 const std::span<const entry> entries)
{
    sqlite3_stmt* const s = l.s->append_entry.get();
    for (const auto& e : entries) {
        const auto index = m.log_len;
        if (!key_fits_in_an_sqlite_integer(index)) [[unlikely]] {
            return std::unexpected(std::make_error_code(std::errc::value_too_large));
        }
        record.resize(encoded_size_of(e) + record_tail_size);
        const auto state = index < m.decided_idx ? entry_state::committed : entry_state::in_work;
        const auto chain = encode_record(index, e, m.last_chain_crc, state, record);
        sqlite3_reset(s);
        if (const int bound = bind_key(s, 1, index); bound != SQLITE_OK) [[unlikely]] {
            return std::unexpected(fail(l, bound));
        }
        if (const int bound = bind_bytes(s, 2, record); bound != SQLITE_OK) [[unlikely]] {
            return std::unexpected(fail(l, bound));
        }
        if (auto r = run(l, s); !r) [[unlikely]] {
            return r;
        }
        if (sqlite3_changes64(l.db) != 1) [[unlikely]] {
            return std::unexpected(make_error_code(storage_errc::malformed_record));
        }
        m.last_chain_crc = chain;
        m.log_len = index + 1;
    }
    return {};
}

std::expected<void, std::error_code> delete_entries(const link& l, const std::uint64_t from, const std::uint64_t to)
{
    if (!key_fits_in_an_sqlite_integer(to)) [[unlikely]] {
        return std::unexpected(std::make_error_code(std::errc::value_too_large));
    }
    sqlite3_stmt* const s = l.s->delete_entries_between.get();
    sqlite3_reset(s);
    if (const int bound = bind_key(s, 1, from); bound != SQLITE_OK) [[unlikely]] {
        return std::unexpected(fail(l, bound));
    }
    if (const int bound = bind_key(s, 2, to); bound != SQLITE_OK) [[unlikely]] {
        return std::unexpected(fail(l, bound));
    }
    if (auto r = run(l, s); !r) [[unlikely]] {
        return r;
    }
    if (static_cast<std::uint64_t>(sqlite3_changes64(l.db)) != to - from) [[unlikely]] {
        return std::unexpected(make_error_code(storage_errc::missing_entry));
    }
    return {};
}

std::expected<std::uint32_t, std::error_code> chain_crc_before(const link& l, const std::uint64_t index)
{
    if (index == 0) {
        return 0;
    }
    const auto chain = read_chain_crc(l, index - 1);
    if (!chain) [[unlikely]] {
        return std::unexpected(chain.error());
    }
    if (!chain->has_value()) [[unlikely]] {
        return std::unexpected(make_error_code(storage_errc::missing_entry));
    }
    return chain->value();
}

std::expected<void, std::error_code> set_entry_states(const link& l, std::vector<std::byte>& rewritten,
                                                      const std::uint64_t from, const std::uint64_t to,
                                                      const std::uint64_t decided_idx)
{
    sqlite3_stmt* const update = l.s->update_entry.get();
    for (auto index = from; index < to; ++index) {
        const auto record = read_record(l, index);
        if (!record) [[unlikely]] {
            return std::unexpected(record.error());
        }
        if (!record->has_value()) [[unlikely]] {
            return std::unexpected(make_error_code(storage_errc::missing_entry));
        }
        const auto state = index < decided_idx ? entry_state::committed : entry_state::in_work;
        if (record->value().state == state) {
            sqlite3_reset(l.s->get_entry.get());
            continue;
        }
        const auto stored = record->value().bytes;
        rewritten.assign(stored.begin(), stored.end());
        sqlite3_reset(l.s->get_entry.get());
        write_state(index, state, rewritten);
        sqlite3_reset(update);
        if (const int bound = bind_key(update, 1, index); bound != SQLITE_OK) [[unlikely]] {
            return std::unexpected(fail(l, bound));
        }
        if (const int bound = bind_bytes(update, 2, rewritten); bound != SQLITE_OK) [[unlikely]] {
            return std::unexpected(fail(l, bound));
        }
        if (auto r = run(l, update); !r) [[unlikely]] {
            return r;
        }
    }
    return {};
}

std::expected<std::optional<std::span<const std::byte>>, std::error_code>
find_value(const link& l, const std::span<const std::byte> key)
{
    sqlite3_stmt* const s = l.s->get_value.get();
    sqlite3_reset(s);
    if (const int bound = bind_bytes(s, 1, key); bound != SQLITE_OK) [[unlikely]] {
        return std::unexpected(fail(l, bound));
    }
    return select_one(l, s);
}

std::expected<void, std::error_code> put_value(const link& l, const std::span<const std::byte> key,
                                               const std::span<const std::byte> value)
{
    sqlite3_stmt* const s = l.s->put_value.get();
    sqlite3_reset(s);
    if (const int bound = bind_bytes(s, 1, key); bound != SQLITE_OK) [[unlikely]] {
        return std::unexpected(fail(l, bound));
    }
    if (const int bound = bind_bytes(s, 2, value); bound != SQLITE_OK) [[unlikely]] {
        return std::unexpected(fail(l, bound));
    }
    return run(l, s);
}

std::expected<bool, std::error_code> erase_value(const link& l, const std::span<const std::byte> key)
{
    sqlite3_stmt* const s = l.s->erase_value.get();
    sqlite3_reset(s);
    if (const int bound = bind_bytes(s, 1, key); bound != SQLITE_OK) [[unlikely]] {
        return std::unexpected(fail(l, bound));
    }
    return run(l, s).transform([&l] { return sqlite3_changes64(l.db) > 0; });
}

std::expected<std::optional<session_record>, std::error_code> find_session_in(const link& l,
                                                                              const std::uint64_t session)
{
    if (!key_fits_in_an_sqlite_integer(session)) [[unlikely]] {
        return std::nullopt;
    }
    sqlite3_stmt* const s = l.s->get_session.get();
    sqlite3_reset(s);
    if (const int bound = bind_key(s, 1, session); bound != SQLITE_OK) [[unlikely]] {
        return std::unexpected(fail(l, bound));
    }
    const auto value = select_one(l, s);
    if (!value) [[unlikely]] {
        return std::unexpected(value.error());
    }
    if (!value->has_value()) {
        return std::nullopt;
    }
    const auto bytes = value->value();
    if (bytes.size() < session_head_size + crc_size) [[unlikely]] {
        sqlite3_reset(s);
        return std::unexpected(make_error_code(storage_errc::malformed_record));
    }
    const auto body = bytes.first(bytes.size() - crc_size);
    if (from_little_endian<std::uint32_t>(bytes.last<crc_size>()) != crc_of(body)) [[unlikely]] {
        sqlite3_reset(s);
        return std::unexpected(make_error_code(storage_errc::crc_mismatch));
    }
    const auto reply = body.subspan(session_head_size);
    session_record r{from_little_endian<std::uint64_t>(body.first<session_head_size>()), {reply.begin(), reply.end()}};
    sqlite3_reset(s);
    return r;
}

std::expected<void, std::error_code> put_session(const link& l, std::vector<std::byte>& record,
                                                 const std::uint64_t session, const session_record& r)
{
    if (!key_fits_in_an_sqlite_integer(session)) [[unlikely]] {
        return std::unexpected(std::make_error_code(std::errc::value_too_large));
    }
    record.resize(session_head_size + r.reply.size() + crc_size);
    std::ranges::copy(r.reply, std::ranges::copy(to_little_endian(r.request_number), record.begin()).out);
    const auto body = std::span<const std::byte>(record).first(record.size() - crc_size);
    std::ranges::copy(to_little_endian(crc_of(body)), std::span(record).last(crc_size).begin());
    sqlite3_stmt* const s = l.s->put_session.get();
    sqlite3_reset(s);
    if (const int bound = bind_key(s, 1, session); bound != SQLITE_OK) [[unlikely]] {
        return std::unexpected(fail(l, bound));
    }
    if (const int bound = bind_bytes(s, 2, record); bound != SQLITE_OK) [[unlikely]] {
        return std::unexpected(fail(l, bound));
    }
    return run(l, s);
}

std::expected<bool, std::error_code> erase_session_in(const link& l, const std::uint64_t session)
{
    if (!key_fits_in_an_sqlite_integer(session)) [[unlikely]] {
        return false;
    }
    sqlite3_stmt* const s = l.s->erase_session.get();
    sqlite3_reset(s);
    if (const int bound = bind_key(s, 1, session); bound != SQLITE_OK) [[unlikely]] {
        return std::unexpected(fail(l, bound));
    }
    return run(l, s).transform([&l] { return sqlite3_changes64(l.db) > 0; });
}

void throw_if_no_environment(const void* const env)
{
    if (env == nullptr) [[unlikely]] {
        throw std::logic_error("konsens: the transaction was moved from, committed or aborted");
    }
}

void throw_if_nested_is_open(const bool nested_open)
{
    if (nested_open) [[unlikely]] {
        throw std::logic_error("konsens: a nested transaction is open, and its parent takes no operation");
    }
}

void throw_if_a_write_did_not_finish(const bool write_unfinished)
{
    if (write_unfinished) [[unlikely]] {
        throw std::logic_error(
            "konsens: a commit or a checkpoint of this storage failed, and the storage takes no operation");
    }
}

void throw_if_an_entry_has_a_field_of_another_kind(const std::span<const entry> entries)
{
    for (const auto& e : entries) {
        throw_if_entry_has_a_field_of_another_kind(e);
    }
}

std::string uri_of(const std::filesystem::path& file)
{
    constexpr std::string_view unreserved =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~/";
    std::string uri = "file:";
    for (const char c : file.string()) {
        if (unreserved.contains(c)) {
            uri.push_back(c);
        } else {
            uri += std::format("%{:02X}", static_cast<unsigned char>(c));
        }
    }
    uri += "?psow=0";
    return uri;
}

std::expected<void, std::error_code> create_with_file_mode(const std::filesystem::path& file)
{
    const int fd = ::open(file.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, file_mode);
    if (fd == -1) [[unlikely]] {
        return std::unexpected(std::error_code{errno, std::generic_category()});
    }
    ::close(fd);
    return {};
}

std::expected<void, std::error_code> create_new_with_file_mode(const std::filesystem::path& file)
{
    const int fd = ::open(file.c_str(), O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, file_mode);
    if (fd == -1) [[unlikely]] {
        return std::unexpected(std::error_code{errno, std::generic_category()});
    }
    ::close(fd);
    return {};
}

std::expected<void, std::error_code> execute(sqlite3* db, const std::string_view sql)
{
    const int status = sqlite3_exec(db, std::string(sql).c_str(), nullptr, nullptr, nullptr);
    if (status != SQLITE_OK) [[unlikely]] {
        if ((status & 0xff) == SQLITE_BUSY) {
            return std::unexpected(std::make_error_code(std::errc::device_or_resource_busy));
        }
        return std::unexpected(error_code_or_throw(status, db, access::write));
    }
    return {};
}

std::expected<std::int64_t, std::error_code> read_integer(sqlite3* db, const std::string_view sql)
{
    auto s = prepare(db, sql);
    if (!s) [[unlikely]] {
        return std::unexpected(to_error_code(s.error(), db));
    }
    const int status = sqlite3_step(s->get());
    if (status != SQLITE_ROW) [[unlikely]] {
        if ((status & 0xff) == SQLITE_BUSY) {
            return std::unexpected(std::make_error_code(std::errc::device_or_resource_busy));
        }
        return std::unexpected(error_code_or_throw(status, db, access::read));
    }
    return sqlite3_column_int64(s->get(), 0);
}

std::expected<std::string, std::error_code> read_text(sqlite3* db, const std::string_view sql)
{
    auto s = prepare(db, sql);
    if (!s) [[unlikely]] {
        return std::unexpected(to_error_code(s.error(), db));
    }
    const int status = sqlite3_step(s->get());
    if (status != SQLITE_ROW) [[unlikely]] {
        if ((status & 0xff) == SQLITE_BUSY) {
            return std::unexpected(std::make_error_code(std::errc::device_or_resource_busy));
        }
        return std::unexpected(error_code_or_throw(status, db, access::read));
    }
    return std::string{reinterpret_cast<const char*>(sqlite3_column_text(s->get(), 0))};
}

int count_frames(void* frames, sqlite3*, const char*, const int frame_count) noexcept
{
    *static_cast<int*>(frames) = frame_count;
    return SQLITE_OK;
}

std::expected<connection, std::error_code> open_connection(const std::filesystem::path& file, const int synchronous)
{
    sqlite3* raw = nullptr;
    const int opened = sqlite3_open_v2(uri_of(file).c_str(), &raw, open_flags, nullptr);
    connection db{raw};
    if (opened != SQLITE_OK) [[unlikely]] {
        return std::unexpected(to_error_code(opened, db.get()));
    }
    int no_checkpoint_on_close = 0;
    if (const int set = sqlite3_db_config(db.get(), SQLITE_DBCONFIG_NO_CKPT_ON_CLOSE, 1, &no_checkpoint_on_close);
        set != SQLITE_OK || no_checkpoint_on_close != 1) [[unlikely]] {
        return std::unexpected(std::make_error_code(std::errc::not_supported));
    }
    const auto journal_mode = read_text(db.get(), "PRAGMA journal_mode = WAL");
    if (!journal_mode) [[unlikely]] {
        return std::unexpected(journal_mode.error());
    }
    if (*journal_mode != "wal") [[unlikely]] {
        return std::unexpected(std::make_error_code(std::errc::not_supported));
    }
    const auto pragma = synchronous == synchronous_full ? "PRAGMA synchronous = FULL" : "PRAGMA synchronous = NORMAL";
    if (auto set = execute(db.get(), pragma); !set) [[unlikely]] {
        return std::unexpected(set.error());
    }
    const auto level = read_integer(db.get(), "PRAGMA synchronous");
    if (!level) [[unlikely]] {
        return std::unexpected(level.error());
    }
    if (*level != synchronous) [[unlikely]] {
        return std::unexpected(std::make_error_code(std::errc::not_supported));
    }
    if (auto set = execute(db.get(), "PRAGMA wal_autocheckpoint = 0"); !set) [[unlikely]] {
        return std::unexpected(set.error());
    }
    return db;
}

std::expected<void, std::error_code> sync_the_wal(sqlite3* db)
{
    sqlite3_file* wal = nullptr;
    if (const int got = sqlite3_file_control(db, "main", SQLITE_FCNTL_JOURNAL_POINTER, &wal);
        got != SQLITE_OK || wal == nullptr || wal->pMethods == nullptr) [[unlikely]] {
        return std::unexpected(std::make_error_code(std::errc::not_supported));
    }
    if (const int synced = wal->pMethods->xSync(wal, SQLITE_SYNC_NORMAL); synced != SQLITE_OK) [[unlikely]] {
        return std::unexpected(to_error_code(synced, db));
    }
    return {};
}

std::expected<void, std::error_code> run_checkpoint(sqlite3* db)
{
    const int status = sqlite3_wal_checkpoint_v2(db, nullptr, SQLITE_CHECKPOINT_PASSIVE, nullptr, nullptr);
    if (status == SQLITE_OK) {
        return {};
    }
    if ((status & 0xff) == SQLITE_FULL) [[unlikely]] {
        throw write_error(to_error_code(status, db), "konsens: the checkpoint of the storage found the disk full");
    }
    return std::unexpected(error_code_or_throw(status, db, access::write));
}

std::expected<void, std::error_code> set_max_page_count(sqlite3* db, const std::size_t max_size)
{
    const auto page_size = read_integer(db, "PRAGMA page_size");
    if (!page_size) [[unlikely]] {
        return std::unexpected(page_size.error());
    }
    const auto max_page_count = max_size / static_cast<std::size_t>(*page_size);
    return execute(db, std::format("PRAGMA max_page_count = {}", max_page_count));
}

std::expected<void, std::error_code> create_the_schema_of_an_empty_file(sqlite3* db)
{
    const auto tables = read_integer(db, "SELECT count(*) FROM sqlite_schema");
    if (!tables) [[unlikely]] {
        return std::unexpected(tables.error());
    }
    if (*tables != 0) {
        return {};
    }
    return execute(db, store_schema);
}

std::expected<void, std::error_code> check_or_write_format(const link& l)
{
    if (auto begun = run(l, l.s->begin_immediate.get()); !begun) [[unlikely]] {
        return begun;
    }
    const auto format = get_checked(l, format_key, sizeof format_version);
    if (!format) [[unlikely]] {
        return std::unexpected(format.error());
    }
    if (format->has_value()) {
        const auto version = from_little_endian<std::uint32_t>(format->value().first<sizeof format_version>());
        roll_back(l.db, *l.s);
        if (version != format_version) [[unlikely]] {
            return std::unexpected(make_error_code(storage_errc::unsupported_format));
        }
        return {};
    }
    const auto rows = read_integer(l.db, "SELECT (SELECT count(*) FROM meta) + (SELECT count(*) FROM log) + "
                                         "(SELECT count(*) FROM session) + (SELECT count(*) FROM data)");
    if (!rows) [[unlikely]] {
        return std::unexpected(rows.error());
    }
    if (*rows != 0) [[unlikely]] {
        return std::unexpected(make_error_code(storage_errc::unsupported_format));
    }
    if (auto put = put_checked(l, format_key, to_little_endian(format_version)); !put) [[unlikely]] {
        return put;
    }
    if (auto put = write_meta(l, meta_record{}); !put) [[unlikely]] {
        return put;
    }
    reset_statements(*l.s);
    return run(l, l.s->commit.get());
}

}

struct persistent_storage::environment {
    connection writer_db;
    connection reader_db;
    statements writer;
    statements reader;
    int frames{};
    bool writer_open{};
    bool nested_open{};
    bool reader_open{};
    bool written{};
    bool log_written{};
    bool write_unfinished{};
    std::optional<std::error_code> failure;
    std::vector<std::byte> record{};

    link of_writer() noexcept { return {writer_db.get(), &writer, &failure}; }
    link of_reader() noexcept { return {reader_db.get(), &reader, nullptr}; }

    void end_the_reader() noexcept
    {
        end_the_read(reader);
        reader_open = false;
    }

    void roll_back_the_writer() noexcept
    {
        roll_back(writer_db.get(), writer);
        nested_open = false;
        writer_open = false;
    }

    void roll_back_the_nested() noexcept
    {
        if (!failure) {
            roll_back_to_the_savepoint(writer);
        }
        nested_open = false;
    }
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
            env_->end_the_reader();
        }
        env_ = std::exchange(other.env_, nullptr);
    }
    return *this;
}

persistent_storage::read_only_transaction::~read_only_transaction()
{
    if (env_ != nullptr) {
        env_->end_the_reader();
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

std::expected<std::optional<std::uint64_t>, std::error_code>
persistent_storage::read_only_transaction::get_config_first_idx() const
{
    throw_if_no_environment(env_);
    return read_config_first_idx(env_->of_reader());
}

std::expected<std::optional<std::span<const std::byte>>, std::error_code>
persistent_storage::read_only_transaction::find(const std::span<const std::byte> key) const
{
    throw_if_no_environment(env_);
    return find_value(env_->of_reader(), key);
}

std::expected<std::optional<session_record>, std::error_code>
persistent_storage::read_only_transaction::find_session(const std::uint64_t session) const
{
    throw_if_no_environment(env_);
    return find_session_in(env_->of_reader(), session);
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
            env_->roll_back_the_writer();
        }
        env_ = std::exchange(other.env_, nullptr);
    }
    return *this;
}

persistent_storage::transaction::~transaction()
{
    if (env_ != nullptr) {
        env_->roll_back_the_writer();
    }
}

std::expected<meta_record, std::error_code> persistent_storage::transaction::get_meta() const
{
    throw_if_no_environment(env_);
    throw_if_nested_is_open(env_->nested_open);
    if (auto f = refuse_after_a_failure(env_->of_writer()); !f) [[unlikely]] {
        return std::unexpected(f.error());
    }
    return read_meta(env_->of_writer());
}

std::expected<std::vector<entry>, read_failure>
persistent_storage::transaction::read_entries(const std::uint64_t from, const std::uint64_t to) const
{
    throw_if_no_environment(env_);
    throw_if_nested_is_open(env_->nested_open);
    if (auto f = refuse_after_a_failure(env_->of_writer()); !f) [[unlikely]] {
        return std::unexpected(read_failure{f.error(), from, 0, 0});
    }
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
    throw_if_nested_is_open(env_->nested_open);
    if (auto f = refuse_after_a_failure(env_->of_writer()); !f) [[unlikely]] {
        return std::unexpected(f.error());
    }
    return read_last_entry(env_->of_writer());
}

std::expected<std::optional<entry_state>, std::error_code>
persistent_storage::transaction::get_entry_state(const std::uint64_t index) const
{
    throw_if_no_environment(env_);
    throw_if_nested_is_open(env_->nested_open);
    if (auto f = refuse_after_a_failure(env_->of_writer()); !f) [[unlikely]] {
        return std::unexpected(f.error());
    }
    return read_entry_state(env_->of_writer(), index);
}

std::expected<std::optional<std::uint32_t>, std::error_code>
persistent_storage::transaction::get_chain_crc(const std::uint64_t index) const
{
    throw_if_no_environment(env_);
    throw_if_nested_is_open(env_->nested_open);
    if (auto f = refuse_after_a_failure(env_->of_writer()); !f) [[unlikely]] {
        return std::unexpected(f.error());
    }
    return read_chain_crc(env_->of_writer(), index);
}

std::expected<std::optional<std::uint64_t>, std::error_code>
persistent_storage::transaction::get_config_first_idx() const
{
    throw_if_no_environment(env_);
    throw_if_nested_is_open(env_->nested_open);
    if (auto f = refuse_after_a_failure(env_->of_writer()); !f) [[unlikely]] {
        return std::unexpected(f.error());
    }
    return read_config_first_idx(env_->of_writer());
}

std::expected<void, std::error_code>
persistent_storage::transaction::append_entries(const std::span<const entry> entries)
{
    throw_if_no_environment(env_);
    throw_if_nested_is_open(env_->nested_open);
    if (auto f = refuse_after_a_failure(env_->of_writer()); !f) [[unlikely]] {
        return f;
    }
    env_->written = true;
    env_->log_written = true;
    throw_if_an_entry_has_a_field_of_another_kind(entries);
    const auto l = env_->of_writer();
    auto m = read_meta(l);
    if (!m) [[unlikely]] {
        return std::unexpected(m.error());
    }
    if (auto put = put_entries(l, *m, env_->record, entries); !put) [[unlikely]] {
        return put;
    }
    return write_meta(l, *m);
}

std::expected<void, std::error_code>
persistent_storage::transaction::append_on_prefix(const std::uint64_t from_idx, const std::span<const entry> entries)
{
    throw_if_no_environment(env_);
    throw_if_nested_is_open(env_->nested_open);
    if (auto f = refuse_after_a_failure(env_->of_writer()); !f) [[unlikely]] {
        return f;
    }
    env_->written = true;
    env_->log_written = true;
    throw_if_an_entry_has_a_field_of_another_kind(entries);
    const auto l = env_->of_writer();
    auto m = read_meta(l);
    if (!m) [[unlikely]] {
        return std::unexpected(m.error());
    }
    const auto prefix_len = std::min(m->log_len, from_idx);
    if (prefix_len < m->log_len) {
        if (auto deleted = delete_entries(l, prefix_len, m->log_len); !deleted) [[unlikely]] {
            return deleted;
        }
        const auto chain = chain_crc_before(l, prefix_len);
        if (!chain) [[unlikely]] {
            return std::unexpected(chain.error());
        }
        m->log_len = prefix_len;
        m->last_chain_crc = *chain;
    }
    if (auto put = put_entries(l, *m, env_->record, entries); !put) [[unlikely]] {
        return put;
    }
    return write_meta(l, *m);
}

std::expected<void, std::error_code> persistent_storage::transaction::set_promise(const ballot promise)
{
    throw_if_no_environment(env_);
    throw_if_nested_is_open(env_->nested_open);
    if (auto f = refuse_after_a_failure(env_->of_writer()); !f) [[unlikely]] {
        return f;
    }
    env_->written = true;
    env_->log_written = true;
    const auto l = env_->of_writer();
    auto m = read_meta(l);
    if (!m) [[unlikely]] {
        return std::unexpected(m.error());
    }
    m->promise = promise;
    return write_meta(l, *m);
}

std::expected<void, std::error_code> persistent_storage::transaction::set_accepted_round(const ballot accepted_round)
{
    throw_if_no_environment(env_);
    throw_if_nested_is_open(env_->nested_open);
    if (auto f = refuse_after_a_failure(env_->of_writer()); !f) [[unlikely]] {
        return f;
    }
    env_->written = true;
    env_->log_written = true;
    const auto l = env_->of_writer();
    auto m = read_meta(l);
    if (!m) [[unlikely]] {
        return std::unexpected(m.error());
    }
    m->accepted_round = accepted_round;
    return write_meta(l, *m);
}

std::expected<void, std::error_code> persistent_storage::transaction::set_decided_index(const std::uint64_t decided_idx)
{
    throw_if_no_environment(env_);
    throw_if_nested_is_open(env_->nested_open);
    if (auto f = refuse_after_a_failure(env_->of_writer()); !f) [[unlikely]] {
        return f;
    }
    env_->written = true;
    const auto l = env_->of_writer();
    auto m = read_meta(l);
    if (!m) [[unlikely]] {
        return std::unexpected(m.error());
    }
    if (decided_idx > m->log_len) [[unlikely]] {
        return std::unexpected(make_error_code(storage_errc::missing_entry));
    }
    if (decided_idx > m->decided_idx) {
        if (auto c = set_entry_states(l, env_->record, m->decided_idx, decided_idx, decided_idx); !c) [[unlikely]] {
            return c;
        }
    }
    m->decided_idx = decided_idx;
    return write_meta(l, *m);
}

std::expected<void, std::error_code>
persistent_storage::transaction::set_config_first_idx(const std::uint64_t config_first_idx)
{
    throw_if_no_environment(env_);
    throw_if_nested_is_open(env_->nested_open);
    if (auto f = refuse_after_a_failure(env_->of_writer()); !f) [[unlikely]] {
        return f;
    }
    env_->written = true;
    env_->log_written = true;
    return put_checked(env_->of_writer(), config_first_key, to_little_endian(config_first_idx));
}

std::expected<std::optional<std::span<const std::byte>>, std::error_code>
persistent_storage::transaction::find(const std::span<const std::byte> key) const
{
    throw_if_no_environment(env_);
    throw_if_nested_is_open(env_->nested_open);
    if (auto f = refuse_after_a_failure(env_->of_writer()); !f) [[unlikely]] {
        return std::unexpected(f.error());
    }
    return find_value(env_->of_writer(), key);
}

std::expected<std::optional<session_record>, std::error_code>
persistent_storage::transaction::find_session(const std::uint64_t session) const
{
    throw_if_no_environment(env_);
    throw_if_nested_is_open(env_->nested_open);
    if (auto f = refuse_after_a_failure(env_->of_writer()); !f) [[unlikely]] {
        return std::unexpected(f.error());
    }
    return find_session_in(env_->of_writer(), session);
}

std::expected<void, std::error_code>
persistent_storage::transaction::insert_or_assign(const std::span<const std::byte> key,
                                                  const std::span<const std::byte> value)
{
    throw_if_no_environment(env_);
    throw_if_nested_is_open(env_->nested_open);
    if (auto f = refuse_after_a_failure(env_->of_writer()); !f) [[unlikely]] {
        return f;
    }
    env_->written = true;
    return put_value(env_->of_writer(), key, value);
}

std::expected<void, std::error_code> persistent_storage::transaction::erase(const std::span<const std::byte> key)
{
    throw_if_no_environment(env_);
    throw_if_nested_is_open(env_->nested_open);
    if (auto f = refuse_after_a_failure(env_->of_writer()); !f) [[unlikely]] {
        return f;
    }
    return erase_value(env_->of_writer(), key).transform([this](const bool erased) {
        env_->written = env_->written || erased;
    });
}

std::expected<void, std::error_code>
persistent_storage::transaction::insert_or_assign_session(const std::uint64_t session, const session_record& record)
{
    throw_if_no_environment(env_);
    throw_if_nested_is_open(env_->nested_open);
    if (auto f = refuse_after_a_failure(env_->of_writer()); !f) [[unlikely]] {
        return f;
    }
    env_->written = true;
    return put_session(env_->of_writer(), env_->record, session, record);
}

std::expected<void, std::error_code> persistent_storage::transaction::erase_session(const std::uint64_t session)
{
    throw_if_no_environment(env_);
    throw_if_nested_is_open(env_->nested_open);
    if (auto f = refuse_after_a_failure(env_->of_writer()); !f) [[unlikely]] {
        return f;
    }
    return erase_session_in(env_->of_writer(), session).transform([this](const bool erased) {
        env_->written = env_->written || erased;
    });
}

std::expected<void, std::error_code> persistent_storage::transaction::set_applied(const std::uint64_t applied_idx,
                                                                                  const std::uint32_t applied_chain_crc)
{
    throw_if_no_environment(env_);
    throw_if_nested_is_open(env_->nested_open);
    if (auto f = refuse_after_a_failure(env_->of_writer()); !f) [[unlikely]] {
        return f;
    }
    env_->written = true;
    const auto l = env_->of_writer();
    auto m = read_meta(l);
    if (!m) [[unlikely]] {
        return std::unexpected(m.error());
    }
    m->applied_idx = applied_idx;
    m->applied_chain_crc = applied_chain_crc;
    return write_meta(l, *m);
}

std::expected<persistent_storage::nested_transaction, std::error_code> persistent_storage::transaction::begin_nested()
{
    throw_if_no_environment(env_);
    throw_if_nested_is_open(env_->nested_open);
    if (auto f = refuse_after_a_failure(env_->of_writer()); !f) [[unlikely]] {
        return std::unexpected(f.error());
    }
    reset_statements(env_->writer);
    if (auto r = run(env_->of_writer(), env_->writer.savepoint.get()); !r) [[unlikely]] {
        return std::unexpected(r.error());
    }
    env_->nested_open = true;
    return nested_transaction{env_};
}

bool persistent_storage::transaction::has_writes() const
{
    throw_if_no_environment(env_);
    return env_->written;
}

bool persistent_storage::transaction::has_log_writes() const
{
    throw_if_no_environment(env_);
    return env_->log_written;
}

std::expected<void, std::error_code> persistent_storage::transaction::commit() &&
{
    throw_if_no_environment(env_);
    sqlite3* const db = env_->writer_db.get();
    if (auto c = std::move(*this).commit_without_a_sync(); !c) [[unlikely]] {
        return c;
    }
    return sync_the_wal(db);
}

std::expected<void, std::error_code> persistent_storage::transaction::commit_without_a_sync() &&
{
    throw_if_no_environment(env_);
    throw_if_nested_is_open(env_->nested_open);
    auto& env = *std::exchange(env_, nullptr);
    env.writer_open = false;
    if (env.failure) [[unlikely]] {
        return std::unexpected(*env.failure);
    }
    reset_statements(env.writer);
    sqlite3_stmt* const s = env.writer.commit.get();
    env.write_unfinished = true;
    const int status = sqlite3_step(s);
    sqlite3_reset(s);
    if (status != SQLITE_DONE) [[unlikely]] {
        env.roll_back_the_writer();
        const auto e = error_code_or_throw(status, env.writer_db.get(), access::write);
        env.write_unfinished = false;
        return std::unexpected(e);
    }
    env.write_unfinished = false;
    return {};
}

persistent_storage::nested_transaction::nested_transaction(environment* const env) noexcept : env_{env}
{
}

persistent_storage::nested_transaction::nested_transaction(nested_transaction&& other) noexcept
    : env_{std::exchange(other.env_, nullptr)}
{
}

persistent_storage::nested_transaction&
persistent_storage::nested_transaction::operator=(nested_transaction&& other) noexcept
{
    if (this != &other) {
        if (env_ != nullptr) {
            env_->roll_back_the_nested();
        }
        env_ = std::exchange(other.env_, nullptr);
    }
    return *this;
}

persistent_storage::nested_transaction::~nested_transaction()
{
    if (env_ != nullptr) {
        env_->roll_back_the_nested();
    }
}

std::expected<std::optional<std::span<const std::byte>>, std::error_code>
persistent_storage::nested_transaction::find(const std::span<const std::byte> key) const
{
    throw_if_no_environment(env_);
    if (auto f = refuse_after_a_failure(env_->of_writer()); !f) [[unlikely]] {
        return std::unexpected(f.error());
    }
    return find_value(env_->of_writer(), key);
}

std::expected<std::optional<session_record>, std::error_code>
persistent_storage::nested_transaction::find_session(const std::uint64_t session) const
{
    throw_if_no_environment(env_);
    if (auto f = refuse_after_a_failure(env_->of_writer()); !f) [[unlikely]] {
        return std::unexpected(f.error());
    }
    return find_session_in(env_->of_writer(), session);
}

std::expected<void, std::error_code>
persistent_storage::nested_transaction::insert_or_assign(const std::span<const std::byte> key,
                                                         const std::span<const std::byte> value)
{
    throw_if_no_environment(env_);
    if (auto f = refuse_after_a_failure(env_->of_writer()); !f) [[unlikely]] {
        return f;
    }
    return put_value(env_->of_writer(), key, value);
}

std::expected<void, std::error_code> persistent_storage::nested_transaction::erase(const std::span<const std::byte> key)
{
    throw_if_no_environment(env_);
    if (auto f = refuse_after_a_failure(env_->of_writer()); !f) [[unlikely]] {
        return f;
    }
    return erase_value(env_->of_writer(), key).transform([](bool) {});
}

std::expected<void, std::error_code>
persistent_storage::nested_transaction::insert_or_assign_session(const std::uint64_t session,
                                                                 const session_record& record)
{
    throw_if_no_environment(env_);
    if (auto f = refuse_after_a_failure(env_->of_writer()); !f) [[unlikely]] {
        return f;
    }
    return put_session(env_->of_writer(), env_->record, session, record);
}

std::expected<void, std::error_code> persistent_storage::nested_transaction::erase_session(const std::uint64_t session)
{
    throw_if_no_environment(env_);
    if (auto f = refuse_after_a_failure(env_->of_writer()); !f) [[unlikely]] {
        return f;
    }
    return erase_session_in(env_->of_writer(), session).transform([](bool) {});
}

persistent_storage::persistent_storage(persistent_storage&&) noexcept = default;
persistent_storage& persistent_storage::operator=(persistent_storage&&) noexcept = default;

persistent_storage::~persistent_storage()
{
    if (env_ && (env_->writer_open || env_->nested_open || env_->reader_open)) [[unlikely]] {
        std::terminate();
    }
}

persistent_storage::persistent_storage(std::unique_ptr<environment> env) noexcept : env_{std::move(env)}
{
}

void persistent_storage::throw_if_moved_from() const
{
    if (!env_) [[unlikely]] {
        throw std::logic_error("The persistent_storage was moved from and has no database");
    }
}

std::expected<persistent_storage, std::error_code> persistent_storage::open(const std::filesystem::path& directory,
                                                                            const std::size_t max_size,
                                                                            const uid_t effective_user_id)
{
    if (effective_user_id == root_user_id) [[unlikely]] {
        return std::unexpected(std::make_error_code(std::errc::operation_not_permitted));
    }
    const auto file = directory / store_file_name;
    if (auto created = create_with_file_mode(file); !created) [[unlikely]] {
        return std::unexpected(created.error());
    }
    auto writer_db = open_connection(file, synchronous_normal);
    if (!writer_db) [[unlikely]] {
        return std::unexpected(writer_db.error());
    }
    if (auto bound = set_max_page_count(writer_db->get(), max_size); !bound) [[unlikely]] {
        return std::unexpected(bound.error());
    }
    if (auto schema = create_the_schema_of_an_empty_file(writer_db->get()); !schema) [[unlikely]] {
        return std::unexpected(schema.error());
    }
    auto writer = prepare_statements(writer_db->get());
    if (!writer) [[unlikely]] {
        return std::unexpected(make_error_code(storage_errc::unsupported_format));
    }
    auto reader_db = open_connection(file, synchronous_full);
    if (!reader_db) [[unlikely]] {
        return std::unexpected(reader_db.error());
    }
    auto reader = prepare_statements(reader_db->get());
    if (!reader) [[unlikely]] {
        return std::unexpected(make_error_code(storage_errc::unsupported_format));
    }
    auto env = std::make_unique<environment>(std::move(*writer_db), std::move(*reader_db), std::move(*writer),
                                             std::move(*reader));
    sqlite3_wal_hook(env->writer_db.get(), count_frames, &env->frames);
    if (auto format = check_or_write_format(env->of_writer()); !format) [[unlikely]] {
        env->roll_back_the_writer();
        return std::unexpected(format.error());
    }
    if (auto synced = sync_the_wal(env->writer_db.get()); !synced) [[unlikely]] {
        return std::unexpected(synced.error());
    }
    env->failure.reset();
    return persistent_storage{std::move(env)};
}

std::expected<persistent_storage::transaction, std::error_code> persistent_storage::begin()
{
    throw_if_moved_from();
    if (env_->writer_open) [[unlikely]] {
        throw std::logic_error("konsens: a write transaction of this storage is open");
    }
    throw_if_a_write_did_not_finish(env_->write_unfinished);
    env_->failure.reset();
    if (env_->frames >= checkpoint_frames) {
        env_->write_unfinished = true;
        const auto c = run_checkpoint(env_->writer_db.get());
        env_->write_unfinished = false;
        if (!c) [[unlikely]] {
            return std::unexpected(c.error());
        }
        env_->frames = 0;
    }
    if (auto begun = run(env_->of_writer(), env_->writer.begin_immediate.get()); !begun) [[unlikely]] {
        env_->failure.reset();
        return std::unexpected(begun.error());
    }
    env_->writer_open = true;
    env_->written = false;
    env_->log_written = false;
    return transaction{env_.get()};
}

std::expected<persistent_storage::read_only_transaction, std::error_code> persistent_storage::begin_read_only() const
{
    throw_if_moved_from();
    if (env_->reader_open) [[unlikely]] {
        throw std::logic_error("konsens: a read-only transaction of this storage is open");
    }
    const auto l = env_->of_reader();
    if (auto begun = run(l, env_->reader.begin_deferred.get()); !begun) [[unlikely]] {
        return std::unexpected(begun.error());
    }
    env_->reader_open = true;
    const auto m = read_meta(l);
    if (!m && m.error().category() != storage_category()) [[unlikely]] {
        env_->end_the_reader();
        return std::unexpected(m.error());
    }
    return read_only_transaction{env_.get()};
}

std::expected<void, std::error_code> persistent_storage::copy(const std::filesystem::path& directory) const
{
    throw_if_moved_from();
    if (env_->reader_open) [[unlikely]] {
        throw std::logic_error("konsens: a read-only transaction of this storage is open");
    }
    const auto file = directory / store_file_name;
    if (auto created = create_new_with_file_mode(file); !created) [[unlikely]] {
        return std::unexpected(created.error());
    }
    const auto name = file.string();
    sqlite3_stmt* const s = env_->reader.vacuum_into.get();
    sqlite3_reset(s);
    if (const int bound = bind_text(s, 1, name); bound != SQLITE_OK) [[unlikely]] {
        return std::unexpected(error_code_or_throw(bound, env_->reader_db.get(), access::read));
    }
    return run(env_->of_reader(), s);
}

struct node_storage::environment {
    connection db;
    statement begin_immediate;
    statement commit;
    statement rollback;
    statement get_record;
    statement put_record;
    statement delete_record;
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
        throw std::logic_error("The node_storage was moved from and has no database");
    }
}

std::expected<node_storage, std::error_code> node_storage::open(const std::filesystem::path& directory,
                                                                const uid_t effective_user_id)
{
    if (effective_user_id == root_user_id) [[unlikely]] {
        return std::unexpected(std::make_error_code(std::errc::operation_not_permitted));
    }
    const auto file = directory / node_file_name;
    if (auto created = create_with_file_mode(file); !created) [[unlikely]] {
        return std::unexpected(created.error());
    }
    auto db = open_connection(file, synchronous_full);
    if (!db) [[unlikely]] {
        return std::unexpected(db.error());
    }
    if (auto schema = execute(db->get(), node_schema); !schema) [[unlikely]] {
        return std::unexpected(schema.error());
    }
    auto env = std::make_unique<environment>(std::move(*db));
    const std::array list{
        std::pair{&env->begin_immediate, std::string_view{"BEGIN IMMEDIATE"}},
        std::pair{&env->commit, std::string_view{"COMMIT"}},
        std::pair{&env->rollback, std::string_view{"ROLLBACK"}},
        std::pair{&env->get_record, std::string_view{"SELECT record FROM meta WHERE name = ?1"}},
        std::pair{&env->put_record,
                  std::string_view{"INSERT INTO meta (name, record) VALUES (?1, ?2) "
                                   "ON CONFLICT (name) DO UPDATE SET record = excluded.record"}},
        std::pair{&env->delete_record, std::string_view{"DELETE FROM meta WHERE name = ?1"}},
    };
    for (const auto& [target, sql] : list) {
        auto prepared = prepare(env->db.get(), sql);
        if (!prepared) [[unlikely]] {
            return std::unexpected(to_error_code(prepared.error(), env->db.get()));
        }
        *target = std::move(*prepared);
    }
    return node_storage{std::move(env)};
}

std::expected<std::optional<fault_proof>, std::error_code> node_storage::get_fault_proof() const
{
    throw_if_moved_from();
    sqlite3_stmt* const s = env_->get_record.get();
    sqlite3_reset(s);
    if (const int bound = bind_text(s, 1, fault_proof_key); bound != SQLITE_OK) [[unlikely]] {
        return std::unexpected(error_code_or_throw(bound, env_->db.get(), access::read));
    }
    const int status = sqlite3_step(s);
    if (status == SQLITE_DONE) {
        sqlite3_reset(s);
        return std::nullopt;
    }
    if (status != SQLITE_ROW) [[unlikely]] {
        sqlite3_reset(s);
        return std::unexpected(error_code_or_throw(status, env_->db.get(), access::read));
    }
    const auto bytes = column_bytes(s, 0);
    if (bytes.size() != fault_proof_size + crc_size) [[unlikely]] {
        sqlite3_reset(s);
        return std::unexpected(make_error_code(storage_errc::malformed_record));
    }
    const auto body = bytes.first<fault_proof_size>();
    if (from_little_endian<std::uint32_t>(bytes.last<crc_size>()) != crc_of(body)) [[unlikely]] {
        sqlite3_reset(s);
        return std::unexpected(make_error_code(storage_errc::crc_mismatch));
    }
    const auto kind = std::to_integer<std::uint8_t>(body[proof_kind_offset]);
    if (kind < std::to_underlying(proof_kind::damage) || kind > std::to_underlying(last_proof_kind)) [[unlikely]] {
        sqlite3_reset(s);
        return std::unexpected(make_error_code(storage_errc::malformed_record));
    }
    const fault_proof proof{
        static_cast<proof_kind>(kind),
        from_little_endian<std::uint64_t>(body.subspan<proof_index_offset, sizeof(fault_proof::index)>()),
        decode_ballot(body.subspan<proof_ballot_offset, ballot_size>()),
        from_little_endian<std::uint32_t>(body.subspan<proof_crc_offset, sizeof(fault_proof::crc)>()),
        from_little_endian<std::uint32_t>(body.subspan<proof_other_crc_offset, sizeof(fault_proof::other_crc)>()),
    };
    sqlite3_reset(s);
    return proof;
}

std::expected<void, std::error_code> node_storage::write_the_proof(const fault_proof& proof)
{
    throw_if_moved_from();
    std::array<std::byte, fault_proof_size + crc_size> record{};
    record[proof_kind_offset] = std::byte{std::to_underlying(proof.kind)};
    std::ranges::copy(to_little_endian(proof.index), std::span(record).subspan<proof_index_offset>().begin());
    std::ranges::copy(encode_ballot(proof.n), std::span(record).subspan<proof_ballot_offset>().begin());
    std::ranges::copy(to_little_endian(proof.crc), std::span(record).subspan<proof_crc_offset>().begin());
    std::ranges::copy(to_little_endian(proof.other_crc), std::span(record).subspan<proof_other_crc_offset>().begin());
    std::ranges::copy(to_little_endian(crc_of(std::span(record).first<fault_proof_size>())),
                      std::span(record).subspan<fault_proof_size>().begin());
    sqlite3_stmt* const s = env_->put_record.get();
    sqlite3_reset(s);
    if (const int bound = bind_text(s, 1, fault_proof_key); bound != SQLITE_OK) [[unlikely]] {
        return std::unexpected(error_code_or_throw(bound, env_->db.get(), access::write));
    }
    if (const int bound = bind_bytes(s, 2, record); bound != SQLITE_OK) [[unlikely]] {
        return std::unexpected(error_code_or_throw(bound, env_->db.get(), access::write));
    }
    const int status = sqlite3_step(s);
    sqlite3_reset(s);
    if (status != SQLITE_DONE) [[unlikely]] {
        return std::unexpected(error_code_or_throw(status, env_->db.get(), access::write));
    }
    return {};
}

}

namespace konsens {

std::expected<std::optional<clean_stop_mark>, std::error_code> node_storage::get_clean_stop_mark() const
{
    throw_if_moved_from();
    sqlite3_stmt* const s = env_->get_record.get();
    sqlite3_reset(s);
    if (const int bound = bind_text(s, 1, clean_stop_key); bound != SQLITE_OK) [[unlikely]] {
        return std::unexpected(error_code_or_throw(bound, env_->db.get(), access::read));
    }
    const int status = sqlite3_step(s);
    if (status == SQLITE_DONE) {
        sqlite3_reset(s);
        return std::nullopt;
    }
    if (status != SQLITE_ROW) [[unlikely]] {
        sqlite3_reset(s);
        return std::unexpected(error_code_or_throw(status, env_->db.get(), access::read));
    }
    const auto bytes = column_bytes(s, 0);
    if (bytes.size() != clean_stop_mark_size + crc_size) [[unlikely]] {
        sqlite3_reset(s);
        return std::unexpected(make_error_code(storage_errc::malformed_record));
    }
    const auto body = bytes.first<clean_stop_mark_size>();
    if (from_little_endian<std::uint32_t>(bytes.last<crc_size>()) != crc_of(body)) [[unlikely]] {
        sqlite3_reset(s);
        return std::unexpected(make_error_code(storage_errc::crc_mismatch));
    }
    const clean_stop_mark mark{
        from_little_endian<node_id>(body.subspan<mark_pid_offset, sizeof(clean_stop_mark::pid)>()),
        from_little_endian<std::uint32_t>(
            body.subspan<mark_configuration_id_offset, sizeof(clean_stop_mark::configuration_id)>()),
        decode_meta(body.subspan<mark_meta_offset, meta_record_size>()),
        from_little_endian<std::uint64_t>(
            body.subspan<mark_verified_idx_offset, sizeof(clean_stop_mark::verified_idx)>()),
    };
    sqlite3_reset(s);
    return mark;
}

std::expected<void, std::error_code> node_storage::write_the_clean_stop_mark(const clean_stop_mark& mark)
{
    throw_if_moved_from();
    std::array<std::byte, clean_stop_mark_size + crc_size> record{};
    auto next = std::ranges::copy(to_little_endian(mark.pid), record.begin()).out;
    next = std::ranges::copy(to_little_endian(mark.configuration_id), next).out;
    next = std::ranges::copy(encode_meta(mark.meta), next).out;
    next = std::ranges::copy(to_little_endian(mark.verified_idx), next).out;
    std::ranges::copy(to_little_endian(crc_of(std::span(record).first<clean_stop_mark_size>())), next);
    sqlite3_stmt* const s = env_->put_record.get();
    sqlite3_reset(s);
    if (const int bound = bind_text(s, 1, clean_stop_key); bound != SQLITE_OK) [[unlikely]] {
        return std::unexpected(error_code_or_throw(bound, env_->db.get(), access::write));
    }
    if (const int bound = bind_bytes(s, 2, record); bound != SQLITE_OK) [[unlikely]] {
        return std::unexpected(error_code_or_throw(bound, env_->db.get(), access::write));
    }
    const int status = sqlite3_step(s);
    sqlite3_reset(s);
    if (status != SQLITE_DONE) [[unlikely]] {
        return std::unexpected(error_code_or_throw(status, env_->db.get(), access::write));
    }
    return {};
}

std::expected<void, std::error_code> node_storage::delete_the_clean_stop_mark()
{
    throw_if_moved_from();
    sqlite3_stmt* const s = env_->delete_record.get();
    sqlite3_reset(s);
    if (const int bound = bind_text(s, 1, clean_stop_key); bound != SQLITE_OK) [[unlikely]] {
        return std::unexpected(error_code_or_throw(bound, env_->db.get(), access::write));
    }
    const int status = sqlite3_step(s);
    sqlite3_reset(s);
    if (status != SQLITE_DONE) [[unlikely]] {
        return std::unexpected(error_code_or_throw(status, env_->db.get(), access::write));
    }
    return {};
}

}
