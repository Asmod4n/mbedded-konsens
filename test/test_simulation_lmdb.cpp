#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <variant>

#include <sys/types.h>

#include "konsens/node.hpp"
#include "konsens/persistent_storage.hpp"
#include "simulation.hpp"
#include "temporary_directory.hpp"

namespace {

// The session that runs these tests runs as root, and open refuses root.
// The tests give a user id that the refusal does not hold.
constexpr uid_t non_root_user_id = 1000;
constexpr std::size_t map_size = std::size_t{16} << 20;

// Each run commits to disk at each write, so fewer seeds than on
// memory_storage keep the run short.
constexpr std::uint64_t seed_count = 50;

// A crash keeps nothing in memory. The node and its LMDB environment are
// closed, and start opens the environment again from the files on disk.
// This is the reopen that a real restart does.
struct lmdb_cluster {
    using storage_type = konsens::persistent_storage;

    std::array<temporary_directory, simulation::node_count> directories;

    std::monostate stop(const konsens::node<konsens::persistent_storage, simulation::concatenation>&) const
    {
        return {};
    }

    konsens::persistent_storage start(const std::size_t i, std::monostate) const
    {
        auto opened = konsens::persistent_storage::open(directories.at(i).path(), map_size, non_root_user_id);
        REQUIRE(opened.has_value());
        return std::move(*opened);
    }
};

}

// The simulation of test_simulation.cpp on persistent_storage, with the two
// clients. It checks that the protocol keeps S1, S2, S2b, S3, S5, S7, E1
// and R1 when each state goes through LMDB, and that the decided entries of
// a node are there after the node is closed and its environment is opened
// again. The entries of the clients go through the LMDB encoding of each
// entry kind. LMDB storage cannot be copied, so this run has no rollback.
// Each delivery is a batch of random size, so one LMDB transaction holds
// the writes of several messages.
TEST_CASE("simulation on LMDB with FIFO channels and two clients keeps the safety properties")
{
    simulation::run_seeds<lmdb_cluster>(simulation::delivery::fifo, seed_count, simulation::rollback::none,
                                        simulation::clients::two);
}
