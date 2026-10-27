#pragma once

#include <doctest/doctest.h>

#include <cstddef>
#include <expected>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

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

// Each call that collects work gives a transaction, and only the commit of
// that transaction releases the messages and the replies of the call. The
// commit syncs the store. A test that commits at once after each call keeps the order of the
// old form, in which each call sent its messages at once.
template <typename Node>
void commit(Node& n, const std::expected<konsens::transaction, std::error_code>& t)
{
    REQUIRE(t);
    const auto committed = n.commit(*t);
    REQUIRE(committed);
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
