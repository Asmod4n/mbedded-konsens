#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

#include "batches.hpp"
#include "konsens/client.hpp"
#include "konsens/node.hpp"
#include "konsens/selection.hpp"
#include "konsens/verdict.hpp"
#include "sqlite_storage.hpp"

// Tests of Execute-Verify (doc/plan-execute-verify.md, with the changes of
// doc/plan-one-environment.md 7): the work, the answers, the selection of
// the leader, the done records and the apply of the chosen results.
// Provisional names made in this tree, for the owner to decide: work,
// answer, try_again, refused, refusal_reason and their members; selection,
// counted_answer, selected_result, leader_log, try_again_target and their
// members; results_agree; done record; client_errc::rejected;
// paxos_errc::result_differs; node_fault; get_selection and
// get_own_result_indexes of node.

namespace {

using konsens::answer;
using konsens::ballot;
using konsens::client_errc;
using konsens::entry;
using konsens::entry_kind;
using konsens::message;
using konsens::node_id;
using konsens::paxos_message;
using konsens::phase;
using konsens::role;

constexpr ballot leader_ballot{1, 1, 0, 3};
constexpr std::array<node_id, 3> three_nodes{1, 2, 3};

std::vector<std::byte> result_of(const std::uint8_t v)
{
    return konsens::encode_result({0, {std::byte{v}}, {}});
}

konsens::indexed_result at(const std::uint64_t index, const std::uint8_t v)
{
    const auto bytes = result_of(v);
    return {index, bytes, konsens::compute_result_crc(index, bytes)};
}

std::uint32_t base_of(const std::vector<std::pair<std::uint64_t, std::uint8_t>>& results)
{
    std::uint32_t crc = 0;
    for (const auto& [index, v] : results) {
        crc = konsens::extend_base_crc(crc, index, konsens::compute_result_crc(index, result_of(v)));
    }
    return crc;
}

answer first_answer(const std::vector<konsens::indexed_result>& results)
{
    return {leader_ballot, 0, results.front().index, 0, results, 0};
}

entry request(const std::uint64_t n)
{
    return {entry_kind::client_request, {std::byte{static_cast<std::uint8_t>(n)}}, 1, n};
}

konsens::selection new_selection()
{
    return konsens::selection::with(leader_ballot, three_nodes, 0, std::nullopt);
}

std::vector<konsens::chosen_result> list_of(const entry& e)
{
    const auto list = konsens::decode_chosen_results(e.result);
    REQUIRE(list.has_value());
    return *list;
}

}

// Plan 2.6.3 and 2.6.4: the leader selects the result that a majority of
// the configuration agrees on, the agreeing node with the lowest id gives
// the bytes, and each counted node that does not agree is asked once to
// run the index again.
TEST_CASE("the leader selects the result of a majority and asks each other node once to run again")
{
    const std::vector<entry> log{request(1)};
    auto s = new_selection();
    s.count_answer(3, first_answer({at(1, 7)}));
    s.count_answer(1, first_answer({at(1, 8)}));
    s.count_answer(2, first_answer({at(1, 7)}));
    const auto targets = s.select_results({0, log});
    CHECK(targets == std::vector<konsens::try_again_target>{{1, 1}});
    REQUIRE(s.get_selected().contains(1));
    CHECK(s.get_selected().at(1).node == 2);
    CHECK(s.get_selected().at(1).chosen == at(1, 7));
    CHECK(s.select_results({0, log}).empty());
}

// Plan 2.6.3: without a majority that agrees, nothing is selected, and the
// answer that completes the majority selects.
TEST_CASE("a result without a majority is not selected")
{
    const std::vector<entry> log{request(1)};
    auto s = new_selection();
    s.count_answer(1, first_answer({at(1, 7)}));
    s.count_answer(2, first_answer({at(1, 8)}));
    CHECK(s.select_results({0, log}).empty());
    CHECK(s.get_selected().empty());
    s.count_answer(3, first_answer({at(1, 8)}));
    CHECK(s.select_results({0, log}) == std::vector<konsens::try_again_target>{{1, 1}});
    CHECK(s.get_selected().at(1).node == 2);
}

// Plan 2.6.2: an answer counts for an index only when it ran on the
// selected base. Node 3 answers for index 2 on a base in which index 1 had
// another result, so its answer does not count until it answers on the
// selected base.
TEST_CASE("an answer on another base is not counted")
{
    const std::vector<entry> log{request(1), request(2)};
    auto s = new_selection();
    s.count_answer(1, first_answer({at(1, 7), at(2, 5)}));
    s.count_answer(2, first_answer({at(1, 7)}));
    s.count_answer(3, {leader_ballot, 0, 2, base_of({{1, 9}}), {at(2, 5)}, 0});
    CHECK(s.select_results({0, log}).empty());
    CHECK(s.get_selected().contains(1));
    CHECK_FALSE(s.get_selected().contains(2));
    s.count_answer(3, {leader_ballot, 0, 2, base_of({{1, 7}}), {at(2, 5)}, 0});
    CHECK(s.select_results({0, log}).empty());
    REQUIRE(s.get_selected().contains(2));
    CHECK(s.get_selected().at(2).node == 1);
}

// Plan 2.5.2: a node answers each step for the indexes it ran since its
// last answer. An answer whose base is the earlier answer extends it; an
// answer whose base does not fit replaces it.
TEST_CASE("a later answer on the same pointer extends the earlier one when its base fits")
{
    auto s = new_selection();
    s.count_answer(1, first_answer({at(1, 7)}));
    s.count_answer(1, {leader_ballot, 0, 2, base_of({{1, 7}}), {at(2, 5)}, 0});
    CHECK(s.get_answers().at(1).results.size() == 2);
    s.count_answer(1, {leader_ballot, 0, 3, base_of({{1, 7}, {2, 6}}), {at(3, 4)}, 0});
    const auto& replaced = s.get_answers().at(1);
    CHECK(replaced.from_idx == 3);
    CHECK(replaced.results.size() == 1);
    s.count_answer(2, {ballot{1, 2, 0, 3}, 0, 1, 0, {at(1, 7)}, 0});
    CHECK_FALSE(s.get_answers().contains(2));
}

