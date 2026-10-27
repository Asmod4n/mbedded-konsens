#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <queue>
#include <ranges>
#include <span>
#include <system_error>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "batches.hpp"
#include "konsens/node.hpp"
#include "konsens/persistent_storage.hpp"
#include "sqlite_storage.hpp"

// Plan-execute-verify 2.4.4 under an open loop: requests arrive at a fixed
// rate, whatever the cluster answers, and each message takes a fixed delay.
// The bench showed that the leader then took every waiting request into its
// log, and the work of each step grew with the requests above the applied
// index until a step was longer than the election timeout. The world here
// runs on a virtual clock, so the run is the same on every machine.

namespace {

using konsens::message;
using konsens::node_id;

using microseconds = std::uint64_t;

constexpr std::size_t node_count = 3;
constexpr std::uint32_t config_id = 1;
constexpr std::uint64_t election_ticks = 5;
constexpr std::uint64_t resend_ticks = 3;
constexpr microseconds tick_period = 100'000;
constexpr microseconds message_delay = 20'000;
constexpr microseconds commit_time = 1'000;
constexpr microseconds request_interval = 100;
constexpr microseconds election_time = 2'000'000;
constexpr microseconds registration_time = 500'000;
constexpr microseconds run_time = 60'000'000;
constexpr std::uint64_t request_count = 200;
constexpr std::uint64_t client_id = 10;
constexpr std::uint64_t first_request_call = 100;

// A counter gives its new value as the reply, so the reply of the request
// with number k holds k when each request runs once and in order.
struct counter {
    static constexpr std::array key{std::byte{'c'}};

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
        const auto b = std::bit_cast<std::array<std::byte, sizeof(std::uint64_t)>>(value_in(view) + 1);
        std::vector<std::byte> next(b.begin(), b.end());
        return {next, {{{key.begin(), key.end()}, next}}};
    }

    std::vector<std::byte> query(std::span<const std::byte>, const konsens::state_view auto&) const { return {}; }
};

using node_t = konsens::node<konsens::persistent_storage, counter>;

struct request_call {
    std::uint64_t call;
    std::uint64_t number;
};
struct register_call {};
struct tick_call {};
using element = std::variant<message, request_call, register_call, tick_call>;

struct event {
    microseconds due;
    std::uint64_t order;
    std::size_t to;
    element e;
};

struct later {
    bool operator()(const event& a, const event& b) const
    {
        return std::tie(a.due, a.order) > std::tie(b.due, b.order);
    }
};

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

std::optional<std::size_t> leader_of(const std::vector<node_t>& nodes)
{
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        if (nodes[i].get_sequence_paxos().get_state() == std::pair{konsens::role::leader, konsens::phase::accept}) {
            return i;
        }
    }
    return std::nullopt;
}

std::uint64_t value_of(const std::vector<std::byte>& reply)
{
    std::array<std::byte, sizeof(std::uint64_t)> b{};
    REQUIRE(reply.size() == b.size());
    std::ranges::copy(reply, b.begin());
    return std::bit_cast<std::uint64_t>(b);
}

// Each node takes every element that is due when it is free, as one batch
// and one commit, and is busy for commit_time after it. Its messages reach
// their receiver message_delay after the commit.
struct world {
    std::vector<node_t> nodes;
    std::priority_queue<event, std::vector<event>, later> queue;
    std::vector<microseconds> free_at = std::vector<microseconds>(node_count, 0);
    std::uint64_t order{};
    microseconds next_tick{};
    std::uint64_t session{};
    std::vector<konsens::client_reply> replies;

    void put(const microseconds due, const std::size_t to, element e) { queue.push({due, order++, to, std::move(e)}); }

    void step()
    {
        const auto i = queue.top().to;
        const auto now = std::max(queue.top().due, free_at[i]);
        std::vector<element> taken;
        std::vector<event> others;
        while (!queue.empty() && queue.top().due <= now && taken.size() < konsens::max_batch_size) {
            auto ev = queue.top();
            queue.pop();
            if (ev.to == i) {
                taken.push_back(std::move(ev.e));
            } else {
                others.push_back(std::move(ev));
            }
        }
        for (auto& o : others) {
            queue.push(std::move(o));
        }
        auto& n = nodes[i];
        std::vector<message> batch;
        std::optional<konsens::transaction> t;
        const auto join = [&t](const std::expected<konsens::transaction, std::error_code>& more) {
            REQUIRE(more);
            if (t) {
                *t += *more;
            } else {
                t = *more;
            }
        };
        for (auto& e : taken) {
            if (auto* m = std::get_if<message>(&e)) {
                batch.push_back(std::move(*m));
            } else if (const auto* r = std::get_if<request_call>(&e)) {
                join(n.client_request(r->call, session, r->number, {}));
            } else if (std::holds_alternative<register_call>(e)) {
                join(n.register_client(client_id));
            } else {
                join(n.tick());
            }
        }
        join(n.handle_incoming(std::move(batch)));
        REQUIRE(n.commit(*t));
        free_at[i] = now + commit_time;
        for (auto& m : n.take_outgoing_messages()) {
            const auto to = batches::to_of(m) - 1;
            put(free_at[i] + message_delay, to, std::move(m));
        }
        for (auto& r : n.take_client_replies()) {
            if (r.client_id == client_id) {
                REQUIRE(r.response);
                session = r.session;
            } else {
                replies.push_back(std::move(r));
            }
        }
    }

    void run_until(const microseconds end)
    {
        for (;;) {
            if (next_tick <= end && (queue.empty() || next_tick <= queue.top().due)) {
                for (std::size_t i = 0; i < node_count; ++i) {
                    put(next_tick, i, tick_call{});
                }
                next_tick += tick_period;
            } else if (!queue.empty() && queue.top().due <= end) {
                step();
            } else {
                return;
            }
        }
    }
};

}

// Under an open loop every request still gets its reply, each reply holds
// its own request number, and the leader stays.
TEST_CASE("under an open loop every request gets its reply and the leader stays")
{
    {
        world w{};
        for (node_id p = 1; p <= node_count; ++p) {
            w.nodes.push_back(*node_t::with(config_of(p), sqlite_storage::open_new(),
                                            sqlite_storage::open_new_node_storage(), counter{}, p));
        }
        w.run_until(election_time);
        const auto l = leader_of(w.nodes);
        REQUIRE(l);
        w.put(election_time, *l, register_call{});
        w.run_until(election_time + registration_time);
        REQUIRE(w.session != 0);
        const auto start = election_time + registration_time;
        for (std::uint64_t k = 0; k < request_count; ++k) {
            w.put(start + k * request_interval, *l, request_call{first_request_call + k, k + 1});
        }
        w.run_until(start + run_time);
        CHECK(leader_of(w.nodes) == l);
        REQUIRE(w.replies.size() == request_count);
        for (const auto& r : w.replies) {
            REQUIRE(r.response);
            CHECK(value_of(*r.response) == r.client_id - first_request_call + 1);
        }
    }
}
