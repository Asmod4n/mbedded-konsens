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
#include <ranges>
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
// disk. The disk counts the commits of write transactions and the entries
// of each append_entries. Its store is LMDB.
struct disk {
    konsens::persistent_storage store;
    std::uint64_t commits;
    std::vector<std::size_t> appended;
};

struct disk_storage {
    using read_only_transaction = konsens::persistent_storage::read_only_transaction;

    class transaction {
    public:
        transaction(konsens::persistent_storage::transaction t, disk* d) : t_{std::move(t)}, d_{d} {}

        auto get_meta() const { return t_.get_meta(); }
        auto get_entries(const std::uint64_t from, const std::uint64_t to) const { return t_.get_entries(from, to); }
        auto get_last_entry() const { return t_.get_last_entry(); }
        auto get_entry_state(const std::uint64_t index) const { return t_.get_entry_state(index); }
        auto get_chain_crc(const std::uint64_t index) const { return t_.get_chain_crc(index); }
        auto find(const std::span<const std::byte> key) const { return t_.find(key); }
        auto find_session(const std::uint64_t session) const { return t_.find_session(session); }
        auto insert_or_assign(const std::span<const std::byte> key, const std::span<const std::byte> value)
        {
            return t_.insert_or_assign(key, value);
        }
        auto erase(const std::span<const std::byte> key) { return t_.erase(key); }
        auto insert_or_assign_session(const std::uint64_t session, const konsens::session_record& record)
        {
            return t_.insert_or_assign_session(session, record);
        }
        auto erase_session(const std::uint64_t session) { return t_.erase_session(session); }
        auto set_applied(const std::uint64_t index, const std::uint32_t crc)
        {
            return t_.set_applied(index, crc);
        }
        auto begin_nested() { return t_.begin_nested(); }
        bool has_writes() const { return t_.has_writes(); }
        auto prepare() &&
        {
            d_->commits += 1;
            return std::move(t_).prepare();
        }

        auto append_entries(const std::span<const entry> entries)
        {
            d_->appended.push_back(entries.size());
            return t_.append_entries(entries);
        }
        auto append_on_prefix(const std::uint64_t from_idx, const std::span<const entry> entries)
        {
            return t_.append_on_prefix(from_idx, entries);
        }
        auto set_promise(const ballot b) { return t_.set_promise(b); }
        auto set_accepted_round(const ballot b) { return t_.set_accepted_round(b); }
        auto set_decided_index(const std::uint64_t index) { return t_.set_decided_index(index); }
        auto write_repaired_log(const std::uint64_t from_idx, const std::span<const entry> entries,
                                const ballot promise, const ballot accepted_round, const std::uint64_t decided_idx)
        {
            return t_.write_repaired_log(from_idx, entries, promise, accepted_round, decided_idx);
        }
        std::expected<void, std::error_code> commit() &&
        {
            d_->commits += 1;
            return std::move(t_).commit();
        }

    private:
        konsens::persistent_storage::transaction t_;
        disk* d_;
    };

    std::shared_ptr<disk> d;

    std::expected<transaction, std::error_code> begin()
    {
        return d->store.begin().transform(
            [this](konsens::persistent_storage::transaction t) { return transaction{std::move(t), d.get()}; });
    }
    std::expected<transaction, std::error_code> begin_without_sync()
    {
        return d->store.begin_without_sync().transform(
            [this](konsens::persistent_storage::transaction t) { return transaction{std::move(t), d.get()}; });
    }
    int file_descriptor() const { return d->store.file_descriptor(); }
    std::expected<read_only_transaction, std::error_code> begin_read_only() const { return d->store.begin_read_only(); }
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
    static constexpr std::array key{std::byte{'c'}};

    static std::vector<std::byte> bytes_of(const std::uint64_t v)
    {
        const auto b = std::bit_cast<std::array<std::byte, sizeof v>>(v);
        return {b.begin(), b.end()};
    }

    static std::uint64_t value_in(const konsens::state_view auto& view)
    {
        std::array<std::byte, sizeof(std::uint64_t)> b{};
        if (const auto found = view.find(key)) {
            std::ranges::copy(*found | std::views::take(b.size()), b.begin());
        }
        return std::bit_cast<std::uint64_t>(b);
    }

    konsens::applied apply(std::span<const std::byte>, const konsens::state_view auto& view) const
    {
        auto next = bytes_of(value_in(view) + 1);
        return {next, {{{key.begin(), key.end()}, next}}};
    }

    std::vector<std::byte> query(std::span<const std::byte>, const konsens::state_view auto& view) const
    {
        return bytes_of(value_in(view));
    }
};

using node_t = konsens::node<disk_storage, counter>;

inline std::uint64_t counter_of(const node_t& n)
{
    const auto t = n.get_sequence_paxos().get_internal_storage().begin_read_only();
    REQUIRE(t.has_value());
    return counter::value_in(konsens::read_view{*t});
}

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
    std::array<std::filesystem::path, node_count> states{lmdb_storage::new_directory(), lmdb_storage::new_directory(),
                                                         lmdb_storage::new_directory()};
    std::vector<node_t> nodes;
    std::uint64_t next_nonce{1};

    node_t start(const std::size_t i)
    {
        next_nonce += 1;
        return *node_t::with(config_of(i + 1), disk_storage{disks.at(i)}, lmdb_storage::open_new_node_storage(), counter{}, next_nonce);
    }

    void restart(const std::size_t i)
    {
        {
            const auto ended = std::move(nodes.at(i));
        }
        nodes.at(i) = start(i);
    }

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
