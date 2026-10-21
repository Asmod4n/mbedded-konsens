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
#include <span>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

#include "konsens/memory_storage.hpp"
#include "konsens/node.hpp"
#include "konsens/sequence_paxos.hpp"
#include "batches.hpp"

namespace {

using konsens::ballot;
using konsens::entry;
using konsens::message;
using konsens::node_id;
using konsens::paxos_message;

constexpr std::size_t node_count = 3;
constexpr std::uint64_t election_ticks = 3;
constexpr std::uint64_t resend_ticks = 2;
constexpr int rounds = 40;
constexpr std::uint64_t client_a = 10;
constexpr std::uint64_t request_count = 64;
constexpr std::uint32_t config_id = 1;

// The disk of a node outlives the node, so a test can end a node between a
// call and its commit, as a crash does, and start it again on the same
// disk. The disk counts the calls of write_atomically and the entries of
// each append_entries. While fail is set, each write gives an error value,
// as a store does when a write fails before its LMDB commit. While
// fail_sync is set, each write throws sync_error, as persistent_storage
// does when mdb_txn_commit fails.
struct disk {
    konsens::memory_storage store;
    std::uint64_t writes;
    std::vector<std::size_t> appended;
    bool fail;
    bool fail_sync;
};

struct disk_storage {
    std::shared_ptr<disk> d;

    std::expected<void, std::error_code> write_atomically(const std::span<const konsens::storage_op> ops)
    {
        if (d->fail) [[unlikely]] {
            return std::unexpected(std::make_error_code(std::errc::no_space_on_device));
        }
        if (d->fail_sync) [[unlikely]] {
            throw konsens::sync_error(std::make_error_code(std::errc::io_error), "the sync of the test disk failed");
        }
        d->writes += 1;
        for (const auto& op : ops) {
            if (const auto* a = std::get_if<konsens::append_entries>(&op)) {
                d->appended.push_back(a->entries.size());
            }
        }
        return d->store.write_atomically(ops);
    }
    auto get_entries(const std::uint64_t from, const std::uint64_t to) const { return d->store.get_entries(from, to); }
    auto get_log_len() const { return d->store.get_log_len(); }
    auto get_promise() const { return d->store.get_promise(); }
    auto get_decided_idx() const { return d->store.get_decided_idx(); }
    auto get_accepted_round() const { return d->store.get_accepted_round(); }
    auto get_last_entry_crc() const { return d->store.get_last_entry_crc(); }
};

static_assert(konsens::storage<disk_storage>);

// A counter shows how often a request was executed: each apply adds one.
struct counter {
    std::uint64_t value;

    static std::vector<std::byte> bytes_of(const std::uint64_t v)
    {
        const auto b = std::bit_cast<std::array<std::byte, sizeof v>>(v);
        return {b.begin(), b.end()};
    }

    std::vector<std::byte> apply(std::span<const std::byte>) { return bytes_of(++value); }
    std::vector<std::byte> query(std::span<const std::byte>) const { return bytes_of(value); }
};

using node_t = konsens::node<disk_storage, counter>;

konsens::node_config config_of(const node_id pid)
{
    std::vector<node_id> peers;
    for (node_id p = 1; p <= node_count; ++p) {
        if (p != pid) {
            peers.push_back(p);
        }
    }
    return {config_id, pid, peers, 0, election_ticks, resend_ticks};
}

entry make_entry(const std::uint8_t v) { return {konsens::entry_kind::normal, {std::byte{v}}}; }

template <typename T>
bool holds(const message& m)
{
    const auto* p = std::get_if<paxos_message>(&m);
    return p != nullptr && std::holds_alternative<T>(p->msg);
}

std::size_t once(const message&) { return 1; }

struct cluster {
    std::array<std::shared_ptr<disk>, node_count> disks{std::make_shared<disk>(), std::make_shared<disk>(),
                                                        std::make_shared<disk>()};
    std::vector<node_t> nodes;
    std::uint64_t next_nonce{1};

    node_t start(const std::size_t i)
    {
        next_nonce += 1;
        return *node_t::with(config_of(i + 1), disk_storage{disks.at(i)}, counter{}, next_nonce);
    }

