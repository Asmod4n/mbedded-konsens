#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <utility>
#include <variant>

#include <sys/types.h>

#include <doctest/doctest.h>

#include "konsens/node.hpp"
#include "konsens/persistent_storage.hpp"
#include "simulation.hpp"
#include "temporary_directory.hpp"

// Each simulation test is a program of its own, so that ctest runs them in
// parallel. A build with AddressSanitizer runs fewer seeds, so that its run
// stays short (owner, 2026-10-24).
#if defined(__SANITIZE_ADDRESS__)
constexpr std::uint64_t seed_count = 200;
#else
constexpr std::uint64_t seed_count = 2000;
#endif

// The session that runs these tests runs as root, and open refuses root.
// The tests give a user id that the refusal does not hold.
constexpr uid_t non_root_user_id = 1000;
constexpr std::size_t map_size = std::size_t{16} << 20;

// The simulation runs on the storage of production, LMDB. A crash keeps
// nothing in memory: the node and its LMDB environment are closed, and
// start opens the environment again from the files. This is the durable
// state that Crash in SequencePaxos.tla keeps. A rollback restores a
// copy that the library made of the environment while the node
// ran: the state after its last commit, as a lost fsync or a backup.
struct lmdb_cluster {
    using storage_type = konsens::persistent_storage;
    using earlier_type = std::filesystem::path;

    std::array<temporary_directory, simulation::node_count> directories;
    std::deque<temporary_directory> copies;
    std::array<std::filesystem::path, simulation::node_count> paths{directories[0].path(), directories[1].path(),
                                                                    directories[2].path()};

    std::monostate stop(const konsens::node<konsens::persistent_storage, simulation::concatenation>&) const
    {
        return {};
    }

    konsens::persistent_storage start(const std::size_t i, std::monostate) const
    {
        auto opened = konsens::persistent_storage::open(paths.at(i), map_size, non_root_user_id);
        REQUIRE(opened.has_value());
        return std::move(*opened);
    }

    std::filesystem::path keep(std::size_t, const konsens::persistent_storage& running)
    {
        const auto& copy = copies.emplace_back();
        REQUIRE(running.copy(copy.path()).has_value());
        return copy.path();
    }

    konsens::persistent_storage restore(const std::size_t i, const std::filesystem::path& kept)
    {
        paths.at(i) = kept;
        return start(i, {});
    }
};
