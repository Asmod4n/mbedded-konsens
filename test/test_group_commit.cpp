#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iterator>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

#include "batches.hpp"
#include "konsens/node.hpp"
#include "konsens/sequence_paxos.hpp"
#include "node_cluster.hpp"

using namespace node_cluster;

// The aim of group commit (doc/plan-group-commit.md section 5): 64 client
// requests that wait at the leader cost one commit of the storage on each
// node in each pass, not one for each request. The leader appends the 64
// entries with one append_entries, and each follower writes the 64 entries
// of its one AcceptDecide in one call. The test adds the 64 transactions
// into one, commits each node once in each pass, and counts the commits of
// the storage around each commit of the node.
TEST_CASE("64 requests make one commit of the storage on each node in each pass")
{
    auto c = elected();
    const auto l = c.leader();
    const auto s = registered(c, l, client_a);
    for (auto& d : c.disks) {
        d->appended.clear();
    }
    std::array<std::optional<konsens::transaction>, node_count> pending;
    pending.at(l) = value_of(c.nodes.at(l).client_request(client_a, s, 1, {}));
    for (std::uint64_t k = 2; k <= request_count; ++k) {
        *pending.at(l) += value_of(c.nodes.at(l).client_request(client_a, s, k, {}));
    }
    std::uint64_t passes = 0;
    for (bool more = true; more; ++passes) {
        more = false;
        std::vector<std::vector<message>> inbox(node_count);
        for (std::size_t i = 0; i < node_count; ++i) {
            if (pending.at(i)) {
                const auto before = c.disks.at(i)->commits;
                REQUIRE(batches::commit_and_sync(c.nodes.at(i), *pending.at(i)));
                CHECK(c.disks.at(i)->commits - before <= 1);
                pending.at(i).reset();
            }
            for (auto& m : c.nodes.at(i).take_outgoing_messages()) {
                more = true;
                inbox.at(batches::to_of(m) - 1).push_back(std::move(m));
            }
        }
        for (std::size_t i = 0; i < node_count; ++i) {
            if (!inbox.at(i).empty()) {
                pending.at(i) = value_of(c.nodes.at(i).handle_incoming(std::move(inbox.at(i))));
            }
        }
    }
    CHECK(passes > 1);
    // Execute-Verify: the done records of the 64 requests are one append,
    // and each later append holds one entry.
    for (const auto& d : c.disks) {
        REQUIRE_FALSE(d->appended.empty());
        CHECK(d->appended.front() == request_count);
        CHECK(std::ranges::all_of(d->appended | std::views::drop(1), [](const std::size_t n) { return n == 1; }));
    }
    const auto replies = c.nodes.at(l).take_client_replies();
    REQUIRE(replies.size() == request_count);
    for (const auto& n : c.nodes) {
        CHECK(counter_of(n) == request_count);
    }
}

