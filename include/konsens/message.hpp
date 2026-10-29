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
#include "konsens/verdict.hpp"

namespace konsens {

inline void validate_cluster(node_id pid, std::span<const node_id> peers)
{
    if (pid == 0 || peers.empty() || std::ranges::contains(peers, node_id{0}) || std::ranges::contains(peers, pid))
        [[unlikely]] {
        throw std::logic_error("konsens: a pid is 0, the peers are empty or contain the own pid");
    }
    std::vector<node_id> sorted(peers.begin(), peers.end());
    std::ranges::sort(sorted);
    if (std::ranges::adjacent_find(sorted) != sorted.end()) [[unlikely]] {
        throw std::logic_error("konsens: the peers contain a pid twice");
    }
}

struct cluster_config {
    std::uint32_t configuration_id;
    std::vector<node_id> nodes;
    friend bool operator==(const cluster_config&, const cluster_config&) = default;
};

inline void validate_next_cluster(const std::uint32_t configuration_id, const node_id pid,
                                  const std::span<const node_id> peers, const cluster_config& next)
{
    if (next.configuration_id != configuration_id + 1) [[unlikely]] {
        throw std::logic_error("konsens: the next configuration id is not the current one plus one");
    }
    if (next.nodes.empty() || std::ranges::contains(next.nodes, node_id{0})) [[unlikely]] {
        throw std::logic_error("konsens: the next configuration is empty or contains the pid 0");
    }
    if (std::ranges::contains(next.nodes, pid) || std::ranges::any_of(peers, [&next](const node_id p) {
            return std::ranges::contains(next.nodes, p);
        })) [[unlikely]] {
        throw std::logic_error("konsens: the next configuration contains a pid of the current configuration");
    }
    std::vector<node_id> sorted = next.nodes;
    std::ranges::sort(sorted);
    if (std::ranges::adjacent_find(sorted) != sorted.end()) [[unlikely]] {
        throw std::logic_error("konsens: the next configuration contains a pid twice");
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
    friend bool operator==(const log_sync&, const log_sync&) = default;
};

struct prepare_req {
    ballot n;
    friend bool operator==(const prepare_req&, const prepare_req&) = default;
};

struct prepare {
    ballot n;
    std::uint64_t decided_idx;
    ballot n_accepted;
    std::uint64_t accepted_idx;
    friend bool operator==(const prepare&, const prepare&) = default;
};

struct promise {
    ballot n;
    ballot n_accepted;
    std::uint64_t decided_idx;
    std::uint64_t accepted_idx;
    std::optional<konsens::log_sync> log_sync;
    std::uint32_t decided_chain_crc;
    std::uint32_t last_chain_crc;
    friend bool operator==(const promise&, const promise&) = default;
};

struct accept_sync {
    ballot n;
    sequence_number seq_num;
    std::uint64_t decided_idx;
    konsens::log_sync log_sync;
    std::uint32_t decided_chain_crc;
    friend bool operator==(const accept_sync&, const accept_sync&) = default;
};

struct accept_decide {
    ballot n;
    sequence_number seq_num;
    std::uint64_t decided_idx;
    std::vector<entry> entries;
    std::uint32_t decided_chain_crc;
    friend bool operator==(const accept_decide&, const accept_decide&) = default;
};

struct accepted {
    ballot n;
    std::uint64_t accepted_idx;
    std::uint32_t accepted_chain_crc;
    friend bool operator==(const accepted&, const accepted&) = default;
};

struct decide {
    ballot n;
    sequence_number seq_num;
    std::uint64_t decided_idx;
    std::uint32_t decided_chain_crc;
    friend bool operator==(const decide&, const decide&) = default;
};

struct not_accepted {
    ballot n;
    friend bool operator==(const not_accepted&, const not_accepted&) = default;
};

struct proposal_forward {
    std::vector<entry> entries;
    friend bool operator==(const proposal_forward&, const proposal_forward&) = default;
};

struct read_index_request {
    std::uint64_t read;
    friend bool operator==(const read_index_request&, const read_index_request&) = default;
};

struct heartbeat {
    ballot n;
    std::uint64_t id;
    friend bool operator==(const heartbeat&, const heartbeat&) = default;
};

struct heartbeat_ack {
    ballot n;
    std::uint64_t id;
    friend bool operator==(const heartbeat_ack&, const heartbeat_ack&) = default;
};

struct read_index_reply {
    std::vector<std::uint64_t> reads;
    std::uint64_t read_index;
    friend bool operator==(const read_index_reply&, const read_index_reply&) = default;
};

struct answer {
    ballot n;
    std::uint64_t pointer;
    std::uint64_t from_idx;
    std::uint32_t base_crc;
    std::vector<indexed_result> results;
    std::uint64_t second_run;
    friend bool operator==(const answer&, const answer&) = default;
};

struct try_again {
    ballot n;
    std::uint64_t index;
    friend bool operator==(const try_again&, const try_again&) = default;
};

enum class refusal_reason : std::uint8_t { crc_mismatch = 1, result_differs, wrong_list, rejected_has_a_majority };

struct refused {
    ballot n;
    std::uint64_t index;
    refusal_reason reason;
    friend bool operator==(const refused&, const refused&) = default;
};

struct work {
    ballot n;
    std::uint64_t index;
    entry request;
    std::uint32_t entry_crc;
    friend bool operator==(const work&, const work&) = default;
};

struct build_request {
    std::uint64_t nonce;
    std::uint64_t from_idx;
    friend bool operator==(const build_request&, const build_request&) = default;
};

struct build_response {
    std::uint64_t nonce;
    std::uint64_t from_idx;
    std::vector<entry> entries;
    std::uint32_t last_chain_crc;
    std::uint64_t decided_idx;
    std::uint32_t decided_chain_crc;
    friend bool operator==(const build_response&, const build_response&) = default;
};

struct build_refused {
    std::uint64_t nonce;
    friend bool operator==(const build_refused&, const build_refused&) = default;
};

using paxos_msg =
    std::variant<prepare_req, prepare, promise, accept_sync, accept_decide, accepted, decide, not_accepted,
                 proposal_forward, read_index_request, heartbeat, heartbeat_ack, read_index_reply, answer, try_again,
                 refused, work, build_request, build_response, build_refused>;

struct paxos_message {
    node_id from;
    node_id to;
    paxos_msg msg;
    friend bool operator==(const paxos_message&, const paxos_message&) = default;
};

struct heartbeat_request {
    std::uint32_t round;
    friend bool operator==(const heartbeat_request&, const heartbeat_request&) = default;
};

struct heartbeat_reply {
    std::uint32_t round;
    konsens::ballot ballot;
    konsens::ballot leader;
    bool happy;
    friend bool operator==(const heartbeat_reply&, const heartbeat_reply&) = default;
};

using heartbeat_msg = std::variant<heartbeat_request, heartbeat_reply>;

struct ble_message {
    node_id from;
    node_id to;
    heartbeat_msg msg;
    friend bool operator==(const ble_message&, const ble_message&) = default;
};

using message = std::variant<paxos_message, ble_message>;

}