// Plan 2.6.3, the hold rule: when p passes an index whose chosen result
// differs from the selection there, each selection above it whose base
// differs goes. When the chosen result is the selected one, the selection
// above holds.
TEST_CASE("a selection above an index whose chosen result differs is dropped")
{
    const std::vector<entry> log{request(1), request(2)};
    for (const std::uint8_t chosen : {std::uint8_t{7}, std::uint8_t{8}}) {
        CAPTURE(int{chosen});
        auto s = new_selection();
        for (const node_id j : three_nodes) {
            s.count_answer(j, first_answer({at(1, 7), at(2, 5)}));
        }
        CHECK(s.select_results({0, log}).empty());
        REQUIRE(s.get_selected().size() == 2);
        s.move_pointer(1, std::vector<konsens::indexed_result>{at(1, chosen)});
        CHECK(s.get_pointer() == 1);
        CHECK(s.get_selected().contains(2) == (chosen == 7));
        CHECK_FALSE(s.get_selected().contains(1));
    }
}

// Plan 2.6.4 and 2.6.5: a node goes into the list only when its second
// answer also does not agree. A node with no second answer at the append is
// not listed. A node whose second answer agrees is not listed. The done
// record is the request with the chosen result (plan-one-environment 3.3).
TEST_CASE("the done record lists a node only when its second run also does not agree")
{
    const std::vector<entry> log{request(1)};
    const konsens::leader_log l{0, log};
    for (const std::uint8_t second : {std::uint8_t{0}, std::uint8_t{8}, std::uint8_t{7}}) {
        CAPTURE(int{second});
        auto s = new_selection();
        s.count_answer(1, first_answer({at(1, 7)}));
        s.count_answer(2, first_answer({at(1, 7)}));
        s.count_answer(3, first_answer({at(1, 8)}));
        CHECK(s.select_results(l) == std::vector<konsens::try_again_target>{{3, 1}});
        if (second != 0) {
            s.count_answer(3, {leader_ballot, 0, 1, 0, {at(1, second)}, 1});
        }
        const auto done = s.find_the_done_records(l);
        REQUIRE(done.size() == 1);
        CHECK(konsens::request_of(done[0]) == request(1));
        CHECK(done[0].result == at(1, 7).result);
        CHECK(done[0].result_crc == at(1, 7).crc);
        CHECK_FALSE(done[0].rejected);
        CHECK(done[0].out == (second == 8 ? std::vector<node_id>{3} : std::vector<node_id>{}));
    }
}

// Plan 2.6.5: a task is rejected only when the counted answers leave no
// result that a majority could still reach. With one node not counted, its
// answer could still make a majority, so the leader waits. The done record
// of a rejected task holds the counted answers.
TEST_CASE("a rejected done record comes only when no result can reach a majority")
{
    const std::vector<entry> log{request(1)};
    const konsens::leader_log l{0, log};
    auto s = new_selection();
    s.count_answer(1, first_answer({at(1, 7)}));
    s.count_answer(2, first_answer({at(1, 8)}));
    CHECK(s.find_the_done_records(l).empty());
    s.count_answer(3, first_answer({at(1, 9)}));
    CHECK(s.select_results(l).empty());
    const auto done = s.find_the_done_records(l);
    REQUIRE(done.size() == 1);
    CHECK(done[0].rejected);
    CHECK(konsens::request_of(done[0]) == request(1));
    CHECK(done[0].result_crc == konsens::compute_result_crc(1, done[0].result));
    const auto list = list_of(done[0]);
    REQUIRE(list.size() == 3);
    CHECK(list[0].node == 1);
    CHECK(list[1].chosen == at(1, 8));
}

// Plan-one-environment 6.2 and 7.6: a step writes the done record of each
// task in log order up to the first request without a selection. A task
// that is no request needs no run, so it follows the task before it.
TEST_CASE("the done records of a step stop at the first request without a selection")
{
    auto s = new_selection();
    for (const node_id j : three_nodes) {
        s.count_answer(j, first_answer({at(1, 7)}));
    }
    const entry normal{entry_kind::normal, {std::byte{4}}};
    const std::vector<entry> log{request(1), normal, request(3), normal};
    CHECK(s.select_results({0, log}).empty());
    const auto done = s.find_the_done_records({0, log});
    REQUIRE(done.size() == 2);
    CHECK(done[0].result == at(1, 7).result);
    CHECK(done[1] == normal);
}

namespace {

// The concatenation of the payloads is the state. A node with a mark gives
// a reply with the mark at its end, so its result differs from the result
// of the others while its mutation is the same.
struct marked {
    static constexpr std::array key{std::byte{'v'}};
    std::uint8_t mark;

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
        auto reply = value;
        if (mark != 0) {
            reply.push_back(std::byte{mark});
        }
        return {reply, {{{key.begin(), key.end()}, value}}};
    }

    std::vector<std::byte> query(std::span<const std::byte>, const konsens::state_view auto& view) const
    {
        return value_in(view);
    }
};

using node_t = konsens::node<konsens::persistent_storage, marked>;

constexpr std::uint64_t election_ticks = 3;
constexpr std::uint64_t resend_ticks = 2;
constexpr int rounds = 40;
constexpr std::uint64_t client_a = 10;

const konsens::cluster_config next_cluster{2, {4, 5, 6}};

