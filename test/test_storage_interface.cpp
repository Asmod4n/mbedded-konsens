#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "konsens/storage.hpp"

// A ballot orders by config_id, n, priority, pid, as Ballot in Omni-Paxos.
TEST_CASE("ballot order") {
    CHECK(konsens::ballot{0, 1, 0, 2} < konsens::ballot{0, 2, 0, 1});
    CHECK(konsens::ballot{0, 1, 0, 1} < konsens::ballot{0, 1, 0, 2});
}
