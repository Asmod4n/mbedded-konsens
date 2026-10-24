#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <spawn.h>
#include <sys/wait.h>

#include "konsens/persistent_storage.hpp"
#include "temporary_directory.hpp"

namespace {

// The session that runs these tests runs as root. The open function takes
// the effective user id as a value, so that the tests can give a user id
// that the refusal of root does not hold.
constexpr uid_t non_root_user_id = 1000;
constexpr std::size_t map_size = std::size_t{16} << 20;
constexpr std::size_t entry_count = 300;
constexpr std::size_t largest_payload = 5000;

// C++ has no form to start a process. The test starts each LMDB program
// with posix_spawn and the environment that it names, and requires exit
// status 0.
void run(const std::vector<std::string>& arguments, const std::vector<std::string>& environment = {})
{
    std::vector<char*> argv;
    for (const auto& a : arguments) {
        argv.push_back(const_cast<char*>(a.c_str()));
    }
    argv.push_back(nullptr);
    std::vector<char*> envp;
    for (const auto& e : environment) {
        envp.push_back(const_cast<char*>(e.c_str()));
    }
    envp.push_back(nullptr);
    pid_t pid = 0;
    REQUIRE(posix_spawn(&pid, argv.front(), nullptr, nullptr, argv.data(), envp.data()) == 0);
    int status = 0;
    REQUIRE(waitpid(pid, &status, 0) == pid);
    REQUIRE(WIFEXITED(status));
    REQUIRE(WEXITSTATUS(status) == 0);
}

std::vector<std::string> lines_of(const std::filesystem::path& file)
{
    std::ifstream in(file);
    std::vector<std::string> lines;
    for (std::string line; std::getline(in, line);) {
        lines.push_back(line);
    }
    return lines;
}

bool has_mapaddr(const std::vector<std::string>& lines)
{
    return std::ranges::any_of(lines, [](const std::string& l) { return l.starts_with("mapaddr="); });
}

// mdb_dump of LMDB 1.0.2 writes the address of its live map as mapaddr, and
// mdb_load then sets MDB_FIXEDMAP in the new file. A database that
// persistent_storage wrote with LMDB 0.9 has no such address. The test
// removes the line, so that the 0.9 database is the one an upgrade meets.
void remove_mapaddr(const std::filesystem::path& file)
{
    const auto lines = lines_of(file);
    std::ofstream out(file, std::ios::trunc);
    for (const auto& line :
         lines | std::views::filter([](const std::string& l) { return !l.starts_with("mapaddr="); })) {
        out << line << '\n';
    }
}

std::vector<konsens::entry> entries_to_write()
{
    std::vector<konsens::entry> entries;
    for (std::size_t i = 0; i < entry_count; ++i) {
        std::vector<std::byte> payload(i * largest_payload / entry_count + 1);
        for (std::size_t j = 0; j < payload.size(); ++j) {
            payload.at(j) = static_cast<std::byte>(i + j);
        }
        entries.push_back({konsens::entry_kind::normal, std::move(payload)});
    }
    entries.push_back({konsens::entry_kind::stop_sign, {}});
    return entries;
}

}

// LMDB 1.0 does not open a data file of LMDB 0.9 (upgrading.doc). The
// upgrade is mdb_dump of 0.9 and mdb_load of 1.0. This test makes a
// database in the 0.9 format with the data of persistent_storage, converts
// it, and checks that each entry, its CRC32C and each meta value came over.
TEST_CASE("mdb_dump of LMDB 0.9 and mdb_load of LMDB 1.0 keep every entry and meta value")
{
    const temporary_directory dir;
    const auto written = dir.path() / "written";
    const auto old_format = dir.path() / "old";
    const auto upgraded = dir.path() / "upgraded";
    for (const auto& d : {written, old_format, upgraded}) {
        std::filesystem::create_directory(d);
    }
    const auto entries = entries_to_write();
    const konsens::ballot promise{1, 7, 3, 2};
    const konsens::ballot accepted{1, 6, 3, 1};
    std::uint32_t last_crc = 0;
    {
        auto opened = konsens::persistent_storage::open(written, map_size, non_root_user_id);
        REQUIRE(opened.has_value());
        const std::vector<konsens::storage_op> ops{konsens::append_entries{entries}, konsens::set_promise{promise},
                                                   konsens::set_decided_index{entry_count},
                                                   konsens::set_accepted_round{accepted}};
        REQUIRE(opened->write_atomically(ops).has_value());
        const auto crc = opened->get_last_entry_crc();
        REQUIRE(crc.has_value());
        last_crc = *crc;
    }

    const auto new_dump = dir.path() / "new.dump";
    run({KONSENS_MDB_DUMP, "-a", "-f", new_dump.string(), written.string()});
    remove_mapaddr(new_dump);
    run({KONSENS_MDB_LOAD_0_9, "-f", new_dump.string(), old_format.string()});

    const auto refused = konsens::persistent_storage::open(old_format, map_size, non_root_user_id);
    REQUIRE_FALSE(refused.has_value());
    CHECK(refused.error() != std::errc::no_such_file_or_directory);

    const auto old_dump = dir.path() / "old.dump";
    run({KONSENS_MDB_DUMP_0_9, "-a", "-f", old_dump.string(), old_format.string()},
        {"LSAN_OPTIONS=suppressions=" KONSENS_LSAN_MDB_DUMP_0_9});
    CHECK_FALSE(has_mapaddr(lines_of(old_dump)));
    run({KONSENS_MDB_LOAD, "-f", old_dump.string(), upgraded.string()});

    const auto storage = konsens::persistent_storage::open(upgraded, map_size, non_root_user_id);
    REQUIRE(storage.has_value());
    CHECK(storage->get_log_len() == entries.size());
    const auto read = storage->read_entries(0, entries.size());
    REQUIRE(read.has_value());
    CHECK(*read == entries);
    CHECK(storage->get_last_entry_crc() == last_crc);
    CHECK(storage->get_promise() == promise);
    CHECK(storage->get_decided_idx() == entry_count);
    CHECK(storage->get_accepted_round() == accepted);
}
