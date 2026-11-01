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
#include <variant>
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

std::vector<std::uint64_t> accepted_indexes_in(const std::vector<message>& out)
{
    std::vector<std::uint64_t> indexes;
    for (const auto& m : out) {
        if (const auto* p = std::get_if<paxos_message>(&m)) {
            if (const auto* a = std::get_if<konsens::accepted>(&p->msg)) {
                indexes.push_back(a->accepted_idx);
            }
        }
    }
    return indexes;
}

// The follower accepts entry 1 in a commit of its own and keeps the sync of
// that commit out. It then accepts entry 2 in a step that ends while the
// sync is out, so entry 2 stays in the open transaction.
struct open_transaction {
    cluster c;
    std::size_t f;
    std::uint64_t committed_len;
    konsens::sync_request out;
};

open_transaction follower_with_an_open_transaction()
{
    auto c = elected_with_syncs();
    const auto l = c.leader();
    const auto f = first_follower(l);
    auto& leader = c.nodes.at(l);
    auto& follower = c.nodes.at(f);
    batches::commit(leader, leader.append(make_entry(1)));
    auto out = leader.take_outgoing_messages();
    batches::commit(follower, follower.handle_incoming(messages_to(out, pid_of(f))));
    const auto committed_len = value_of(follower.get_sequence_paxos().get_accepted_idx());
    const auto request = follower.take_the_sync_request();
    REQUIRE(request);
    batches::commit(leader, leader.append(make_entry(2)));
    out = leader.take_outgoing_messages();
    auto to_f = messages_to(out, pid_of(f));
    REQUIRE(std::ranges::any_of(to_f, holds<konsens::accept_decide>));
    const auto commits = c.disks.at(f)->commits;
    batches::commit(follower, follower.handle_incoming(std::move(to_f)));
    REQUIRE(c.disks.at(f)->commits == commits);
    REQUIRE(follower.get_sequence_paxos().the_transaction_holds_ended_steps());
    REQUIRE(value_of(follower.get_sequence_paxos().get_accepted_idx()) == committed_len + 1);
    return {std::move(c), f, committed_len, *request};
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
    while (sync_and_report(leader)) {
        std::ranges::move(leader.take_outgoing_messages(), std::back_inserter(out));
    }
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

// R7 and Q7: under arm S2 a step that ends while a sync is out commits
// nothing, so the follower commits once for each sync. The commit after a
// report checkpoints the WAL, and the first write of the next open
// transaction starts the WAL again at its head while the sync of that
// commit is out. The descriptor of each request must still name the WAL
// file, and each sync must still cover the log. The storage checkpoints
// after checkpoint_frames frames of the WAL (src/persistent_storage.cpp);
// each commit writes at least one frame, so more commits than that on the
// follower run at least one checkpoint. The leader and the other follower
// form a majority, so the cluster decides while the sync of the follower is
// out.
TEST_CASE("checkpoints between the syncs keep the descriptor on the WAL")
{
    constexpr std::uint64_t checkpoint_frames = 1000;
    auto c = elected_with_syncs();
    const auto l = c.leader();
    const auto f = first_follower(l);
    auto& leader = c.nodes.at(l);
    auto& slow = c.nodes.at(f);
    const auto decided = decided_of(leader);
    delays delayed{};
    delayed.at(f) = true;
    const auto commits = c.disks.at(f)->commits;
    std::optional<konsens::sync_request> request;
    std::optional<int> descriptor;
    std::uint64_t appended = 0;
    while (c.disks.at(f)->commits <= commits + checkpoint_frames) {
        appended += 1;
        batches::commit(leader, leader.append(make_entry(static_cast<std::uint8_t>(appended))));
        const auto before = c.disks.at(f)->commits;
        settle(c, delayed);
        if (request) {
            REQUIRE(c.disks.at(f)->commits == before);
            REQUIRE(::fdatasync(request->file_descriptor) == 0);
            batches::commit(slow, slow.the_log_is_synced(request->sync_number));
            REQUIRE(c.disks.at(f)->commits == before + 1);
        }
        request = slow.take_the_sync_request();
        REQUIRE(request);
        if (!descriptor) {
            descriptor = request->file_descriptor;
        }
        REQUIRE(request->file_descriptor == *descriptor);
    }
    CHECK(decided_of(leader) == decided + appended);
    struct stat by_descriptor {};
    REQUIRE(::fstat(request->file_descriptor, &by_descriptor) == 0);
    CHECK(by_descriptor.st_nlink > 0);
    REQUIRE(::fdatasync(request->file_descriptor) == 0);
    batches::commit(slow, slow.the_log_is_synced(request->sync_number));
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

// Q1 of section 17: the output of a step in the open transaction gets the
// sync number of the commit of that transaction, which is above the sync
// that is out. If it took the number of the sync that is out, the report of
// that sync would release an Accepted of entry 2, which only the cache of
// the follower holds, and the leader would count it as a durable vote.
TEST_CASE("an Accepted of a step in the open transaction does not leave on the report of the sync that was out")
{
    auto o = follower_with_an_open_transaction();
    auto& follower = o.c.nodes.at(o.f);
    const auto commits = o.c.disks.at(o.f)->commits;
    REQUIRE(::fdatasync(o.out.file_descriptor) == 0);
    batches::commit(follower, follower.the_log_is_synced(o.out.sync_number));
    const auto released = accepted_indexes_in(follower.take_outgoing_messages());
    CHECK_FALSE(released.empty());
    CHECK(std::ranges::all_of(released, [&o](const std::uint64_t i) { return i <= o.committed_len; }));
    CHECK(o.c.disks.at(o.f)->commits == commits + 1);
    REQUIRE(sync_and_report(follower));
    CHECK(std::ranges::contains(accepted_indexes_in(follower.take_outgoing_messages()), o.committed_len + 1));
}

// Q2 of section 17: a failure inside a step rolls back the open
// transaction, and with it every step since the last commit. Their state in
// memory, such as the accepted indexes, no longer matches the store, so the
// node stops voting. Their held output must not leave, also not on the
// report of the sync that was out, because the entry it states is gone.
TEST_CASE("a rollback of the open transaction stops voting and drops its held output")
{
    auto o = follower_with_an_open_transaction();
    auto& follower = o.c.nodes.at(o.f);
    o.c.disks.at(o.f)->meta_read_error = std::make_error_code(std::errc::io_error);
    const auto t = follower.handle_incoming(std::vector<message>{});
    REQUIRE(t);
    CHECK_FALSE(follower.commit(*t));
    o.c.disks.at(o.f)->meta_read_error.reset();
    CHECK_FALSE(follower.get_sequence_paxos().votes());
    CHECK_FALSE(follower.get_sequence_paxos().the_transaction_holds_ended_steps());
    CHECK(sqlite_storage::meta_of(o.c.disks.at(o.f)->store).log_len == o.committed_len);
    REQUIRE(::fdatasync(o.out.file_descriptor) == 0);
    REQUIRE(follower.the_log_is_synced(o.out.sync_number));
    const auto released = accepted_indexes_in(follower.take_outgoing_messages());
    CHECK(std::ranges::all_of(released, [&o](const std::uint64_t i) { return i <= o.committed_len; }));
}

// Q4 of section 17: the leader applies a registration while its sync is
// out, so the session stands only in the open transaction. A check of the
// session that read only the committed state would find no session and
// answer session_expired to a client whose registration was decided.
TEST_CASE("a session registered in the open transaction does not expire as unknown")
{
    auto c = elected_with_syncs();
    const auto l = c.leader();
    auto& leader = c.nodes.at(l);
    batches::commit(leader, leader.append(make_entry(1)));
    const auto request = leader.take_the_sync_request();
    REQUIRE(request);
    batches::commit(leader, leader.register_client(client_a));
    delays delayed{};
    delayed.at(l) = true;
    settle(c, delayed);
    REQUIRE(leader.get_sequence_paxos().the_transaction_holds_ended_steps());
    const auto session = value_of(leader.get_sequence_paxos().get_accepted_idx());
    REQUIRE(leader.get_applied_idx() >= session);
    REQUIRE(leader.find_client_session(session));
    {
        const auto committed = leader.get_sequence_paxos().get_internal_storage().begin_read_only();
        REQUIRE(committed);
        CHECK_FALSE(value_of(committed->find_session(session)));
    }
    batches::commit(leader, leader.expire_session(client_a, session));
    REQUIRE(::fdatasync(request->file_descriptor) == 0);
    batches::commit(leader, leader.the_log_is_synced(request->sync_number));
    settle(c);
    const auto replies = c.replies_of_all();
    CHECK(std::ranges::any_of(replies, [session](const konsens::client_reply& r) {
        return r.session == session && r.response;
    }));
    CHECK_FALSE(leader.find_client_session(session));
}

// Q3 of section 17: a clean stop while steps wait in the open transaction
// commits that transaction and syncs it before it writes the mark. A mark
// over a log that lost entry 2 would let the next start serve a state that
// the store no longer holds.
TEST_CASE("a clean stop commits and syncs the open transaction before the mark")
{
    auto o = follower_with_an_open_transaction();
    o.c.stop_and_start(o.f);
    CHECK_FALSE(o.c.nodes.at(o.f).retired());
    CHECK(sqlite_storage::meta_of(o.c.disks.at(o.f)->store).log_len == o.committed_len + 1);
}
