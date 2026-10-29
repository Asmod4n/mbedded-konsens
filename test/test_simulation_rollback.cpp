#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "sqlite_cluster.hpp"

// Plan v0.1 F15: the run of test_simulation with one rollback in addition. A crash
// keeps the durable state, so only a rollback gives a node a log that misses
// accepted entries. A node that promised with such a log would let a new
// leader decide over them. The rolled back node starts without a clean stop
// and retires, so it promises nothing.
TEST_CASE("simulation with one rollback keeps the safety properties")
{
    const auto c =
        simulation::run_seeds<sqlite_cluster>(simulation::delivery::fifo, seed_count, simulation::rollback::one);
    // A rollback counts when the restored log is shorter than the log before
    // it. A copy that restore skips leaves the count at zero.
    CHECK(c.rollbacks > 0);
}
