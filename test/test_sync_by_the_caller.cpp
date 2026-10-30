#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <vector>

#include <sys/stat.h>
#include <unistd.h>

#include "batches.hpp"
#include "konsens/node.hpp"
#include "konsens/sequence_paxos.hpp"
#include "node_cluster.hpp"

using namespace node_cluster;

namespace {

using delays = std::array<bool, node_count>;

// The caller of a node with sync_by_the_caller syncs the file that the
// request names, as konsensd does through io_uring, and reports the end.
// The report is a step, and its commit releases what it decided.
bool sync_and_report(node_t& n)
{
    const auto request = n.take_the_sync_request();
    if (!request) {
        return false;
    }
    REQUIRE(::fdatasync(request->file_descriptor) == 0);
    batches::commit(n, n.the_log_is_synced(request->sync_number));
    return true;
}

// A sync report can release the output of many steps at once, more than
// one batch takes. The caller then gives them in several batches.
void deliver_in_batches(node_t& n, std::vector<message> inbox)
{
    while (!inbox.empty()) {
        const auto count = std::min(inbox.size(), konsens::max_batch_size);
        std::vector<message> batch(
            std::make_move_iterator(inbox.begin()),
            std::make_move_iterator(std::next(inbox.begin(), static_cast<std::ptrdiff_t>(count))));
        inbox.erase(inbox.begin(), std::next(inbox.begin(), static_cast<std::ptrdiff_t>(count)));
        batches::commit(n, n.handle_incoming(std::move(batch)));
    }
}

// One pass syncs every node that is not delayed and gives each node the
// messages for it. The passes end when no node released a message and no
// node that is not delayed has a sync to do.
void settle(cluster& c, const delays delayed = {})
{
    for (bool more = true; more;) {
        more = false;
        for (std::size_t i = 0; i < c.nodes.size(); ++i) {
            if (!delayed.at(i) && sync_and_report(c.nodes.at(i))) {
                more = true;
            }
        }
        std::vector<std::vector<message>> inbox(c.nodes.size());
        for (auto& n : c.nodes) {
            for (auto& m : n.take_outgoing_messages()) {
                more = true;
                inbox.at(batches::to_of(m) - 1).push_back(std::move(m));
            }
        }
        for (std::size_t i = 0; i < c.nodes.size(); ++i) {
            deliver_in_batches(c.nodes[i], std::move(inbox[i]));
        }
    }
}

cluster elected_with_syncs()
{
    cluster c;
    c.sync_by_the_caller = true;
    for (std::size_t i = 0; i < node_count; ++i) {
        c.nodes.push_back(c.start(i));
    }
    for (int r = 0; r < rounds; ++r) {
        settle(c);
        for (auto& n : c.nodes) {
            batches::tick_and_commit(n);
        }
    }
    settle(c);
    static_cast<void>(c.leader());
    return c;
}

std::uint64_t decided_of(const node_t& n)
{
    return value_of(n.get_sequence_paxos().get_decided_idx());
}

template <typename T>
std::size_t first_index_of(const std::vector<message>& out)
{
    return static_cast<std::size_t>(std::ranges::distance(out.begin(), std::ranges::find_if(out, holds<T>)));
}

}

// Section 15 of doc/plan-write-path.md, item 3: a follower with
// sync_by_the_caller commits its AcceptDecide without a sync, so its
// Accepted waits until the caller reports a sync that covers the step. An
// Accepted that left before the sync would count a vote that a crash of the
// system can lose.
TEST_CASE("with sync_by_the_caller a step with log writes holds its Accepted until the_log_is_synced")
{
    auto c = elected_with_syncs();
    const auto l = c.leader();
    const auto f = first_follower(l);
    batches::commit(c.nodes.at(l), c.nodes.at(l).append(make_entry(1)));
    auto out = c.nodes.at(l).take_outgoing_messages();
    auto to_f = messages_to(out, pid_of(f));
    REQUIRE(std::ranges::any_of(to_f, holds<konsens::accept_decide>));
    const auto commits = c.disks.at(f)->commits;
    batches::commit(c.nodes.at(f), c.nodes.at(f).handle_incoming(std::move(to_f)));
    CHECK(c.disks.at(f)->commits == commits + 1);
    CHECK_FALSE(std::ranges::any_of(c.nodes.at(f).take_outgoing_messages(), holds<konsens::accepted>));
    const auto request = c.nodes.at(f).take_the_sync_request();
    REQUIRE(request);
    CHECK_FALSE(c.nodes.at(f).take_the_sync_request());
    REQUIRE(::fdatasync(request->file_descriptor) == 0);
    batches::commit(c.nodes.at(f), c.nodes.at(f).the_log_is_synced(request->sync_number));
    CHECK(std::ranges::any_of(c.nodes.at(f).take_outgoing_messages(), holds<konsens::accepted>));
    CHECK_FALSE(c.nodes.at(f).take_the_sync_request());
}

