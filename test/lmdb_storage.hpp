#pragma once

#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <sys/types.h>

#include <doctest/doctest.h>

#include "konsens/persistent_storage.hpp"
#include "temporary_directory.hpp"

// The unit tests run on the storage of production, LMDB. Each storage has
// a directory of its own below one directory of the test program, and the
// end of the program removes them all. A test that needs a copy of a
// storage takes the copy that the library makes.
namespace lmdb_storage {

// The session that runs these tests runs as root, and open refuses root.
constexpr uid_t non_root_user_id = 1000;
constexpr std::size_t map_size = std::size_t{16} << 20;

inline const std::filesystem::path& root_directory()
{
    static const temporary_directory root;
    return root.path();
}

inline std::filesystem::path new_directory()
{
    std::string name = (root_directory() / "storage-XXXXXX").string();
    if (mkdtemp(name.data()) == nullptr) [[unlikely]] {
        throw std::system_error(errno, std::generic_category(), "mkdtemp");
    }
    return name;
}

inline konsens::persistent_storage open_in(const std::filesystem::path& directory, const std::size_t size = map_size)
{
    auto opened = konsens::persistent_storage::open(directory, size, non_root_user_id);
    REQUIRE(opened.has_value());
    return std::move(*opened);
}

inline konsens::persistent_storage open_new()
{
    return open_in(new_directory());
}

inline konsens::persistent_storage open_copy_of(const konsens::persistent_storage& storage)
{
    const auto directory = new_directory();
    REQUIRE(storage.copy(directory).has_value());
    return open_in(directory);
}

// A test sets up a storage as a step of a node writes it: every write in
// one write transaction, and one commit at the end.
template <typename Writes>
void write(konsens::persistent_storage& storage, const Writes& writes)
{
    auto t = storage.begin();
    REQUIRE(t.has_value());
    writes(*t);
    REQUIRE(std::move(*t).commit().has_value());
}

inline konsens::meta_record meta_of(const konsens::persistent_storage& storage)
{
    const auto t = storage.begin_read_only();
    REQUIRE(t.has_value());
    const auto m = t->get_meta();
    REQUIRE(m.has_value());
    return *m;
}

inline std::vector<konsens::entry> log_of(const konsens::persistent_storage& storage)
{
    const auto t = storage.begin_read_only();
    REQUIRE(t.has_value());
    const auto m = t->get_meta();
    REQUIRE(m.has_value());
    const auto log = t->get_entries(0, m->log_len);
    REQUIRE(log.has_value());
    return *log;
}

}
