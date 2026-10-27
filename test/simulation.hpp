#pragma once

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <iterator>
#include <map>
#include <optional>
#include <random>
#include <set>
#include <span>
#include <string>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "batches.hpp"
#include "konsens/node.hpp"
#include "sqlite_storage.hpp"

// The simulation runs on each storage. A Cluster gives the storage type,
// start(i, kept) gives the storage of node i, and stop(node) gives what a
// crash keeps. A crash destroys the node before start, so that a storage
// that holds a file, as SQLite does, is closed before it is opened again.
// Plan clean-stop 4.9: each crash is one of two kinds, drawn at random: a
// clean stop with a start, after which the node votes, or a crash, after
// which the node retires. The node storage of each node outlives it.
// With rollback::one and a Cluster that can keep(i, storage) and
// restore(i, kept), one node restarts from an earlier copy of its storage
// (a lost fsync or a backup), as
// Rollback in SequencePaxos.tla, run F6b. The rolled back node retires.
// When a majority retired, no marker can be decided, and the run checks
// the safety properties only.
// A crash or a rollback starts only while no node recovers: two nodes that
// recover at once leave one voting node of three, no primary, and a
// recovery that waits (plan v0.1 section 7). VR Revisited 4.3 also allows
// at most f nodes to fail at once. A recovering node refuses an append, so
// the markers of the quiet end wait until no node recovers.
// With clients::two, two clients register, send requests with request
// numbers, send a call again after a timeout, read and expire sessions, as
// ClientRegister, ClientRequest, ReadStart and ExpireSession in the model
// (plan v0.1 F16). The check adds E1 and R1 in the form of the model. The
// draws of the random generator stay the same with clients::none, so seed
// 57 of the reorder test runs the same steps as before.
// With batches::random, a delivery gives the receiver a batch of 1 to
// largest_batch messages, and the node commits after the batch. A second
// generator from the same seed draws the sizes, so the steps of the first
// generator stay the same. A batch takes the messages to one node in the
// order of the network, so each channel stays FIFO.
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
constexpr std::size_t client_count = 2;
constexpr int call_timeout_steps = 20;
constexpr std::size_t client_share = 4;
constexpr int quiet_round_steps = 5;
constexpr std::size_t call_choices = 20;
constexpr std::size_t query_choices = 5;
constexpr std::size_t expire_choice = query_choices;
constexpr std::uint64_t session_stride = std::uint64_t{1} << 32;
constexpr std::size_t value_size = 8;
constexpr std::size_t largest_batch = 8;
constexpr std::uint64_t batch_seed_offset = 0x9e3779b97f4a7c15;

enum class delivery { fifo, reorder };
enum class rollback { none, one };
enum class clients { none, two };
enum class batches { single, random };

// The echo machine holds no state, so a read cannot show which requests it
// saw. This machine keeps the concatenation of the applied payloads, and a
// query gives that value. Each payload of a request is the 8 bytes of one
// value, so the state is the list of the executed requests in their order.
struct concatenation {
    static constexpr std::array key{std::byte{'v'}};

    static std::vector<std::byte> value_in(const konsens::state_view auto& view)
    {
        const auto found = view.find(key);
        if (!found) {
            return {};
        }
        return {found->begin(), found->end()};
    }

    konsens::applied apply(const std::span<const std::byte> payload, const konsens::state_view auto& view) const
    {
        auto value = value_in(view);
        value.insert(value.end(), payload.begin(), payload.end());
        return {{payload.begin(), payload.end()}, {{{key.begin(), key.end()}, std::move(value)}}};
    }

    std::vector<std::byte> query(std::span<const std::byte>, const konsens::state_view auto& view) const
    {
        return value_in(view);
    }
};

template <typename N>
std::vector<std::byte> concatenation_of(const N& n)
{
    const auto t = n.get_sequence_paxos().get_internal_storage().begin_read_only();
    REQUIRE(t.has_value());
    return concatenation::value_in(konsens::read_view{*t});
}

static_assert(konsens::state_machine<concatenation>);

template <typename Cluster>
using node_of = konsens::node<typename Cluster::storage_type, concatenation>;

konsens::node_config config_of(node_id pid)
{
    std::vector<node_id> peers;
    for (node_id p = 1; p <= node_count; ++p) {
        if (p != pid) {
            peers.push_back(p);
        }
    }
    return {1, pid, peers, 0, election_ticks, resend_ticks, {}, true};
}

