#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "konsens/node.hpp"
#include "konsens/persistent_storage.hpp"
#include "konsens/wire.hpp"
#include "temporary_directory.hpp"

namespace {

std::vector<std::byte> bytes_of(const std::uint64_t v)
{
    const auto b = std::bit_cast<std::array<std::byte, sizeof v>>(v);
    return {b.begin(), b.end()};
}

std::uint64_t value_of(const std::span<const std::byte> bytes)
{
    std::array<std::byte, sizeof(std::uint64_t)> b{};
    std::ranges::copy(bytes, b.begin());
    return std::bit_cast<std::uint64_t>(b);
}

struct counter {
    std::uint64_t value;

    std::vector<std::byte> apply(std::span<const std::byte>) { return bytes_of(++value); }
    std::vector<std::byte> query(std::span<const std::byte>) const { return bytes_of(value); }
};

using node_t = konsens::node<konsens::persistent_storage, counter>;

constexpr std::size_t map_size = std::size_t{1} << 24;
constexpr std::uint64_t client_id = 7;
constexpr int round_limit = 200;

// The test machine runs as root, and the library refuses root. The README
// passes geteuid(); the test passes a user id that is not root.
constexpr uid_t non_root_user_id = 1000;

std::uint64_t fresh_nonce()
{
    std::random_device rd;
    return (std::uint64_t{rd()} << 32) | rd();
}

node_t start(const konsens::node_id pid, const std::filesystem::path& dir)
{
    std::vector<konsens::node_id> peers;
    for (konsens::node_id p = 1; p <= 3; ++p) {
        if (p != pid) {
            peers.push_back(p);
        }
    }
    auto storage = konsens::persistent_storage::open(dir, map_size, non_root_user_id).value();
    return node_t::with({.configuration_id = 1, .pid = pid, .peers = peers, .leader_priority = 0,
                         .election_tick_timeout = 5, .resend_message_tick_timeout = 3},
                        std::move(storage), counter{}, fresh_nonce())
        .value();
}

struct cluster {
    std::vector<node_t> nodes;
    std::vector<konsens::client_reply> replies;

    void round()
    {
        std::deque<std::string> network;
        auto collect = [&] {
            for (auto& n : nodes) {
                for (const auto& m : n.take_outgoing_messages()) {
                    network.push_back(konsens::encode(m).value());
                }
                for (auto& r : n.take_client_replies()) {
                    replies.push_back(std::move(r));
                }
            }
        };
        for (auto& n : nodes) {
            REQUIRE(n.tick());
        }
        collect();
        while (!network.empty()) {
            auto bytes = std::move(network.front());
            network.pop_front();
            auto r = konsens::decode(std::move(bytes)).value();
            REQUIRE(nodes.at(r.to - 1).handle_incoming(r));
            collect();
        }
    }

    std::optional<konsens::client_reply> wait_for_reply()
    {
        for (int i = 0; i < round_limit && replies.empty(); ++i) {
            round();
        }
        if (replies.empty()) {
            return std::nullopt;
        }
        auto r = std::move(replies.front());
        replies.erase(replies.begin());
        return r;
    }
};

}

// The README shows these calls as the way to run a node, write through a
// session and read. This test makes the same calls on three nodes on LMDB,
// with the network of the README: encode, then decode and handle.
TEST_CASE("the calls of the README write and read a counter")
{
    temporary_directory d1, d2, d3;
    cluster c;
    c.nodes.push_back(start(1, d1.path()));
    c.nodes.push_back(start(2, d2.path()));
    c.nodes.push_back(start(3, d3.path()));

    std::optional<konsens::client_reply> registered;
    for (int i = 0; i < round_limit && !registered; ++i) {
        c.round();
        if (c.nodes.at(1).register_client(client_id)) {
            registered = c.wait_for_reply();
        }
    }
    REQUIRE(registered);
    REQUIRE(registered->response);
    const auto session = registered->session;

    REQUIRE(c.nodes.at(1).client_request(client_id, session, 1, {}));
    const auto written = c.wait_for_reply();
    REQUIRE(written);
    REQUIRE(written->response);
    CHECK(value_of(*written->response) == 1);

    REQUIRE(c.nodes.at(2).client_query(client_id, {}));
    const auto read = c.wait_for_reply();
    REQUIRE(read);
    REQUIRE(read->response);
    CHECK(value_of(*read->response) == 1);
}
