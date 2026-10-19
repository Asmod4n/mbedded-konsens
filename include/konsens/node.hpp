#pragma once

#include <cstdint>
#include <expected>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

#include "konsens/ballot_leader_election.hpp"
#include "konsens/message.hpp"
#include "konsens/sequence_paxos.hpp"
#include "konsens/storage.hpp"

namespace konsens {

struct node_config {
    std::uint32_t configuration_id;
    node_id pid;
    std::vector<node_id> peers;
    std::uint32_t leader_priority;
    std::uint64_t election_tick_timeout;
    std::uint64_t resend_message_tick_timeout;
};

struct logical_clock {
    std::uint64_t time;
    std::uint64_t timeout;

    bool tick_and_check_timeout()
    {
        time += 1;
        if (time < timeout) {
            return false;
        }
        time = 0;
        return true;
    }
};

template <storage S>
class node {
public:
    using result = std::expected<void, std::error_code>;

    static std::expected<node, std::error_code> with(const node_config& config, S internal_storage)
    {
        if (config.configuration_id == 0 || config.election_tick_timeout == 0 ||
            config.resend_message_tick_timeout == 0) [[unlikely]] {
            throw std::logic_error("konsens: configuration_id and each tick timeout must not be 0");
        }
        auto sp = sequence_paxos<S>::with({config.pid, config.peers}, std::move(internal_storage));
        if (!sp) [[unlikely]] {
            return std::unexpected(sp.error());
        }
        auto ble = ballot_leader_election::with(
            {config.configuration_id, config.pid, config.peers, config.leader_priority}, sp->get_promise());
        return node(config, std::move(*sp), std::move(ble));
    }

    result tick()
    {
        if (election_clock_.tick_and_check_timeout()) {
            if (const auto leader = ble_.hb_timeout(seq_paxos_.get_promise())) {
                if (auto r = seq_paxos_.handle_leader(*leader); !r) [[unlikely]] {
                    return r;
                }
            }
        }
        if (resend_message_clock_.tick_and_check_timeout()) {
            return seq_paxos_.resend_message_timeout();
        }
        return {};
    }

    result handle(message m)
    {
        if (auto* p = std::get_if<paxos_message>(&m)) {
            return seq_paxos_.handle(std::move(*p));
        }
        ble_.handle(std::get<ble_message>(m));
        return {};
    }

    result append(entry e) { return seq_paxos_.append(std::move(e)); }

    std::vector<message> take_outgoing_messages()
    {
        auto paxos = seq_paxos_.take_outgoing_messages();
        auto ble = ble_.take_outgoing_messages();
        std::vector<message> out;
        out.reserve(paxos.size() + ble.size());
        for (auto& m : paxos) {
            out.emplace_back(std::move(m));
        }
        for (auto& m : ble) {
            out.emplace_back(std::move(m));
        }
        return out;
    }

    std::expected<std::vector<entry>, std::error_code> read_decided_suffix(std::uint64_t from_idx) const
    {
        return seq_paxos_.read_decided_suffix(from_idx);
    }

    const sequence_paxos<S>& get_sequence_paxos() const { return seq_paxos_; }
    const ballot_leader_election& get_ballot_leader_election() const { return ble_; }

private:
    node(const node_config& config, sequence_paxos<S> seq_paxos, ballot_leader_election ble)
        : seq_paxos_(std::move(seq_paxos)), ble_(std::move(ble)), election_clock_{0, config.election_tick_timeout},
          resend_message_clock_{0, config.resend_message_tick_timeout}
    {
    }

    sequence_paxos<S> seq_paxos_;
    ballot_leader_election ble_;
    logical_clock election_clock_;
    logical_clock resend_message_clock_;
};

}
