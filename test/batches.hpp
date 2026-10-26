#pragma once

#include <doctest/doctest.h>

#include <cstddef>
#include <expected>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

#include <unistd.h>

#include "konsens/message.hpp"
#include "konsens/node.hpp"

namespace batches {

inline konsens::node_id from_of(const konsens::message& m)
{
    return std::visit([](const auto& x) { return x.from; }, m);
}

inline konsens::node_id to_of(const konsens::message& m)
{
    return std::visit([](const auto& x) { return x.to; }, m);
}

// The library makes no sync. The caller syncs the file of the store and
// says so, until the node needs no further sync. A test syncs at once after
// each step, so each step releases its messages before the next call.
template <typename Node>
void sync(Node& n, konsens::step_end end)
{
    while (end != konsens::step_end::nothing) {
        REQUIRE(fdatasync(n.get_sequence_paxos().get_internal_storage().file_descriptor()) == 0);
        const auto next = n.synced();
        REQUIRE(next);
        end = *next;
    }
}

// A commit that succeeds is synced at once. The result is the result of the
// commit, for a test that checks an error of the commit.
template <typename Node>
std::expected<konsens::step_end, std::error_code> commit_and_sync(Node& n, const konsens::transaction t)
{
    const auto end = n.commit(t);
    if (end) {
        sync(n, *end);
    }
    return end;
}

// The library makes no sync. A step of sequence_paxos that wrote waits for
// the sync of the caller before the next step begins, so a test syncs at
// once.
template <typename S>
std::expected<void, std::error_code> commit_and_sync(konsens::sequence_paxos<S>& sp)
{
    auto r = sp.commit();
    if (sp.is_prepared()) {
        REQUIRE(fdatasync(sp.get_internal_storage().file_descriptor()) == 0);
        sp.synced();
    }
    return r;
}

// A node that starts may hold a step of its start that waits for its sync.
template <typename Node>
void sync_after_the_start(Node& n)
{
    sync(n, n.get_step_end());
}

// Each call that collects work gives a transaction, and only the commit of
// that transaction and its sync release the messages and the replies of the
// call. A test that commits at once after each call keeps the order of the
// old form, in which each call sent its messages at once.
template <typename Node>
void commit(Node& n, const std::expected<konsens::transaction, std::error_code>& t)
{
    REQUIRE(t);
    const auto end = n.commit(*t);
    REQUIRE(end);
    sync(n, *end);
}

// Each pass takes what each node released and gives each node the messages
// for it as one batch, which the node commits at once. copies(m) says how
// often the receiver gets m: 0 is a loss, 2 a duplicate. The passes end when
// no node released a message.
template <typename Node, typename Copies>
void deliver(std::vector<Node>& nodes, const Copies copies)
{
    for (bool more = true; more;) {
        more = false;
        std::vector<std::vector<konsens::message>> inbox(nodes.size());
        for (auto& n : nodes) {
            for (auto& m : n.take_outgoing_messages()) {
                more = true;
                auto& to = inbox.at(to_of(m) - 1);
                const std::size_t k = copies(m);
                for (std::size_t c = 1; c < k; ++c) {
                    to.push_back(m);
                }
                if (k > 0) {
                    to.push_back(std::move(m));
                }
            }
        }
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            if (!inbox[i].empty()) {
                commit(nodes[i], nodes[i].handle_incoming(std::move(inbox[i])));
            }
        }
    }
}

// A tick collects its work as every call does, and the commit releases it.
template <typename Node>
void tick_and_commit(Node& n)
{
    commit(n, n.tick());
}

}