    void restart(const std::size_t i) { nodes.at(i) = start(i); }

    void run(const int count)
    {
        for (int r = 0; r < count; ++r) {
            batches::deliver(nodes, once);
            for (auto& n : nodes) {
                batches::tick_and_commit(n);
            }
        }
        batches::deliver(nodes, once);
    }

    std::size_t leader() const
    {
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            if (nodes[i].get_sequence_paxos().get_state() == std::pair{konsens::role::leader, konsens::phase::accept}) {
                return i;
            }
        }
        FAIL("no node leads in Accept");
        return 0;
    }

    std::vector<konsens::client_reply> replies_of_all()
    {
        std::vector<konsens::client_reply> all;
        for (auto& n : nodes) {
            std::ranges::move(n.take_client_replies(), std::back_inserter(all));
        }
        return all;
    }
};

cluster elected()
{
    cluster c;
    for (std::size_t i = 0; i < node_count; ++i) {
        c.nodes.push_back(c.start(i));
    }
    c.run(rounds);
    static_cast<void>(c.leader());
    return c;
}

std::uint64_t registered(cluster& c, const std::size_t at, const std::uint64_t client_id)
{
    batches::commit(c.nodes.at(at), c.nodes.at(at).register_client(client_id));
    batches::deliver(c.nodes, once);
    const auto replies = c.nodes.at(at).take_client_replies();
    REQUIRE(replies.size() == 1);
    REQUIRE(replies[0].response);
    return replies[0].session;
}

std::vector<message> messages_to(std::vector<message>& out, const node_id to)
{
    std::vector<message> taken;
    for (auto& m : out) {
        if (batches::to_of(m) == to) {
            taken.push_back(std::move(m));
        }
    }
    std::erase_if(out, [to](const message& m) { return batches::to_of(m) == to; });
    return taken;
}

std::size_t first_follower(const std::size_t leader) { return leader == 0 ? 1 : 0; }

std::size_t second_follower(const std::size_t leader) { return leader == 2 ? 1 : 2; }

node_id pid_of(const std::size_t i) { return static_cast<node_id>(i + 1); }

template <typename T>
T value_of(const std::expected<T, std::error_code>& r)
{
    REQUIRE(r);
    return *r;
}

}

// The aim of group commit (doc/plan-group-commit.md section 5): 64 client
// requests that wait at the leader cost one write_atomically on each node
// in each pass, not one for each request. The leader appends the 64
// entries with one append_entries, and each follower writes the 64 entries
// of its one AcceptDecide in one call. The test adds the 64 transactions
// into one, commits each node once in each pass, and counts the calls of
// write_atomically around each commit.
TEST_CASE("64 requests make one write_atomically on each node in each pass") {
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
                const auto before = c.disks.at(i)->writes;
                REQUIRE(c.nodes.at(i).commit(*pending.at(i)));
                CHECK(c.disks.at(i)->writes - before <= 1);
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
    for (const auto& d : c.disks) {
        CHECK(d->appended == std::vector<std::size_t>{request_count});
    }
    const auto replies = c.nodes.at(l).take_client_replies();
    REQUIRE(replies.size() == request_count);
    for (const auto& n : c.nodes) {
        CHECK(n.get_state_machine().value == request_count);
    }
}

