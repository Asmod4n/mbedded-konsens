#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "batches.hpp"
#include "konsens/client.hpp"
#include "konsens/node.hpp"
#include "sqlite_storage.hpp"

// Tests of the build of a node of the next configuration
// (doc/plan-reconfiguration.md 2.7 to 2.9 and 4.4 to 4.6). The nodes 1, 2
// and 3 form configuration 1, and the nodes 4, 5 and 6 form configuration 2.

namespace {

using konsens::client_errc;
using konsens::entry;
using konsens::entry_kind;
using konsens::message;
using konsens::node_id;
using konsens::paxos_message;
using konsens::phase;
using konsens::role;

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
        return {value, {{{key.begin(), key.end()}, value}}};
    }

    std::vector<std::byte> query(std::span<const std::byte>, const konsens::state_view auto& view) const
    {
        return value_in(view);
    }
};

using node_t = konsens::node<konsens::persistent_storage, concatenation>;

constexpr std::array<node_id, 3> old_nodes{1, 2, 3};
constexpr std::array<node_id, 3> new_nodes{4, 5, 6};
constexpr std::uint64_t election_ticks = 3;
constexpr std::uint64_t resend_ticks = 2;
constexpr int rounds = 40;
constexpr std::uint64_t client_a = 10;

const konsens::cluster_config next_cluster{2, {4, 5, 6}};

std::vector<node_id> others(const std::array<node_id, 3>& nodes, const node_id pid)
{
    std::vector<node_id> peers;
    for (const auto p : nodes) {
        if (p != pid) {
            peers.push_back(p);
        }
    }
    return peers;
}

konsens::node_config config_of(const node_id pid)
{
    if (pid <= old_nodes.back()) {
        return {1, pid, others(old_nodes, pid), 0, election_ticks, resend_ticks};
    }
    return {2, pid, others(new_nodes, pid), 0, election_ticks, resend_ticks, {1, 2, 3}};
}

std::vector<std::byte> bytes_of(const std::string_view text)
{
    std::vector<std::byte> bytes;
    for (const auto c : text) {
        bytes.push_back(std::byte{static_cast<unsigned char>(c)});
    }
    return bytes;
}

template <typename T>
const T* body_of(const message& m)
{
    const auto* p = std::get_if<paxos_message>(&m);
    return p == nullptr ? nullptr : std::get_if<T>(&p->msg);
}

// A slot of the cluster holds a node or nothing, so that a test starts the
// nodes of the next configuration after the decide, and ends and starts a
// node on its directories as a crash and a start do.
struct cluster {
    std::vector<std::optional<node_t>> nodes;
    std::vector<std::filesystem::path> logs;
    std::vector<std::filesystem::path> owns;

    node_t& at(const node_id p) { return *nodes.at(p - 1); }

    void start(const node_id p)
    {
        nodes.at(p - 1).emplace(*node_t::with(config_of(p), sqlite_storage::open_in(logs.at(p - 1)),
                                              sqlite_storage::open_node_storage_in(owns.at(p - 1)), concatenation{},
                                              p));
    }

    void stop(const node_id p) { nodes.at(p - 1).reset(); }

    void deliver()
    {
        for (bool more = true; more;) {
            more = false;
            std::vector<std::vector<message>> inbox(nodes.size());
            for (auto& n : nodes) {
                if (!n) {
                    continue;
                }
                for (auto& m : n->take_outgoing_messages()) {
                    more = true;
                    inbox.at(batches::to_of(m) - 1).push_back(std::move(m));
                }
            }
            for (std::size_t i = 0; i < nodes.size(); ++i) {
                if (nodes[i] && !inbox[i].empty()) {
                    batches::commit(*nodes[i], nodes[i]->handle_incoming(std::move(inbox[i])));
                }
            }
        }
    }

    void run(const int count)
    {
        for (int i = 0; i < count; ++i) {
            deliver();
            for (auto& n : nodes) {
                if (n) {
                    batches::tick_and_commit(*n);
                }
            }
        }
        deliver();
    }

    node_id leader_of(const std::array<node_id, 3>& configuration)
    {
        for (const auto p : configuration) {
            if (nodes.at(p - 1) && at(p).get_sequence_paxos().get_state() == std::pair{role::leader, phase::accept}) {
                return p;
            }
        }
        FAIL("no node leads in Accept");
        return 0;
    }

