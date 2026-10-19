#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <exception>
#include <expected>
#include <format>
#include <iostream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

#include "konsens/memory_storage.hpp"
#include "konsens/node.hpp"

namespace {

struct counter {
    std::uint64_t value;

    static std::vector<std::byte> bytes_of(const std::uint64_t v)
    {
        const auto b = std::bit_cast<std::array<std::byte, sizeof v>>(v);
        return {b.begin(), b.end()};
    }

    std::vector<std::byte> apply([[maybe_unused]] const std::span<const std::byte> payload)
    {
        value += 1;
        return bytes_of(value);
    }

    std::vector<std::byte> query([[maybe_unused]] const std::span<const std::byte> payload) const
    {
        return bytes_of(value);
    }
};

using node_t = konsens::node<konsens::memory_storage, counter>;

constexpr std::size_t node_count = 3;
constexpr std::uint64_t election_ticks = 5;
constexpr std::uint64_t resend_ticks = 3;
constexpr int round_limit = 200;
constexpr std::uint64_t increment_count = 5;

std::uint64_t value_of(const std::span<const std::byte> bytes)
{
    std::array<std::byte, sizeof(std::uint64_t)> b{};
    if (bytes.size() != b.size()) [[unlikely]] {
        throw std::logic_error("a counter reply has 8 bytes");
    }
    std::ranges::copy(bytes, b.begin());
    return std::bit_cast<std::uint64_t>(b);
}

konsens::node_config config_of(const konsens::node_id pid)
{
    std::vector<konsens::node_id> peers;
    for (konsens::node_id p = 1; p <= node_count; ++p) {
        if (p != pid) {
            peers.push_back(p);
        }
    }
    return {1, pid, peers, 0, election_ticks, resend_ticks};
}

struct cluster {
    std::array<std::optional<node_t>, node_count> nodes;
    std::deque<konsens::message> network;
    std::vector<konsens::client_reply> replies;
    std::uint64_t next_call{1};

    void collect()
    {
        for (auto& n : nodes) {
            if (n) {
                for (auto& m : n->take_outgoing_messages()) {
                    network.push_back(std::move(m));
                }
                for (auto& r : n->take_client_replies()) {
                    replies.push_back(std::move(r));
                }
            }
        }
    }

    std::expected<void, std::error_code> run_round()
    {
        for (auto& n : nodes) {
            if (n) {
                if (auto r = n->tick(); !r) [[unlikely]] {
                    return r;
                }
            }
        }
        collect();
        while (!network.empty()) {
            auto m = std::move(network.front());
            network.pop_front();
            auto& target = nodes.at(std::visit([](const auto& x) { return x.to; }, m) - 1);
            if (!target) {
                continue;
            }
            if (auto r = target->handle(std::move(m)); !r) [[unlikely]] {
                return r;
            }
            collect();
        }
        return {};
    }

    std::optional<std::size_t> leader() const
    {
        for (std::size_t i = 0; i < node_count; ++i) {
            if (nodes.at(i) && nodes.at(i)->get_sequence_paxos().get_state() ==
                                   std::pair{konsens::role::leader, konsens::phase::accept}) {
                return i;
            }
        }
        return std::nullopt;
    }

    std::expected<void, std::error_code> elect()
    {
        for (int r = 0; r < round_limit; ++r) {
            if (leader()) {
                return {};
            }
            if (auto ran = run_round(); !ran) [[unlikely]] {
                return ran;
            }
        }
        return std::unexpected(std::make_error_code(std::errc::timed_out));
    }

    std::expected<konsens::client_reply, std::error_code> wait_for(const std::uint64_t call)
    {
        for (int r = 0; r < round_limit; ++r) {
            const auto it = std::ranges::find(replies, call, &konsens::client_reply::client_id);
            if (it != replies.end()) {
                auto reply = std::move(*it);
                replies.erase(it);
                if (!reply.response) [[unlikely]] {
                    return std::unexpected(reply.response.error());
                }
                return reply;
            }
            if (auto ran = run_round(); !ran) [[unlikely]] {
                return std::unexpected(ran.error());
            }
        }
        return std::unexpected(std::make_error_code(std::errc::timed_out));
    }

