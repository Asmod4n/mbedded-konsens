#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "lmdb_cluster.hpp"

// Plan v0.1 section 3 and F16: the run of all parts. Two clients register,
// send requests, send a call again after a timeout and read, while a
// message is lost, a message is duplicated, one node crashes and one node
// rolls back. Each state also keeps E1 and R1 of the model. The counts show
// that the run took each client path: a lost call, a call sent again and a
// read across a change of leader.
TEST_CASE("simulation with two clients and one rollback keeps the safety properties")
{
    const auto c = simulation::run_seeds<lmdb_cluster>(simulation::delivery::fifo, seed_count,
                                                       simulation::rollback::one, simulation::clients::two);
    CHECK(c.rollbacks > 0);
    CHECK(c.sessions > 0);
    CHECK(c.replies > 0);
    CHECK(c.resends > 0);
    CHECK(c.reads > 0);
    CHECK(c.reads_across_leader_change > 0);
    CHECK(c.expired > 0);
}