// Section 2 of the plan: a crash before the commit is a crash before the
// messages of the batch arrived. The leader handles the Accepted messages
// that decide a request, and it ends before its commit. Neither the Decide
// nor the reply left the node, and the disk holds no new decided index.
// After the restart the client sends the request again with the same
// request number. The session executes it once and gives one reply to the
// call that is still open.
TEST_CASE("a crash between the batch and the commit sends nothing, and the retry executes once") {
    auto c = elected();
    const auto l = c.leader();
    const auto s = registered(c, l, client_a);
    constexpr std::uint64_t lost_call = client_a + 1;
    constexpr std::uint64_t retry_call = client_a + 2;
    batches::commit(c.nodes.at(l), c.nodes.at(l).client_request(lost_call, s, 1, {}));
    auto out = c.nodes.at(l).take_outgoing_messages();
    std::vector<message> accepted;
    for (const auto f : {first_follower(l), second_follower(l)}) {
        batches::commit(c.nodes.at(f), c.nodes.at(f).handle_incoming(messages_to(out, pid_of(f))));
        std::ranges::move(c.nodes.at(f).take_outgoing_messages(), std::back_inserter(accepted));
    }
    REQUIRE(std::ranges::any_of(accepted, holds<konsens::accepted>));
    const auto decided_on_disk = *c.disks.at(l)->store.get_decided_idx();
    const auto uncommitted = c.nodes.at(l).handle_incoming(messages_to(accepted, pid_of(l)));
    REQUIRE(uncommitted);
    REQUIRE(c.nodes.at(l).get_sequence_paxos().get_decided_idx() > decided_on_disk);
    CHECK(c.nodes.at(l).take_outgoing_messages().empty());
    CHECK(c.nodes.at(l).take_client_replies().empty());
    c.restart(l);
    CHECK(*c.disks.at(l)->store.get_decided_idx() == decided_on_disk);
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
        CHECK(n.get_state_machine().value == 1);
    }
}

// The same crash at a follower: it handles the AcceptDecide and ends before
// its commit. No Accepted left the follower, so the leader cannot count an
// entry that the disk of the follower does not hold.
TEST_CASE("a follower that ends before its commit sends no Accepted") {
    auto c = elected();
    const auto l = c.leader();
    const auto f = first_follower(l);
    batches::commit(c.nodes.at(l), c.nodes.at(l).append(make_entry(1)));
    auto out = c.nodes.at(l).take_outgoing_messages();
    const auto log_on_disk = *c.disks.at(f)->store.get_log_len();
    const auto uncommitted = c.nodes.at(f).handle_incoming(messages_to(out, pid_of(f)));
    REQUIRE(uncommitted);
    REQUIRE(c.nodes.at(f).get_sequence_paxos().get_accepted_idx() == log_on_disk + 1);
    CHECK(c.nodes.at(f).take_outgoing_messages().empty());
    c.restart(f);
    CHECK(*c.disks.at(f)->store.get_log_len() == log_on_disk);
}

// AcceptDecide and the Decide of its entries can arrive in one batch. The
// follower decides the entries in memory while it handles the batch, but
// the state machine applies them only after the commit wrote them, so a
// reply never stands on an entry that a crash can lose.
TEST_CASE("AcceptDecide and Decide in one batch apply the entries after the commit") {
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
    REQUIRE(c.nodes.at(slow).commit(t));
    CHECK(c.nodes.at(slow).get_applied_idx() == applied + 1);
}

// P1 of the plan, R4 of the review: a handler that reads the storage while
// ops of the batch wait must read what those ops write. A follower gets an
// AcceptDecide and then a Prepare of a higher ballot in one batch. The
// Promise carries the log from the decided index of the Prepare, and that
// log holds the entry of the AcceptDecide only if the ops were written
// before the read.
TEST_CASE("a Prepare after an AcceptDecide in one batch reads the entry of the AcceptDecide") {
    using sp_t = konsens::sequence_paxos<disk_storage>;
    const auto d = std::make_shared<disk>();
    auto sp = *sp_t::with({1, {2, 3}}, disk_storage{d}, 1);
    const ballot first{config_id, 1, 0, 3};
    const ballot second{config_id, 2, 0, 2};
    REQUIRE(sp.handle({3, 1, konsens::prepare{first, 0, ballot{}, 0}}));
    REQUIRE(sp.handle({3, 1, konsens::accept_sync{first, {1, 1}, 0, {{}, 0}}}));
    REQUIRE(sp.commit());
    static_cast<void>(sp.take_outgoing_messages());
    const auto writes = d->writes;
    REQUIRE(sp.handle({3, 1, konsens::accept_decide{first, {1, 2}, 0, {make_entry(7)}}}));
    REQUIRE(sp.handle({2, 1, konsens::prepare{second, 0, ballot{}, 0}}));
    REQUIRE(sp.commit());
    CHECK(d->writes - writes == 2);
    const auto out = sp.take_outgoing_messages();
    const auto it = std::ranges::find_if(out, [](const paxos_message& m) {
        return m.to == 2 && std::holds_alternative<konsens::promise>(m.msg);
    });
    REQUIRE(it != out.end());
    const auto& p = std::get<konsens::promise>(it->msg);
    REQUIRE(p.log_sync);
    CHECK(p.log_sync->suffix == std::vector{make_entry(7)});
    CHECK(p.accepted_idx == 1);
}