konsens::node_config config_of(const node_id pid)
{
    std::vector<node_id> peers;
    for (const auto p : three_nodes) {
        if (p != pid) {
            peers.push_back(p);
        }
    }
    return {1, pid, peers, 0, election_ticks, resend_ticks, {}, true};
}

std::vector<std::byte> bytes_of(const char c)
{
    return {std::byte{static_cast<unsigned char>(c)}};
}

std::size_t once(const message&)
{
    return 1;
}

template <typename T>
bool holds(const message& m)
{
    const auto* p = std::get_if<paxos_message>(&m);
    return p != nullptr && std::holds_alternative<T>(p->msg);
}

struct cluster {
    std::vector<node_t> nodes;

    node_t& at(const node_id p) { return nodes.at(p - 1); }

    template <typename Copies>
    void run(const int count, const Copies copies)
    {
        for (int i = 0; i < count; ++i) {
            batches::deliver(nodes, copies);
            for (auto& n : nodes) {
                batches::tick_and_commit(n);
            }
        }
        batches::deliver(nodes, copies);
    }

    node_id leader() const
    {
        for (const auto p : three_nodes) {
            if (nodes.at(p - 1).get_sequence_paxos().get_state() == std::pair{role::leader, phase::accept}) {
                return p;
            }
        }
        FAIL("no node leads in Accept");
        return 0;
    }

    std::vector<entry> decided_log(const node_id p) { return *at(p).read_decided_suffix(0); }
};

cluster elected(const std::array<std::uint8_t, 3> marks)
{
    cluster c;
    for (const auto p : three_nodes) {
        c.nodes.push_back(*node_t::with(config_of(p), sqlite_storage::open_new(),
                                        sqlite_storage::open_new_node_storage(), marked{marks.at(p - 1)}, p));
    }
    c.run(rounds, once);
    static_cast<void>(c.leader());
    return c;
}

std::uint64_t registered(cluster& c, const node_id at)
{
    batches::commit(c.at(at), c.at(at).register_client(client_a));
    c.run(1, once);
    const auto replies = c.at(at).take_client_replies();
    REQUIRE(replies.size() == 1);
    REQUIRE(replies[0].response);
    return replies[0].session;
}

std::vector<std::byte> state_of(const node_t& n)
{
    const auto t = n.get_sequence_paxos().get_internal_storage().begin_read_only();
    REQUIRE(t.has_value());
    return marked::value_in(konsens::read_view{*t});
}

}

// Plan-one-environment 2 items 5 to 7 and 4.2: a request gets its reply
// when a majority synced its done record and the done record is applied.
// While no accepted reaches the leader, the done record stands in the log
// of each node, it is not decided, and the state does not hold it.
TEST_CASE("a request gets its reply only after a majority synced its done record")
{
    auto c = elected({0, 0, 0});
    const auto l = c.leader();
    const auto s = registered(c, l);
    batches::commit(c.at(l), c.at(l).client_request(client_a, s, 1, bytes_of('a')));
    batches::deliver(c.nodes, [](const message& m) -> std::size_t { return holds<konsens::accepted>(m) ? 0 : 1; });
    CHECK(c.decided_log(l).size() == 1);
    CHECK(c.at(l).get_sequence_paxos().get_accepted_idx() == 2u);
    CHECK(c.at(l).take_client_replies().empty());
    for (const auto& n : c.nodes) {
        CHECK(state_of(n).empty());
    }
    // A connection keeps the order of its messages, so a lost accepted
    // means that the connection was lost. Both ends see the new connection.
    for (const auto f : three_nodes) {
        if (f != l) {
            batches::commit(c.at(l), c.at(l).reconnected(f));
            batches::commit(c.at(f), c.at(f).reconnected(l));
        }
    }
    c.run(rounds, once);
    const auto replies = c.at(l).take_client_replies();
    REQUIRE(replies.size() == 1);
    CHECK(replies[0].response == bytes_of('a'));
    const auto log = c.decided_log(l);
    REQUIRE(log.size() == 2);
    CHECK(konsens::request_of(log[1]) == entry{entry_kind::client_request, bytes_of('a'), s, 1});
    CHECK(log[1].result ==
          konsens::encode_result({0, bytes_of('a'), {{{marked::key.begin(), marked::key.end()}, bytes_of('a')}}}));
    for (const auto& n : c.nodes) {
        CHECK(n.get_applied_idx() == log.size());
        CHECK(state_of(n) == bytes_of('a'));
        CHECK(n.get_own_result_indexes().empty());
    }
}

// Plan 2.3.4, 2.6.4 and the mutation ApplyOwnResult of the model: a node
// whose result differs is not selected, and it applies the chosen bytes,
// not its own, so the states and the sessions of the nodes stay equal.
TEST_CASE("a node whose result differs applies the chosen result")
{
    for (const node_id wrong : three_nodes) {
        CAPTURE(wrong);
        std::array<std::uint8_t, 3> marks{0, 0, 0};
        marks.at(wrong - 1) = 'x';
        auto c = elected(marks);
        const auto l = c.leader();
        const auto s = registered(c, l);
        batches::commit(c.at(l), c.at(l).client_request(client_a, s, 1, bytes_of('a')));
        c.run(rounds, once);
        const auto replies = c.at(l).take_client_replies();
        REQUIRE(replies.size() == 1);
        CHECK(replies[0].response == bytes_of('a'));
        const auto log = c.decided_log(l);
        REQUIRE(log.size() == 2);
        // The selection does not wait for the second answer (2.6.4), and here
        // the leader appends in the step of the selection, so the list holds
        // the node only when its second answer came first.
        CHECK((log[1].out.empty() || log[1].out == std::vector<node_id>{wrong}));
        for (const auto& n : c.nodes) {
            CHECK(state_of(n) == bytes_of('a'));
            CHECK(n.find_client_session(s) == konsens::session_record{1, bytes_of('a')});
        }
    }
}