    std::expected<konsens::client_reply, std::error_code> call(const auto give)
    {
        const auto id = next_call++;
        if (auto r = give(id); !r) [[unlikely]] {
            return std::unexpected(r.error());
        }
        return wait_for(id);
    }
};

int fail(const std::string_view step, const std::error_code& code)
{
    std::cerr << std::format("The step \"{}\" failed: {}.", step, code.message()) << '\n';
    return EXIT_FAILURE;
}

int run()
{
    cluster c{};
    for (std::size_t i = 0; i < node_count; ++i) {
        auto n = node_t::with(config_of(static_cast<konsens::node_id>(i + 1)), konsens::memory_storage{}, counter{}, i + 1);
        if (!n) [[unlikely]] {
            return fail("start the nodes", n.error());
        }
        c.nodes.at(i).emplace(std::move(*n));
    }
    if (auto r = c.elect(); !r) [[unlikely]] {
        return fail("elect a leader", r.error());
    }
    std::cout << std::format("Node {} is the leader.", *c.leader() + 1) << '\n';

    const auto registered = c.call([&c](const std::uint64_t id) { return c.nodes.at(0)->register_client(id); });
    if (!registered) [[unlikely]] {
        return fail("register the client", registered.error());
    }
    const auto session = registered->session;
    std::cout << std::format("The client registers at node 1. Its session is {}.", session) << '\n';

    for (std::uint64_t k = 1; k <= increment_count; ++k) {
        const auto at = (k - 1) % node_count;
        const auto reply = c.call([&c, at, session, k](const std::uint64_t id) {
            return c.nodes.at(at)->client_request(id, session, k, {});
        });
        if (!reply) [[unlikely]] {
            return fail("increment the counter", reply.error());
        }
        std::cout << std::format("Increment {} at node {}: the counter is {}.", k, at + 1, value_of(*reply->response))
                  << '\n';
    }

    const auto again = c.call([&c, session](const std::uint64_t id) {
        return c.nodes.at(1)->client_request(id, session, increment_count, {});
    });
    if (!again) [[unlikely]] {
        return fail("send the last increment again", again.error());
    }
    std::cout << std::format("Increment {} again at node 2: the counter stays {}.", increment_count,
                             value_of(*again->response))
              << '\n';

    for (std::size_t i = 0; i < node_count; ++i) {
        const auto read = c.call([&c, i](const std::uint64_t id) { return c.nodes.at(i)->client_query(id, {}); });
        if (!read) [[unlikely]] {
            return fail("read the counter", read.error());
        }
        std::cout << std::format("Read at node {}: the counter is {}.", i + 1, value_of(*read->response)) << '\n';
        if (value_of(*read->response) != increment_count) [[unlikely]] {
            return fail("compare the counter", std::make_error_code(std::errc::state_not_recoverable));
        }
    }

    const auto old_leader = *c.leader();
    c.nodes.at(old_leader).reset();
    std::cout << std::format("Node {} (the leader) is stopped.", old_leader + 1) << '\n';
    c.collect();
    if (auto r = c.elect(); !r) [[unlikely]] {
        return fail("elect a new leader", r.error());
    }
    const auto new_leader = *c.leader();
    std::cout << std::format("Node {} is the new leader.", new_leader + 1) << '\n';
    const auto after = c.call([&c, new_leader, session](const std::uint64_t id) {
        return c.nodes.at(new_leader)->client_request(id, session, increment_count + 1, {});
    });
    if (!after) [[unlikely]] {
        return fail("increment after the change of leader", after.error());
    }
    std::cout << std::format("Increment {} at node {}: the counter is {}.", increment_count + 1, new_leader + 1,
                             value_of(*after->response))
              << '\n';
    return value_of(*after->response) == increment_count + 1 ? EXIT_SUCCESS : EXIT_FAILURE;
}

}

int main()
{
    try {
        return run();
    } catch (const std::exception& e) {
        std::cerr << std::format("The example ended with an exception: {}", e.what()) << '\n';
        return EXIT_FAILURE;
    }
}
