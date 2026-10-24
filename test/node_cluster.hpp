#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <iterator>
#include <memory>
#include <span>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include "batches.hpp"
#include "konsens/node.hpp"
#include "konsens/persistent_storage.hpp"
#include "konsens/sequence_paxos.hpp"
#include "lmdb_storage.hpp"

// Three nodes on disks that outlive them, for the tests of group commit
// and of the faults of a disk under a node.
namespace node_cluster {

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
// each append_entries. Its store is LMDB.
struct disk {
    konsens::persistent_storage store;
    std::uint64_t writes;
    std::vector<std::size_t> appended;
};

struct disk_storage {
    std::shared_ptr<disk> d;

    std::expected<void, std::error_code> write_atomically(const std::span<const konsens::storage_op> ops)
    {
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

inline std::shared_ptr<disk> disk_in(const std::filesystem::path& directory)
{
    return std::make_shared<disk>(lmdb_storage::open_in(directory));
}

inline std::shared_ptr<disk> new_disk()
{
    return disk_in(lmdb_storage::new_directory());
}

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

inline konsens::node_config config_of(const node_id pid)
{
    std::vector<node_id> peers;
    for (node_id p = 1; p <= node_count; ++p) {
        if (p != pid) {
            peers.push_back(p);
        }
    }
    return {config_id, pid, peers, 0, election_ticks, resend_ticks};
}

inline entry make_entry(const std::uint8_t v)
{
    return {konsens::entry_kind::normal, {std::byte{v}}};
}

template <typename T>
inline bool holds(const message& m)
{
    const auto* p = std::get_if<paxos_message>(&m);
    return p != nullptr && std::holds_alternative<T>(p->msg);
}

inline std::size_t once(const message&)
{
    return 1;
}

struct cluster {
    std::array<std::shared_ptr<disk>, node_count> disks{new_disk(), new_disk(), new_disk()};
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

inline cluster elected(cluster c = {})
{
    for (std::size_t i = 0; i < node_count; ++i) {
        c.nodes.push_back(c.start(i));
    }
    c.run(rounds);
    static_cast<void>(c.leader());
    return c;
}

inline std::uint64_t registered(cluster& c, const std::size_t at, const std::uint64_t client_id)
{
    batches::commit(c.nodes.at(at), c.nodes.at(at).register_client(client_id));
    batches::deliver(c.nodes, once);
    const auto replies = c.nodes.at(at).take_client_replies();
    REQUIRE(replies.size() == 1);
    REQUIRE(replies[0].response);
    return replies[0].session;
}

inline std::vector<message> messages_to(std::vector<message>& out, const node_id to)
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

inline std::size_t first_follower(const std::size_t leader)
{
    return leader == 0 ? 1 : 0;
}

inline std::size_t second_follower(const std::size_t leader)
{
    return leader == 2 ? 1 : 2;
}

inline node_id pid_of(const std::size_t i)
{
    return static_cast<node_id>(i + 1);
}

template <typename T>
inline T value_of(const std::expected<T, std::error_code>& r)
{
    REQUIRE(r);
    return *r;
}

}