// Plan 2.3.4 and 2.6.5: when every node gives another result, no result has
// a majority. The leader appends a rejected done record, which keeps the
// index, changes no state and no session, and its client gets rejected.
TEST_CASE("a request whose results all differ is void and its client gets rejected")
{
    auto c = elected({'x', 'y', 'z'});
    const auto l = c.leader();
    const auto s = registered(c, l);
    batches::commit(c.at(l), c.at(l).client_request(client_a, s, 1, bytes_of('a')));
    c.run(rounds, once);
    const auto replies = c.at(l).take_client_replies();
    REQUIRE(replies.size() == 1);
    REQUIRE_FALSE(replies[0].response.has_value());
    CHECK(replies[0].response.error() == client_errc::rejected);
    const auto log = c.decided_log(l);
    CHECK(std::ranges::count(log, true, &entry::rejected) == 1);
    for (const auto& n : c.nodes) {
        CHECK(state_of(n).empty());
        CHECK(n.find_client_session(s) == konsens::session_record{0, {}});
        CHECK(n.get_applied_idx() == log.size());
    }
}

// Plan-one-environment 7.7: a leader proposes a StopSign only when no
// request is in work. Before, the call gives retry.
TEST_CASE("a stop sign waits while a request is in work")
{
    auto c = elected({0, 0, 0});
    const auto l = c.leader();
    const auto s = registered(c, l);
    batches::commit(c.at(l), c.at(l).client_request(client_a, s, 1, bytes_of('a')));
    batches::deliver(c.nodes, [](const message& m) -> std::size_t { return holds<answer>(m) ? 0 : 1; });
    const auto refused = c.at(l).reconfigure(next_cluster);
    REQUIRE_FALSE(refused.has_value());
    CHECK(refused.error() == client_errc::retry);
    c.run(rounds, once);
    batches::commit(c.at(l), c.at(l).reconfigure(next_cluster));
}

// Plan reconfiguration 2.1, 2.4 and 2.6: a decided StopSign gives every
// node the next configuration, and after it no node takes a client call.
TEST_CASE("a decided StopSign ends the configuration on every node")
{
    auto c = elected({0, 0, 0});
    const auto l = c.leader();
    const auto s = registered(c, l);
    batches::commit(c.at(l), c.at(l).reconfigure(next_cluster));
    c.run(rounds, once);
    for (const auto& n : c.nodes) {
        REQUIRE(n.reconfigured().has_value());
        CHECK(*n.reconfigured() == next_cluster);
    }
    for (const auto p : three_nodes) {
        CHECK(c.at(p).client_request(client_a, s, 1, bytes_of('a')).error() == client_errc::reconfiguring);
        CHECK(c.at(p).client_query(client_a, {}).error() == client_errc::reconfiguring);
        CHECK(c.at(p).register_client(client_a).error() == client_errc::reconfiguring);
        CHECK(c.at(p).append({entry_kind::normal, {}}).error() == client_errc::reconfiguring);
    }
    CHECK(c.decided_log(l).back().kind == entry_kind::stop_sign);
}

// Plan reconfiguration 2.3: a StopSign that a follower forwards becomes a
// task behind the request that the leader holds in work. The leader
// appends it after the done record of the request, so the request gets its
// reply in the old configuration.
TEST_CASE("a forwarded StopSign waits behind a request in work")
{
    auto c = elected({0, 0, 0});
    const auto l = c.leader();
    const node_id f = l == 1 ? 2 : 1;
    const auto s = registered(c, l);
    batches::commit(c.at(l), c.at(l).client_request(client_a, s, 1, bytes_of('a')));
    batches::deliver(c.nodes, [](const message& m) -> std::size_t { return holds<answer>(m) ? 0 : 1; });
    batches::commit(c.at(f), c.at(f).reconfigure(next_cluster));
    c.run(rounds, once);
    const auto replies = c.at(l).take_client_replies();
    REQUIRE(replies.size() == 1);
    CHECK(replies[0].response == bytes_of('a'));
    const auto log = c.decided_log(l);
    REQUIRE(log.size() == 3);
    CHECK(log[1].kind == entry_kind::client_request);
    CHECK(log[2].kind == entry_kind::stop_sign);
    for (const auto& n : c.nodes) {
        CHECK(n.reconfigured() == next_cluster);
    }
}

// Plan reconfiguration 4.7: the calls that a new user tries first with a
// wrong configuration. Each is a wrong use.
TEST_CASE("a wrong next configuration is a wrong use")
{
    auto c = elected({0, 0, 0});
    const auto l = c.leader();
    const std::vector<konsens::cluster_config> wrong{
        {1, {4, 5, 6}}, {3, {4, 5, 6}}, {2, {}}, {2, {4, 0}}, {2, {4, 4}}, {2, {4, l}}, {2, {4, l == 1 ? 2u : 1u}},
    };
    for (const auto& w : wrong) {
        CAPTURE(w.configuration_id);
        CHECK_THROWS_AS((void)c.at(l).reconfigure(w), std::logic_error);
    }
    CHECK_THROWS_AS((void)c.at(l).append({entry_kind::stop_sign, {}}), std::logic_error);
}

// Plan-one-environment 3.3: only the leader writes a done record. A done
// record from the user is a wrong use.
TEST_CASE("a done record from the user is a wrong use")
{
    auto c = elected({0, 0, 0});
    const auto l = c.leader();
    CHECK_THROWS_AS((void)c.at(l).append({.kind = entry_kind::client_request,
                                          .session = 1,
                                          .request_number = 1,
                                          .result = result_of(1)}),
                    std::logic_error);
    CHECK_THROWS_AS((void)c.at(l).append({.kind = entry_kind::expire_session, .session = 1, .rejected = true}),
                    std::logic_error);
}

