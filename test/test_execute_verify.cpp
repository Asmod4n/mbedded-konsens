#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
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
#include "lmdb_storage.hpp"

// Tests of Execute-Verify (doc/plan-execute-verify.md), step 2: the
// answers, the selection of the leader, the verdict entries and the apply
// of the chosen results. Provisional names made in this tree, for the owner
// to decide: answer, try_again, refused, refusal_reason and their members;
// selection, counted_answer, selected_result, leader_log, try_again_target
// and their members; results_agree; max_in_flight of node_config;
// client_errc::rejected; paxos_errc::result_differs; node_fault;
// get_selection and get_own_result_indexes of node.

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
    return konsens::selection::with(leader_ballot, three_nodes, 0);
}

std::vector<konsens::chosen_result> list_of(const entry& e)
{
    const auto list = konsens::decode_chosen_results(e.payload);
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
    const auto targets = s.select_results({0, 1, log});
    CHECK(targets == std::vector<konsens::try_again_target>{{1, 1}});
    REQUIRE(s.get_selected().contains(1));
    CHECK(s.get_selected().at(1).node == 2);
    CHECK(s.get_selected().at(1).chosen == at(1, 7));
    CHECK(s.select_results({0, 1, log}).empty());
}

// Plan 2.6.3: without a majority that agrees, nothing is selected, and the
// answer that completes the majority selects.
TEST_CASE("a result without a majority is not selected")
{
    const std::vector<entry> log{request(1)};
    auto s = new_selection();
    s.count_answer(1, first_answer({at(1, 7)}));
    s.count_answer(2, first_answer({at(1, 8)}));
    CHECK(s.select_results({0, 1, log}).empty());
    CHECK(s.get_selected().empty());
    s.count_answer(3, first_answer({at(1, 8)}));
    CHECK(s.select_results({0, 1, log}) == std::vector<konsens::try_again_target>{{1, 1}});
    CHECK(s.get_selected().at(1).node == 2);
}

// Plan 2.6.2: an answer counts for an index only when it ran on the
// selected base. Node 3 answers for index 2 on a base in which index 1 had
// another result, so its answer does not count until it answers on the
// selected base. A selection is made only of decided indexes.
TEST_CASE("an answer on another base is not counted")
{
    const std::vector<entry> log{request(1), request(2)};
    auto s = new_selection();
    s.count_answer(1, first_answer({at(1, 7), at(2, 5)}));
    s.count_answer(2, first_answer({at(1, 7)}));
    s.count_answer(3, {leader_ballot, 0, 2, base_of({{1, 9}}), {at(2, 5)}, 0});
    CHECK(s.select_results({0, 1, log}).empty());
    CHECK_FALSE(s.get_selected().contains(2));
    CHECK(s.select_results({0, 2, log}).empty());
    CHECK(s.get_selected().contains(1));
    CHECK_FALSE(s.get_selected().contains(2));
    s.count_answer(3, {leader_ballot, 0, 2, base_of({{1, 7}}), {at(2, 5)}, 0});
    CHECK(s.select_results({0, 2, log}).empty());
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
        CHECK(s.select_results({0, 2, log}).empty());
        REQUIRE(s.get_selected().size() == 2);
        s.move_pointer(1, std::vector<konsens::chosen_result>{{at(1, chosen), 1, {}}});
        CHECK(s.get_pointer() == 1);
        CHECK(s.get_selected().contains(2) == (chosen == 7));
        CHECK_FALSE(s.get_selected().contains(1));
    }
}

// Plan 2.6.4 and 2.6.5: a node goes into the list only when its second
// answer also does not agree. A node with no second answer at the append is
// not listed. A node whose second answer agrees is not listed.
TEST_CASE("the results entry lists a node only when its second run also does not agree")
{
    const std::vector<entry> log{request(1)};
    const konsens::leader_log l{0, 1, log};
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
        const auto verdict = s.find_the_verdict_entry(l);
        REQUIRE(verdict.has_value());
        CHECK(verdict->kind == entry_kind::results);
        CHECK(verdict->from_idx == 1);
        CHECK(verdict->to_idx == 1);
        const auto list = list_of(*verdict);
        REQUIRE(list.size() == 1);
        CHECK(list[0].chosen == at(1, 7));
        CHECK(list[0].node == 1);
        CHECK(list[0].out == (second == 8 ? std::vector<node_id>{3} : std::vector<node_id>{}));
    }
}