// Item 8 and R2: the leader counts its own accepted index only up to its
// synced log. With one follower down and the other one synced, the leader
// and that follower are a majority only after the sync of the leader. So
// no Decide may stand on an entry that only the cache of the leader holds.
TEST_CASE("the leader does not decide on its own unsynced append")
{
    auto c = elected_with_syncs();
    const auto l = c.leader();
    const auto up = first_follower(l);
    const auto down = second_follower(l);
    const auto decided = decided_of(c.nodes.at(l));
    batches::commit(c.nodes.at(l), c.nodes.at(l).append(make_entry(1)));
    const auto request = c.nodes.at(l).take_the_sync_request();
    REQUIRE(request);
    auto out = c.nodes.at(l).take_outgoing_messages();
    batches::commit(c.nodes.at(up), c.nodes.at(up).handle_incoming(messages_to(out, pid_of(up))));
    REQUIRE(sync_and_report(c.nodes.at(up)));
    auto from_up = c.nodes.at(up).take_outgoing_messages();
    REQUIRE(std::ranges::any_of(from_up, holds<konsens::accepted>));
    batches::commit(c.nodes.at(l), c.nodes.at(l).handle_incoming(messages_to(from_up, pid_of(l))));
    CHECK(decided_of(c.nodes.at(l)) == decided);
    CHECK_FALSE(std::ranges::any_of(c.nodes.at(l).take_outgoing_messages(), holds<konsens::decide>));
    REQUIRE(::fdatasync(request->file_descriptor) == 0);
    batches::commit(c.nodes.at(l), c.nodes.at(l).the_log_is_synced(request->sync_number));
    CHECK(decided_of(c.nodes.at(l)) == decided + 1);
    out = c.nodes.at(l).take_outgoing_messages();
    CHECK(std::ranges::any_of(messages_to(out, pid_of(up)), holds<konsens::decide>));
    CHECK(decided_of(c.nodes.at(down)) == decided);
}

// Item 4: an AcceptDecide may leave before the sync only when nothing is
// held for its peer. A new leader holds its AcceptSync until its sync; an
// AcceptDecide of a later step that overtook it would reach the follower
// before the AcceptSync that opens its round. The test delays the sync of
// the node that leads, so that its AcceptSync stays held.
TEST_CASE("a held AcceptSync and a later AcceptDecide to one peer leave in step order")
{
    cluster c;
    c.sync_by_the_caller = true;
    for (std::size_t i = 0; i < node_count; ++i) {
        c.nodes.push_back(c.start(i));
    }
    const auto leads = [&c](const std::size_t i) {
        return c.nodes.at(i).get_sequence_paxos().get_state() ==
               std::pair{konsens::role::leader, konsens::phase::accept};
    };
    std::optional<std::size_t> l;
    for (int r = 0; r < rounds && !l; ++r) {
        for (std::size_t i = 0; i < node_count && !l; ++i) {
            if (leads(i)) {
                l = i;
            }
        }
        if (l) {
            break;
        }
        delays delayed{};
        for (std::size_t i = 0; i < node_count; ++i) {
            delayed.at(i) = leads(i);
        }
        for (std::size_t i = 0; i < node_count; ++i) {
            if (!delayed.at(i)) {
                while (sync_and_report(c.nodes.at(i))) {
                }
            }
        }
        std::vector<std::vector<message>> inbox(c.nodes.size());
        for (auto& n : c.nodes) {
            for (auto& m : n.take_outgoing_messages()) {
                inbox.at(batches::to_of(m) - 1).push_back(std::move(m));
            }
        }
        for (std::size_t i = 0; i < c.nodes.size(); ++i) {
            if (!inbox[i].empty()) {
                batches::commit(c.nodes[i], c.nodes[i].handle_incoming(std::move(inbox[i])));
            }
        }
        for (auto& n : c.nodes) {
            batches::tick_and_commit(n);
        }
    }
    REQUIRE(l);
    auto& leader = c.nodes.at(*l);
    CHECK_FALSE(std::ranges::any_of(leader.take_outgoing_messages(), holds<konsens::accept_sync>));
    batches::commit(leader, leader.append(make_entry(1)));
    CHECK_FALSE(std::ranges::any_of(leader.take_outgoing_messages(), holds<konsens::accept_decide>));
    REQUIRE(sync_and_report(leader));
    auto out = leader.take_outgoing_messages();
    bool checked = false;
    for (std::size_t i = 0; i < node_count; ++i) {
        if (i == *l) {
            continue;
        }
        const auto to = messages_to(out, pid_of(i));
        if (!std::ranges::any_of(to, holds<konsens::accept_sync>)) {
            continue;
        }
        REQUIRE(std::ranges::any_of(to, holds<konsens::accept_decide>));
        CHECK(first_index_of<konsens::accept_sync>(to) < first_index_of<konsens::accept_decide>(to));
        checked = true;
    }
    CHECK(checked);
}