// The decision of the owner (2026-10-21) for a store that fails to write
// before its LMDB commit, a full map for example: the commit throws
// write_error with the error code of the store. None of the messages and
// replies of the transaction leaves the node, because each of them depends
// on the write that did not happen. The program does not repeat the
// commit, so a later commit is a wrong use.
TEST_CASE("a store that fails at the commit throws write_error and releases nothing") {
    auto c = elected();
    const auto l = c.leader();
    const auto s = registered(c, l, client_a);
    auto t = value_of(c.nodes.at(l).client_request(client_a, s, 1, {}));
    t += value_of(c.nodes.at(l).tick());
    c.disks.at(l)->fail = true;
    bool thrown = false;
    try {
        static_cast<void>(c.nodes.at(l).commit(t));
    } catch (const konsens::write_error& e) {
        thrown = true;
        CHECK(e.code() == std::errc::no_space_on_device);
    }
    CHECK(thrown);
    CHECK(c.nodes.at(l).take_outgoing_messages().empty());
    CHECK(c.nodes.at(l).take_client_replies().empty());
    c.disks.at(l)->fail = false;
    const auto next = value_of(c.nodes.at(l).tick());
    CHECK_THROWS_AS(static_cast<void>(c.nodes.at(l).commit(next)), std::logic_error);
    CHECK(c.nodes.at(l).take_outgoing_messages().empty());
}

// The same decision for an LMDB commit that fails, which persistent_storage
// raises as sync_error: nothing in the library catches it, and nothing of
// the transaction leaves the node. The test disk throws as
// persistent_storage does; test_persistent_storage.cpp makes a real LMDB
// commit fail.
TEST_CASE("a failed sync at the commit throws sync_error and releases nothing") {
    auto c = elected();
    const auto l = c.leader();
    const auto s = registered(c, l, client_a);
    const auto t = value_of(c.nodes.at(l).client_request(client_a, s, 1, {}));
    c.disks.at(l)->fail_sync = true;
    CHECK_THROWS_AS(static_cast<void>(c.nodes.at(l).commit(t)), konsens::sync_error);
    CHECK(c.nodes.at(l).take_outgoing_messages().empty());
    CHECK(c.nodes.at(l).take_client_replies().empty());
    c.disks.at(l)->fail_sync = false;
    const auto next = value_of(c.nodes.at(l).tick());
    CHECK_THROWS_AS(static_cast<void>(c.nodes.at(l).commit(next)), std::logic_error);
}

// R3 and R9 of the review: every message waits for the commit, also the
// ones that write nothing. A reply to a read depends on the applied state,
// and a BLE reply carries the promise, which the batch may have changed in
// memory only.
TEST_CASE("a read reply and a BLE reply wait for the commit") {
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
    REQUIRE(c.nodes.at(l).commit(read));
    const auto replies = c.nodes.at(l).take_client_replies();
    REQUIRE(replies.size() == 1);
    CHECK(replies[0].client_id == client_a);

    std::vector<message> request;
    request.emplace_back(konsens::ble_message{pid_of(l), pid_of(f), konsens::heartbeat_request{1}});
    const auto ble = value_of(c.nodes.at(f).handle_incoming(std::move(request)));
    CHECK(c.nodes.at(f).take_outgoing_messages().empty());
    REQUIRE(c.nodes.at(f).commit(ble));
    const auto sent = c.nodes.at(f).take_outgoing_messages();
    CHECK(std::ranges::any_of(sent, [](const message& m) {
        const auto* b = std::get_if<konsens::ble_message>(&m);
        return b != nullptr && std::holds_alternative<konsens::heartbeat_reply>(b->msg);
    }));
}