    std::vector<entry> decided_log(const node_id p) { return *at(p).read_decided_suffix(0); }
};

cluster old_configuration()
{
    cluster c;
    for (std::size_t i = 0; i < old_nodes.size() + new_nodes.size(); ++i) {
        c.nodes.emplace_back();
        c.logs.push_back(sqlite_storage::new_directory());
        c.owns.push_back(sqlite_storage::new_directory());
    }
    for (const auto p : old_nodes) {
        c.start(p);
    }
    c.run(rounds);
    static_cast<void>(c.leader_of(old_nodes));
    return c;
}

std::uint64_t registered(cluster& c, const node_id at)
{
    batches::commit(c.at(at), c.at(at).register_client(client_a));
    c.run(1);
    const auto replies = c.at(at).take_client_replies();
    REQUIRE(replies.size() == 1);
    REQUIRE(replies[0].response);
    return replies[0].session;
}

std::vector<std::byte> request_and_reply(cluster& c, const std::array<node_id, 3>& configuration,
                                         const std::uint64_t session, const std::uint64_t number,
                                         const std::string_view payload)
{
    const auto l = c.leader_of(configuration);
    batches::commit(c.at(l), c.at(l).client_request(client_a, session, number, bytes_of(payload)));
    c.run(1);
    const auto replies = c.at(l).take_client_replies();
    REQUIRE(replies.size() == 1);
    REQUIRE(replies[0].response);
    return *replies[0].response;
}

// One hop of the network: the messages of from to to arrive, and every
// other message of from is lost.
void hop(cluster& c, const node_id from, const node_id to)
{
    std::vector<message> batch;
    for (auto& m : c.at(from).take_outgoing_messages()) {
        if (batches::to_of(m) == to) {
            batch.push_back(std::move(m));
        }
    }
    batches::commit(c.at(to), c.at(to).handle_incoming(std::move(batch)));
}

message the_build_request_of(cluster& c, const node_id p)
{
    const auto sent = c.at(p).take_outgoing_messages();
    const auto found =
        std::ranges::find_if(sent, [](const message& m) { return body_of<konsens::build_request>(m) != nullptr; });
    REQUIRE(found != sent.end());
    CHECK(std::ranges::count_if(sent, [](const message& m) { return body_of<konsens::build_request>(m) != nullptr; }) ==
          1);
    return *found;
}

std::vector<message> answers_to_a_build_request(cluster& c, const node_id at, const node_id from,
                                                const std::uint64_t from_idx)
{
    static_cast<void>(c.at(at).take_outgoing_messages());
    std::vector<message> batch{paxos_message{from, at, konsens::build_request{7, from_idx}}};
    batches::commit(c.at(at), c.at(at).handle_incoming(std::move(batch)));
    std::vector<message> answers;
    for (auto& m : c.at(at).take_outgoing_messages()) {
        if (batches::to_of(m) == from) {
            answers.push_back(std::move(m));
        }
    }
    return answers;
}

void append_entries(cluster& c, const std::size_t count)
{
    const auto l = c.leader_of(old_nodes);
    for (std::size_t i = 0; i < count; ++i) {
        const auto t = c.at(l).append({entry_kind::normal, bytes_of("x")});
        REQUIRE(t);
        if (i + 1 == count) {
            batches::commit(c.at(l), t);
        }
    }
    c.run(1);
}

void reconfigure(cluster& c)
{
    const auto l = c.leader_of(old_nodes);
    batches::commit(c.at(l), c.at(l).reconfigure(next_cluster));
    c.run(1);
    for (const auto p : old_nodes) {
        REQUIRE(c.at(p).reconfigured() == next_cluster);
    }
}

}

