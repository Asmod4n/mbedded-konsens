#pragma once

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <random>
#include <set>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "konsens/node.hpp"
#include "echo.hpp"

// The simulation runs on each storage. A Cluster gives the storage type,
// start(i, kept) gives the storage of node i, and stop(node) gives what a
// crash keeps. A crash destroys the node before start, so that a storage
// that holds a file, as LMDB does, is closed before it is opened again.
// With rollback::one and a storage that can be copied, one node restarts
// from an earlier copy of its storage (a lost fsync or a backup), as
// Rollback in SequencePaxos.tla, run F6b. Only recovery repairs that node.
// A crash or a rollback starts only while no node recovers: two nodes that
// recover at once leave one voting node of three, no primary, and a
// recovery that waits (plan v0.1 section 7). VR Revisited 4.3 also allows
// at most f nodes to fail at once. A recovering node refuses an append, so
// the markers of the quiet end wait until no node recovers.
namespace simulation {


using konsens::ballot;
using konsens::entry;
using konsens::message;
using konsens::node_id;

constexpr std::size_t node_count = 3;
constexpr std::uint64_t election_ticks = 3;
constexpr std::uint64_t resend_ticks = 2;
constexpr int random_steps = 250;
constexpr int quiet_rounds = 50;
constexpr int marker_round = 10;
constexpr std::size_t trace_tail = 400;

enum class delivery { fifo, reorder };
enum class rollback { none, one };

template <typename Cluster>
using node_of = konsens::node<typename Cluster::storage_type, echo>;

konsens::node_config config_of(node_id pid)
{
    std::vector<node_id> peers;
    for (node_id p = 1; p <= node_count; ++p) {
        if (p != pid) {
            peers.push_back(p);
        }
    }
    return {1, pid, peers, 0, election_ticks, resend_ticks};
}

entry entry_of(std::uint64_t v)
{
    std::vector<std::byte> payload;
    for (int shift = 0; shift < 64; shift += 8) {
        payload.push_back(static_cast<std::byte>((v >> shift) & 0xff));
    }
    return {konsens::entry_kind::normal, payload};
}

std::uint64_t value_of(const entry& e)
{
    std::uint64_t v = 0;
    int shift = 0;
    for (const auto b : e.payload) {
        v |= std::to_integer<std::uint64_t>(b) << shift;
        shift += 8;
    }
    return v;
}

node_id from_of(const message& m) { return std::visit([](const auto& x) { return x.from; }, m); }
node_id to_of(const message& m) { return std::visit([](const auto& x) { return x.to; }, m); }

std::string name_of(const message& m)
{
    static constexpr std::array<const char*, 15> paxos{
        "prepare_req", "prepare",           "promise",            "accept_sync", "accept_decide",
        "accepted",    "decide",            "not_accepted",       "proposal_forward", "recovery",
        "recovery_response", "read_index_request", "heartbeat", "heartbeat_ack", "read_index_reply"};
    static constexpr std::array<const char*, 2> ble{"heartbeat_request", "heartbeat_reply"};
    if (const auto* p = std::get_if<konsens::paxos_message>(&m)) {
        return paxos.at(p->msg.index());
    }
    return ble.at(std::get<konsens::ble_message>(m).msg.index());
}

std::string ballot_text(const ballot& b) { return "(" + std::to_string(b.n) + "," + std::to_string(b.pid) + ")"; }

std::string detail_of(const message& m)
{
    std::string t = name_of(m);
    if (const auto* p = std::get_if<konsens::paxos_message>(&m)) {
        std::visit(
            [&t](const auto& x) {
                using T = std::decay_t<decltype(x)>;
                if constexpr (requires { x.n; }) {
                    t += " n " + ballot_text(x.n);
                }
                if constexpr (requires { x.seq_num; }) {
                    t += " seq " + std::to_string(x.seq_num.session) + "." + std::to_string(x.seq_num.counter);
                }
                if constexpr (requires { x.decided_idx; }) {
                    t += " dec " + std::to_string(x.decided_idx);
                }
                if constexpr (requires { x.accepted_idx; }) {
                    t += " acc " + std::to_string(x.accepted_idx);
                }
                if constexpr (std::is_same_v<T, konsens::accept_sync>) {
                    t += " sync_idx " + std::to_string(x.log_sync.sync_idx) + " suffix " +
                         std::to_string(x.log_sync.suffix.size());
                }
                if constexpr (requires { x.entries; }) {
                    t += " entries " + std::to_string(x.entries.size());
                }
            },
            p->msg);
    }
    return t;
}

bool is_prefix(std::span<const entry> a, std::span<const entry> b)
{
    return a.size() <= b.size() && std::ranges::equal(a, b.first(a.size()));
}

bool compatible(std::span<const entry> a, std::span<const entry> b) { return is_prefix(a, b) || is_prefix(b, a); }

struct view {
    std::vector<entry> log;
    std::size_t decided;
    ballot n_accepted;
    std::pair<konsens::role, konsens::phase> state;
    ballot n_leader;