namespace {

// A log whose decided done record carries a CRC that does not belong to its
// result, as a bit flip in the memory of the node that wrote it would give.
std::filesystem::path decided_log_with_a_wrong_crc()
{
    const auto directory = sqlite_storage::new_directory();
    auto storage = sqlite_storage::open_in(directory);
    auto w = storage.begin();
    REQUIRE(w.has_value());
    const auto bytes = konsens::encode_result({0, {}, {}});
    const std::vector<entry> log{{.kind = entry_kind::register_client,
                                  .client_id = client_a,
                                  .pid = 1,
                                  .result = bytes,
                                  .result_crc = konsens::compute_result_crc(1, bytes) + 1}};
    REQUIRE(w->set_promise(leader_ballot));
    REQUIRE(w->set_accepted_round(leader_ballot));
    REQUIRE(w->append_entries(log));
    REQUIRE(w->set_decided_index(log.size()));
    REQUIRE(std::move(*w).commit());
    return directory;
}

// A start of node 1 on the store in `directory` after a clean stop: the
// node storage in `own_directory` holds the mark of that store.
template <typename Machine>
std::expected<node_t, std::error_code> start_after_a_clean_stop(const std::filesystem::path& directory,
                                                                const std::filesystem::path& own_directory,
                                                                const Machine machine, const std::uint64_t nonce)
{
    auto storage = sqlite_storage::open_in(directory);
    auto own = sqlite_storage::open_node_storage_stopped_cleanly(storage, 1, 1, own_directory);
    return node_t::with(config_of(1), std::move(storage), std::move(own), machine, nonce);
}

std::expected<node_t, std::error_code> start_after_a_crash(const std::filesystem::path& directory,
                                                           const std::filesystem::path& own_directory,
                                                           const marked machine, const std::uint64_t nonce)
{
    auto config = config_of(1);
    config.first_start = false;
    return node_t::with(config, sqlite_storage::open_in(directory), sqlite_storage::open_node_storage_in(own_directory),
                        machine, nonce);
}

}

// Plan 2.3.5: before a node writes a chosen result, it checks the CRC that
// the node that ran it computed. A difference is a fault of the own node,
// and the node throws node_fault.
TEST_CASE("a chosen result whose CRC fails on the node throws node_fault")
{
    const auto directory = decided_log_with_a_wrong_crc();
    const auto own_directory = sqlite_storage::new_directory();
    CHECK_THROWS_AS(static_cast<void>(start_after_a_clean_stop(directory, own_directory, marked{0}, 1)),
                    konsens::node_fault);
    // Plan 3.1: the proof is durable before the throw.
    const auto bytes = konsens::encode_result({0, {}, {}});
    const auto crc = konsens::compute_result_crc(1, bytes);
    const konsens::fault_proof proof{konsens::proof_kind::result_crc, 1, leader_ballot, crc + 1, crc};
    CHECK(sqlite_storage::open_node_storage_in(own_directory).get_fault_proof().value() == std::optional{proof});
    // Plan clean-stop 2.3: the start that threw deleted the mark, so the
    // next start retires the node and does not throw again.
    const auto n = start_after_a_crash(directory, own_directory, marked{0}, 2);
    REQUIRE(n.has_value());
    CHECK(n->retired());
}

// Plan clean-stop 2.6: a node with a proof of a fault never votes again,
// also when a mark agrees with its store. So a fault that stays on the disk
// does not restart the node without end.
TEST_CASE("a node with a proof of a fault retires at the start, with or without a mark")
{
    const auto directory = decided_log_with_a_wrong_crc();
    const auto own_directory = sqlite_storage::new_directory();
    CHECK_THROWS_AS(static_cast<void>(start_after_a_clean_stop(directory, own_directory, marked{0}, 1)),
                    konsens::node_fault);
    const auto with_mark = start_after_a_clean_stop(directory, own_directory, marked{0}, 2);
    REQUIRE(with_mark.has_value());
    CHECK(with_mark->retired());
    const auto without_mark = start_after_a_crash(directory, own_directory, marked{0}, 3);
    REQUIRE(without_mark.has_value());
    CHECK(without_mark->retired());
}

namespace {

// batches::deliver with a change of each message on its way, as a fault of
// the network or of the sender would make it. The node that got a changed
// entry finds later that its chain CRC differs from the leader. It throws
// node_fault (plan 3.2), and the delivery leaves it out from then on, as a
// process that ended. The result is the set of nodes that threw.
template <typename Change>
std::set<node_id> deliver_changed(std::vector<node_t>& nodes, const Change change)
{
    std::set<node_id> faulted;
    for (bool more = true; more;) {
        more = false;
        std::vector<std::vector<message>> inbox(nodes.size());
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            if (faulted.contains(static_cast<node_id>(i + 1))) {
                continue;
            }
            for (auto& m : nodes[i].take_outgoing_messages()) {
                more = true;
                change(m);
                inbox.at(batches::to_of(m) - 1).push_back(std::move(m));
            }
        }
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            if (inbox[i].empty() || faulted.contains(static_cast<node_id>(i + 1))) {
                continue;
            }
            try {
                const auto t = nodes[i].handle_incoming(std::move(inbox[i]));
                if (t) {
                    static_cast<void>(nodes[i].commit(*t));
                }
            } catch (const konsens::node_fault&) {
                faulted.insert(static_cast<node_id>(i + 1));
            }
        }
    }
    return faulted;
}

// The index of the done record of the request after the registration.
constexpr std::uint64_t request_index = 2;