std::vector<std::byte> bytes_of(std::uint64_t v)
{
    std::vector<std::byte> payload;
    for (std::size_t shift = 0; shift < value_size * 8; shift += 8) {
        payload.push_back(static_cast<std::byte>((v >> shift) & 0xff));
    }
    return payload;
}

entry entry_of(std::uint64_t v)
{
    return {konsens::entry_kind::normal, bytes_of(v)};
}

std::uint64_t value_of(std::span<const std::byte> bytes)
{
    std::uint64_t v = 0;
    int shift = 0;
    for (const auto b : bytes) {
        v |= std::to_integer<std::uint64_t>(b) << shift;
        shift += 8;
    }
    return v;
}

std::uint64_t value_of(const entry& e)
{
    return value_of(std::span(e.payload));
}

std::vector<std::uint64_t> values_of(std::span<const std::byte> bytes)
{
    std::vector<std::uint64_t> values;
    for (std::size_t k = 0; k + value_size <= bytes.size(); k += value_size) {
        values.push_back(value_of(bytes.subspan(k, value_size)));
    }
    return values;
}

// The proposed set of the model holds whole entries. A std::set needs an
// order, and entry has none, so the set holds the fields as a tuple.
using entry_fields =
    std::tuple<konsens::entry_kind, std::vector<std::byte>, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t>;

entry_fields fields_of(const entry& e)
{
    return {e.kind, e.payload, e.session, e.request_number, e.client_id, e.pid};
}

std::uint64_t request_value(std::uint64_t session, std::uint64_t request_number)
{
    return session * session_stride + request_number;
}

// Executed(p, k) of SequencePaxos.tla in one pass over p. A request at index
// k (from 1) of session s is executed when s < k, the entry at s is a
// RegisterClient entry, no ExpireSession entry of s stands between s and k,
// the request number is above 0, and each request of s between s and k has
// a lower request number. The result lists the executed requests in log
// order, each as request_value(session, request number).
std::vector<std::uint64_t> executed_of(std::span<const entry> p)
{
    struct session_state {
        bool expired;
        std::uint64_t highest;
    };
    std::map<std::uint64_t, session_state> table;
    std::vector<std::uint64_t> executed;
    for (std::size_t k = 1; k <= p.size(); ++k) {
        const auto& e = p[k - 1];
        if (e.session == 0 || e.session >= k) {
            continue;
        }
        if (e.kind == konsens::entry_kind::expire_session) {
            table[e.session].expired = true;
        }
        if (e.kind != konsens::entry_kind::client_request) {
            continue;
        }
        auto& t = table[e.session];
        const bool opened = p[e.session - 1].kind == konsens::entry_kind::register_client;
        if (opened && !t.expired && e.request_number > 0 && e.request_number > t.highest) {
            executed.push_back(request_value(e.session, e.request_number));
        }
        t.highest = std::max(t.highest, e.request_number);
    }
    return executed;
}

bool executed_once(const std::vector<std::uint64_t>& values)
{
    std::set<std::uint64_t> seen;
    return std::ranges::all_of(values, [&seen](const std::uint64_t x) { return seen.insert(x).second; });
}

node_id from_of(const message& m)
{
    return std::visit([](const auto& x) { return x.from; }, m);
}
node_id to_of(const message& m)
{
    return std::visit([](const auto& x) { return x.to; }, m);
}

std::string name_of(const message& m)
{
    static constexpr std::array<const char*, std::variant_size_v<konsens::paxos_msg>> paxos{
        "prepare_req",       "prepare",        "promise",      "accept_sync",        "accept_decide",
        "accepted",          "decide",         "not_accepted", "proposal_forward",   "recovery",
        "recovery_response", "get_state",      "new_state",    "read_index_request", "heartbeat",
        "heartbeat_ack",     "read_index_reply", "repair_request", "repair_response", "get_repair_entries",
        "repair_entries",    "answer",         "try_again",    "refused",            "work",
        "build_request",     "build_response", "build_refused"};
    static constexpr std::array<const char*, 2> ble{"heartbeat_request", "heartbeat_reply"};
    if (const auto* p = std::get_if<konsens::paxos_message>(&m)) {
        return paxos.at(p->msg.index());
    }
    return ble.at(std::get<konsens::ble_message>(m).msg.index());
}

