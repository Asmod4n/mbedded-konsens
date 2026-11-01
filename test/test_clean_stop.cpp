#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <vector>

#include "batches.hpp"
#include "konsens/client.hpp"
#include "konsens/node.hpp"
#include "konsens/sequence_paxos.hpp"
#include "node_cluster.hpp"
#include "sqlite_storage.hpp"

#include <sqlite3.h>

using namespace node_cluster;
using konsens::client_errc;

namespace {

bool decides(cluster& c, const std::size_t at, const std::uint8_t v)
{
    const auto before = value_of(c.nodes.at(at).get_sequence_paxos().get_decided_idx());
    batches::commit(c.nodes.at(at), c.nodes.at(at).append(make_entry(v)));
    c.run(rounds);
    return value_of(c.nodes.at(at).get_sequence_paxos().get_decided_idx()) > before;
}

void change_the_node_storage(const std::filesystem::path& directory, const char* const sql)
{
    sqlite3* db = nullptr;
    const auto file = (directory / "node.db").string();
    REQUIRE(sqlite3_open_v2(file.c_str(), &db, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK);
    CHECK(sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);
}

std::expected<node_t, std::error_code> start_as_first(cluster& c, const std::size_t i, std::shared_ptr<disk> d)
{
    auto config = config_of(pid_of(i));
    config.first_start = true;
    return node_t::with(config, disk_storage{std::move(d)}, sqlite_storage::open_node_storage_in(c.states.at(i)),
                        counter{}, 1000 + i);
}

}

// Plan clean-stop 4.2: a clean stop loses no sync, so the node keeps its
// identity. It starts as the fail-recovery of Omni-Paxos does, sends
// PrepareReq, and the cluster decides with it while the third node is
// retired.
TEST_CASE("a node that stops cleanly and starts again votes")
{
    auto c = elected();
    const auto l = c.leader();
    const auto f = first_follower(l);
    c.stop_and_start(f);
    CHECK(!c.nodes.at(f).retired());
    CHECK(c.nodes.at(f).get_sequence_paxos().get_state() == std::pair{konsens::role::follower, konsens::phase::recover});
    const auto out = c.nodes.at(f).take_outgoing_messages();
    CHECK(std::ranges::count_if(out, holds<konsens::prepare_req>) == 2);
    c.restart(second_follower(l));
    REQUIRE(c.nodes.at(second_follower(l)).retired());
    c.run(rounds);
    CHECK(decides(c, c.leader(), 1));
}

// Plan clean-stop 4.2 and review 1 item 13: a leader that stops and starts
// again never leads with its old ballot, because a follower may hold a
// promise of that ballot with other entries.
TEST_CASE("a leader that stops cleanly leads again only with a higher ballot")
{
    auto c = elected();
    const auto l = c.leader();
    const auto old = c.nodes.at(l).get_sequence_paxos().get_leader_state().n_leader;
    for (std::size_t i = 0; i < node_count; ++i) {
        if (i != l) {
            c.nodes.at(i).stop();
        }
    }
    c.stop_and_start(l);
    for (std::size_t i = 0; i < node_count; ++i) {
        if (i != l) {
            c.restart(i);
        }
    }
    c.run(rounds);
    const auto now = c.leader();
    CHECK(c.nodes.at(now).get_sequence_paxos().get_leader_state().n_leader > old);
    CHECK(decides(c, now, 2));
}

// Plan clean-stop 4.3 and 4.4: a start without the mark may follow a lost
// sync, so the node retires. It sends nothing, refuses each call, and the
// mark of an earlier clean stop does not help, because the start before
// deleted it.
TEST_CASE("a start without a clean stop retires the node, also at each later start")
{
    auto c = elected();
    const auto f = first_follower(c.leader());
    c.stop_and_start(f);
    c.run(rounds);
    REQUIRE(!c.nodes.at(f).retired());
    c.restart(f);
    CHECK(c.nodes.at(f).retired());
    c.run(rounds);
    CHECK(c.nodes.at(f).take_outgoing_messages().empty());
    const auto refused = [](const auto& r) { return !r && r.error() == client_errc::retired; };
    CHECK(refused(c.nodes.at(f).append(make_entry(3))));
    CHECK(refused(c.nodes.at(f).register_client(client_a)));
    CHECK(refused(c.nodes.at(f).client_request(client_a, 1, 1, {std::byte{1}})));
    CHECK(refused(c.nodes.at(f).client_query(client_a, {std::byte{1}})));
    CHECK(refused(c.nodes.at(f).expire_session(client_a, 1)));
    c.restart(f);
    CHECK(c.nodes.at(f).retired());
    CHECK(decides(c, c.leader(), 4));
}

// Plan clean-stop 4.5: the mark names the pid, the configuration and the
// whole meta record. A store restored from an older copy, or the node
// storage of another node, holds another state than the one that stopped.
TEST_CASE("a mark that does not agree with the store retires the node")
{
    auto c = elected();
    const auto l = c.leader();
    const auto f = first_follower(l);
    const auto older = sqlite_storage::open_copy_of(c.disks.at(f)->store);
    CHECK(decides(c, l, 5));
    c.nodes.at(f).stop();
    c.disks.at(f)->store = sqlite_storage::open_copy_of(older);
    c.restart(f);
    CHECK(c.nodes.at(f).retired());

    const auto s = second_follower(l);
    c.nodes.at(s).stop();
    std::swap(c.states.at(s), c.states.at(l));
    c.restart(s);
    CHECK(c.nodes.at(s).retired());
}

// Plan clean-stop 2.2 and 4.6: a stop with an open step would write a mark
// for a state that the node did not commit. A retired node has no state
// to keep. After the stop, the node takes no call. A call that waits for
// its decide gets stopped, because the node cannot answer it any more.
TEST_CASE("the wrong uses of stop, and a call that waits at the stop")
{
    auto c = elected();
    const auto l = c.leader();
    const auto f = first_follower(l);
    const auto open = c.nodes.at(f).append(make_entry(6));
    REQUIRE(open);
    CHECK_THROWS_AS(c.nodes.at(f).stop(), std::logic_error);
    REQUIRE(c.nodes.at(f).commit(*open));

    batches::commit(c.nodes.at(f), c.nodes.at(f).register_client(client_a));
    c.nodes.at(f).stop();
    const auto replies = c.nodes.at(f).take_client_replies();
    REQUIRE(replies.size() == 1);
    CHECK(replies[0].client_id == client_a);
    CHECK(replies[0].response.error() == client_errc::stopped);
    CHECK_THROWS_AS(static_cast<void>(c.nodes.at(f).tick()), std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(c.nodes.at(f).append(make_entry(7))), std::logic_error);
    CHECK_THROWS_AS(c.nodes.at(f).stop(), std::logic_error);

    c.restart(second_follower(l));
    REQUIRE(c.nodes.at(second_follower(l)).retired());
    CHECK_THROWS_AS(c.nodes.at(second_follower(l)).stop(), std::logic_error);
}

// Plan clean-stop 4.8, the question of the owner at 13:10: when every node
// stops cleanly, the cluster starts again without the operator, elects a
// leader and decides.
TEST_CASE("a whole cluster that stops cleanly starts again and decides")
{
    auto c = elected();
    CHECK(decides(c, c.leader(), 8));
    for (auto& n : c.nodes) {
        n.stop();
    }
    for (std::size_t i = 0; i < node_count; ++i) {
        c.restart(i);
        CHECK(!c.nodes.at(i).retired());
    }
    c.run(rounds);
    CHECK(decides(c, c.leader(), 9));
}

// Jepsen, ParticleDB 1.1.4, issue 112: a node that lost its data started
// fresh and served an empty database. Here a store that is deleted or new
// holds no promise, so the promise alone cannot tell it from a true first
// start. The first start record in the node storage can: a node that
// started once never starts as the first again, and votes with no empty
// log that the model forbids (SequencePaxos.tla, only Init starts a seed,
// mutation StartAsNewNode).
TEST_CASE("a first start on a node storage that started before is refused, also with a new store")
{
    auto c = elected();
    const auto f = first_follower(c.leader());
    {
        const auto ended = std::move(c.nodes.at(f));
    }
    CHECK_THROWS_AS(static_cast<void>(start_as_first(c, f, new_disk())), std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(start_as_first(c, f, c.disks.at(f))), std::logic_error);
    c.nodes.at(f) = c.start(f);
    CHECK(c.nodes.at(f).retired());
    CHECK(decides(c, c.leader(), 6));
}

// Plan first-start-record 2.3: the record is written at the first start and
// at a clean start that finds none, so a node storage of an older version
// gets it too.
TEST_CASE("a first start writes the first start record, and a clean start writes it when it is missing")
{
    auto c = elected();
    for (std::size_t i = 0; i < node_count; ++i) {
        const auto record = sqlite_storage::open_node_storage_in(c.states.at(i)).get_first_start_record();
        REQUIRE(record.has_value());
        CHECK(*record == konsens::first_start_record{pid_of(i)});
    }
    const auto f = first_follower(c.leader());
    c.nodes.at(f).stop();
    change_the_node_storage(c.states.at(f), "DELETE FROM meta WHERE name = 'first_start'");
    c.restart(f);
    CHECK(!c.nodes.at(f).retired());
    const auto record = sqlite_storage::open_node_storage_in(c.states.at(f)).get_first_start_record();
    REQUIRE(record.has_value());
    CHECK(*record == konsens::first_start_record{pid_of(f)});
}

// Plan first-start-record 2.2: a node storage that holds the first start of
// another pid belongs to another node, as a mark of another node does.
TEST_CASE("a node storage with the first start record of another pid retires the node")
{
    auto c = elected();
    const auto f = first_follower(c.leader());
    c.nodes.at(f).stop();
    const auto other = pid_of((f + 1) % node_count);
    {
        auto own = sqlite_storage::open_node_storage_in(c.states.at(f));
        REQUIRE(own.write_the_first_start_record({other}).has_value());
    }
    c.restart(f);
    CHECK(c.nodes.at(f).retired());
}

// Plan first-start-record 2.1: a damaged record is never read as a missing
// one, or a damage would make a used node storage look new.
TEST_CASE("a damaged first start record stops the start")
{
    auto c = elected();
    const auto f = first_follower(c.leader());
    c.nodes.at(f).stop();
    change_the_node_storage(c.states.at(f),
                            "UPDATE meta SET record = x'0100000000000000ffffffff' WHERE name = 'first_start'");
    const auto damaged = start_as_first(c, f, c.disks.at(f));
    REQUIRE(!damaged);
    CHECK(damaged.error() == make_error_code(konsens::storage_errc::crc_mismatch));
    change_the_node_storage(c.states.at(f), "UPDATE meta SET record = x'01' WHERE name = 'first_start'");
    const auto short_record = start_as_first(c, f, c.disks.at(f));
    REQUIRE(!short_record);
    CHECK(short_record.error() == make_error_code(konsens::storage_errc::malformed_record));
}
