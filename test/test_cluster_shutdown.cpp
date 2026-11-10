#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

#include "batches.hpp"
#include "echo.hpp"
#include "konsens/node.hpp"
#include "sqlite_damage.hpp"
#include "sqlite_storage.hpp"

// Plan cluster-stop, sections 2 and 4. The names come from Hazelcast:
// Cluster.shutdown() and the lifecycle state SHUTTING_DOWN.
namespace {

using konsens::entry;
using konsens::entry_kind;
using konsens::message;
using konsens::node_id;
using konsens::persistent_storage;
using node_t = konsens::node<persistent_storage, echo>;

constexpr std::size_t node_count = 3;
constexpr std::uint64_t election_ticks = 3;
constexpr std::uint64_t resend_ticks = 2;
constexpr int rounds = 40;

konsens::node_config config_of(const node_id pid)
{
    std::vector<node_id> peers;
    for (node_id p = 1; p <= node_count; ++p) {
        if (p != pid) {
            peers.push_back(p);
        }
    }
    return {1, pid, peers, 0, election_ticks, resend_ticks, {}, true};
}

entry make_entry(const std::uint8_t v)
{
    return {entry_kind::normal, {std::byte{v}}};
}

const entry shutdown_entry{entry_kind::cluster_shutdown};

std::error_code refusal_of(const std::expected<konsens::transaction, std::error_code>& r)
{
    REQUIRE_FALSE(r.has_value());
    return r.error();
}

bool is_a_cluster_shutdown(const message& m)
{
    const auto* s = std::get_if<konsens::shutdown_message>(&m);
    return s != nullptr && std::holds_alternative<konsens::cluster_shutdown>(s->msg);
}

bool is_a_repair_answer(const message& m)
{
    const auto* p = std::get_if<konsens::paxos_message>(&m);
    return p != nullptr && std::holds_alternative<konsens::repair_entries>(p->msg);
}

// The disks of the three nodes outlive the nodes, so that each node stops
// cleanly and starts again on its own disk.
struct cluster {
    std::vector<std::filesystem::path> directories;
    std::vector<std::filesystem::path> own_directories;
    std::vector<node_t> nodes;
    std::array<bool, node_count> alive{true, true, true};

    cluster()
    {
        for (node_id p = 1; p <= node_count; ++p) {
            directories.push_back(sqlite_storage::new_directory());
            own_directories.push_back(sqlite_storage::new_directory());
            nodes.push_back(*node_t::with(config_of(p), sqlite_storage::open_in(directories.back()),
                                          sqlite_storage::open_node_storage_in(own_directories.back()), echo{}, 0));
        }
    }

    template <typename Copies>
    void step(const Copies copies)
    {
        batches::deliver(nodes, [this, &copies](const message& m) -> std::size_t {
            if (!alive.at(batches::from_of(m) - 1) || !alive.at(batches::to_of(m) - 1)) {
                return 0;
            }
            return copies(m);
        });
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            if (alive.at(i)) {
                batches::tick_and_commit(nodes[i]);
            }
        }
    }

    void steps()
    {
        for (int i = 0; i < rounds; ++i) {
            step([](const message&) -> std::size_t { return 1; });
        }
    }

    node_id leader() const
    {
        for (const auto& n : nodes) {
            if (n.get_sequence_paxos().get_state().first == konsens::role::leader) {
                return n.get_sequence_paxos().get_pid();
            }
        }
        FAIL("no node leads");
        return 0;
    }

    node_t& at(const node_id pid) { return nodes.at(pid - 1); }

    node_id a_follower() const { return leader() == 1 ? 2 : 1; }

    void restart(const node_id pid)
    {
        auto later = config_of(pid);
        later.first_start = false;
        const auto gone = std::move(at(pid));
        at(pid) = *node_t::with(later, sqlite_storage::open_in(directories.at(pid - 1)),
                                sqlite_storage::open_node_storage_in(own_directories.at(pid - 1)), echo{}, 10);
    }
};

void flip_the_kind_of_the_last_record(const std::filesystem::path& directory)
{
    sqlite_damage::change_with_sqlite(directory, [](sqlite3* db) {
        const auto index =
            static_cast<std::uint64_t>(sqlite_damage::integer_of(db, "SELECT max(idx) FROM log", [](sqlite3_stmt*) {}));
        auto changed = sqlite_damage::entry_record_of(db, index);
        changed.at(0) ^= std::byte{0x40};
        sqlite_damage::put_entry_record(db, index, changed);
    });
}

}