std::string text_of(const entry& e)
{
    switch (e.kind) {
    case konsens::entry_kind::register_client:
        return "register(" + std::to_string(e.client_id) + ")";
    case konsens::entry_kind::client_request:
        return "request(" + std::to_string(e.session) + "." + std::to_string(e.request_number) + ")";
    case konsens::entry_kind::expire_session:
        return "expire(" + std::to_string(e.session) + ")";
    case konsens::entry_kind::normal:
    case konsens::entry_kind::stop_sign:
        break;
    }
    return std::to_string(value_of(e));
}

std::string ballot_text(const ballot& b)
{
    return "(" + std::to_string(b.n) + "," + std::to_string(b.pid) + ")";
}

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
                if constexpr (std::is_same_v<T, konsens::read_index_reply>) {
                    t += " read_index " + std::to_string(x.read_index);
                }
            },
            p->msg);
    }
    return t;
}

template <typename T>
bool is_prefix(std::span<const T> a, std::span<const T> b)
{
    return a.size() <= b.size() && std::ranges::equal(a, b.first(a.size()));
}

bool compatible(std::span<const entry> a, std::span<const entry> b)
{
    return is_prefix(a, b) || is_prefix(b, a);
}

struct view {
    std::vector<entry> log;
    std::size_t decided;
    ballot n_accepted;
    std::pair<konsens::role, konsens::phase> state;
    ballot n_leader;

    std::span<const entry> decided_prefix() const { return std::span(log).first(decided); }
};

// A read of the storage of a node that fails ends the run of the seed.
template <typename T>
T read_value(std::expected<T, std::error_code> r)
{
    REQUIRE(r.has_value());
    return std::move(*r);
}

template <typename N>
view view_of(const N& n)
{
    const auto& sp = n.get_sequence_paxos();
    const auto t = read_value(sp.get_internal_storage().begin_read_only());
    const auto m = read_value(t.get_meta());
    auto log = read_value(t.get_entries(0, m.log_len));
    const auto decided = std::min<std::size_t>(m.decided_idx, log.size());
    return {std::move(log), decided, m.accepted_round, sp.get_state(), sp.get_leader_state().n_leader};
}

template <typename N>
std::string state_of(node_id pid, const view& v, const N& n)
{
    std::string t = "node " + std::to_string(pid) + " promise " +
                    ballot_text(read_value(n.get_sequence_paxos().get_promise())) + " n_accepted " +
                    ballot_text(v.n_accepted) + " decided " + std::to_string(v.decided) + " applied " +
                    std::to_string(n.get_applied_idx()) + " log";
    for (const auto& e : v.log) {
        t += " " + text_of(e);
    }
    return t;
}

enum class call_kind { register_client, client_request, client_query, expire_session };

// One call that a client gave to a node. The node takes the id of the call
// as its client id (plan v0.1 F9), so each reply names the call it answers.
// A read keeps the executed requests of the decided history at its start:
// decided_at_start of ReadStart in the model.
struct call {
    std::size_t client;
    call_kind kind;
    std::uint64_t session;
    std::uint64_t request_number;
    std::vector<std::uint64_t> decided_at_start;
};

// The client of the model: no session (0), or a session and the request
// number that it sends until it has the reply. pending is the call that the
// client waits on, and since is the step of that call. read_began holds the
// highest promise of the cluster when the client began a read that has no
// answer yet; the client reads again until it has one.
struct client {
    std::uint64_t session;
    std::uint64_t request_number;
    std::optional<std::uint64_t> pending;
    int since;
    std::optional<ballot> read_began;
};

// Counts that show which client paths a run took.
struct coverage {
    std::uint64_t sessions;
    std::uint64_t replies;
    std::uint64_t resends;
    std::uint64_t reads;
    std::uint64_t reads_across_leader_change;
    std::uint64_t expired;
    std::uint64_t rollbacks;

    coverage& operator+=(const coverage& o)
    {
        sessions += o.sessions;
        replies += o.replies;
        resends += o.resends;
        reads += o.reads;
        reads_across_leader_change += o.reads_across_leader_change;
        expired += o.expired;
        rollbacks += o.rollbacks;
        return *this;
    }
};

template <typename Cluster>
struct world {
    using node_t = node_of<Cluster>;