// Section 2 of the plan: a crash before the commit is a crash before the
// messages of the batch arrived. The work and its answers pass, the leader
// writes the done record, and it handles the Accepted messages that decide
// the request. It ends before its commit. Neither the Decide
// nor the reply left the node, and the disk holds no new decided index.
// After the restart the client sends the request again with the same
// request number. The session executes it once and gives one reply to the
// call that is still open.
TEST_CASE("a crash between the batch and the commit sends nothing, and the retry executes once")
{
    auto c = elected();
    const auto l = c.leader();
    const auto s = registered(c, l, client_a);
    constexpr std::uint64_t lost_call = client_a + 1;
    constexpr std::uint64_t retry_call = client_a + 2;
    batches::commit(c.nodes.at(l), c.nodes.at(l).client_request(lost_call, s, 1, {}));
    std::vector<message> accepted;
    for (int pass = 0; pass < rounds && !std::ranges::any_of(accepted, holds<konsens::accepted>); ++pass) {
        auto out = c.nodes.at(l).take_outgoing_messages();
        accepted.clear();
        for (const auto f : {first_follower(l), second_follower(l)}) {
            batches::commit(c.nodes.at(f), c.nodes.at(f).handle_incoming(messages_to(out, pid_of(f))));
            std::ranges::move(c.nodes.at(f).take_outgoing_messages(), std::back_inserter(accepted));
        }
        if (!std::ranges::any_of(accepted, holds<konsens::accepted>)) {
            batches::commit(c.nodes.at(l), c.nodes.at(l).handle_incoming(messages_to(accepted, pid_of(l))));
        }
    }
    REQUIRE(std::ranges::any_of(accepted, holds<konsens::accepted>));
    const auto decided_on_disk = lmdb_storage::meta_of(c.disks.at(l)->store).decided_idx;
    const auto uncommitted = c.nodes.at(l).handle_incoming(messages_to(accepted, pid_of(l)));
    REQUIRE(uncommitted);
    REQUIRE(value_of(c.nodes.at(l).get_sequence_paxos().get_decided_idx()) > decided_on_disk);
    CHECK(c.nodes.at(l).take_outgoing_messages().empty());
    CHECK(c.nodes.at(l).take_client_replies().empty());
    c.restart(l);
    CHECK(lmdb_storage::meta_of(c.disks.at(l)->store).decided_idx == decided_on_disk);
    c.run(rounds);
    CHECK(c.replies_of_all().empty());
    const auto at = first_follower(l);
    batches::commit(c.nodes.at(at), c.nodes.at(at).client_request(retry_call, s, 1, {}));
    c.run(rounds);
    const auto replies = c.replies_of_all();
    REQUIRE(replies.size() == 1);
    CHECK(replies[0].client_id == retry_call);
    CHECK(replies[0].response == counter::bytes_of(1));
    for (const auto& n : c.nodes) {
        CHECK(counter_of(n) == 1);
    }
}

// The same crash at a follower: it handles the AcceptDecide and ends before
// its commit. No Accepted left the follower, so the leader cannot count an
// entry that the disk of the follower does not hold.
TEST_CASE("a follower that ends before its commit sends no Accepted")
{
    auto c = elected();
    const auto l = c.leader();
    const auto f = first_follower(l);
    batches::commit(c.nodes.at(l), c.nodes.at(l).append(make_entry(1)));
    auto out = c.nodes.at(l).take_outgoing_messages();
    const auto log_on_disk = lmdb_storage::meta_of(c.disks.at(f)->store).log_len;
    const auto uncommitted = c.nodes.at(f).handle_incoming(messages_to(out, pid_of(f)));
    REQUIRE(uncommitted);
    REQUIRE(c.nodes.at(f).get_sequence_paxos().get_accepted_idx() == log_on_disk + 1);
    CHECK(c.nodes.at(f).take_outgoing_messages().empty());
    c.restart(f);
    CHECK(lmdb_storage::meta_of(c.disks.at(f)->store).log_len == log_on_disk);
}

// AcceptDecide and the Decide of its entries can arrive in one batch. The
// follower decides the entries in memory while it handles the batch, but
// the state machine applies them only after the commit wrote them, so a
// reply never stands on an entry that a crash can lose.
TEST_CASE("AcceptDecide and Decide in one batch apply the entries after the commit")
{
    auto c = elected();
    const auto l = c.leader();
    const auto slow = first_follower(l);
    const auto fast = second_follower(l);
    batches::commit(c.nodes.at(l), c.nodes.at(l).append(make_entry(1)));
    auto out = c.nodes.at(l).take_outgoing_messages();
    auto to_slow = messages_to(out, pid_of(slow));
    REQUIRE(std::ranges::any_of(to_slow, holds<konsens::accept_decide>));
    batches::commit(c.nodes.at(fast), c.nodes.at(fast).handle_incoming(messages_to(out, pid_of(fast))));
    auto from_fast = c.nodes.at(fast).take_outgoing_messages();
    batches::commit(c.nodes.at(l), c.nodes.at(l).handle_incoming(messages_to(from_fast, pid_of(l))));
    out = c.nodes.at(l).take_outgoing_messages();
    std::ranges::move(messages_to(out, pid_of(slow)), std::back_inserter(to_slow));
    REQUIRE(std::ranges::any_of(to_slow, holds<konsens::decide>));
    const auto applied = c.nodes.at(slow).get_applied_idx();
    const auto t = value_of(c.nodes.at(slow).handle_incoming(std::move(to_slow)));
    CHECK(c.nodes.at(slow).get_sequence_paxos().get_decided_idx() == applied + 1);
    CHECK(c.nodes.at(slow).get_applied_idx() == applied);
    REQUIRE(batches::commit_and_sync(c.nodes.at(slow), t));
    CHECK(c.nodes.at(slow).get_applied_idx() == applied + 1);
}

