#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "sqlite_cluster.hpp"

// Plan v0.1 F15: the run of test_simulation with one rollback in addition. A crash
// keeps the durable state, so only a rollback shows that recovery is
// needed: without recovery, the rolled back node promises with a log that
// misses accepted entries, and a new leader can decide over them.
// Seeds 32 and 542 show a stop of progress after the power-loss path of
// doc/plan-power-loss-code.md: the leader of ballot (2,1) rolls back, no
// peer is a primary, and the node recovers through the power-loss path with
// the highest promise, its own ballot (2,1). The election then names (2,1)
// for this node, and handle_leader refuses a ballot that is not above the
// promise. The node takes the next round above its promise instead
// (ballot_above_promise in node.hpp, a provisional form).
TEST_CASE("simulation with one rollback keeps the safety properties")
{
    const auto c =
        simulation::run_seeds<sqlite_cluster>(simulation::delivery::fifo, seed_count, simulation::rollback::one);
    // A rollback counts when the restored log is shorter than the log before
    // it. A copy that restore skips leaves the count at zero.
    CHECK(c.rollbacks > 0);
}