    Cluster cluster;
    std::vector<node_t> nodes;
    std::array<std::filesystem::path, node_count> own_directories{
        sqlite_storage::new_directory(), sqlite_storage::new_directory(), sqlite_storage::new_directory()};
    std::vector<message> network;
    std::set<entry_fields> proposed;
    std::vector<entry> decided_history;
    std::vector<ballot> decided_ballot;
    std::array<std::uint64_t, node_count> last_decided{};
    std::array<view, node_count> views{};
    std::vector<std::string> trace;
    bool detailed = false;
    std::uint64_t next_value = 1;
    std::uint64_t nonce = 0;
    batches sizes = batches::single;
    std::mt19937_64 batch_random;

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
    std::optional<std::pair<std::size_t, typename Cluster::earlier_type>> earlier;
    bool rolled_back = false;
    std::array<client, client_count> users{};
    std::map<std::uint64_t, call> calls;
    std::uint64_t next_call = 1;
    std::optional<std::string> client_violation;
    coverage counts{};
    std::size_t history_checked = 0;
    std::size_t proposed_checked = 0;
    std::array<std::pair<std::size_t, std::uint64_t>, node_count> node_checked{};

    void commit(std::size_t i, const std::expected<konsens::transaction, std::error_code>& t)
    {
        REQUIRE(t);
        REQUIRE(nodes[i].commit(*t));
        collect(i);
    }

    void collect(std::size_t i)
    {
        for (auto& m : nodes[i].take_outgoing_messages()) {
            network.push_back(std::move(m));
        }
        views[i] = view_of(nodes[i]);
        for (auto& r : nodes[i].take_client_replies()) {
            receive(i, std::move(r));
        }
    }

    void deliver(std::size_t k)
    {
        const auto to = to_of(network[k]);
        std::vector<message> batch;
        batch.push_back(std::move(network[k]));
        network.erase(std::next(network.begin(), static_cast<std::ptrdiff_t>(k)));
        if (sizes == batches::random) {
            const auto size = std::uniform_int_distribution<std::size_t>(1, largest_batch)(batch_random);
            for (auto it = network.begin(); it != network.end() && batch.size() < size;) {
                if (to_of(*it) != to) {
                    ++it;
                    continue;
                }
                batch.push_back(std::move(*it));
                it = network.erase(it);
            }
        }
        for (const auto& m : batch) {
            note([&] {
                return std::string("deliver " + std::to_string(from_of(m)) + "->" + std::to_string(to_of(m)) + " " +
                                   detail_of(m));
            });
        }
        note([&] { return std::string("commit " + std::to_string(to) + " after " + std::to_string(batch.size())); });
        const auto i = to - 1;
        commit(i, nodes[i].handle_incoming(std::move(batch)));
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
        commit(i, nodes[i].tick());
    }

    void append(std::size_t i)
    {
        const auto e = entry_of(next_value);
        note([&] { return std::string("append " + std::to_string(i + 1) + " value " + std::to_string(next_value)); });
        next_value += 1;
        const auto r = nodes[i].append(e);
        REQUIRE((r || r.error() == konsens::client_errc::recovering || r.error() == konsens::client_errc::retired));
        if (r) {
            proposed.insert(fields_of(e));
            REQUIRE(nodes[i].commit(*r));
        }
        collect(i);
    }

    static konsens::node_config later_config_of(const std::size_t i)
    {
        auto config = config_of(static_cast<node_id>(i + 1));
        config.first_start = false;
        return config;
    }

    void crash(std::size_t i, const bool clean)
    {
        note([&] { return std::string(clean ? "stop and start " : "crash ") + std::to_string(i + 1); });
        const std::vector<entry> decided_before(views[i].decided_prefix().begin(), views[i].decided_prefix().end());
        if (clean && !nodes[i].retired() && !nodes[i].get_sequence_paxos().get_error()) {
            nodes[i].stop();
            for (auto& r : nodes[i].take_client_replies()) {
                receive(i, std::move(r));
            }
        }
        auto kept = [this, i] {
            const node_t stopped = std::move(nodes[i]);
            return cluster.stop(stopped);
        }();
        nonce += 1;
        nodes[i] = *node_t::with(later_config_of(i), cluster.start(i, std::move(kept)),
                                 sqlite_storage::open_node_storage_in(own_directories[i]), concatenation{}, nonce);
        collect(i);
        survived_crash = survived_crash && is_prefix(std::span<const entry>(decided_before), views[i].decided_prefix());
    }

    bool recovers() const
    {
        return std::ranges::any_of(nodes, [](const node_t& n) { return n.get_sequence_paxos().recovering(); });
    }