// P1 of the plan, R4 of the review: a handler that reads the storage in a
// step must read what the earlier handlers of the step wrote. A follower
// gets an AcceptDecide and then a Prepare of a higher ballot in one step.
// The Promise carries the log from the decided index of the Prepare, and
// that log holds the entry of the AcceptDecide only if the read sees the
// writes of the same transaction. The step makes one commit.
TEST_CASE("a Prepare after an AcceptDecide in one batch reads the entry of the AcceptDecide")
{
    using sp_t = konsens::sequence_paxos<disk_storage>;
    const auto d = new_disk();
    auto sp = *sp_t::with({1, {2, 3}}, disk_storage{d}, 1);
    const ballot first{config_id, 1, 0, 3};
    const ballot second{config_id, 2, 0, 2};
    REQUIRE(sp.handle({3, 1, konsens::prepare{first, 0, ballot{}, 0}}));
    REQUIRE(sp.handle({3, 1, konsens::accept_sync{first, {1, 1}, 0, {{}, 0}, 0}}));
    REQUIRE(batches::commit_and_sync(sp));
    static_cast<void>(sp.take_outgoing_messages());
    const auto commits = d->commits;
    REQUIRE(sp.handle({3, 1, konsens::accept_decide{first, {1, 2}, 0, {make_entry(7)}, 0}}));
    REQUIRE(sp.handle({2, 1, konsens::prepare{second, 0, ballot{}, 0}}));
    REQUIRE(batches::commit_and_sync(sp));
    CHECK(d->commits - commits == 1);
    const auto out = sp.take_outgoing_messages();
    const auto it = std::ranges::find_if(
        out, [](const paxos_message& m) { return m.to == 2 && std::holds_alternative<konsens::promise>(m.msg); });
    REQUIRE(it != out.end());
    const auto& p = std::get<konsens::promise>(it->msg);
    REQUIRE(p.log_sync);
    CHECK(p.log_sync->suffix == std::vector{make_entry(7)});
    CHECK(p.accepted_idx == 1);
}

// R3 and R9 of the review: every message waits for the commit, also the
// ones that write nothing. A reply to a read depends on the applied state,
// and a BLE reply carries the promise, which the batch may have changed in
// memory only.
TEST_CASE("a read reply and a BLE reply wait for the commit")
{
    auto c = elected();
    const auto l = c.leader();
    const auto f = first_follower(l);
    batches::commit(c.nodes.at(l), c.nodes.at(l).client_query(client_a, {}));
    auto out = c.nodes.at(l).take_outgoing_messages();
    REQUIRE(std::ranges::any_of(out, holds<konsens::heartbeat>));
    std::vector<message> acks;
    for (const auto peer : {first_follower(l), second_follower(l)}) {
        batches::commit(c.nodes.at(peer), c.nodes.at(peer).handle_incoming(messages_to(out, pid_of(peer))));
        std::ranges::move(c.nodes.at(peer).take_outgoing_messages(), std::back_inserter(acks));
    }
    const auto read = value_of(c.nodes.at(l).handle_incoming(messages_to(acks, pid_of(l))));
    CHECK(c.nodes.at(l).take_client_replies().empty());
    REQUIRE(batches::commit_and_sync(c.nodes.at(l), read));
    const auto replies = c.nodes.at(l).take_client_replies();
    REQUIRE(replies.size() == 1);
    CHECK(replies[0].client_id == client_a);

    std::vector<message> request;
    request.emplace_back(konsens::ble_message{pid_of(l), pid_of(f), konsens::heartbeat_request{1}});
    const auto ble = value_of(c.nodes.at(f).handle_incoming(std::move(request)));
    CHECK(c.nodes.at(f).take_outgoing_messages().empty());
    REQUIRE(batches::commit_and_sync(c.nodes.at(f), ble));
    const auto sent = c.nodes.at(f).take_outgoing_messages();
    CHECK(std::ranges::any_of(sent, [](const message& m) {
        const auto* b = std::get_if<konsens::ble_message>(&m);
        return b != nullptr && std::holds_alternative<konsens::heartbeat_reply>(b->msg);
    }));
}