// Plan 2.6.5: rejected(p + 1) only when the counted answers leave no result
// that a majority could still reach. With one node not counted, its answer
// could still make a majority, so the leader waits.
TEST_CASE("rejected comes only when no result can reach a majority")
{
    const std::vector<entry> log{request(1)};
    const konsens::leader_log l{0, 1, log};
    auto s = new_selection();
    s.count_answer(1, first_answer({at(1, 7)}));
    s.count_answer(2, first_answer({at(1, 8)}));
    CHECK_FALSE(s.find_the_verdict_entry(l).has_value());
    s.count_answer(3, first_answer({at(1, 9)}));
    CHECK(s.select_results(l).empty());
    const auto verdict = s.find_the_verdict_entry(l);
    REQUIRE(verdict.has_value());
    CHECK(verdict->kind == entry_kind::rejected);
    CHECK(verdict->from_idx == 1);
    const auto list = list_of(*verdict);
    REQUIRE(list.size() == 3);
    CHECK(list[0].node == 1);
    CHECK(list[1].chosen == at(1, 8));
}

// Plan 2.6.5: the leader appends no verdict entry while a verdict entry of
// its log above p is not decided, so two verdicts for one range do not
// race.
TEST_CASE("no verdict entry while a verdict entry above the pointer is not decided")
{
    auto s = new_selection();
    for (const node_id j : three_nodes) {
        s.count_answer(j, first_answer({at(1, 7)}));
    }
    std::vector<entry> log{request(1)};
    CHECK(s.select_results({0, 1, log}).empty());
    const auto verdict = s.find_the_verdict_entry({0, 1, log});
    REQUIRE(verdict.has_value());
    log.push_back(*verdict);
    CHECK_FALSE(s.find_the_verdict_entry({0, 1, log}).has_value());
    CHECK(s.find_the_verdict_entry({0, 2, log}).has_value());
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

konsens::node_config config_of(const node_id pid, const std::uint64_t max_in_flight)
{
    std::vector<node_id> peers;
    for (const auto p : three_nodes) {
        if (p != pid) {
            peers.push_back(p);
        }
    }
    return {1, pid, peers, 0, election_ticks, resend_ticks, max_in_flight};
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

cluster elected(const std::array<std::uint8_t, 3> marks, const std::uint64_t max_in_flight = 16)
{
    cluster c;
    for (const auto p : three_nodes) {
        c.nodes.push_back(*node_t::with(config_of(p, max_in_flight), lmdb_storage::open_new(),
                                        lmdb_storage::open_new_node_storage(), lmdb_storage::open_new_state_storage(),
                                        marked{marks.at(p - 1)}, p));
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
    const auto t = n.get_state_storage().begin_read_only();
    REQUIRE(t.has_value());
    return marked::value_in(konsens::read_view{*t});
}

std::size_t count_of(const std::vector<entry>& log, const entry_kind kind)
{
    return static_cast<std::size_t>(std::ranges::count(log, kind, &entry::kind));
}

}

// Plan 2.9.1: a request gets its reply when its entry is applied, and that
// is the decide of its results entry, a second round after the decide of
// the request. While no answer reaches the leader, the request is decided
// and has no reply.
TEST_CASE("a request gets its reply only after the decide of its results entry")
{
    auto c = elected({0, 0, 0});
    const auto l = c.leader();
    const auto s = registered(c, l);
    batches::commit(c.at(l), c.at(l).client_request(client_a, s, 1, bytes_of('a')));
    batches::deliver(c.nodes, [](const message& m) -> std::size_t { return holds<answer>(m) ? 0 : 1; });
    CHECK(count_of(c.decided_log(l), entry_kind::client_request) == 1);
    CHECK(count_of(c.decided_log(l), entry_kind::results) == 1);
    CHECK(c.at(l).take_client_replies().empty());
    CHECK(state_of(c.at(l)).empty());
    c.run(rounds, once);
    const auto replies = c.at(l).take_client_replies();
    REQUIRE(replies.size() == 1);
    CHECK(replies[0].response == bytes_of('a'));
    const auto log = c.decided_log(l);
    REQUIRE(log.size() == 4);
    CHECK(log[3].kind == entry_kind::results);
    CHECK(log[3].from_idx == 2);
    CHECK(log[3].to_idx == 3);
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
        const auto verdict = std::ranges::find_if(
            log, [](const entry& e) { return e.kind == entry_kind::results && e.to_idx == 3; });
        REQUIRE(verdict != log.end());
        const auto list = list_of(*verdict);
        REQUIRE(list.size() == 1);
        // The selection does not wait for the second answer (2.6.4), and here
        // the leader appends in the step of the selection, so the list holds
        // the node only when its second answer came first.
        CHECK((list[0].out.empty() || list[0].out == std::vector<node_id>{wrong}));
        CHECK(list[0].node != wrong);
        for (const auto& n : c.nodes) {
            CHECK(state_of(n) == bytes_of('a'));
            CHECK(n.find_client_session(s) == konsens::session_record{1, bytes_of('a')});
        }
    }
}

// Plan 2.3.4 and 2.6.5: when every node gives another result, no result has
// a majority. The leader appends rejected(k), the entry keeps its index and
// changes no state and no session, and its client gets rejected.
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
    CHECK(count_of(log, entry_kind::rejected) == 1);
    for (const auto& n : c.nodes) {
        CHECK(state_of(n).empty());
        CHECK(n.find_client_session(s) == konsens::session_record{0, {}});
        CHECK(n.get_applied_idx() == log.size());
    }
}