// Changes the done record of the request in each accept_decide to node `to`.
template <typename ChangeRecord>
auto change_the_done_record_to(const node_id to, const ChangeRecord change_record)
{
    return [to, change_record](message& m) {
        auto* p = std::get_if<paxos_message>(&m);
        if (p == nullptr || p->to != to) {
            return;
        }
        auto* a = std::get_if<konsens::accept_decide>(&p->msg);
        if (a == nullptr) {
            return;
        }
        for (auto& e : a->entries) {
            if (e.kind == entry_kind::client_request) {
                change_record(e);
            }
        }
    };
}

void give_another_result(entry& e)
{
    e.result = konsens::encode_result({0, bytes_of('z'), {}});
    e.result_crc = konsens::compute_result_crc(request_index, e.result);
}

node_id first_follower_of(const node_id leader)
{
    return leader == 1 ? 2 : 1;
}

}

// Plan 2.7.1, 2.7.6 and plan-one-environment 7.4: a node accepts a done
// record only after it checked it. Here the record that reaches one
// follower holds another result, with a CRC that fits it, as a bit flip in
// the leader would give. The follower runs again, its result still differs,
// it is not listed, so it refuses with the reason, and each accepted that
// it sends stays below the record. The leader decides with the other
// follower.
TEST_CASE("a follower refuses a done record whose result differs from its own")
{
    auto c = elected({0, 0, 0});
    const auto l = c.leader();
    const auto f = first_follower_of(l);
    const auto s = registered(c, l);
    batches::commit(c.at(l), c.at(l).client_request(client_a, s, 1, bytes_of('a')));
    std::uint64_t highest_accepted = 0;
    const auto flip = change_the_done_record_to(f, give_another_result);
    deliver_changed(c.nodes, [&](message& m) {
        flip(m);
        if (const auto* p = std::get_if<paxos_message>(&m); p != nullptr && p->from == f) {
            if (const auto* a = std::get_if<konsens::accepted>(&p->msg)) {
                highest_accepted = std::max(highest_accepted, a->accepted_idx);
            }
        }
    });
    const auto& refusal = c.at(f).get_refusal();
    REQUIRE(refusal.has_value());
    CHECK(refusal->index == request_index);
    CHECK(refusal->reason == konsens::refusal_reason::result_differs);
    CHECK(highest_accepted < request_index);
    const auto replies = c.at(l).take_client_replies();
    REQUIRE(replies.size() == 1);
    CHECK(replies[0].response == bytes_of('a'));
}

// Plan 2.7.1: a listed node accepts only when its result still does not
// agree after the second run; when it agrees, the list is wrong and the
// node refuses. A CRC that does not fit its result is refused before any
// run.
TEST_CASE("a follower refuses a done record with a wrong list or a wrong CRC")
{
    for (const auto reason : {konsens::refusal_reason::wrong_list, konsens::refusal_reason::crc_mismatch}) {
        CAPTURE(int{std::to_underlying(reason)});
        auto c = elected({0, 0, 0});
        const auto l = c.leader();
        const auto f = first_follower_of(l);
        const auto s = registered(c, l);
        batches::commit(c.at(l), c.at(l).client_request(client_a, s, 1, bytes_of('a')));
        deliver_changed(c.nodes, change_the_done_record_to(f, [f, reason](entry& e) {
            if (reason == konsens::refusal_reason::wrong_list) {
                e.out.push_back(f);
            } else {
                e.result_crc += 1;
            }
        }));
        const auto& refusal = c.at(f).get_refusal();
        REQUIRE(refusal.has_value());
        CHECK(refusal->reason == reason);
    }
}

// Plan-one-environment 7.4: the leader checks its own done record in the
// step that appends it, as every node does, before it counts its own
// accept. Here no done record reaches a follower, so the record is not
// decided, and the leader passed it.
TEST_CASE("the leader checks its own done record in the step that appends it")
{
    auto c = elected({0, 0, 0});
    const auto l = c.leader();
    const auto s = registered(c, l);
    batches::commit(c.at(l), c.at(l).client_request(client_a, s, 1, bytes_of('a')));
    const auto& sp = c.at(l).get_sequence_paxos();
    batches::deliver(c.nodes, [](const message& m) -> std::size_t {
        const auto* p = std::get_if<paxos_message>(&m);
        const auto* a = p == nullptr ? nullptr : std::get_if<konsens::accept_decide>(&p->msg);
        return a != nullptr && std::ranges::any_of(a->entries, konsens::entry_is_a_request) ? 0 : 1;
    });
    CHECK(sp.get_accepted_idx() == request_index);
    CHECK(sp.get_verified_idx() == request_index);
    CHECK(sp.get_decided_idx() == request_index - 1);
}

// Plan 2.7.4: verified_idx is not durable. After a start it is the decided
// index, raised over each entry after it that is no request.
TEST_CASE("after a start the verified index is the decided index and the entries after it that are no request")
{
    auto c = elected({0, 0, 0});
    const auto l = c.leader();
    const auto s = registered(c, l);
    batches::commit(c.at(l), c.at(l).client_request(client_a, s, 1, bytes_of('a')));
    c.run(rounds, once);
    const auto log_len = c.at(1).get_sequence_paxos().get_accepted_idx();
    REQUIRE(log_len.has_value());
    auto copy = sqlite_storage::open_copy_of(c.at(1).get_sequence_paxos().get_internal_storage());
    auto own = sqlite_storage::open_node_storage_stopped_cleanly(copy, 1);
    auto n = *node_t::with(config_of(1), std::move(copy), std::move(own), marked{0}, 9);
    CHECK(n.get_sequence_paxos().get_verified_idx() == *log_len);
}

