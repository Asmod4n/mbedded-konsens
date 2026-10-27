#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <sys/types.h>
#include <unistd.h>

#include "konsens/persistent_storage.hpp"
#include "mounted_error_fs.hpp"
#include "node_cluster.hpp"
#include "sqlite_damage.hpp"
#include "temporary_directory.hpp"

namespace {

// The session that runs these tests runs as root, and open refuses root.
constexpr uid_t non_root_user_id = 1000;
constexpr std::size_t map_size = std::size_t{16} << 20;
constexpr std::size_t large_entry_size = std::size_t{64} << 10;
constexpr int pattern_period = 251;

std::vector<std::byte> bytes_of(const std::string_view text)
{
    std::vector<std::byte> result;
    std::ranges::transform(text, std::back_inserter(result), [](const char c) { return static_cast<std::byte>(c); });
    return result;
}

konsens::persistent_storage open_storage(const std::filesystem::path& directory)
{
    auto opened = konsens::persistent_storage::open(directory, map_size, non_root_user_id);
    REQUIRE(opened.has_value());
    return std::move(*opened);
}

void write_entries(const std::filesystem::path& directory, const std::vector<konsens::entry>& entries)
{
    auto storage = open_storage(directory);
    auto t = storage.begin();
    REQUIRE(t.has_value());
    REQUIRE(t->append_entries(entries).has_value());
    REQUIRE(std::move(*t).commit().has_value());
}

// A payload that does not repeat inside one period, so that the test finds
// it in konsens.db.
std::vector<std::byte> pattern_payload()
{
    std::vector<std::byte> payload(large_entry_size);
    for (std::size_t i = 0; i < payload.size(); ++i) {
        payload.at(i) = static_cast<std::byte>(i % pattern_period);
    }
    return payload;
}

struct byte_range {
    std::uint64_t offset;
    std::uint64_t length;
};

// The whole pages of the file that lie inside the payload. SQLite keeps the
// part of a large record that does not fit its leaf page on overflow pages
// of its own, so a read of these pages is a read of this entry and of
// nothing else.
byte_range pages_inside(const std::filesystem::path& data_file, const std::vector<std::byte>& payload)
{
    std::ifstream in(data_file, std::ios::binary);
    const std::vector<char> chars{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    std::vector<std::byte> file(chars.size());
    std::ranges::transform(chars, file.begin(), [](const char c) { return static_cast<std::byte>(c); });
    const auto found = std::ranges::search(file, std::span(payload).first(pattern_period));
    REQUIRE_FALSE(found.empty());
    const auto start = static_cast<std::uint64_t>(std::distance(file.begin(), found.begin()));
    const auto page = static_cast<std::uint64_t>(sysconf(_SC_PAGESIZE));
    const auto first = (start + page - 1) / page * page;
    const auto end = (start + payload.size()) / page * page;
    REQUIRE(end > first);
    return {first, end - first};
}

struct mount_directories {
    mount_directories()
    {
        std::filesystem::create_directory(backing);
        std::filesystem::create_directory(mount_point);
    }

    temporary_directory dir;
    std::filesystem::path backing = dir.path() / "backing";
    std::filesystem::path mount_point = dir.path() / "mount";
};

}

// A disk that fails a sync after a write is the case of the fsync error of
// PostgreSQL in 2018: the page cache can drop the written pages, so a
// second sync can report success for data that is gone. The owner decided
// that a failed sync is raised as sync_error and that nothing syncs again.
// SQLite writes the frames of the commit into the WAL before its sync, so
// the disk keeps the last commit, and it can keep the commit whose sync
// failed. That commit was never confirmed to a client.
TEST_CASE("a failed fsync at commit throws sync_error and keeps the last commit")
{
    const mount_directories d;
    const std::vector<konsens::entry> written{{konsens::entry_kind::normal, bytes_of("first")},
                                              {konsens::entry_kind::normal, bytes_of("second")}};
    {
        mounted_error_fs clean(d.backing, d.mount_point, "");
        write_entries(d.mount_point, written);
        clean.unmount();
    }
    std::optional<std::error_code> thrown;
    {
        mounted_error_fs failing(d.backing, d.mount_point, "error_fsync");
        {
            auto storage = open_storage(d.mount_point);
            const std::vector<konsens::entry> later{{konsens::entry_kind::normal, bytes_of("later")}};
            auto t = storage.begin();
            REQUIRE(t.has_value());
            REQUIRE(t->append_entries(later).has_value());
            try {
                static_cast<void>(std::move(*t).commit());
            } catch (const konsens::sync_error& e) {
                thrown = e.code();
            }
        }
        failing.unmount();
    }
    REQUIRE(thrown);
    CHECK(*thrown == std::errc::io_error);
    mounted_error_fs clean(d.backing, d.mount_point, "");
    {
        const auto storage = open_storage(d.mount_point);
        const auto t = storage.begin_read_only();
        REQUIRE(t.has_value());
        const auto m = t->get_meta();
        REQUIRE(m.has_value());
        CHECK((m->log_len == written.size() || m->log_len == written.size() + 1));
        CHECK(t->get_entries(0, written.size()) == written);
    }
    clean.unmount();
}

// SQLite reads its file with pread, so a read error of the disk reaches the
// storage as SQLITE_IOERR_READ. The storage raises it as read_error with
// the error code of the disk (plan sqlite-storage 6.1). A read that the
// kernel ends early gives SQLITE_IOERR_SHORT_READ, which SQLite fills with
// zeros, and the B-tree check then gives SQLITE_CORRUPT. Both ends give
// no entry and no crash. The open before the read meets no damaged page.
// The test moves the WAL into the database file first, so that the read
// of the entry is a read of konsens.db.
TEST_CASE("a failed read of an entry throws read_error or gives corrupt")
{
    const mount_directories d;
    const auto payload = pattern_payload();
    const std::vector<konsens::entry> written{{konsens::entry_kind::normal, bytes_of("small")},
                                              {konsens::entry_kind::normal, payload}};
    {
        mounted_error_fs clean(d.backing, d.mount_point, "");
        write_entries(d.mount_point, written);
        clean.unmount();
    }
    sqlite_damage::move_the_wal_into_the_file(d.backing);
    const auto range = pages_inside(d.backing / sqlite_damage::store_file_name, payload);
    mounted_error_fs failing(d.backing, d.mount_point,
                             "error_reads_offset=" + std::to_string(range.offset) +
                                 ",error_reads_length=" + std::to_string(range.length));
    {
        const auto storage = open_storage(d.mount_point);
        const auto t = storage.begin_read_only();
        REQUIRE(t.has_value());
        std::optional<std::error_code> failed;
        try {
            const auto read = t->get_entries(1, 2);
            REQUIRE_FALSE(read.has_value());
            CHECK(read.error() == make_error_code(konsens::storage_errc::corrupt));
            failed = read.error();
        } catch (const konsens::read_error& e) {
            CHECK(e.code() == std::errc::io_error);
            failed = e.code();
        }
        CHECK(failed);
    }
    failing.unmount();
}

namespace {

node_cluster::cluster cluster_on(const std::filesystem::path& mount_point)
{
    std::array<std::shared_ptr<node_cluster::disk>, node_cluster::node_count> disks;
    for (std::size_t i = 0; i < disks.size(); ++i) {
        const auto directory = mount_point / std::to_string(i + 1);
        std::filesystem::create_directory(directory);
        disks.at(i) = node_cluster::disk_in(directory);
    }
    return {.disks = disks, .nodes = {}, .next_nonce = 1};
}

}

// The decision of the owner (2026-10-21) for a write that fails at the
// commit: the commit throws write_error with the error code of the disk.
// The leader writes nothing for a client request before the results of
// Execute-Verify arrive, so the transaction also holds an append, which
// the leader writes at once. None of the messages and replies of the
// transaction leaves the node, because each of them depends on the write
// that did not happen. The
// program does not repeat the commit, so a later step is a wrong use. The
// owner chose (2026-10-24) that the fault comes from the file system under
// the storage: error_fs fails each write of konsens.db and of its WAL with
// ENOSPC while the node runs. SQLite gives SQLITE_FULL for ENOSPC.
TEST_CASE("a write that fails at the commit throws write_error and releases nothing")
{
    using namespace node_cluster;
    const mount_directories d;
    mounted_error_fs fs(d.backing, d.mount_point, "");
    {
        auto c = elected(cluster_on(d.mount_point));
        const auto l = c.leader();
        const auto s = registered(c, l, client_a);
        auto t = value_of(c.nodes.at(l).client_request(client_a, s, 1, {}));
        t += value_of(c.nodes.at(l).append(make_entry(1)));
        t += value_of(c.nodes.at(l).tick());
        fs.set_faults("error_writes");
        bool thrown = false;
        try {
            static_cast<void>(c.nodes.at(l).commit(t));
        } catch (const konsens::write_error& e) {
            thrown = true;
            CHECK(e.code() == std::errc::no_space_on_device);
        }
        CHECK(thrown);
        CHECK(c.nodes.at(l).take_outgoing_messages().empty());
        CHECK(c.nodes.at(l).take_client_replies().empty());
        fs.set_faults("none");
        CHECK_THROWS_AS(static_cast<void>(c.nodes.at(l).commit(value_of(c.nodes.at(l).tick()))), std::logic_error);
        CHECK(c.nodes.at(l).take_outgoing_messages().empty());
    }
    fs.unmount();
}

// The same decision for a sync that fails at the commit, which
// persistent_storage raises as sync_error: nothing in the library catches
// it, and nothing of the transaction leaves the node. error_fs fails each
// fsync while the node runs. The append makes the leader write.
TEST_CASE("a failed sync at the commit throws sync_error and releases nothing")
{
    using namespace node_cluster;
    const mount_directories d;
    mounted_error_fs fs(d.backing, d.mount_point, "");
    {
        auto c = elected(cluster_on(d.mount_point));
        const auto l = c.leader();
        const auto s = registered(c, l, client_a);
        auto t = value_of(c.nodes.at(l).client_request(client_a, s, 1, {}));
        t += value_of(c.nodes.at(l).append(make_entry(1)));
        fs.set_faults("error_fsync");
        CHECK_THROWS_AS(static_cast<void>(c.nodes.at(l).commit(t)), konsens::sync_error);
        CHECK(c.nodes.at(l).take_outgoing_messages().empty());
        CHECK(c.nodes.at(l).take_client_replies().empty());
        fs.set_faults("none");
        CHECK_THROWS_AS(static_cast<void>(c.nodes.at(l).commit(value_of(c.nodes.at(l).tick()))), std::logic_error);
    }
    fs.unmount();
}
