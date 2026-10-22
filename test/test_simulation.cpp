#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>

#include "konsens/memory_storage.hpp"
#include "konsens/node.hpp"
#include "simulation.hpp"

namespace {

constexpr std::uint64_t seed_count = 2000;

// A crash of a node on memory_storage keeps a copy of its storage. This is
// the durable state that Crash in SequencePaxos.tla keeps.
struct memory_cluster {
    using storage_type = konsens::memory_storage;

    konsens::memory_storage stop(const konsens::node<konsens::memory_storage, simulation::concatenation>& stopped) const
    {
        return stopped.get_sequence_paxos().get_internal_storage();
    }

    konsens::memory_storage start(std::size_t, konsens::memory_storage kept) const { return kept; }
};

}

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
    simulation::run_seeds<memory_cluster>(simulation::delivery::fifo, seed_count);
}

// Plan v0.1 F15: the same run with one rollback in addition. A crash
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
    simulation::run_seeds<memory_cluster>(simulation::delivery::fifo, seed_count, simulation::rollback::one);
}

// Plan v0.1 section 3 and F16: the run of all parts. Two clients register,
// send requests, send a call again after a timeout and read, while a
// message is lost, a message is duplicated, one node crashes and one node
// rolls back. Each state also keeps E1 and R1 of the model. The counts show
// that the run took each client path: a lost call, a call sent again and a
// read across a change of leader.
TEST_CASE("simulation with two clients and one rollback keeps the safety properties")
{
    const auto c = simulation::run_seeds<memory_cluster>(simulation::delivery::fifo, seed_count,
                                                         simulation::rollback::one, simulation::clients::two);
    CHECK(c.sessions > 0);
    CHECK(c.replies > 0);
    CHECK(c.resends > 0);
    CHECK(c.reads > 0);
    CHECK(c.reads_across_leader_change > 0);
    CHECK(c.expired > 0);
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
    const auto o = simulation::run<memory_cluster>(57, simulation::delivery::reorder, false, simulation::rollback::none,
                                                   simulation::clients::none, simulation::batches::single);
    CHECK_FALSE(o.violation);
}

// Without FIFO channels other paths still break the properties: seed 72
// breaks S2b. The library requires FIFO channels, so this case is skipped.
// It stays to show what the requirement protects.
TEST_CASE("simulation with reordered messages keeps the safety properties" * doctest::skip())
{
    simulation::run_seeds<memory_cluster>(simulation::delivery::reorder, seed_count);
}