namespace {

const std::array<std::byte, 1> value_key{std::byte{'v'}};

// A decided log of the done record of a registration at index 1 and the
// done record of a request with the payload 'a' at index 2 that lists `out`
// as the nodes that did not agree with its chosen result.
std::filesystem::path decided_log_with_a_listed(const std::vector<node_id>& out)
{
    const auto directory = sqlite_storage::new_directory();
    auto storage = sqlite_storage::open_in(directory);
    auto w = storage.begin();
    REQUIRE(w.has_value());
    const auto registration = konsens::encode_result({0, {}, {}});
    const auto request = konsens::encode_result(
        {0, bytes_of('a'), {{{value_key.begin(), value_key.end()}, bytes_of('a')}}});
    constexpr std::uint64_t session = 1;
    const std::vector<entry> log{{.kind = entry_kind::register_client,
                                  .client_id = client_a,
                                  .pid = 1,
                                  .result = registration,
                                  .result_crc = konsens::compute_result_crc(1, registration)},
                                 {.kind = entry_kind::client_request,
                                  .payload = bytes_of('a'),
                                  .session = session,
                                  .request_number = 1,
                                  .result = request,
                                  .result_crc = konsens::compute_result_crc(2, request),
                                  .out = out}};
    REQUIRE(w->set_promise(leader_ballot));
    REQUIRE(w->set_accepted_round(leader_ballot));
    REQUIRE(w->append_entries(log));
    REQUIRE(w->set_decided_index(log.size()));
    REQUIRE(std::move(*w).commit());
    return directory;
}

}

// Plan 2.8.1 and 3.1: a listed node runs the index again on the check base
// when it applies the done record. Its result still differs, so it
// throws after its proof is durable. The list makes it run again; the
// second run makes it throw.
TEST_CASE("a listed node whose result still differs throws at the apply")
{
    const auto directory = decided_log_with_a_listed({1});
    const auto own_directory = sqlite_storage::new_directory();
    CHECK_THROWS_AS(static_cast<void>(start_after_a_clean_stop(directory, own_directory, marked{'x'}, 1)),
                    konsens::node_fault);
    const auto proof = sqlite_storage::open_node_storage_in(own_directory).get_fault_proof();
    REQUIRE(proof.has_value());
    REQUIRE(proof->has_value());
    CHECK((*proof)->kind == konsens::proof_kind::listed_result_differs);
    CHECK((*proof)->index == 2);
    CHECK((*proof)->n == leader_ballot);
}

// Plan 2.8.1 and 2.8.2: a listed node whose second run agrees does not
// throw, because the list is wrong and not its result. A node that is not
// listed does not run again.
TEST_CASE("a listed node whose result agrees and a node that is not listed do not throw")
{
    for (const auto& out : {std::vector<node_id>{1}, std::vector<node_id>{2}}) {
        CAPTURE(out.front());
        auto n = *start_after_a_clean_stop(decided_log_with_a_listed(out), sqlite_storage::new_directory(), marked{0}, 1);
        CHECK(state_of(n) == bytes_of('a'));
        CHECK(n.get_fault_proof().value() == std::optional<konsens::fault_proof>{});
    }
}

// Plan 2.8.1 and 2.8.2a: a listed node whose second run agrees reports the
// wrong list as a record to the log sink, with the proof. It sends no
// message to another node.
TEST_CASE("a listed node whose result agrees gives a record of the wrong list")
{
    auto n = *start_after_a_clean_stop(decided_log_with_a_listed({1}), sqlite_storage::new_directory(), marked{0}, 1);
    const auto request = konsens::encode_result(
        {0, bytes_of('a'), {{{value_key.begin(), value_key.end()}, bytes_of('a')}}});
    const auto crc = konsens::compute_result_crc(2, request);
    CHECK(n.take_log_records() ==
          std::vector<konsens::fault_proof>{{konsens::proof_kind::wrong_list, 2, leader_ballot, crc, crc}});
    CHECK(n.take_log_records().empty());
    CHECK_FALSE(std::ranges::any_of(n.take_outgoing_messages(), [](const message& m) {
        return holds<konsens::refused>(m) || holds<konsens::answer>(m);
    }));
}

// Plan 2.7.6: a leader that gets refused from a majority for a done record
// that its own ballot appended knows that its own selection or copy is
// wrong. Here both followers get another result in the record, so both
// refuse it. The leader throws after its proof is durable.
TEST_CASE("a leader whose done record a majority refused throws")
{
    auto c = elected({0, 0, 0});
    const auto l = c.leader();
    const auto s = registered(c, l);
    batches::commit(c.at(l), c.at(l).client_request(client_a, s, 1, bytes_of('a')));
    std::vector<std::function<void(message&)>> changes;
    for (const auto p : three_nodes) {
        if (p != l) {
            changes.emplace_back(change_the_done_record_to(p, give_another_result));
        }
    }
    const auto faulted = deliver_changed(c.nodes, [&changes](message& m) {
        for (const auto& change : changes) {
            change(m);
        }
    });
    CHECK(faulted.contains(l));
    const auto proof = c.at(l).get_fault_proof();
    REQUIRE(proof.has_value());
    REQUIRE(proof->has_value());
    CHECK((*proof)->kind == konsens::proof_kind::refused_by_a_majority);
    CHECK((*proof)->index == request_index);
}

// Plan 3.1 and 3.2: a damage that a step finds is no returned value any
// more. The follower that got a changed entry finds that its chain CRC
// differs from the leader, keeps the proof and throws.
TEST_CASE("a damage that a step finds throws after its proof is durable")
{
    auto c = elected({0, 0, 0});
    const auto l = c.leader();
    const auto f = first_follower_of(l);
    const auto s = registered(c, l);
    batches::commit(c.at(l), c.at(l).client_request(client_a, s, 1, bytes_of('a')));
    const auto faulted = deliver_changed(c.nodes, change_the_done_record_to(f, give_another_result));
    REQUIRE(faulted.contains(f));
    CHECK_FALSE(faulted.contains(l));
    const auto proof = c.at(f).get_fault_proof();
    REQUIRE(proof.has_value());
    REQUIRE(proof->has_value());
    CHECK((*proof)->kind == konsens::proof_kind::damage);
}

