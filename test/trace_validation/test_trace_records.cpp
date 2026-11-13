#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cstdint>
#include <filesystem>
#include <format>
#include <string>

#include "../sqlite_cluster.hpp"

// Step 2 of doc/plan-trace-validation.md. The simulation writes one trace
// for each seed of phase 1 (F6: FIFO channels, one message for each
// batch, no clients, no rollback). The ctest tests trace_check_seed_N then
// check each trace against model/tla/TraceSimulation.tla with TLC.
namespace {

constexpr std::uint64_t traced_seeds = KONSENS_TRACED_SEEDS;

simulation::outcome run_phase_one(const std::uint64_t seed, const std::filesystem::path& trace_file)
{
    return simulation::run<sqlite_cluster>(seed, simulation::delivery::fifo, false, simulation::rollback::none,
                                           simulation::clients::none, simulation::batches::single, trace_file);
}

}

TEST_CASE("the simulation writes a trace of each seed of phase 1")
{
    const std::filesystem::path directory = KONSENS_TRACE_DIRECTORY;
    std::filesystem::create_directories(directory);
    for (std::uint64_t seed = 1; seed <= traced_seeds; ++seed) {
        const auto file = directory / std::format("TraceData_{}.tla", seed);
        const auto o = run_phase_one(seed, file);
        INFO("seed " << seed);
        CHECK_FALSE(o.violation.has_value());
        CHECK(std::filesystem::file_size(file) > 0);
    }
}

// The trace must show the run that the other tests check. So the records
// must not draw a random number: a seed with records ends as the same seed
// without them.
TEST_CASE("the records do not change the run of a seed")
{
    const auto file = std::filesystem::path(KONSENS_TRACE_DIRECTORY) / "TraceData_same_run.tla";
    std::filesystem::create_directories(file.parent_path());
    for (std::uint64_t seed = 1; seed <= traced_seeds; ++seed) {
        const auto with = run_phase_one(seed, file);
        const auto without = run_phase_one(seed, {});
        INFO("seed " << seed);
        CHECK(with.violation == without.violation);
        CHECK(with.majority_decided == without.majority_decided);
        CHECK(with.all_decided == without.all_decided);
        CHECK(with.survived_crash == without.survived_crash);
        CHECK(with.majority_retired == without.majority_retired);
    }
    std::filesystem::remove(file);
}