// Plan 2.4.4: the leader proposes no more request entries while its own
// results above its applied index reach max_in_flight. The request waits in
// the node and goes into the log after the verdict.
TEST_CASE("the leader holds a request while max_in_flight results wait for their verdict")
{
    auto c = elected({0, 0, 0}, 1);
    const auto l = c.leader();
    const auto s = registered(c, l);
    const auto no_answer = [](const message& m) -> std::size_t { return holds<answer>(m) ? 0 : 1; };
    batches::commit(c.at(l), c.at(l).client_request(client_a, s, 1, bytes_of('a')));
    batches::deliver(c.nodes, no_answer);
    CHECK(c.at(l).get_own_result_indexes() == std::vector<std::uint64_t>{3});
    batches::commit(c.at(l), c.at(l).client_request(client_a, s, 2, bytes_of('b')));
    batches::deliver(c.nodes, no_answer);
    CHECK(count_of(*c.at(l).read_decided_suffix(0), entry_kind::client_request) == 1);
    const auto accepted = c.at(l).get_sequence_paxos().get_accepted_idx();
    REQUIRE(accepted.has_value());
    CHECK(*accepted == 3);
    c.run(rounds, once);
    CHECK(count_of(c.decided_log(l), entry_kind::client_request) == 2);
    CHECK(state_of(c.at(l)) == std::vector<std::byte>{std::byte{'a'}, std::byte{'b'}});
}

// Plan 2.3.6: a leader proposes a StopSign only when p covers every request
// entry of its log. Before, the call gives retry.
TEST_CASE("a stop sign waits while a request entry has no verdict")
{
    auto c = elected({0, 0, 0});
    const auto l = c.leader();
    const auto s = registered(c, l);
    batches::commit(c.at(l), c.at(l).client_request(client_a, s, 1, bytes_of('a')));
    batches::deliver(c.nodes, [](const message& m) -> std::size_t { return holds<answer>(m) ? 0 : 1; });
    const auto refused = c.at(l).append({entry_kind::stop_sign, {}});
    REQUIRE_FALSE(refused.has_value());
    CHECK(refused.error() == client_errc::retry);
    c.run(rounds, once);
    batches::commit(c.at(l), c.at(l).append({entry_kind::stop_sign, {}}));
}

// Plan 2.1.2: only the leader appends a verdict entry. A verdict entry
// from the user is a wrong use.
TEST_CASE("a verdict entry from the user is a wrong use")
{
    auto c = elected({0, 0, 0});
    const auto l = c.leader();
    CHECK_THROWS_AS((void)c.at(l).append({.kind = entry_kind::results, .payload = {}, .from_idx = 1, .to_idx = 1}),
                    std::logic_error);
    CHECK_THROWS_AS((void)c.at(l).append({.kind = entry_kind::rejected, .payload = {}, .from_idx = 1}),
                    std::logic_error);
}