// Plan 4.4: the operator calls on a follower, and the follower forwards the
// entry to the leader. After the decide every node is shutting down, stops
// cleanly, starts clean and votes at once, because a clean start runs no
// recovery. The entry is applied again at no start, and a write after the
// restart is decided.
TEST_CASE("a call on a follower shuts the cluster down, and each node starts clean")
{
    cluster c;
    c.steps();
    const auto leader = c.leader();
    batches::commit(c.at(leader), c.at(leader).append(make_entry(1)));
    c.steps();
    const auto follower = c.a_follower();
    batches::commit(c.at(follower), c.at(follower).shutdown_cluster());
    c.steps();
    for (auto& n : c.nodes) {
        CHECK(n.shutting_down());
        CHECK(n.peers_that_did_not_acknowledge().empty());
        n.stop();
    }
    for (node_id p = 1; p <= node_count; ++p) {
        c.restart(p);
        CHECK_FALSE(c.at(p).retired());
        CHECK(c.at(p).get_sequence_paxos().votes());
        CHECK_FALSE(c.at(p).shutting_down());
    }
    c.steps();
    const auto next = c.leader();
    batches::commit(c.at(next), c.at(next).append(make_entry(2)));
    c.steps();
    for (const auto& n : c.nodes) {
        CHECK_FALSE(n.shutting_down());
        CHECK(*n.read_decided_suffix(0) == std::vector{make_entry(1), shutdown_entry, make_entry(2)});
    }
}

// Plan 2.2: after the entry, the leader appends nothing in its ballot. A
// second call and a write are refused with stopped, and a reconfiguration
// as well.
TEST_CASE("a node that shuts the cluster down refuses a write and a second call")
{
    cluster c;
    c.steps();
    const auto leader = c.leader();
    batches::commit(c.at(leader), c.at(leader).shutdown_cluster());
    CHECK(refusal_of(c.at(leader).shutdown_cluster()) == konsens::client_errc::stopped);
    CHECK(refusal_of(c.at(leader).append(make_entry(1))) == konsens::client_errc::stopped);
    c.steps();
    CHECK(c.at(leader).shutting_down());
    CHECK(refusal_of(c.at(leader).append(make_entry(1))) == konsens::client_errc::stopped);
    CHECK(refusal_of(c.at(leader).reconfigure({2, {4, 5, 6}})) == konsens::client_errc::stopped);
}

// Plan 4.4: a node that was down at the call catches up over the entry after
// the restart and stays running. The entry acts only under the ballot that
// decided it, and the new leader sends nothing for it. Plan 4.5: the leader
// says that it is shutting down after cluster_shutdown_resends resend
// timeouts and names the node that did not acknowledge.
TEST_CASE("a node that was down at the call catches up over the entry and runs")
{
    cluster c;
    c.steps();
    const auto leader = c.leader();
    const node_id down = leader == 3 ? 2 : 3;
    batches::commit(c.at(down), c.at(down).append(make_entry(1)));
    c.steps();
    c.at(down).stop();
    c.alive.at(down - 1) = false;
    batches::commit(c.at(leader), c.at(leader).shutdown_cluster());
    c.steps();
    const node_id other = 6 - leader - down;
    CHECK(c.at(leader).shutting_down());
    CHECK(c.at(leader).peers_that_did_not_acknowledge() == std::vector{down});
    CHECK(c.at(other).shutting_down());
    c.at(leader).stop();
    c.at(other).stop();
    for (node_id p = 1; p <= node_count; ++p) {
        c.restart(p);
    }
    c.alive.at(down - 1) = true;
    c.steps();
    for (const auto& n : c.nodes) {
        CHECK_FALSE(n.shutting_down());
        CHECK(n.get_sequence_paxos().votes());
        CHECK(*n.read_decided_suffix(0) == std::vector{make_entry(1), shutdown_entry});
    }
}

// Plan 4.5: a lost message is sent again at the resend timeout, and the
// follower that gets the second copy shuts down and acknowledges it.
TEST_CASE("a lost cluster_shutdown is sent again at the resend timeout")
{
    cluster c;
    c.steps();
    const auto leader = c.leader();
    batches::commit(c.at(leader), c.at(leader).shutdown_cluster());
    int lost = 0;
    for (int i = 0; i < rounds; ++i) {
        c.step([&lost](const message& m) -> std::size_t {
            if (is_a_cluster_shutdown(m) && lost < 2) {
                lost += 1;
                return 0;
            }
            return 1;
        });
    }
    CHECK(lost == 2);
    for (const auto& n : c.nodes) {
        CHECK(n.shutting_down());
    }
    CHECK(c.at(leader).peers_that_did_not_acknowledge().empty());
}

