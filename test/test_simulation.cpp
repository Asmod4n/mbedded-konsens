#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "sqlite_cluster.hpp"

// The proof of v0 in the plan: S1, S2, S2b, S3, S5 and S7 of
// SequencePaxos.tla hold in each state of a run with the faults that the
// complete model runs checked: one crash that keeps the durable state, one
// lost message and one duplicate. Channels are FIFO here, as in the model.
// The quiet end of each run checks progress: after the faults, three new
// entries are decided by a majority. One node may stay behind: a node that
// is in Accept while the leader has reset its promise ignores each Prepare
// of the same ballot (follower.rs:13-51), as in seed 18336 of a longer run.
// Each delivery is a batch of random size, and the node commits after it,
// so a message leaves only after the commit of the batch that made it.
TEST_CASE("simulation with FIFO channels keeps the safety properties")
{
    simulation::run_seeds<sqlite_cluster>(simulation::delivery::fifo, seed_count);
}

// The same run, but each delivery takes any message in flight. The model
// and Omni-Paxos assume FIFO channels, and TCP gives FIFO only inside one
// connection. The owner decided (2026-10-19) that FIFO stays the
// requirement and that a follower also refuses a stale AcceptSync. Seed 57
// breaks S1 without that check (the first reordered seed that does): a
// late AcceptSync shortens the log of a follower, and a later AcceptSync
// puts its suffix at the wrong index. The run delivers one message for
// each batch, so the steps of seed 57 stay the steps that broke S1.
TEST_CASE("a follower refuses the stale AcceptSync of seed 57")
{
    const auto o = simulation::run<sqlite_cluster>(57, simulation::delivery::reorder, false, simulation::rollback::none,
                                                   simulation::clients::none, simulation::batches::single);
    CHECK_FALSE(o.violation);
}

// Without FIFO channels other paths still break the properties: seed 72
// breaks S2b. The library requires FIFO channels, so this case is skipped.
// It stays to show what the requirement protects.
TEST_CASE("simulation with reordered messages keeps the safety properties" * doctest::skip())
{
    simulation::run_seeds<sqlite_cluster>(simulation::delivery::reorder, seed_count);
}