    void keep_earlier(std::size_t i)
    {
        note([&] { return std::string("keep earlier storage of " + std::to_string(i + 1)); });
        earlier.emplace(i, cluster.keep(i, nodes[i].get_sequence_paxos().get_internal_storage()));
    }

    void roll_back()
    {
        const auto i = earlier->first;
        note([&] { return std::string("roll back " + std::to_string(i + 1)); });
        const auto len_before = read_value(nodes[i].get_sequence_paxos().get_accepted_idx());
        nonce += 1;
        nodes[i] = *node_t::with(later_config_of(i), cluster.restore(i, earlier->second),
                                 sqlite_storage::open_node_storage_in(own_directories[i]), concatenation{}, nonce);
        if (read_value(nodes[i].get_sequence_paxos().get_accepted_idx()) < len_before) {
            counts.rollbacks += 1;
        }
        last_decided[i] = read_value(nodes[i].get_sequence_paxos().get_decided_idx());
        collect(i);
        rolled_back = true;
    }

    // A marker that is appended while the leader changes can be lost before
    // it is decided: Paxos keeps only decided entries. Seed 1224 with
    // batches lost one so: node 3 reached a majority with the Promise of
    // node 2, and the Promise of node 1 later in the same batch held the
    // marker. So the markers wait until one leader in Accept holds the
    // promise of every node.
    bool settled() const
    {
        const auto leader = std::ranges::find_if(nodes, [](const node_t& n) {
            return n.get_sequence_paxos().get_state() == std::pair{konsens::role::leader, konsens::phase::accept};
        });
        if (leader == nodes.end()) {
            return false;
        }
        const auto promise = leader->get_sequence_paxos().get_promise();
        return std::ranges::all_of(nodes, [promise](const node_t& n) {
            return n.retired() || n.get_sequence_paxos().get_promise() == promise;
        });
    }

    std::size_t retired_count() const
    {
        return static_cast<std::size_t>(std::ranges::count_if(nodes, [](const node_t& n) { return n.retired(); }));
    }

    ballot highest_promise() const
    {
        ballot b{};
        for (const auto& n : nodes) {
            b = std::max(b, read_value(n.get_sequence_paxos().get_promise()));
        }
        return b;
    }

    void fail_client(std::string v)
    {
        if (!client_violation) {
            client_violation = std::move(v);
        }
    }

    void receive(std::size_t i, konsens::client_reply r)
    {
        const auto it = calls.find(r.client_id);
        if (it == calls.end()) {
            fail_client("a reply of node " + std::to_string(i + 1) + " names no open call " +
                        std::to_string(r.client_id));
            return;
        }
        const auto id = it->first;
        const auto k = std::move(it->second);
        calls.erase(it);
        auto& c = users[k.client];
        if (!r.response && r.response.error() == konsens::client_errc::stopped) {
            note([&] { return std::string("call " + std::to_string(id) + " stopped at node " + std::to_string(i + 1)); });
            if (c.pending == id) {
                c.pending.reset();
            }
            return;
        }
        note([&] {
            return std::string("reply to call " + std::to_string(id) + " at node " + std::to_string(i + 1) +
                               (r.response ? "" : " error " + r.response.error().message()));
        });
        switch (k.kind) {
        case call_kind::register_client: {
            const auto& log = views[i].log;
            if (!r.response || r.session == 0 || r.session > log.size() ||
                log[r.session - 1].kind != konsens::entry_kind::register_client || log[r.session - 1].client_id != id) {
                fail_client("the reply to RegisterClient call " + std::to_string(id) + " names no entry of it");
                return;
            }
            if (c.session == 0) {
                c = {r.session, 1, std::nullopt, 0, std::nullopt};
                counts.sessions += 1;
            }
            return;
        }
        case call_kind::client_request:
            if (r.session != k.session) {
                fail_client("the reply to request call " + std::to_string(id) + " names another session");
                return;
            }
            if (r.response) {
                if (*r.response != bytes_of(request_value(k.session, k.request_number))) {
                    fail_client("the reply to request call " + std::to_string(id) + " is not the reply of its request");
                    return;
                }
                if (c.session == k.session && c.request_number == k.request_number) {
                    c.request_number += 1;
                    c.pending.reset();
                    counts.replies += 1;
                }
            } else if (r.response.error() == konsens::client_errc::session_expired && c.session == k.session) {
                c = {};
                counts.expired += 1;
            }
            return;
        case call_kind::client_query:
            if (c.pending == id) {
                c.pending.reset();
            }
            if (!r.response) {
                if (r.response.error() != konsens::client_errc::retry) {
                    fail_client("a read gave the error " + r.response.error().message());
                }
                return;
            }
            if (!is_prefix(std::span<const std::uint64_t>(k.decided_at_start),
                           std::span<const std::uint64_t>(values_of(*r.response)))) {
                fail_client("R1: read call " + std::to_string(id) + " at node " + std::to_string(i + 1) +
                            " misses a request that was decided before the read started");
                return;
            }
            counts.reads += 1;
            if (c.read_began && *c.read_began != ballot{} && *c.read_began != highest_promise()) {
                counts.reads_across_leader_change += 1;
            }
            c.read_began.reset();
            return;
        case call_kind::expire_session:
            return;
        }
    }

