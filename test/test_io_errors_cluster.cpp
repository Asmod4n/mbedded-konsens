#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <vector>

#include "io_error_vfs.hpp"
#include "sqlite_cluster.hpp"

// I/O error testing in the form of SQLite, scenario e of
// doc/plan-io-error-testing.md: three nodes in the simulation, and the storage
// of one of them fails at its N-th I/O operation. The node that raises is
// handled as a crash and starts again. The cluster must keep every decided
// entry and decide the markers with a majority, and the failed node must not
// write a proof of a fault: an I/O error is not damage. One fixed seed, with clients and no
// check threads, so that each N names the same operation.
namespace {

constexpr std::uint64_t fixed_seed = 1;
constexpr std::size_t failing_node = 0;

io_error_vfs::registered_vfs& vfs()
{
    static io_error_vfs::registered_vfs registered;
    return registered;
}

enum class failure_mode { once, persist, diskfull };

struct sweep_result {
    int runs = 0;
    int restarts = 0;
};

sweep_result sweep(const failure_mode mode)
{
    sweep_result r;
    for (int n = 1;; ++n) {
        int hits = 0;
        const simulation::storage_fault fault{
            .node = failing_node,
            .arm =
                [&](const std::vector<std::filesystem::path>& directories) {
                    vfs().watch(directories);
                    vfs().arm(mode == failure_mode::diskfull ? 0 : n, mode == failure_mode::persist,
                              mode == failure_mode::diskfull ? n : 0);
                },
            .disarm =
                [&] {
                    hits += vfs().count().io_error_hit;
                    vfs().disarm();
                },
        };
        INFO("mode " << static_cast<int>(mode) << ", N = " << n);
        const auto o = simulation::run<sqlite_cluster>(fixed_seed, simulation::delivery::fifo, false,
                                                       simulation::rollback::none, simulation::clients::two,
                                                       simulation::batches::random, &fault);
        hits += vfs().count().io_error_hit;
        vfs().disarm();
        vfs().watch({});
        r.runs = n;
        r.restarts += o.storage_failures;
        CHECK_FALSE(o.violation.has_value());
        CHECK(o.survived_crash);
        CHECK(o.majority_decided);
        CHECK_FALSE(o.fault_proof_written);
        if (hits == 0) {
            return r;
        }
    }
}

}

TEST_CASE("an I/O error at one node of three ends as a crash of that node, and the cluster keeps every decided entry")
{
    static_cast<void>(vfs());
    for (const auto mode : {failure_mode::once, failure_mode::persist, failure_mode::diskfull}) {
        const auto start = std::chrono::steady_clock::now();
        const auto r = sweep(mode);
        const auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        MESSAGE("mode " << static_cast<int>(mode) << ": " << r.runs << " runs, " << r.restarts << " restarts, "
                        << seconds << " s");
    }
}