// Form B of the owner: the transactions of several calls add up to one, and
// one commit of the sum releases the work of every call. Until that commit,
// nothing leaves the node, however many calls collect.
TEST_CASE("the sum of the transactions of several calls commits all of them at once") {
    auto c = elected();
    const auto l = c.leader();
    auto t = value_of(c.nodes.at(l).append(make_entry(1)));
    for (std::uint64_t k = 0; k <= resend_ticks; ++k) {
        t += value_of(c.nodes.at(l).tick());
    }
    t += c.nodes.at(l).reconnected(pid_of(first_follower(l)));
    CHECK(c.nodes.at(l).take_outgoing_messages().empty());
    CHECK(c.nodes.at(l).take_client_replies().empty());
    REQUIRE(c.nodes.at(l).commit(t));
    CHECK(std::ranges::any_of(c.nodes.at(l).take_outgoing_messages(), holds<konsens::accept_decide>));
}

// Form B of the owner: a commit takes only a transaction of its own node
// that is not yet committed. Anything else is a wrong use and throws: a
// second commit of the same transaction, a commit of a transaction of
// another node, and the sum of transactions of two nodes or of two commits.
TEST_CASE("a transaction of another node or of a done commit is a wrong use") {
    auto c = elected();
    const auto l = c.leader();
    const auto f = first_follower(l);
    const auto mine = value_of(c.nodes.at(l).tick());
    const auto other = value_of(c.nodes.at(f).tick());
    CHECK_THROWS_AS(static_cast<void>(c.nodes.at(l).commit(other)), std::logic_error);
    auto sum = mine;
    CHECK_THROWS_AS(sum += other, std::logic_error);
    REQUIRE(c.nodes.at(l).commit(mine));
    REQUIRE(c.nodes.at(f).commit(other));
    CHECK_THROWS_AS(static_cast<void>(c.nodes.at(l).commit(mine)), std::logic_error);
    auto later = value_of(c.nodes.at(l).tick());
    CHECK_THROWS_AS(later += mine, std::logic_error);
    REQUIRE(c.nodes.at(l).commit(later));
}

// R4 of the review: a read of the storage from outside while ops wait
// would see a state that the node has not written. It is a wrong use and
// throws, so that it cannot pass unseen.
TEST_CASE("a storage read from outside while ops wait is a wrong use") {
    auto c = elected();
    const auto l = c.leader();
    const auto f = first_follower(l);
    batches::commit(c.nodes.at(l), c.nodes.at(l).append(make_entry(1)));
    auto out = c.nodes.at(l).take_outgoing_messages();
    const auto t = value_of(c.nodes.at(f).handle_incoming(messages_to(out, pid_of(f))));
    CHECK_THROWS_AS(static_cast<void>(c.nodes.at(f).read_decided_suffix(0)), std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(c.nodes.at(f).get_sequence_paxos().get_internal_storage()), std::logic_error);
    REQUIRE(c.nodes.at(f).commit(t));
    CHECK(c.nodes.at(f).read_decided_suffix(0));
    CHECK(c.nodes.at(f).get_sequence_paxos().get_internal_storage().get_log_len() == 1u);
}

// R6 of the review: the messages that wait for a commit have one limit,
// max_batch_size. A batch that would pass it is a wrong use, also when it
// is the second batch before one commit, because the node keeps every
// message of the batches until the commit.
TEST_CASE("more messages than max_batch_size before a commit are a wrong use") {
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
    REQUIRE(c.nodes.at(f).commit(t));
    batches::commit(c.nodes.at(f), c.nodes.at(f).handle_incoming(requests(1)));
}

// A node that starts after a crash asks every peer with Recovery before it
// has a transaction to commit. Recovery carries only the nonce of the start
// and depends on no write, so node::with releases it at once.
TEST_CASE("a node releases the Recovery messages of its start with no commit") {
    auto c = elected();
    const auto f = first_follower(c.leader());
    c.restart(f);
    REQUIRE(c.nodes.at(f).get_sequence_paxos().recovering());
    const auto out = c.nodes.at(f).take_outgoing_messages();
    CHECK(std::ranges::count_if(out, holds<konsens::recovery>) == static_cast<std::ptrdiff_t>(node_count - 1));
}
