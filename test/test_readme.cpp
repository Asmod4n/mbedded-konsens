#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <unistd.h>

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
    static constexpr std::array key{std::byte{'c'}};

    static std::uint64_t value_in(const konsens::state_view auto& view)
    {
        const auto found = view.find(key);
        return found ? value_of(*found) : 0;
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

using node_t = konsens::node<konsens::persistent_storage, counter>;

constexpr std::size_t max_size = std::size_t{1} << 24;
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
    auto storage = konsens::persistent_storage::open(dir, max_size, non_root_user_id).value();
    const std::filesystem::path node_dir{dir.string() + "-node"};
    std::filesystem::create_directories(node_dir);
    auto own_storage = konsens::node_storage::open(node_dir, non_root_user_id).value();
    auto node = node_t::with({.configuration_id = 1,
                              .pid = pid,
                              .peers = peers,
                              .leader_priority = 0,
                              .election_tick_timeout = 5,
                              .resend_message_tick_timeout = 3,
                              .first_start = true},
                             std::move(storage), std::move(own_storage), counter{}, fresh_nonce())
                    .value();
    return node;
}

struct cluster {
    std::vector<node_t> nodes;
    std::vector<std::string> network;
    std::vector<konsens::client_reply> replies;

    void round()
    {
        std::vector<std::vector<konsens::received>> arrived(nodes.size());
        for (auto& bytes : std::exchange(network, {})) {
            auto r = konsens::decode(std::move(bytes)).value();
            arrived.at(r.to - 1).push_back(std::move(r));
        }
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            auto& node = nodes.at(i);
            auto t = node.tick().value();
            t += node.handle_incoming(std::move(arrived.at(i))).value();
            node.commit(t).value();
            for (const auto& m : node.take_outgoing_messages()) {
                network.push_back(konsens::encode(m).value());
            }
            for (auto& r : node.take_client_replies()) {
                replies.push_back(std::move(r));
            }
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

    std::optional<konsens::client_reply> register_client_at(const std::size_t at)
    {
        for (int i = 0; i < round_limit; ++i) {
            round();
            if (const auto t = nodes.at(at).register_client(client_id)) {
                nodes.at(at).commit(*t).value();
                return wait_for_reply();
            }
        }
        return std::nullopt;
    }
};

}

// The README shows these calls as the way to run a node, write through a
// session and read. This test makes the same calls on three nodes on SQLite,
// with the network of the README: on each tick, decode the messages that
// arrived, handle them as one batch, commit the transaction, then encode
// and send what the commit released.
TEST_CASE("the calls of the README write and read a counter")
{
    temporary_directory d1, d2, d3;
    cluster c;
    c.nodes.push_back(start(1, d1.path()));
    c.nodes.push_back(start(2, d2.path()));
    c.nodes.push_back(start(3, d3.path()));

    const auto registered = c.register_client_at(1);
    REQUIRE(registered);
    REQUIRE(registered->response);
    const auto session = registered->session;

    c.nodes.at(1).commit(c.nodes.at(1).client_request(client_id, session, 1, {}).value()).value();
    const auto written = c.wait_for_reply();
    REQUIRE(written);
    REQUIRE(written->response);
    CHECK(value_of(*written->response) == 1);

    c.nodes.at(2).commit(c.nodes.at(2).client_query(client_id, {}).value()).value();
    const auto read = c.wait_for_reply();
    REQUIRE(read);
    REQUIRE(read->response);
    CHECK(value_of(*read->response) == 1);
}