// Item 7 and R6: the node never repeats a sync that failed, because the
// kernel may have dropped the dirty pages. The report of the failure raises
// sync_error, the type that only main catches, and the storage then refuses
// every write transaction, as after a write that did not finish.
TEST_CASE("a failed sync ends the node with sync_error and every later begin throws")
{
    auto c = elected_with_syncs();
    const auto l = c.leader();
    auto& n = c.nodes.at(l);
    batches::commit(n, n.append(make_entry(1)));
    const auto request = n.take_the_sync_request();
    REQUIRE(request);
    CHECK_THROWS_AS(n.throw_after_a_failed_sync(request->sync_number, std::make_error_code(std::errc::io_error)),
                    konsens::sync_error);
    CHECK_THROWS_AS(static_cast<void>(c.disks.at(l)->store.begin()), std::logic_error);
    CHECK_THROWS_AS(
        [&n] {
            const auto t = n.tick();
            static_cast<void>(n.commit(*t));
        }(),
        std::logic_error);
}

// A report of a sync that take_the_sync_request did not give is a wrong
// use: the node would release output that no sync covers.
TEST_CASE("a report of a sync that was not requested is a wrong use")
{
    auto c = elected_with_syncs();
    auto& n = c.nodes.at(c.leader());
    batches::commit(n, n.append(make_entry(1)));
    CHECK_THROWS_AS(static_cast<void>(n.the_log_is_synced(n.get_sequence_paxos().get_last_sync_number())),
                    std::logic_error);
    const auto request = n.take_the_sync_request();
    REQUIRE(request);
    CHECK_THROWS_AS(static_cast<void>(n.the_log_is_synced(request->sync_number + 1)), std::logic_error);
    batches::commit(n, n.the_log_is_synced(request->sync_number));
}

// R10: the report of a sync is a step. Between the two phases of a commit
// no call of the node may run, so the report there is a wrong use too.
TEST_CASE("a sync report between commit_phase_one and commit_phase_two is a wrong use")
{
    auto c = elected_with_syncs();
    auto& n = c.nodes.at(c.leader());
    batches::commit(n, n.append(make_entry(1)));
    const auto request = n.take_the_sync_request();
    REQUIRE(request);
    const auto t = n.append(make_entry(2));
    REQUIRE(t);
    REQUIRE(n.commit_phase_one(*t));
    CHECK_THROWS_AS(static_cast<void>(n.the_log_is_synced(request->sync_number)), std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(n.take_the_sync_request()), std::logic_error);
    REQUIRE(n.commit_phase_two());
    batches::commit(n, n.the_log_is_synced(request->sync_number));
}

// R7: a checkpoint of SQLite runs while a sync of the caller is out, and
// the WAL starts again at its head. The descriptor of the request must
// still name the WAL file, and the sync must still cover the log. The
// storage checkpoints after checkpoint_frames frames of the WAL
// (src/persistent_storage.cpp); each commit writes at least one frame, so
// more commits than that on the follower run at least one checkpoint. The
// leader and the other follower form a majority, so the cluster decides
// while the sync of the follower is out. The sync of a follower is the one
// that is delayed, because a leader with a sync out holds the work of
// execute-verify for every follower, and its AcceptDecide waits behind it.
TEST_CASE("checkpoints that run while a sync is out keep the descriptor on the WAL")
{
    constexpr std::uint64_t checkpoint_frames = 1000;
    auto c = elected_with_syncs();
    const auto l = c.leader();
    const auto f = first_follower(l);
    auto& leader = c.nodes.at(l);
    auto& slow = c.nodes.at(f);
    const auto decided = decided_of(leader);
    batches::commit(leader, leader.append(make_entry(1)));
    delays delayed{};
    delayed.at(f) = true;
    settle(c, delayed);
    const auto request = slow.take_the_sync_request();
    REQUIRE(request);
    const auto commits = c.disks.at(f)->commits;
    std::uint64_t appended = 1;
    while (c.disks.at(f)->commits <= commits + checkpoint_frames) {
        batches::commit(leader, leader.append(make_entry(static_cast<std::uint8_t>(appended))));
        appended += 1;
        settle(c, delayed);
    }
    CHECK(decided_of(leader) == decided + appended);
    struct stat by_descriptor {};
    REQUIRE(::fstat(request->file_descriptor, &by_descriptor) == 0);
    CHECK(by_descriptor.st_nlink > 0);
    REQUIRE(::fdatasync(request->file_descriptor) == 0);
    batches::commit(slow, slow.the_log_is_synced(request->sync_number));
    const auto next = slow.take_the_sync_request();
    REQUIRE(next);
    CHECK(next->file_descriptor == request->file_descriptor);
    REQUIRE(::fdatasync(next->file_descriptor) == 0);
    batches::commit(slow, slow.the_log_is_synced(next->sync_number));
    settle(c);
    for (std::size_t i = 0; i < node_count; ++i) {
        CHECK(decided_of(c.nodes.at(i)) == decided_of(leader));
    }
}

// R5: with sync_by_the_caller no commit syncs, so a clean stop must sync the
// last commit itself before it writes its mark, even while a sync of the
// caller is out. The next start is then a clean start and not a retirement.
TEST_CASE("a clean stop with a sync out syncs the last commit and starts clean")
{
    auto c = elected_with_syncs();
    const auto l = c.leader();
    auto& n = c.nodes.at(l);
    batches::commit(n, n.append(make_entry(1)));
    REQUIRE(n.take_the_sync_request());
    c.stop_and_start(l);
    CHECK_FALSE(c.nodes.at(l).retired());
}