namespace {

// RFC 8949 3.3: a double is the initial byte 0xfb and the 8 bytes of the
// binary64 value in network byte order.
std::vector<std::byte> double_item(const double value)
{
    const auto bits = std::bit_cast<std::uint64_t>(value);
    std::vector<std::byte> out{std::byte{0xfb}};
    for (int shift = 56; shift >= 0; shift -= 8) {
        out.push_back(static_cast<std::byte>(bits >> shift));
    }
    return out;
}

// A state machine with the float rule. Its reply and its value are one
// encoded CBOR data item each: a double of one third, with an error of the
// node in its last digits, as two libraries of the same function give.
struct floating {
    static constexpr std::array key{std::byte{'f'}};
    std::uint32_t float_digits;
    std::optional<std::uint32_t> float_denominator;
    double error;
    bool raw;

    konsens::applied apply(std::span<const std::byte>, const konsens::state_view auto&) const
    {
        const auto item = raw ? std::vector<std::byte>{std::byte{0x61}} : double_item(1.0 / 3 * (1 + error));
        return {item, {{{key.begin(), key.end()}, item}}};
    }

    std::vector<std::byte> query(std::span<const std::byte>, const konsens::state_view auto& view) const
    {
        const auto found = view.find(key);
        return found ? std::vector<std::byte>{found->begin(), found->end()} : std::vector<std::byte>{};
    }
};

using floating_node = konsens::node<konsens::persistent_storage, floating>;

std::vector<floating_node> floating_cluster(const std::array<floating, 3>& machines)
{
    std::vector<floating_node> nodes;
    for (const auto p : three_nodes) {
        nodes.push_back(*floating_node::with(config_of(p), sqlite_storage::open_new(),
                                             sqlite_storage::open_new_node_storage(), machines.at(p - 1), p));
    }
    return nodes;
}

void run_floating(std::vector<floating_node>& nodes, const int count)
{
    for (int i = 0; i < count; ++i) {
        batches::deliver(nodes, once);
        for (auto& n : nodes) {
            batches::tick_and_commit(n);
        }
    }
    batches::deliver(nodes, once);
}

floating_node& leader_of(std::vector<floating_node>& nodes)
{
    for (auto& n : nodes) {
        if (n.get_sequence_paxos().get_state() == std::pair{role::leader, phase::accept}) {
            return n;
        }
    }
    FAIL("no node leads in Accept");
    return nodes.front();
}

std::vector<std::byte> stored_value(const floating_node& n)
{
    const auto t = n.get_sequence_paxos().get_internal_storage().begin_read_only();
    REQUIRE(t.has_value());
    const auto found = konsens::read_view{*t}.find(floating::key);
    REQUIRE(found.has_value());
    return {found->begin(), found->end()};
}

std::vector<konsens::client_reply> write_once(std::vector<floating_node>& nodes)
{
    run_floating(nodes, rounds);
    auto& l = leader_of(nodes);
    batches::commit(l, l.register_client(client_a));
    run_floating(nodes, rounds);
    const auto registration = l.take_client_replies();
    REQUIRE(registration.size() == 1);
    batches::commit(l, l.client_request(client_a, registration[0].session, 1, bytes_of('a')));
    run_floating(nodes, rounds);
    return l.take_client_replies();
}

}

// Plan 2.2.2: the results of the three nodes differ in their last bits and
// agree to float_digits digits. The leader selects one of them, asks no
// node to run again, and every node stores the chosen bytes.
TEST_CASE("results whose floats agree to float_digits digits give one chosen result")
{
    auto nodes = floating_cluster({floating{12, std::nullopt, 0, false}, floating{12, std::nullopt, 1e-14, false},
                                   floating{12, std::nullopt, -1e-14, false}});
    const auto replies = write_once(nodes);
    REQUIRE(replies.size() == 1);
    REQUIRE(replies[0].response.has_value());
    for (const auto& n : nodes) {
        CHECK(stored_value(n) == *replies[0].response);
    }
}

// Plan 2.2a: each node rounds its own result to float_denominator before
// the CRC. One third with an error in its last digits rounds to 3.333333e-01
// on every node, so the three results have the same bytes.
TEST_CASE("each node rounds its own result to float_denominator")
{
    auto nodes = floating_cluster(
        {floating{12, 6, 0, false}, floating{12, 6, 1e-14, false}, floating{12, 6, -1e-14, false}});
    const auto replies = write_once(nodes);
    REQUIRE(replies.size() == 1);
    CHECK(replies[0].response == double_item(0.3333333));
    for (const auto& n : nodes) {
        CHECK(stored_value(n) == double_item(0.3333333));
    }
}

// Plan 2.2b: a state machine with the float rule gives its reply and each
// value as one encoded CBOR data item. A reply that is no such item is a
// wrong use.
TEST_CASE("a reply that is no CBOR data item under the float rule is a wrong use")
{
    auto nodes = floating_cluster(
        {floating{12, 6, 0, true}, floating{12, 6, 0, true}, floating{12, 6, 0, true}});
    run_floating(nodes, rounds);
    auto& l = leader_of(nodes);
    batches::commit(l, l.register_client(client_a));
    run_floating(nodes, rounds);
    const auto registration = l.take_client_replies();
    REQUIRE(registration.size() == 1);
    // The leader runs the request in the step that makes it a task.
    CHECK_THROWS_AS(batches::commit(l, l.client_request(client_a, registration[0].session, 1, bytes_of('a'))),
                    std::logic_error);
}