// Form B of the owner: the transactions of several calls add up to one, and
// one commit of the sum releases the work of every call. Until that commit,
// nothing leaves the node, however many calls collect.
TEST_CASE("the sum of the transactions of several calls commits all of them at once")
{
    auto c = elected();
    const auto l = c.leader();
    auto t = value_of(c.nodes.at(l).append(make_entry(1)));
    for (std::uint64_t k = 0; k <= resend_ticks; ++k) {
        t += value_of(c.nodes.at(l).tick());
    }
    t += value_of(c.nodes.at(l).reconnected(pid_of(first_follower(l))));
    CHECK(c.nodes.at(l).take_outgoing_messages().empty());
    CHECK(c.nodes.at(l).take_client_replies().empty());
    REQUIRE(batches::commit_and_sync(c.nodes.at(l), t));
    CHECK(std::ranges::any_of(c.nodes.at(l).take_outgoing_messages(), holds<konsens::accept_decide>));
}

// Form B of the owner: a commit takes only a transaction of its own node
// that is not yet committed. Anything else is a wrong use and throws: a
// second commit of the same transaction, a commit of a transaction of
// another node, and the sum of transactions of two nodes or of two commits.
TEST_CASE("a transaction of another node or of a done commit is a wrong use")
{
    auto c = elected();
    const auto l = c.leader();
    const auto f = first_follower(l);
    const auto mine = value_of(c.nodes.at(l).tick());
    const auto other = value_of(c.nodes.at(f).tick());
    CHECK_THROWS_AS(static_cast<void>(batches::commit_and_sync(c.nodes.at(l), other)), std::logic_error);
    auto sum = mine;
    CHECK_THROWS_AS(sum += other, std::logic_error);
    REQUIRE(batches::commit_and_sync(c.nodes.at(l), mine));
    REQUIRE(batches::commit_and_sync(c.nodes.at(f), other));
    CHECK_THROWS_AS(static_cast<void>(batches::commit_and_sync(c.nodes.at(l), mine)), std::logic_error);
    auto later = value_of(c.nodes.at(l).tick());
    CHECK_THROWS_AS(later += mine, std::logic_error);
    REQUIRE(batches::commit_and_sync(c.nodes.at(l), later));
}

// R4 of the review: a read of the storage from outside while ops wait
// would see a state that the node has not written. It is a wrong use and
// throws, so that it cannot pass unseen.
TEST_CASE("a storage read from outside while ops wait is a wrong use")
{
    auto c = elected();
    const auto l = c.leader();
    const auto f = first_follower(l);
    batches::commit(c.nodes.at(l), c.nodes.at(l).append(make_entry(1)));
    auto out = c.nodes.at(l).take_outgoing_messages();
    const auto t = value_of(c.nodes.at(f).handle_incoming(messages_to(out, pid_of(f))));
    CHECK_THROWS_AS(static_cast<void>(c.nodes.at(f).read_decided_suffix(0)), std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(c.nodes.at(f).get_sequence_paxos().get_internal_storage()), std::logic_error);
    REQUIRE(batches::commit_and_sync(c.nodes.at(f), t));
    CHECK(c.nodes.at(f).read_decided_suffix(0));
    CHECK(lmdb_storage::meta_of(c.disks.at(f)->store).log_len == 1u);
}