// Plan 2.3.5: before a node writes a chosen result, it checks the CRC that
// the node that ran it computed. A difference is a fault of the own node,
// and the node throws node_fault. The log here holds a results entry whose
// listed CRC does not belong to its bytes, as a bit flip in the memory of
// this node would give.
TEST_CASE("a chosen result whose CRC fails on the node throws node_fault")
{
    const auto directory = lmdb_storage::new_directory();
    {
        auto storage = lmdb_storage::open_in(directory);
        auto w = storage.begin();
        REQUIRE(w.has_value());
        const auto bytes = konsens::encode_result({0, {}, {}});
        const std::vector<konsens::chosen_result> list{
            {{1, bytes, konsens::compute_result_crc(1, bytes) + 1}, 1, {}}};
        const std::vector<entry> log{{entry_kind::register_client, {}, 0, 0, client_a, 1},
                                     {.kind = entry_kind::results,
                                      .payload = konsens::encode_chosen_results(list),
                                      .from_idx = 1,
                                      .to_idx = 1}};
        REQUIRE(w->set_promise(leader_ballot));
        REQUIRE(w->set_accepted_round(leader_ballot));
        REQUIRE(w->append_entries(log));
        REQUIRE(w->set_decided_index(log.size()));
        REQUIRE(std::move(*w).commit());
    }
    CHECK_THROWS_AS((void)node_t::with(config_of(1, 16), lmdb_storage::open_in(directory),
                                       lmdb_storage::open_new_node_storage(), lmdb_storage::open_new_state_storage(),
                                       marked{0}, 1),
                    konsens::node_fault);
}

// A max_in_flight of 0 would hold every request for good, so it is a wrong
// use.
TEST_CASE("a max_in_flight of 0 is a wrong use")
{
    CHECK_THROWS_AS((void)node_t::with(config_of(1, 0), lmdb_storage::open_new(), lmdb_storage::open_new_node_storage(),
                                       lmdb_storage::open_new_state_storage(), marked{0}, 1),
                    std::logic_error);
}

namespace {

// batches::deliver with a change of each message on its way, as a fault of
// the network or of the sender would make it. The node that got a changed
// entry finds later that its chain CRC differs from the leader and stops
// voting, so a failed call or commit does not end the delivery.
template <typename Change>
void deliver_changed(std::vector<node_t>& nodes, const Change change)
{
    for (bool more = true; more;) {
        more = false;
        std::vector<std::vector<message>> inbox(nodes.size());
        for (auto& n : nodes) {
            for (auto& m : n.take_outgoing_messages()) {
                more = true;
                change(m);
                inbox.at(batches::to_of(m) - 1).push_back(std::move(m));
            }
        }
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            if (inbox[i].empty()) {
                continue;
            }
            const auto t = nodes[i].handle_incoming(std::move(inbox[i]));
            if (t) {
                static_cast<void>(nodes[i].commit(*t));
            }
        }
    }
}

// Changes the results entry in each accept_decide to node `to`.
template <typename ChangeList>
auto change_the_results_entry_to(const node_id to, const ChangeList change_list)
{
    return [to, change_list](message& m) {
        auto* p = std::get_if<paxos_message>(&m);
        if (p == nullptr || p->to != to) {
            return;
        }
        auto* a = std::get_if<konsens::accept_decide>(&p->msg);
        if (a == nullptr) {
            return;
        }
        for (auto& e : a->entries) {
            if (e.kind == entry_kind::results) {
                auto list = list_of(e);
                change_list(list);
                e.payload = konsens::encode_chosen_results(list);
            }
        }
    };
}

node_id first_follower_of(const node_id leader)
{
    return leader == 1 ? 2 : 1;
}

}

// Plan 2.7.1, 2.7.4 and 2.7.6: a node accepts a results entry only after
// it checked it. Here the entry that reaches one follower lists another
// result, with a CRC that fits it, as a bit flip in the leader would give.
// The follower runs again, its result still differs, it is not listed, so
// it refuses with the reason, and its verified_idx stays below the entry:
// each accepted that it sends stays below it. The leader decides with the
// other follower.
TEST_CASE("a follower refuses a results entry whose result differs from its own")
{
    auto c = elected({0, 0, 0});
    const auto l = c.leader();
    const auto f = first_follower_of(l);
    const auto s = registered(c, l);
    batches::commit(c.at(l), c.at(l).client_request(client_a, s, 1, bytes_of('a')));
    std::uint64_t highest_accepted = 0;
    const auto flip = change_the_results_entry_to(f, [](std::vector<konsens::chosen_result>& list) {
        const auto bytes = konsens::encode_result({0, bytes_of('z'), {}});
        list.at(0).chosen.result = bytes;
        list.at(0).chosen.crc = konsens::compute_result_crc(list.at(0).chosen.index, bytes);
    });
    deliver_changed(c.nodes, [&](message& m) {
        flip(m);
        if (const auto* p = std::get_if<paxos_message>(&m); p != nullptr && p->from == f) {
            if (const auto* a = std::get_if<konsens::accepted>(&p->msg)) {
                highest_accepted = std::max(highest_accepted, a->accepted_idx);
            }
        }
    });
    constexpr std::uint64_t verdict_index = 4;
    const auto& refusal = c.at(f).get_refusal();
    REQUIRE(refusal.has_value());
    CHECK(refusal->index == verdict_index);
    CHECK(refusal->reason == konsens::refusal_reason::result_differs);
    CHECK(highest_accepted < verdict_index);
    const auto replies = c.at(l).take_client_replies();
    REQUIRE(replies.size() == 1);
    CHECK(replies[0].response == bytes_of('a'));
}

