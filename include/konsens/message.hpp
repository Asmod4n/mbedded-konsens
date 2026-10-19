#pragma once

#include <algorithm>
#include <compare>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <variant>
#include <vector>

#include "konsens/storage.hpp"

namespace konsens {

using node_id = std::uint64_t;

inline void validate_cluster(node_id pid, std::span<const node_id> peers)
{
    if (pid == 0 || peers.empty() || std::ranges::contains(peers, node_id{0}) ||
        std::ranges::contains(peers, pid)) [[unlikely]] {
        throw std::logic_error("konsens: a pid is 0, the peers are empty or contain the own pid");
    }
    std::vector<node_id> sorted(peers.begin(), peers.end());
    std::ranges::sort(sorted);
    if (std::ranges::adjacent_find(sorted) != sorted.end()) [[unlikely]] {
        throw std::logic_error("konsens: the peers contain a pid twice");
    }
}

enum class message_status : std::uint8_t { expected, dropped_preceding, outdated };

struct sequence_number {
    std::uint64_t session;
    std::uint64_t counter;

    friend auto operator<=>(const sequence_number&, const sequence_number&) = default;

    message_status check_msg_status(sequence_number msg_seq_num) const
    {
        if (msg_seq_num.session == session && msg_seq_num.counter == counter + 1) {
            return message_status::expected;
        }
        if (msg_seq_num <= *this) {
            return message_status::outdated;
        }
        return message_status::dropped_preceding;
    }
};

struct log_sync {
    std::vector<entry> suffix;
    std::uint64_t sync_idx;
};

struct prepare_req {
    ballot n;
};

struct prepare {
    ballot n;
    std::uint64_t decided_idx;
    ballot n_accepted;
    std::uint64_t accepted_idx;
};

struct promise {
    ballot n;
    ballot n_accepted;
    std::uint64_t decided_idx;
    std::uint64_t accepted_idx;
    std::optional<konsens::log_sync> log_sync;
};

struct accept_sync {
    ballot n;
    sequence_number seq_num;
    std::uint64_t decided_idx;
    konsens::log_sync log_sync;
};

struct accept_decide {
    ballot n;
    sequence_number seq_num;
    std::uint64_t decided_idx;
    std::vector<entry> entries;
};

struct accepted {
    ballot n;
    std::uint64_t accepted_idx;
};

struct decide {
    ballot n;
    sequence_number seq_num;
    std::uint64_t decided_idx;
};

struct not_accepted {
    ballot n;
};

struct proposal_forward {
    std::vector<entry> entries;
};

struct recovery {
    std::uint64_t nonce;
};

struct recovery_response {
    std::uint64_t nonce;
    ballot promise;
    bool primary;
    ballot n_accepted;
    std::vector<entry> log;
    std::uint64_t decided_idx;
};

struct read_index_request {
    std::uint64_t read;
};

struct heartbeat {
    ballot n;
    std::uint64_t id;
};

struct heartbeat_ack {
    ballot n;
    std::uint64_t id;
};

struct read_index_reply {
    std::vector<std::uint64_t> reads;
    std::uint64_t read_index;
};

using paxos_msg = std::variant<prepare_req, prepare, promise, accept_sync, accept_decide, accepted,
                               decide, not_accepted, proposal_forward, recovery, recovery_response,
                               read_index_request, heartbeat, heartbeat_ack, read_index_reply>;

struct paxos_message {
    node_id from;
    node_id to;
    paxos_msg msg;
};

struct heartbeat_request {
    std::uint32_t round;
};

struct heartbeat_reply {
    std::uint32_t round;
    konsens::ballot ballot;
    konsens::ballot leader;
    bool happy;
};

using heartbeat_msg = std::variant<heartbeat_request, heartbeat_reply>;

struct ble_message {
    node_id from;
    node_id to;
    heartbeat_msg msg;
};

using message = std::variant<paxos_message, ble_message>;

}