    std::span<const entry> decided_prefix() const { return std::span(log).first(decided); }
};

template <typename N>
view view_of(const N& n)
{
    const auto& sp = n.get_sequence_paxos();
    const auto& s = sp.get_internal_storage();
    auto log = *s.get_entries(0, *s.get_log_len());
    const auto decided = std::min<std::size_t>(sp.get_decided_idx(), log.size());
    return {std::move(log), decided, sp.get_accepted_round(), sp.get_state(), sp.get_leader_state().n_leader};
}

template <typename N>
std::string state_of(node_id pid, const view& v, const N& n)
{
    std::string t = "node " + std::to_string(pid) + " promise " + ballot_text(n.get_sequence_paxos().get_promise()) +
                    " n_accepted " + ballot_text(v.n_accepted) + " decided " + std::to_string(v.decided) + " log";
    for (const auto& e : v.log) {
        t += " " + std::to_string(value_of(e));
    }
    return t;
}

template <typename Cluster>
struct world {
    using node_t = node_of<Cluster>;

    Cluster cluster;
    std::vector<node_t> nodes;
    std::vector<message> network;
    std::set<std::uint64_t> proposed;
    std::vector<entry> decided_history;
    std::vector<ballot> decided_ballot;
    std::array<std::uint64_t, node_count> last_decided{};
    std::array<view, node_count> views{};
    std::vector<std::string> trace;
    bool detailed = false;
    std::uint64_t next_value = 1;
    std::uint64_t nonce = 0;

    template <typename F>
    void note(F make)
    {
        if (detailed) {
            trace.push_back(make());
        }
    }

    bool dropped = false;
    bool duplicated = false;
    bool crashed = false;
    bool survived_crash = true;
    std::optional<std::pair<std::size_t, typename Cluster::storage_type>> earlier;
    bool rolled_back = false;

    void collect(std::size_t i)
    {
        for (auto& m : nodes[i].take_outgoing_messages()) {
            network.push_back(std::move(m));
        }
        views[i] = view_of(nodes[i]);
    }

    void deliver(std::size_t k)
    {
        auto m = std::move(network[k]);
        network.erase(std::next(network.begin(), static_cast<std::ptrdiff_t>(k)));
        note([&] { return std::string("deliver " + std::to_string(from_of(m)) + "->" + std::to_string(to_of(m)) + " " +
                        detail_of(m)); });
        const auto i = to_of(m) - 1;
        REQUIRE(nodes[i].handle(std::move(m)));
        collect(i);
    }

    std::size_t first_of_channel(std::size_t k) const
    {
        const auto from = from_of(network[k]);
        const auto to = to_of(network[k]);
        for (std::size_t j = 0; j < network.size(); ++j) {
            if (from_of(network[j]) == from && to_of(network[j]) == to) {
                return j;
            }
        }
        return k;
    }

    void tick(std::size_t i)
    {
        note([&] { return std::string("tick " + std::to_string(i + 1)); });
        REQUIRE(nodes[i].tick());
        collect(i);
    }

    void append(std::size_t i)
    {
        const auto e = entry_of(next_value);
        note([&] { return std::string("append " + std::to_string(i + 1) + " value " + std::to_string(next_value)); });
        next_value += 1;
        const auto r = nodes[i].append(e);
        REQUIRE((r || r.error() == konsens::client_errc::recovering));
        if (r) {
            proposed.insert(next_value - 1);
        }
        collect(i);
    }

    void crash(std::size_t i)
    {
        note([&] { return std::string("crash " + std::to_string(i + 1)); });
        const std::vector<entry> decided_before(views[i].decided_prefix().begin(), views[i].decided_prefix().end());
        auto kept = [this, i] {
            const node_t stopped = std::move(nodes[i]);
            return cluster.stop(stopped);
        }();
        nonce += 1;
        nodes[i] = *node_t::with(config_of(i + 1), cluster.start(i, std::move(kept)), echo{}, nonce);
        collect(i);
        survived_crash = survived_crash && is_prefix(decided_before, views[i].decided_prefix());
    }