// Plan 2.7.1: a listed node accepts only when its result still does not
// agree after the second run; when it agrees, the list is wrong and the
// node refuses. A CRC that does not fit its result is refused before any
// run.
TEST_CASE("a follower refuses a results entry with a wrong list or a wrong CRC")
{
    for (const auto reason : {konsens::refusal_reason::wrong_list, konsens::refusal_reason::crc_mismatch}) {
        CAPTURE(int{std::to_underlying(reason)});
        auto c = elected({0, 0, 0});
        const auto l = c.leader();
        const auto f = first_follower_of(l);
        const auto s = registered(c, l);
        batches::commit(c.at(l), c.at(l).client_request(client_a, s, 1, bytes_of('a')));
        deliver_changed(c.nodes, change_the_results_entry_to(f, [f, reason](std::vector<konsens::chosen_result>& list) {
            if (reason == konsens::refusal_reason::wrong_list) {
                list.at(0).out.push_back(f);
            } else {
                list.at(0).chosen.crc += 1;
            }
        }));
        const auto& refusal = c.at(f).get_refusal();
        REQUIRE(refusal.has_value());
        CHECK(refusal->reason == reason);
    }
}

// Plan 2.7.4: the leader counts its own accept of a verdict entry only up
// to its own verified_idx, so it checks its own entry after the append, as
// every node does (mutation OwnAcceptUnchecked of the model).
TEST_CASE("the leader passes its own verdict entry only after its check")
{
    auto c = elected({0, 0, 0});
    const auto l = c.leader();
    const auto s = registered(c, l);
    batches::commit(c.at(l), c.at(l).client_request(client_a, s, 1, bytes_of('a')));
    const auto& sp = c.at(l).get_sequence_paxos();
    batches::deliver(c.nodes, [](const message& m) -> std::size_t {
        return holds<konsens::accept_decide>(m) && !std::get<konsens::accept_decide>(std::get<paxos_message>(m).msg)
                                                         .entries.empty() &&
                       std::get<konsens::accept_decide>(std::get<paxos_message>(m).msg).entries[0].kind ==
                           entry_kind::results
                   ? 0
                   : 1;
    });
    const auto accepted = sp.get_accepted_idx();
    REQUIRE(accepted.has_value());
    CHECK(*accepted == 4);
    CHECK(sp.get_verified_idx() == 3);
    batches::commit(c.at(l), c.at(l).tick());
    CHECK(sp.get_verified_idx() == 4);
}

// Plan 2.7.4: verified_idx is not durable. After a start it is the decided
// index, raised over each entry after it that is not a verdict entry.
TEST_CASE("after a start the verified index is the decided index and the entries after it that are no verdict")
{
    auto c = elected({0, 0, 0});
    const auto l = c.leader();
    const auto s = registered(c, l);
    batches::commit(c.at(l), c.at(l).client_request(client_a, s, 1, bytes_of('a')));
    c.run(rounds, once);
    const auto log_len = c.at(1).get_sequence_paxos().get_accepted_idx();
    REQUIRE(log_len.has_value());
    auto n = *node_t::with(config_of(1, 16), lmdb_storage::open_copy_of(c.at(1).get_sequence_paxos().get_internal_storage()),
                           lmdb_storage::open_new_node_storage(), lmdb_storage::open_new_state_storage(), marked{0}, 9);
    CHECK(n.get_sequence_paxos().get_verified_idx() == *log_len);
}