// R6 of the review: the messages that wait for a commit have one limit,
// max_batch_size. A batch that would pass it is a wrong use, also when it
// is the second batch before one commit, because the node keeps every
// message of the batches until the commit.
TEST_CASE("more messages than max_batch_size before a commit are a wrong use")
{
    auto c = elected();
    const auto l = c.leader();
    const auto f = first_follower(l);
    const auto requests = [l, f](const std::size_t count) {
        return std::vector<message>(count, konsens::ble_message{pid_of(l), pid_of(f), konsens::heartbeat_request{1}});
    };
    CHECK_THROWS_AS(static_cast<void>(c.nodes.at(f).handle_incoming(requests(konsens::max_batch_size + 1))),
                    std::logic_error);
    auto t = value_of(c.nodes.at(f).handle_incoming(requests(konsens::max_batch_size - 1)));
    t += value_of(c.nodes.at(f).handle_incoming(requests(1)));
    CHECK_THROWS_AS(static_cast<void>(c.nodes.at(f).handle_incoming(requests(1))), std::logic_error);
    REQUIRE(batches::commit_and_sync(c.nodes.at(f), t));
    batches::commit(c.nodes.at(f), c.nodes.at(f).handle_incoming(requests(1)));
}

// A node that starts after a crash asks every peer with Recovery before it
// has a transaction to commit. Recovery carries only the nonce of the start
// and depends on no write, so node::with releases it at once.
TEST_CASE("a node releases the Recovery messages of its start with no commit")
{
    auto c = elected();
    const auto f = first_follower(c.leader());
    c.restart(f);
    REQUIRE(c.nodes.at(f).get_sequence_paxos().recovering());
    const auto out = c.nodes.at(f).take_outgoing_messages();
    CHECK(std::ranges::count_if(out, holds<konsens::recovery>) == static_cast<std::ptrdiff_t>(node_count - 1));
}

// Plan one-environment 3.4 item 2: a step that writes nothing aborts its
// transaction and asks for no sync, when no step before waits for one.
TEST_CASE("a step that writes nothing needs no sync")
{
    auto c = elected();
    const auto l = c.leader();
    const auto end = c.nodes.at(l).commit(value_of(c.nodes.at(l).tick()));
    REQUIRE(end);
    CHECK(*end == konsens::step_end::nothing);
}

// Plan one-environment 3.5 item 3 (Q1): the sync of a step makes its data
// durable, and its meta page becomes durable with the next sync. A message
// of the step leaves only after that second sync.
TEST_CASE("the messages of a step leave only after the sync that makes the step durable")
{
    auto c = elected();
    const auto l = c.leader();
    auto& n = c.nodes.at(l);
    const auto fd = c.disks.at(l)->store.file_descriptor();
    const auto end = n.commit(value_of(n.append(make_entry(1))));
    REQUIRE(end);
    CHECK(*end == konsens::step_end::commit);
    CHECK(n.take_outgoing_messages().empty());
    REQUIRE(fdatasync(fd) == 0);
    const auto first = n.synced();
    REQUIRE(first);
    CHECK(*first == konsens::step_end::sync_only);
    CHECK(n.take_outgoing_messages().empty());
    REQUIRE(fdatasync(fd) == 0);
    const auto second = n.synced();
    REQUIRE(second);
    CHECK(*second == konsens::step_end::nothing);
    CHECK(std::ranges::any_of(n.take_outgoing_messages(), holds<konsens::accept_decide>));
}

// Plan one-environment 3.5 item 4: the prepared transaction keeps the
// writer lock of LMDB until the sync. A step that begins before the sync
// is a wrong use.
TEST_CASE("a step that begins before the sync of the step before is a wrong use")
{
    auto c = elected();
    const auto l = c.leader();
    auto& n = c.nodes.at(l);
    const auto end = n.commit(value_of(n.append(make_entry(1))));
    REQUIRE(end);
    REQUIRE(*end == konsens::step_end::commit);
    CHECK_THROWS_AS(static_cast<void>(n.commit(value_of(n.tick()))), std::logic_error);
    batches::sync(n, n.get_step_end());
}