    bool recovers() const
    {
        return std::ranges::any_of(nodes, [](const node_t& n) { return n.get_sequence_paxos().recovering(); });
    }

    void keep_earlier(std::size_t i)
    {
        note([&] { return std::string("keep earlier storage of " + std::to_string(i + 1)); });
        earlier.emplace(i, nodes[i].get_sequence_paxos().get_internal_storage());
    }

    void roll_back()
    {
        const auto i = earlier->first;
        note([&] { return std::string("roll back " + std::to_string(i + 1)); });
        nonce += 1;
        nodes[i] = *node_t::with(config_of(i + 1), earlier->second, echo{}, nonce);
        last_decided[i] = nodes[i].get_sequence_paxos().get_decided_idx();
        collect(i);
        rolled_back = true;
    }

    std::optional<std::string> check()
    {
        const auto& v = views;
        for (std::size_t i = 0; i < node_count; ++i) {
            const auto d = nodes[i].get_sequence_paxos().get_decided_idx();
            if (d < last_decided[i]) {
                return "S7: decided_idx of node " + std::to_string(i + 1) + " fell";
            }
            last_decided[i] = d;
        }
        for (std::size_t i = 0; i < node_count; ++i) {
            for (std::size_t j = i + 1; j < node_count; ++j) {
                if (!compatible(v[i].decided_prefix(), v[j].decided_prefix())) {
                    return "S1: decided prefixes of nodes " + std::to_string(i + 1) + " and " +
                           std::to_string(j + 1) + " differ";
                }
            }
        }
        std::optional<std::size_t> longer;
        for (std::size_t i = 0; i < node_count; ++i) {
            const auto p = v[i].decided_prefix();
            if (p.size() > decided_history.size() && is_prefix(decided_history, p) &&
                (!longer || p.size() > v[*longer].decided)) {
                longer = i;
            }
        }
        if (longer) {
            const auto p = v[*longer].decided_prefix();
            for (std::size_t k = decided_history.size(); k < p.size(); ++k) {
                decided_ballot.push_back(v[*longer].n_accepted);
            }
            decided_history.assign(p.begin(), p.end());
        }
        for (std::size_t i = 0; i < node_count; ++i) {
            if (!is_prefix(v[i].decided_prefix(), decided_history)) {
                return "S2: decided prefix of node " + std::to_string(i + 1) + " is no prefix of the history";
            }
        }
        for (std::size_t i = 0; i < node_count; ++i) {
            if (v[i].state != std::pair{konsens::role::leader, konsens::phase::accept}) {
                continue;
            }
            std::size_t k = 0;
            for (std::size_t x = 0; x < decided_ballot.size(); ++x) {
                if (decided_ballot[x] <= v[i].n_leader) {
                    k = x + 1;
                }
            }
            if (!is_prefix(std::span(decided_history).first(k), v[i].log)) {
                return "S2b: leader " + std::to_string(i + 1) + " misses a decided entry";
            }
        }
        for (std::size_t i = 0; i < node_count; ++i) {
            for (const auto& e : v[i].decided_prefix()) {
                if (!proposed.contains(value_of(e))) {
                    return "S3: node " + std::to_string(i + 1) + " decided an entry that was not proposed";
                }
            }
        }
        for (std::size_t i = 0; i < node_count; ++i) {
            for (std::size_t j = i + 1; j < node_count; ++j) {
                if (v[i].n_accepted == v[j].n_accepted && v[i].n_accepted != ballot{} &&
                    !compatible(v[i].log, v[j].log)) {
                    return "S5: logs of nodes " + std::to_string(i + 1) + " and " + std::to_string(j + 1) +
                           " with one accepted round differ";
                }
            }
        }
        return std::nullopt;
    }
};

struct outcome {
    std::optional<std::string> violation;
    std::vector<std::string> trace;
    bool majority_decided;
    bool all_decided;
    bool survived_crash;
};

template <typename Cluster>
outcome run(std::uint64_t seed, delivery mode, bool detailed, rollback faults = rollback::none)
{
    std::mt19937_64 rng(seed);
    const auto pick = [&rng](std::size_t n) { return std::uniform_int_distribution<std::size_t>(0, n - 1)(rng); };
    world<Cluster> w;
    w.detailed = detailed;
    for (std::size_t i = 0; i < node_count; ++i) {
        w.nodes.push_back(*node_of<Cluster>::with(config_of(i + 1), w.cluster.start(i, {}), echo{}, w.nonce));
    }
    for (std::size_t i = 0; i < node_count; ++i) {
        w.collect(i);
    }
    const auto failed = [&w](std::string v) {
        for (std::size_t i = 0; i < node_count; ++i) {
            w.note([&] { return std::string(state_of(i + 1, w.views[i], w.nodes[i])); });
        }
        return outcome{std::move(v), std::move(w.trace), false, false, w.survived_crash};
    };
    for (int s = 0; s < random_steps; ++s) {
        const auto action = pick(100);
        if (action < 60 && !w.network.empty()) {
            const auto k = pick(w.network.size());
            w.deliver(mode == delivery::reorder ? k : w.first_of_channel(k));
        } else if (action < 85) {
            w.tick(pick(node_count));
        } else if (action < 95) {
            w.append(pick(node_count));
        } else if (action == 96 && !w.dropped && !w.network.empty()) {
            const auto k = pick(w.network.size());
            w.note([&] { return std::string("drop " + name_of(w.network[k])); });
            w.network.erase(std::next(w.network.begin(), static_cast<std::ptrdiff_t>(k)));
            w.dropped = true;
        } else if (action == 97 && !w.duplicated && !w.network.empty()) {
            const auto k = pick(w.network.size());
            w.note([&] { return std::string("duplicate " + name_of(w.network[k])); });
            w.network.push_back(w.network[k]);
            w.duplicated = true;
        } else if (action == 98 && !w.crashed && !w.recovers()) {
            w.crash(pick(node_count));
            w.crashed = true;
        } else if constexpr (std::copyable<typename Cluster::storage_type>) {
            if (faults == rollback::one && action == 95 && !w.earlier) {
                const auto i = pick(node_count);
                if (w.nodes[i].get_sequence_paxos().get_promise() != ballot{}) {
                    w.keep_earlier(i);
                }
            } else if (faults == rollback::one && action == 99 && w.earlier && !w.rolled_back && !w.recovers()) {
                w.roll_back();
            }
        }
        if (auto v = w.check()) {
            return failed(std::move(*v));
        }
    }
    std::vector<std::vector<std::byte>> markers;
    for (int r = 0; r < quiet_rounds; ++r) {
        if (markers.empty() && r >= marker_round && !w.recovers()) {
            for (std::size_t i = 0; i < node_count; ++i) {
                markers.push_back(entry_of(w.next_value).payload);
                w.append(i);
            }
        }
        while (!w.network.empty()) {
            const bool paxos = std::holds_alternative<konsens::paxos_message>(w.network.front());
            w.deliver(0);
            if (!paxos) {
                continue;
            }
            if (auto v = w.check()) {
                return failed(std::move(*v));
            }
        }
        for (std::size_t i = 0; i < node_count; ++i) {
            w.tick(i);
        }
    }
    bool majority_decided = true;
    bool all_decided = true;
    for (const auto& m : markers) {
        std::size_t holders = 0;
        for (const auto& n : w.nodes) {
            const auto d = *n.read_decided_suffix(0);
            if (std::ranges::any_of(d, [&m](const entry& e) { return e.payload == m; })) {
                holders += 1;
            }
        }
        majority_decided = majority_decided && holders > node_count / 2;
        all_decided = all_decided && holders == node_count;
    }
    return {std::nullopt, {}, majority_decided, all_decided, w.survived_crash};
}

template <typename Cluster>
void run_seeds(delivery mode, std::uint64_t seed_count, rollback faults = rollback::none)
{
    std::uint64_t lagging = 0;
    for (std::uint64_t seed = 1; seed <= seed_count; ++seed) {
        const auto o = run<Cluster>(seed, mode, false, faults);
        if (o.violation) {
            const auto d = run<Cluster>(seed, mode, true, faults);
            std::string tail;
            const auto start = d.trace.size() > trace_tail ? d.trace.size() - trace_tail : 0;
            for (std::size_t i = start; i < d.trace.size(); ++i) {
                tail += d.trace[i] + "\n";
            }
            FAIL("seed " << seed << ": " << *o.violation << "\nlast steps:\n" << tail);
        }
        INFO("seed " << seed);
        CHECK(o.majority_decided);
        CHECK(o.survived_crash);
        if (!o.all_decided) {
            lagging += 1;
        }
    }
    MESSAGE(seed_count << " seeds, seeds where one node did not decide every marker: " << lagging);
}

}