// Plan 4.5 and rule 2 of the owner: when the leader loses its place before
// the decide, nobody shuts down. The new leader decides the entry under
// its own ballot, which is not the ballot that appended it, and sends
// nothing. The operator calls again.
TEST_CASE("a leader change before the decide shuts nobody down")
{
    cluster c;
    c.steps();
    const auto leader = c.leader();
    batches::commit(c.at(leader), c.at(leader).shutdown_cluster());
    c.alive.at(leader - 1) = false;
    c.steps();
    c.alive.at(leader - 1) = true;
    c.steps();
    for (const auto& n : c.nodes) {
        CHECK_FALSE(n.shutting_down());
    }
    const auto next = c.leader();
    batches::commit(c.at(next), c.at(next).shutdown_cluster());
    c.steps();
    for (const auto& n : c.nodes) {
        CHECK(n.shutting_down());
    }
}

// Plan 4.6 and 2.5: a node in a repair takes the message of a ballot that
// is not below its promise, acknowledges it and stops cleanly. Its next
// start repairs by itself, because a repair that a clean stop interrupts
// does not count.
TEST_CASE("a node in a repair takes the cluster shutdown and stops")
{
    cluster c;
    c.steps();
    const node_id repairing = c.leader() == 1 ? 2 : 1;
    batches::commit(c.at(c.leader()), c.at(c.leader()).append(make_entry(1)));
    c.steps();
    flip_the_kind_of_the_last_record(c.directories.at(repairing - 1));
    batches::commit(c.at(repairing), c.at(repairing).handle_incoming(std::vector<message>{}));
    REQUIRE(c.at(repairing).get_sequence_paxos().repairs());
    const auto without_repair = [](const message& m) -> std::size_t { return is_a_repair_answer(m) ? 0 : 1; };
    for (int i = 0; i < rounds; ++i) {
        c.step(without_repair);
    }
    REQUIRE(c.at(repairing).get_sequence_paxos().repairs());
    const auto leader = c.leader();
    batches::commit(c.at(leader), c.at(leader).shutdown_cluster());
    for (int i = 0; i < rounds; ++i) {
        c.step(without_repair);
    }
    for (const auto& n : c.nodes) {
        CHECK(n.shutting_down());
    }
    for (auto& n : c.nodes) {
        n.stop();
    }
    for (node_id p = 1; p <= node_count; ++p) {
        c.restart(p);
    }
    CHECK(c.at(repairing).get_sequence_paxos().repairs());
    CHECK_FALSE(c.at(repairing).waits_for_the_operator());
    c.steps();
    for (const auto& n : c.nodes) {
        CHECK(n.get_sequence_paxos().votes());
        CHECK(*n.read_decided_suffix(0) == std::vector{make_entry(1), shutdown_entry});
    }
}

// Plan 2.5: a message whose ballot is below the promise of a node in a
// repair is an old one, and a message of a node that is not a peer comes
// from outside the configuration. Neither shuts the node down. A voting
// node takes only the ballot of its promise.
TEST_CASE("a cluster shutdown of an old ballot or of a stranger shuts no node down")
{
    cluster c;
    c.steps();
    batches::commit(c.at(c.leader()), c.at(c.leader()).append(make_entry(1)));
    c.steps();
    const node_id follower = c.a_follower();
    const auto promise = *c.at(follower).get_sequence_paxos().get_promise();
    const auto deliver = [&c, follower](const node_id from, const konsens::ballot n) {
        std::vector<message> batch{konsens::shutdown_message{from, follower, konsens::cluster_shutdown{n, 0}}};
        batches::commit(c.at(follower), c.at(follower).handle_incoming(std::move(batch)));
    };
    auto older = promise;
    older.n -= 1;
    auto newer = promise;
    newer.n += 1;
    deliver(promise.pid, older);
    deliver(promise.pid, newer);
    deliver(node_count + 1, promise);
    CHECK_FALSE(c.at(follower).shutting_down());
    flip_the_kind_of_the_last_record(c.directories.at(follower - 1));
    batches::commit(c.at(follower), c.at(follower).handle_incoming(std::vector<message>{}));
    REQUIRE(c.at(follower).get_sequence_paxos().repairs());
    deliver(promise.pid, older);
    CHECK_FALSE(c.at(follower).shutting_down());
    deliver(promise.pid, newer);
    CHECK(c.at(follower).shutting_down());
}