    template <typename F>
    bool give(std::size_t i, std::uint64_t id, call k, F send)
    {
        const auto r = send();
        if (r) {
            calls.emplace(id, std::move(k));
            REQUIRE(nodes[i].commit(*r));
        } else {
            const auto e = r.error();
            if (e != konsens::client_errc::recovering && e != konsens::client_errc::retry &&
                e != konsens::client_errc::session_expired && e != konsens::client_errc::retired) {
                fail_client("a call at node " + std::to_string(i + 1) + " gave the error " + e.message());
            }
            note([&] { return std::string("call " + std::to_string(id) + " refused: " + e.message()); });
        }
        collect(i);
        return r.has_value();
    }

    // One step of client c at node i. A client waits on its pending call
    // until the timeout, and then gives up on it. A client with no session
    // registers. A client with a read that has no answer reads again.
    // Otherwise choice picks a read, an expiry of its own session or a
    // request. A request always carries the request number that has no reply
    // yet, so a request after a loss or a timeout is the same request again,
    // sent to any node (VR Revisited 4.1).
    void act(std::size_t c, std::size_t i, std::size_t choice, int step)
    {
        auto& cl = users[c];
        if (cl.pending && step - cl.since < call_timeout_steps) {
            return;
        }
        if (cl.pending) {
            counts.resends += 1;
            cl.pending.reset();
        }
        const auto id = next_call;
        next_call += 1;
        auto& n = nodes[i];
        const auto who =
            "client " + std::to_string(c + 1) + " call " + std::to_string(id) + " at node " + std::to_string(i + 1);
        const auto wait = [&cl, id, step](const bool given) {
            if (given) {
                cl.pending = id;
                cl.since = step;
            }
        };
        if (cl.session == 0) {
            note([&] { return who + ": register"; });
            wait(give(i, id, {c, call_kind::register_client, 0, 0, {}}, [&] {
                const auto r = n.register_client(id);
                if (r) {
                    proposed.insert(fields_of({konsens::entry_kind::register_client, {}, 0, 0, id, i + 1}));
                }
                return r;
            }));
            return;
        }
        if (cl.read_began || choice < query_choices) {
            note([&] { return who + ": read"; });
            if (!cl.read_began) {
                cl.read_began = highest_promise();
            }
            wait(give(i, id, {c, call_kind::client_query, cl.session, 0, executed_of(decided_history)},
                      [&] { return n.client_query(id, {}); }));
            return;
        }
        if (choice == expire_choice) {
            note([&] { return who + ": expire session " + std::to_string(cl.session); });
            const entry expiry{konsens::entry_kind::expire_session, {}, cl.session};
            give(i, id, {c, call_kind::expire_session, cl.session, 0, {}}, [&] {
                const auto r = n.expire_session(id, cl.session);
                if (r) {
                    proposed.insert(fields_of(expiry));
                }
                return r;
            });
            return;
        }
        const entry request{konsens::entry_kind::client_request, bytes_of(request_value(cl.session, cl.request_number)),
                            cl.session, cl.request_number};
        note([&] { return who + ": request " + text_of(request); });
        wait(give(i, id, {c, call_kind::client_request, cl.session, cl.request_number, {}}, [&] {
            const auto r = n.client_request(id, cl.session, cl.request_number, request.payload);
            if (r) {
                proposed.insert(fields_of(request));
            }
            return r;
        }));
    }