// Plan reconfiguration 4.4: three nodes hand over to three new nodes. Each
// new node copies the decided log of an old node, the next configuration
// elects its leader and decides entries, and a client session of
// configuration 1 goes on in configuration 2, with the reply of a
// duplicate request from the state.
TEST_CASE("three nodes hand over to three new nodes, and a client session goes on")
{
    auto c = old_configuration();
    const auto s = registered(c, c.leader_of(old_nodes));
    CHECK(request_and_reply(c, old_nodes, s, 1, "a") == bytes_of("a"));
    reconfigure(c);
    for (const auto p : new_nodes) {
        c.start(p);
        CHECK(c.at(p).get_sequence_paxos().building());
    }
    c.run(rounds);
    const auto old_log = c.decided_log(1);
    REQUIRE(old_log.back().kind == entry_kind::stop_sign);
    for (const auto p : new_nodes) {
        CHECK_FALSE(c.at(p).get_sequence_paxos().building());
        CHECK(c.at(p).get_sequence_paxos().get_config_first_idx() == old_log.size());
        CHECK_FALSE(c.at(p).reconfigured().has_value());
    }
    CHECK(request_and_reply(c, new_nodes, s, 2, "b") == bytes_of("ab"));
    CHECK(request_and_reply(c, new_nodes, s, 2, "b") == bytes_of("ab"));
    const auto new_log = c.decided_log(c.leader_of(new_nodes));
    REQUIRE(new_log.size() > old_log.size());
    CHECK(std::ranges::equal(std::span(new_log).first(old_log.size()), old_log));
    for (const auto p : new_nodes) {
        CHECK(c.decided_log(p) == new_log);
    }
}

// Plan reconfiguration 2.7 and 4.5: an old node gives its decided prefix only
// after its StopSign is decided, and only to a node that the StopSign lists.
// It never gives an entry above its decided index.
TEST_CASE("an old node gives its decided prefix only to a listed node after the decide")
{
    auto c = old_configuration();
    append_entries(c, 2);
    auto refused = answers_to_a_build_request(c, 1, 4, 0);
    REQUIRE(refused.size() == 1);
    CHECK(body_of<konsens::build_refused>(refused[0]) != nullptr);
    reconfigure(c);
    CHECK(answers_to_a_build_request(c, 1, 9, 0).empty());
    const auto log = c.decided_log(1);
    refused = answers_to_a_build_request(c, 1, 4, log.size() + 1);
    REQUIRE(refused.size() == 1);
    CHECK(body_of<konsens::build_refused>(refused[0]) != nullptr);
    const auto answered = answers_to_a_build_request(c, 1, 4, 1);
    REQUIRE(answered.size() == 1);
    const auto* r = body_of<konsens::build_response>(answered[0]);
    REQUIRE(r != nullptr);
    CHECK(r->nonce == 7u);
    CHECK(r->from_idx == 1u);
    CHECK(r->decided_idx == log.size());
    CHECK(std::ranges::equal(r->entries, std::span(log).subspan(1)));
}

// Plan reconfiguration 2.9 (decided 2026-10-27 after the model run of
// results/20261027T134426Z-build-from-recovering.md): an old node that
// starts after the decide recovers, and it gives its decided prefix during
// the recovery, because a decided StopSign fixes that prefix.
TEST_CASE("an old node that recovers after the decide gives its decided prefix")
{
    auto c = old_configuration();
    reconfigure(c);
    const auto log = c.decided_log(1);
    c.stop(1);
    c.start(1);
    REQUIRE(c.at(1).get_sequence_paxos().recovering());
    const auto answered = answers_to_a_build_request(c, 1, 4, 0);
    REQUIRE(answered.size() == 1);
    const auto* r = body_of<konsens::build_response>(answered[0]);
    REQUIRE(r != nullptr);
    CHECK(r->entries == log);
}

// Plan reconfiguration 2.7 and 4.6: the build writes each part in one
// transaction. A new node that ends after a part starts again in its build
// and asks for the entries after the end of its log.
TEST_CASE("a new node that ends during its build continues from the end of its log")
{
    auto c = old_configuration();
    append_entries(c, konsens::max_build_entries + 2);
    reconfigure(c);
    const auto log = c.decided_log(1);
    REQUIRE(log.size() > konsens::max_build_entries);
    c.start(4);
    hop(c, 4, 1);
    hop(c, 1, 4);
    REQUIRE(c.at(4).get_sequence_paxos().building());
    CHECK(c.at(4).get_sequence_paxos().get_accepted_idx() == konsens::max_build_entries);
    c.stop(4);
    c.start(4);
    REQUIRE(c.at(4).get_sequence_paxos().building());
    const auto sent = the_build_request_of(c, 4);
    CHECK(body_of<konsens::build_request>(sent)->from_idx == konsens::max_build_entries);
    batches::commit(c.at(1), c.at(1).handle_incoming(std::vector<message>{sent}));
    hop(c, 1, 4);
    CHECK_FALSE(c.at(4).get_sequence_paxos().building());
    CHECK(c.decided_log(4) == log);
}

