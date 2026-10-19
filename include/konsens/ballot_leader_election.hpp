#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "konsens/message.hpp"
#include "konsens/storage.hpp"

namespace konsens {

struct ble_config {
    std::uint32_t configuration_id;
    node_id pid;
    std::vector<node_id> peers;
    std::uint32_t priority;
};

class ballot_leader_election {
public:
    static constexpr std::uint32_t initial_round = 1;
    static constexpr std::uint32_t recovery_round = 0;

    static ballot_leader_election with(const ble_config& config, ballot recovered_leader)
    {
        validate_cluster(config.pid, config.peers);
        ballot_leader_election ble;
        ble.configuration_id_ = config.configuration_id;
        ble.pid_ = config.pid;
        ble.peers_ = config.peers;
        ble.majority_ = (config.peers.size() + 1) / 2 + 1;
        ble.current_ballot_ = {config.configuration_id, initial_round, config.priority, config.pid};
        ble.leader_ = ble.current_ballot_;
        if (recovered_leader != ballot{}) {
            ble.current_ballot_.n = recovery_round;
            ble.leader_ = recovered_leader;
        }
        ble.new_hb_round();
        return ble;
    }

    void handle(const ble_message& m)
    {
        if (const auto* req = std::get_if<heartbeat_request>(&m.msg)) {
            handle_request(m.from, *req);
        } else {
            handle_reply(std::get<heartbeat_reply>(m.msg));
        }
    }

    std::optional<ballot> hb_timeout(ballot seq_paxos_promise)
    {
        update_leader();
        update_happiness();
        check_takeover();
        new_hb_round();
        if (seq_paxos_promise > leader_) {
            leader_ = seq_paxos_promise;
            if (seq_paxos_promise.pid == pid_) {
                current_ballot_ = seq_paxos_promise;
            }
            happy_ = true;
        }
        if (leader_ == current_ballot_) {
            return current_ballot_;
        }
        return std::nullopt;
    }

    std::vector<ble_message> take_outgoing_messages() { return std::exchange(outgoing_, {}); }

    ballot get_current_ballot() const { return current_ballot_; }
    ballot get_leader() const { return leader_; }
    bool is_happy() const { return happy_; }

private:
    ballot_leader_election() = default;

    bool is_quorum(std::size_t num_nodes) const { return num_nodes >= majority_; }

    void new_hb_round()
    {
        heartbeat_replies_.clear();
        hb_round_ += 1;
        for (const auto peer : peers_) {
            outgoing_.push_back({pid_, peer, heartbeat_request{hb_round_}});
        }
    }

    void update_leader()
    {
        for (const auto& r : heartbeat_replies_) {
            if (r.ballot > leader_) {
                leader_ = r.ballot;
            }
        }
    }

    void update_happiness()
    {
        if (leader_ != current_ballot_) {
            happy_ = std::ranges::any_of(heartbeat_replies_,
                                         [this](const heartbeat_reply& r) { return r.ballot == leader_ && r.happy; });
            return;
        }
        const auto potential_followers = std::ranges::count_if(
            heartbeat_replies_, [this](const heartbeat_reply& r) { return r.leader <= current_ballot_; });
        if (is_quorum(static_cast<std::size_t>(potential_followers) + 1)) {
            happy_ = true;
            return;
        }
        happy_ = std::ranges::any_of(heartbeat_replies_, [this](const heartbeat_reply& r) {
            return r.leader > current_ballot_ && r.happy;
        });
    }

    void check_takeover()
    {
        if (happy_) {
            return;
        }
        const bool all_neighbors_unhappy =
            std::ranges::none_of(heartbeat_replies_, [](const heartbeat_reply& r) { return r.happy; });
        if (all_neighbors_unhappy && is_quorum(heartbeat_replies_.size() + 1)) {
            current_ballot_.n = leader_.n + 1;
            leader_ = current_ballot_;
            happy_ = true;
        }
    }

    void handle_request(node_id from, heartbeat_request req)
    {
        outgoing_.push_back({pid_, from, heartbeat_reply{req.round, current_ballot_, leader_, happy_}});
    }

    void handle_reply(const heartbeat_reply& rep)
    {
        if (rep.round == hb_round_ && rep.ballot.config_id == configuration_id_) {
            heartbeat_replies_.push_back(rep);
        }
    }

    std::uint32_t configuration_id_{};
    node_id pid_{};
    std::vector<node_id> peers_;
    std::size_t majority_{};
    std::uint32_t hb_round_{};
    std::vector<heartbeat_reply> heartbeat_replies_;
    ballot current_ballot_{};
    ballot leader_{};
    bool happy_{true};
    std::vector<ble_message> outgoing_;
};

}