    // E1 of SequencePaxos.tla: each (session, request number) is executed
    // at most once, in the decided history and in the decided prefix of each
    // node. The library executes in its state machine, so the check also
    // reads the applied state of each node: it holds each request at most
    // once, and it is the list that Executed gives for the applied prefix.
    // A part is checked again only when its index changed. The checks of
    // S1 and S2 run first, so a decided prefix with the same length has the
    // same entries, and a state machine applies the same entries again
    // after a restart.
    std::optional<std::string> check_executed_once()
    {
        if (decided_history.size() != history_checked) {
            history_checked = decided_history.size();
            if (!executed_once(executed_of(decided_history))) {
                return "E1: the decided history executes one request twice";
            }
        }
        for (std::size_t i = 0; i < node_count; ++i) {
            const std::pair<std::size_t, std::uint64_t> now{views[i].decided, nodes[i].get_applied_idx()};
            if (now == node_checked[i]) {
                continue;
            }
            node_checked[i] = now;
            if (!executed_once(executed_of(views[i].decided_prefix()))) {
                return "E1: the decided prefix of node " + std::to_string(i + 1) + " executes one request twice";
            }
            const auto applied = values_of(concatenation_of(nodes[i]));
            if (!executed_once(applied)) {
                return "E1: the state of node " + std::to_string(i + 1) + " executed one request twice";
            }
            const auto a = nodes[i].get_applied_idx();
            if (a > views[i].decided) {
                return "Executed: node " + std::to_string(i + 1) + " applied beyond its decided prefix";
            }
            if (applied != executed_of(std::span(views[i].log).first(a))) {
                return "Executed: the state of node " + std::to_string(i + 1) +
                       " differs from the requests that Executed gives for its applied prefix";
            }
        }
        return std::nullopt;
    }