// Plan reconfiguration 2.7 and 4.6: a part whose chain CRC does not agree
// is not written, and the next request goes to the next node of
// build_from.
TEST_CASE("a part whose chain CRC fails is not written, and the next source is asked")
{
    auto c = old_configuration();
    reconfigure(c);
    const auto log = c.decided_log(1);
    c.start(4);
    const auto sent = the_build_request_of(c, 4);
    const auto* request = body_of<konsens::build_request>(sent);
    CHECK(batches::to_of(sent) == 1u);
    const konsens::build_response damaged{request->nonce, 0, log, 0, log.size(), 0};
    batches::commit(c.at(4), c.at(4).handle_incoming(std::vector<message>{paxos_message{1, 4, damaged}}));
    CHECK(c.at(4).get_sequence_paxos().get_accepted_idx() == 0u);
    for (std::uint64_t i = 0; i < resend_ticks; ++i) {
        batches::tick_and_commit(c.at(4));
    }
    const auto again = c.at(4).take_outgoing_messages();
    const auto to_node_2 = std::ranges::find_if(again, [](const message& m) {
        return body_of<konsens::build_request>(m) != nullptr && batches::to_of(m) == 2;
    });
    CHECK(to_node_2 != again.end());
}

// Plan reconfiguration 2.7 and 4.6: the store says which configuration a
// node belongs to. A start with another configuration_id is a wrong use, and
// a new node without build_from cannot build. A node of configuration 1
// starts its configuration at index 0.
TEST_CASE("a start with a configuration_id that the store does not hold is a wrong use")
{
    auto c = old_configuration();
    CHECK(c.at(1).get_sequence_paxos().get_config_first_idx() == 0u);
    reconfigure(c);
    for (const auto p : new_nodes) {
        c.start(p);
    }
    c.run(rounds);
    c.stop(1);
    c.stop(4);
    auto wrong_old = config_of(1);
    wrong_old.configuration_id = 2;
    wrong_old.build_from = {7};
    CHECK_THROWS_AS(
        static_cast<void>(node_t::with(wrong_old, sqlite_storage::open_in(c.logs.at(0)),
                                       sqlite_storage::open_node_storage_in(c.owns.at(0)), concatenation{}, 1)),
        std::logic_error);
    auto wrong_new = config_of(4);
    wrong_new.configuration_id = 1;
    CHECK_THROWS_AS(
        static_cast<void>(node_t::with(wrong_new, sqlite_storage::open_in(c.logs.at(3)),
                                       sqlite_storage::open_node_storage_in(c.owns.at(3)), concatenation{}, 4)),
        std::logic_error);
    auto no_source = config_of(4);
    no_source.build_from.clear();
    CHECK_THROWS_AS(static_cast<void>(node_t::with(no_source, sqlite_storage::open_new(),
                                                   sqlite_storage::open_new_node_storage(), concatenation{}, 4)),
                    std::logic_error);
}

// Plan reconfiguration 2.8 and 4.6: in the log of configuration 2, the
// StopSign of configuration 1 stands before config_first_idx. A node of
// configuration 2 that starts again recovers, and that StopSign does not end
// its configuration.
TEST_CASE("a node of configuration 2 starts again with the old StopSign in its log")
{
    auto c = old_configuration();
    const auto s = registered(c, c.leader_of(old_nodes));
    reconfigure(c);
    for (const auto p : new_nodes) {
        c.start(p);
    }
    c.run(rounds);
    CHECK(request_and_reply(c, new_nodes, s, 1, "a") == bytes_of("a"));
    const auto first = c.at(4).get_sequence_paxos().get_config_first_idx();
    c.stop(4);
    c.start(4);
    c.run(rounds);
    CHECK(c.at(4).get_sequence_paxos().get_config_first_idx() == first);
    CHECK_FALSE(c.at(4).reconfigured().has_value());
    CHECK(request_and_reply(c, new_nodes, s, 2, "b") == bytes_of("ab"));
}