    std::optional<std::string> check()
    {
        if (client_violation) {
            return client_violation;
        }
        const auto& v = views;
        for (std::size_t i = 0; i < node_count; ++i) {
            const auto d = read_value(nodes[i].get_sequence_paxos().get_decided_idx());
            if (d < last_decided[i]) {
                return "S7: decided_idx of node " + std::to_string(i + 1) + " fell";
            }
            last_decided[i] = d;
        }
        for (std::size_t i = 0; i < node_count; ++i) {
            for (std::size_t j = i + 1; j < node_count; ++j) {
                if (!compatible(v[i].decided_prefix(), v[j].decided_prefix())) {
                    return "S1: decided prefixes of nodes " + std::to_string(i + 1) + " and " + std::to_string(j + 1) +
                           " differ";
                }
            }
        }
        std::optional<std::size_t> longer;
        for (std::size_t i = 0; i < node_count; ++i) {
            const auto p = v[i].decided_prefix();
            if (p.size() > decided_history.size() && is_prefix(std::span<const entry>(decided_history), p) &&
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
            if (!is_prefix(v[i].decided_prefix(), std::span<const entry>(decided_history))) {
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
            if (!is_prefix(std::span<const entry>(decided_history).first(k), std::span<const entry>(v[i].log))) {
                return "S2b: leader " + std::to_string(i + 1) + " misses a decided entry";
            }
        }
        // S2 holds here, so each decided prefix is a prefix of the decided
        // history, and S3 on the new entries of the history covers each node.
        // The done record of a request carries the request, so each entry of
        // the decided history was proposed (plan-one-environment 3.3).
        for (; proposed_checked < decided_history.size(); ++proposed_checked) {
            if (!proposed.contains(fields_of(decided_history[proposed_checked]))) {
                return "S3: entry " + std::to_string(proposed_checked + 1) + " of the decided history was not proposed";
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
        return check_executed_once();
    }
};

struct outcome {
    std::optional<std::string> violation;
    std::vector<std::string> trace;
    bool majority_decided;
    bool all_decided;
    bool survived_crash;
    coverage counts;
    bool majority_retired;
};

template <typename Cluster>
outcome run(std::uint64_t seed, delivery mode, bool detailed, rollback faults = rollback::none,
            clients people = clients::none, batches sizes = batches::random)
{
    std::mt19937_64 rng(seed);
    const auto pick = [&rng](std::size_t n) { return std::uniform_int_distribution<std::size_t>(0, n - 1)(rng); };
    world<Cluster> w;
    w.detailed = detailed;
    w.sizes = sizes;
    w.batch_random.seed(seed + batch_seed_offset);
    for (std::size_t i = 0; i < node_count; ++i) {
        w.nodes.push_back(*node_of<Cluster>::with(config_of(i + 1), w.cluster.start(i, {}),
                                                  sqlite_storage::open_node_storage_in(w.own_directories[i]),
                                                  concatenation{}, w.nonce));
    }
    for (std::size_t i = 0; i < node_count; ++i) {
        w.collect(i);
    }
    const auto failed = [&w](std::string v) {
        for (std::size_t i = 0; i < node_count; ++i) {
            w.note([&] { return std::string(state_of(i + 1, w.views[i], w.nodes[i])); });
        }
        return outcome{std::move(v), std::move(w.trace), false, false, w.survived_crash, w.counts, false};
    };
    for (int s = 0; s < random_steps; ++s) {
        const auto action = pick(100);
        if (action < 60 && !w.network.empty()) {
            const auto k = pick(w.network.size());
            w.deliver(mode == delivery::reorder ? k : w.first_of_channel(k));
        } else if (action < 85) {
            w.tick(pick(node_count));
        } else if (action < 95) {
            if (people == clients::two && pick(client_share) != 0) {
                const auto c = pick(client_count);
                const auto i = pick(node_count);
                w.act(c, i, pick(call_choices), s);
            } else {
                w.append(pick(node_count));
            }
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
            const auto i = pick(node_count);
            w.crash(i, pick(2) == 0);
            w.crashed = true;
        } else if constexpr (requires { typename Cluster::earlier_type; }) {
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
        if (markers.empty() && r >= marker_round && !w.recovers() && w.settled()) {
            for (std::size_t i = 0; i < node_count; ++i) {
                if (!w.nodes[i].retired()) {
                    markers.push_back(entry_of(w.next_value).payload);
                    w.append(i);
                }
            }
        }
        if (people == clients::two) {
            for (std::size_t c = 0; c < client_count; ++c) {
                const auto i = pick(node_count);
                w.act(c, i, pick(call_choices), random_steps + r * quiet_round_steps);
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
        if (auto v = w.check()) {
            return failed(std::move(*v));
        }
    }
    bool majority_decided = !markers.empty();
    bool all_decided = !markers.empty();
    for (const auto& m : markers) {
        std::size_t holders = 0;
        for (const auto& n : w.nodes) {
            const auto d = *n.read_decided_suffix(0);
            if (std::ranges::any_of(d, [&m](const entry& e) { return e.payload == m; })) {
                holders += 1;
            }
        }
        majority_decided = majority_decided && holders > node_count / 2;
        all_decided = all_decided && holders == node_count - w.retired_count();
    }
    return {std::nullopt, {}, majority_decided, all_decided, w.survived_crash, w.counts,
            w.retired_count() > node_count / 2};
}

template <typename Cluster>
coverage run_seeds(delivery mode, std::uint64_t seed_count, rollback faults = rollback::none,
                   clients people = clients::none)
{
    std::uint64_t lagging = 0;
    std::uint64_t majority_retired = 0;
    coverage total{};
    for (std::uint64_t seed = 1; seed <= seed_count; ++seed) {
        const auto o = run<Cluster>(seed, mode, false, faults, people);
        total += o.counts;
        if (o.violation) {
            const auto d = run<Cluster>(seed, mode, true, faults, people);
            std::string tail;
            const auto start = d.trace.size() > trace_tail ? d.trace.size() - trace_tail : 0;
            for (std::size_t i = start; i < d.trace.size(); ++i) {
                tail += d.trace[i] + "\n";
            }
            FAIL("seed " << seed << ": " << *o.violation << "\nlast steps:\n" << tail);
        }
        INFO("seed " << seed);
        CHECK(o.survived_crash);
        if (o.majority_retired) {
            majority_retired += 1;
            continue;
        }
        CHECK(o.majority_decided);
        if (!o.all_decided) {
            lagging += 1;
        }
    }
    MESSAGE(seed_count << " seeds, seeds where one node did not decide every marker: " << lagging
                       << ", seeds where a majority retired: " << majority_retired);
    if (people == clients::two) {
        MESSAGE("sessions " << total.sessions << ", requests answered " << total.replies << ", calls sent again "
                            << total.resends << ", reads answered " << total.reads
                            << ", reads across a change of leader " << total.reads_across_leader_change
                            << ", sessions found expired " << total.expired);
    }
    return total;
}

}
