#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iterator>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "konsens/client.hpp"
#include "konsens/message.hpp"
#include "konsens/storage.hpp"

namespace konsens {

enum class role : std::uint8_t { follower, leader };

enum class phase : std::uint8_t { prepare, accept, recover, none };

struct promise_meta_data {
    ballot n_accepted;
    std::uint64_t accepted_idx;
    std::uint64_t decided_idx;
    node_id pid;
};

struct not_promised {};

struct promised_higher {};

using promise_state = std::variant<not_promised, promise_meta_data, promised_higher>;

struct leader_state {
    ballot n_leader;
    std::vector<node_id> nodes;
    std::vector<promise_state> promises_meta;
    std::vector<sequence_number> follower_seq_nums;
    std::vector<std::uint64_t> accepted_indexes;
    promise_meta_data max_promise_meta;
    std::optional<konsens::log_sync> max_promise_sync;

    static leader_state with(ballot n_leader, std::span<const node_id> nodes)
    {
        return {n_leader,
                std::vector<node_id>(nodes.begin(), nodes.end()),
                std::vector<promise_state>(nodes.size(), not_promised{}),
                std::vector<sequence_number>(nodes.size(), sequence_number{}),
                std::vector<std::uint64_t>(nodes.size(), 0),
                promise_meta_data{},
                std::nullopt};
    }

    std::size_t pid_to_idx(node_id pid) const
    {
        const auto it = std::ranges::find(nodes, pid);
        if (it == nodes.end()) [[unlikely]] {
            throw std::logic_error("konsens: leader_state has no node with this pid");
        }
        return static_cast<std::size_t>(std::ranges::distance(nodes.begin(), it));
    }

    std::size_t majority() const { return nodes.size() / 2 + 1; }

    void increment_seq_num_session(node_id pid)
    {
        auto& s = follower_seq_nums[pid_to_idx(pid)];
        s.session += 1;
        s.counter = 0;
    }

    sequence_number next_seq_num(node_id pid)
    {
        auto& s = follower_seq_nums[pid_to_idx(pid)];
        s.counter += 1;
        return s;
    }

    bool set_promise(const promise_meta_data& meta, std::optional<konsens::log_sync> sync,
                     bool check_max_prom)
    {
        const auto& mm = max_promise_meta;
        const bool greater = meta.n_accepted > mm.n_accepted ||
                             (meta.n_accepted == mm.n_accepted && meta.accepted_idx > mm.accepted_idx);
        if (check_max_prom && greater) {
            max_promise_meta = meta;
            max_promise_sync = std::move(sync);
        }
        promises_meta[pid_to_idx(meta.pid)] = meta;
        const auto promised = std::ranges::count_if(promises_meta, [](const promise_state& p) {
            return std::holds_alternative<promise_meta_data>(p);
        });
        return static_cast<std::size_t>(promised) >= majority();
    }

    void reset_promise(node_id pid) { promises_meta[pid_to_idx(pid)] = not_promised{}; }

    void lost_promise(node_id pid) { promises_meta[pid_to_idx(pid)] = promised_higher{}; }

    std::optional<konsens::log_sync> take_max_promise_sync() { return std::exchange(max_promise_sync, std::nullopt); }

    std::uint64_t get_max_decided_idx() const
    {
        std::uint64_t max = 0;
        for (const auto& p : promises_meta) {
            if (const auto* m = std::get_if<promise_meta_data>(&p)) {
                max = std::max(max, m->decided_idx);
            }
        }
        return max;
    }

    const promise_meta_data& get_promise_meta(node_id pid) const
    {
        const auto* m = std::get_if<promise_meta_data>(&promises_meta[pid_to_idx(pid)]);
        if (m == nullptr) [[unlikely]] {
            throw std::logic_error("konsens: no promise meta data for a promised follower");
        }
        return *m;
    }

    auto get_promised_followers() const
    {
        return std::views::iota(std::size_t{0}, nodes.size()) |
               std::views::filter([this](std::size_t i) {
                   return std::holds_alternative<promise_meta_data>(promises_meta[i]) &&
                          nodes[i] != n_leader.pid;
               }) |
               std::views::transform([this](std::size_t i) { return nodes[i]; });
    }

    auto get_preparable_peers(node_id own_pid) const
    {
        return std::views::iota(std::size_t{0}, nodes.size()) |
               std::views::filter([this, own_pid](std::size_t i) {
                   return std::holds_alternative<not_promised>(promises_meta[i]) &&
                          nodes[i] != own_pid;
               }) |
               std::views::transform([this](std::size_t i) { return nodes[i]; });
    }

    void set_accepted_idx(node_id pid, std::uint64_t idx) { accepted_indexes[pid_to_idx(pid)] = idx; }

    bool is_chosen(std::uint64_t idx) const
    {
        const auto n = std::ranges::count_if(accepted_indexes, [idx](std::uint64_t a) { return a >= idx; });
        return static_cast<std::size_t>(n) >= majority();
    }
};

struct pending_read {
    node_id node;
    std::uint64_t read;
};

struct read_round {
    ballot n;
    std::uint64_t id;
    std::vector<pending_read> reads;
    std::uint64_t read_index;
    std::vector<node_id> acks;
};

struct indexed_read {
    std::uint64_t read;
    std::uint64_t read_index;
    konsens::ballot ballot;
};

struct sequence_paxos_config {
    node_id pid;
    std::vector<node_id> peers;
};

struct decided_update {
    std::uint64_t decided_idx;
    bool flushed;
};

constexpr decided_update update_decided(std::uint64_t decided_idx, std::uint64_t accepted_idx,
                                        std::uint64_t new_decided_idx)
{
    if (new_decided_idx <= decided_idx) {
        return {decided_idx, false};
    }
    if (new_decided_idx > accepted_idx) {
        return {std::min(new_decided_idx, accepted_idx), true};
    }
    return {new_decided_idx, false};
}

template <storage S>
class sequence_paxos {
public:
    using result = std::expected<void, std::error_code>;

    static std::expected<sequence_paxos, std::error_code> with(const sequence_paxos_config& config,
                                                               S internal_storage, const std::uint64_t nonce)
    {
        validate_cluster(config.pid, config.peers);
        const auto p = internal_storage.get_promise();
        const auto na = internal_storage.get_accepted_round();
        const auto dec = internal_storage.get_decided_idx();
        const auto len = internal_storage.get_log_len();
        if (!p) [[unlikely]] {
            return std::unexpected(p.error());
        }
        if (!na) [[unlikely]] {
            return std::unexpected(na.error());
        }
        if (!dec) [[unlikely]] {
            return std::unexpected(dec.error());
        }
        if (!len) [[unlikely]] {
            return std::unexpected(len.error());
        }
        sequence_paxos sp(config, std::move(internal_storage));
        sp.promise_ = *p;
        sp.accepted_round_ = *na;
        sp.decided_idx_ = *dec;
        sp.accepted_idx_ = *len;
        sp.leader_state_ = leader_state::with(*p, sp.nodes_);
        sp.nonce_ = nonce;
        if (*p != ballot{}) {
            sp.state_ = {role::follower, phase::recover};
            sp.recovering_ = true;
            sp.send_recovery_to_all_peers();
        }
        return sp;
    }

    result handle(paxos_message m)
    {
        if (error_) [[unlikely]] {
            return std::unexpected(*error_);
        }
        if (m.from == pid_ || !std::ranges::contains(peers_, m.from)) [[unlikely]] {
            return {};
        }
        if (recovering_ && !std::holds_alternative<recovery_response>(m.msg)) {
            return {};
        }
        const node_id from = m.from;
        return std::visit(
            [this, from](auto&& msg) -> result {
                using T = std::decay_t<decltype(msg)>;
                if constexpr (std::is_same_v<T, prepare_req>) {
                    handle_preparereq(msg, from);
                    return {};
                } else if constexpr (std::is_same_v<T, prepare>) {
                    return handle_prepare(msg, from);
                } else if constexpr (std::is_same_v<T, konsens::promise>) {
                    return handle_promise(std::move(msg), from);
                } else if constexpr (std::is_same_v<T, accept_sync>) {
                    return handle_acceptsync(std::move(msg), from);
                } else if constexpr (std::is_same_v<T, accept_decide>) {
                    return handle_acceptdecide(std::move(msg));
                } else if constexpr (std::is_same_v<T, accepted>) {
                    return handle_accepted(msg, from);
                } else if constexpr (std::is_same_v<T, decide>) {
                    return handle_decide(msg);
                } else if constexpr (std::is_same_v<T, not_accepted>) {
                    handle_notaccepted(msg, from);
                    return {};
                } else if constexpr (std::is_same_v<T, recovery>) {
                    return handle_recovery(msg, from);
                } else if constexpr (std::is_same_v<T, recovery_response>) {
                    return handle_recovery_response(std::move(msg), from);
                } else if constexpr (std::is_same_v<T, read_index_request>) {
                    handle_read_index_request(msg, from);
                    return {};
                } else if constexpr (std::is_same_v<T, heartbeat>) {
                    handle_heartbeat(msg, from);
                    return {};
                } else if constexpr (std::is_same_v<T, heartbeat_ack>) {
                    handle_heartbeat_ack(msg, from);
                    return {};
                } else if constexpr (std::is_same_v<T, read_index_reply>) {
                    handle_read_index_reply(msg);
                    return {};
                } else {
                    static_assert(std::is_same_v<T, proposal_forward>);
                    return handle_forwarded_proposal(std::move(msg.entries));
                }
            },
            std::move(m.msg));
    }

    result handle_leader(ballot n)
    {
        if (error_) [[unlikely]] {
            return std::unexpected(*error_);
        }
        if (recovering_ || n <= leader_state_.n_leader || n <= promise_) {
            return {};
        }
        if (n.pid != pid_) {
            state_.first = role::follower;
            return {};
        }
        const std::array<storage_op, 1> ops{set_promise{n}};
        if (auto w = internal_storage_.write_atomically(ops); !w) [[unlikely]] {
            return stop_voting(w.error());
        }
        promise_ = n;
        leader_state_ = leader_state::with(n, nodes_);
        leader_state_.set_promise({accepted_round_, accepted_idx_, decided_idx_, pid_}, std::nullopt, true);
        state_ = {role::leader, phase::prepare};
        for (const auto peer : peers_) {
            send_prepare(peer);
        }
        return {};
    }

    result append(entry e)
    {
        if (error_) [[unlikely]] {
            return std::unexpected(*error_);
        }
        if (auto r = refuse_if_recovering(); !r) [[unlikely]] {
            return r;
        }
        if (state_ == std::pair{role::leader, phase::prepare}) {
            buffered_proposals_.push_back(std::move(e));
            return {};
        }
        std::vector<entry> entries;
        entries.push_back(std::move(e));
        if (state_ == std::pair{role::leader, phase::accept}) {
            return accept_entries_leader(std::move(entries));
        }
        forward_proposals(std::move(entries));
        return {};
    }

    result resend_message_timeout()
    {
        if (error_) [[unlikely]] {
            return std::unexpected(*error_);
        }
        if (recovering_) {
            send_recovery_to_all_peers();
            return {};
        }
        if (state_.first == role::leader) {
            if (state_.second == phase::prepare || state_.second == phase::accept) {
                for (const auto peer : leader_state_.get_preparable_peers(pid_)) {
                    send_prepare(peer);
                }
            }
            return {};
        }
        if (state_.second == phase::prepare) {
            if (cached_promise_message_) {
                outgoing_.push_back({pid_, cached_promise_message_->n.pid, *cached_promise_message_});
            } else {
                state_ = {role::follower, phase::recover};
                send_preparereq_to_all_peers();
            }
        } else if (state_.second == phase::recover) {
            send_preparereq_to_all_peers();
        }
        return {};
    }

    result refuse_if_recovering() const
    {
        if (recovering_) [[unlikely]] {
            return std::unexpected(make_error_code(client_errc::recovering));
        }
        return {};
    }

    std::vector<paxos_message> take_outgoing_messages() { return std::exchange(outgoing_, {}); }

    result start_read(const std::uint64_t read)
    {
        if (error_) [[unlikely]] {
            return std::unexpected(*error_);
        }
        if (state_.first == role::leader) {
            read_pending_.push_back({pid_, read});
            start_read_round();
            return {};
        }
        const node_id leader = promise_.pid;
        if (leader == 0 || leader == pid_) [[unlikely]] {
            return std::unexpected(make_error_code(client_errc::retry));
        }
        outgoing_.push_back({pid_, leader, read_index_request{read}});
        return {};
    }

    void resend_heartbeats()
    {
        if (error_ || state_ != std::pair{role::leader, phase::accept} || read_round_.reads.empty() ||
            read_round_.n != leader_state_.n_leader) {
            return;
        }
        for (const auto peer : peers_) {
            if (!std::ranges::contains(read_round_.acks, peer)) {
                outgoing_.push_back({pid_, peer, heartbeat{read_round_.n, read_round_.id}});
            }
        }
    }

    std::vector<indexed_read> take_indexed_reads() { return std::exchange(indexed_reads_, {}); }

    std::expected<std::vector<entry>, std::error_code> read_decided_suffix(std::uint64_t from_idx) const
    {
        const auto decided = std::min(decided_idx_, accepted_idx_);
        return internal_storage_.get_entries(std::min(from_idx, decided), decided);
    }

    std::pair<role, phase> get_state() const { return state_; }
    ballot get_promise() const { return promise_; }
    ballot get_accepted_round() const { return accepted_round_; }
    std::uint64_t get_decided_idx() const { return decided_idx_; }
    std::uint64_t get_accepted_idx() const { return accepted_idx_; }
    const leader_state& get_leader_state() const { return leader_state_; }
    const konsens::read_round& get_read_round() const { return read_round_; }
    const S& get_internal_storage() const { return internal_storage_; }
    node_id get_pid() const { return pid_; }
    bool votes() const { return !error_; }
    bool recovering() const { return recovering_; }
    std::optional<std::error_code> get_error() const { return error_; }

    void reconnected(const node_id pid)
    {
        if (pid == pid_ || recovering_) {
            return;
        }
        if (pid == promise_.pid) {
            state_ = {role::follower, phase::recover};
        }
        outgoing_.push_back({pid_, pid, prepare_req{promise_}});
    }

private:
    sequence_paxos(const sequence_paxos_config& config, S internal_storage)
        : pid_(config.pid), peers_(config.peers), internal_storage_(std::move(internal_storage))
    {
        nodes_ = peers_;
        nodes_.push_back(pid_);
        std::ranges::sort(nodes_);
    }

    std::unexpected<std::error_code> stop_voting(std::error_code e)
    {
        error_ = e;
        return std::unexpected(e);
    }

    std::expected<konsens::log_sync, std::error_code> create_log_sync(std::uint64_t common_prefix_idx) const
    {
        auto suffix = internal_storage_.get_entries(std::min(common_prefix_idx, accepted_idx_), accepted_idx_);
        if (!suffix) [[unlikely]] {
            return std::unexpected(suffix.error());
        }
        return konsens::log_sync{std::move(*suffix), common_prefix_idx};
    }

    void send_preparereq_to_all_peers()
    {
        for (const auto peer : peers_) {
            outgoing_.push_back({pid_, peer, prepare_req{promise_}});
        }
    }

    void send_recovery_to_all_peers()
    {
        for (const auto peer : peers_) {
            outgoing_.push_back({pid_, peer, recovery{nonce_}});
        }
    }

    void send_prepare(node_id to)
    {
        outgoing_.push_back(
            {pid_, to, prepare{leader_state_.n_leader, decided_idx_, accepted_round_, accepted_idx_}});
    }

    bool check_valid_ballot(ballot message_ballot)
    {
        if (promise_ == message_ballot) {
            return true;
        }
        if (promise_ > message_ballot) {
            outgoing_.push_back({pid_, message_ballot.pid, not_accepted{promise_}});
            return false;
        }
        reconnected(message_ballot.pid);
        return false;
    }

    message_status handle_sequence_num(sequence_number seq_num, node_id from)
    {
        const auto status = current_seq_num_.check_msg_status(seq_num);
        if (status == message_status::expected) {
            current_seq_num_ = seq_num;
        } else if (status == message_status::dropped_preceding) {
            reconnected(from);
        }
        return status;
    }

    void forward_proposals(std::vector<entry> entries)
    {
        const node_id leader = promise_.pid;
        if (leader > 0 && leader != pid_) {
            outgoing_.push_back({pid_, leader, proposal_forward{std::move(entries)}});
        } else {
            buffered_proposals_.insert(buffered_proposals_.end(), std::make_move_iterator(entries.begin()),
                                       std::make_move_iterator(entries.end()));
        }
    }

    result accept_entries_leader(std::vector<entry> entries)
    {
        if (entries.empty()) {
            return {};
        }
        const auto count = entries.size();
        const std::array<storage_op, 1> ops{append_entries{entries}};
        if (auto w = internal_storage_.write_atomically(ops); !w) [[unlikely]] {
            return stop_voting(w.error());
        }
        accepted_idx_ += count;
        leader_state_.set_accepted_idx(pid_, accepted_idx_);
        for (const auto f : leader_state_.get_promised_followers()) {
            outgoing_.push_back({pid_, f,
                                 accept_decide{leader_state_.n_leader, leader_state_.next_seq_num(f),
                                               decided_idx_, entries}});
        }
        return {};
    }

    void handle_preparereq(const prepare_req& prepreq, node_id from)
    {
        if (state_.first == role::leader && prepreq.n <= leader_state_.n_leader) {
            leader_state_.reset_promise(from);
            send_prepare(from);
        }
    }

    result handle_prepare(const prepare& prep, node_id from)
    {
        if (!(promise_ < prep.n || (promise_ == prep.n && state_.second == phase::recover))) {
            return {};
        }
        std::optional<konsens::log_sync> sync;
        if (accepted_round_ > prep.n_accepted) {
            auto s = create_log_sync(prep.decided_idx);
            if (!s) [[unlikely]] {
                return stop_voting(s.error());
            }
            sync = std::move(*s);
        } else if (accepted_round_ == prep.n_accepted && accepted_idx_ > prep.accepted_idx) {
            auto s = create_log_sync(prep.accepted_idx);
            if (!s) [[unlikely]] {
                return stop_voting(s.error());
            }
            sync = std::move(*s);
        }
        const std::array<storage_op, 1> ops{set_promise{prep.n}};
        if (auto w = internal_storage_.write_atomically(ops); !w) [[unlikely]] {
            return stop_voting(w.error());
        }
        promise_ = prep.n;
        state_ = {role::follower, phase::prepare};
        current_seq_num_ = {};
        konsens::promise p{prep.n, accepted_round_, decided_idx_, accepted_idx_, std::move(sync)};
        cached_promise_message_ = p;
        outgoing_.push_back({pid_, from, std::move(p)});
        return {};
    }

    result handle_promise(konsens::promise prom, node_id from)
    {
        if (state_.first != role::leader || prom.n != leader_state_.n_leader) {
            return {};
        }
        const promise_meta_data meta{prom.n_accepted, prom.accepted_idx, prom.decided_idx, from};
        if (state_.second == phase::prepare) {
            if (leader_state_.set_promise(meta, std::move(prom.log_sync), true)) {
                return handle_majority_promises();
            }
            return {};
        }
        if (state_.second == phase::accept) {
            leader_state_.set_promise(meta, std::nullopt, false);
            return send_accsync(from);
        }
        return {};
    }

    result handle_majority_promises()
    {
        auto sync = leader_state_.take_max_promise_sync();
        const auto decided_idx = leader_state_.get_max_decided_idx();
        const auto sync_idx = sync ? sync->sync_idx : accepted_idx_;
        auto suffix = sync ? std::move(sync->suffix) : std::vector<entry>{};
        auto proposals = std::exchange(buffered_proposals_, {});
        const auto accepted_idx = std::min(sync_idx, accepted_idx_) + suffix.size() + proposals.size();
        const std::array<storage_op, 4> ops{append_on_prefix{sync_idx, std::move(suffix)},
                                            append_entries{std::move(proposals)},
                                            set_accepted_round{leader_state_.n_leader},
                                            set_decided_index{decided_idx}};
        if (auto w = internal_storage_.write_atomically(ops); !w) [[unlikely]] {
            return stop_voting(w.error());
        }
        accepted_round_ = leader_state_.n_leader;
        decided_idx_ = decided_idx;
        accepted_idx_ = accepted_idx;
        state_ = {role::leader, phase::accept};
        leader_state_.set_accepted_idx(pid_, accepted_idx_);
        for (const auto f : leader_state_.get_promised_followers()) {
            if (auto r = send_accsync(f); !r) [[unlikely]] {
                return r;
            }
        }
        start_read_round();
        return {};
    }

    result send_accsync(node_id to)
    {
        const auto& mm = leader_state_.max_promise_meta;
        const auto& fm = leader_state_.get_promise_meta(to);
        const auto valid = fm.n_accepted == leader_state_.n_leader ? fm.accepted_idx
                           : fm.n_accepted == mm.n_accepted        ? std::min(mm.accepted_idx, fm.accepted_idx)
                                                                   : fm.decided_idx;
        auto sync = create_log_sync(valid);
        if (!sync) [[unlikely]] {
            return stop_voting(sync.error());
        }
        leader_state_.increment_seq_num_session(to);
        const auto seq_num = leader_state_.next_seq_num(to);
        outgoing_.push_back(
            {pid_, to, accept_sync{leader_state_.n_leader, seq_num, decided_idx_, std::move(*sync)}});
        return {};
    }

    result handle_acceptsync(accept_sync accsync, node_id from)
    {
        if (!check_valid_ballot(accsync.n) || state_ != std::pair{role::follower, phase::prepare}) {
            return {};
        }
        const auto sync_idx = accsync.log_sync.sync_idx;
        if (sync_idx > accepted_idx_ ||
            (accsync.n == applied_sync_.n && accsync.seq_num.session <= applied_sync_.session)) [[unlikely]] {
            return {};
        }
        const auto decided_idx = std::max(decided_idx_, accsync.decided_idx);
        const auto accepted_idx = std::min(sync_idx, accepted_idx_) + accsync.log_sync.suffix.size();
        const std::array<storage_op, 3> ops{append_on_prefix{sync_idx, std::move(accsync.log_sync.suffix)},
                                            set_accepted_round{accsync.n}, set_decided_index{decided_idx}};
        if (auto w = internal_storage_.write_atomically(ops); !w) [[unlikely]] {
            return stop_voting(w.error());
        }
        accepted_round_ = accsync.n;
        decided_idx_ = decided_idx;
        accepted_idx_ = accepted_idx;
        cached_promise_message_.reset();
        if (!buffered_proposals_.empty()) {
            forward_proposals(std::exchange(buffered_proposals_, {}));
        }
        state_ = {role::follower, phase::accept};
        current_seq_num_ = accsync.seq_num;
        applied_sync_ = {accsync.n, accsync.seq_num.session};
        outgoing_.push_back({pid_, from, accepted{accsync.n, accepted_idx_}});
        return {};
    }

    result handle_acceptdecide(accept_decide acc_dec)
    {
        if (!check_valid_ballot(acc_dec.n) || state_ != std::pair{role::follower, phase::accept} ||
            handle_sequence_num(acc_dec.seq_num, acc_dec.n.pid) != message_status::expected) {
            return {};
        }
        const auto count = acc_dec.entries.size();
        const auto accepted_idx = accepted_idx_ + count;
        const auto u = update_decided(decided_idx_, accepted_idx, acc_dec.decided_idx);
        if (count > 0 || u.decided_idx != decided_idx_) {
            const std::array<storage_op, 2> ops{append_entries{std::move(acc_dec.entries)},
                                                set_decided_index{u.decided_idx}};
            if (auto w = internal_storage_.write_atomically(ops); !w) [[unlikely]] {
                return stop_voting(w.error());
            }
        }
        accepted_idx_ = accepted_idx;
        decided_idx_ = u.decided_idx;
        if (count > 0 || u.flushed) {
            outgoing_.push_back({pid_, acc_dec.n.pid, accepted{acc_dec.n, accepted_idx_}});
        }
        return {};
    }

    result handle_decide(const decide& dec)
    {
        if (!check_valid_ballot(dec.n) || state_.second != phase::accept ||
            handle_sequence_num(dec.seq_num, dec.n.pid) != message_status::expected) {
            return {};
        }
        const auto u = update_decided(decided_idx_, accepted_idx_, dec.decided_idx);
        if (u.decided_idx != decided_idx_) {
            const std::array<storage_op, 1> ops{set_decided_index{u.decided_idx}};
            if (auto w = internal_storage_.write_atomically(ops); !w) [[unlikely]] {
                return stop_voting(w.error());
            }
        }
        decided_idx_ = u.decided_idx;
        if (u.flushed) {
            outgoing_.push_back({pid_, dec.n.pid, accepted{dec.n, accepted_idx_}});
        }
        return {};
    }

    result handle_accepted(const accepted& acc, node_id from)
    {
        if (acc.n != leader_state_.n_leader || state_ != std::pair{role::leader, phase::accept}) {
            return {};
        }
        leader_state_.set_accepted_idx(from, acc.accepted_idx);
        if (acc.accepted_idx <= decided_idx_ || !leader_state_.is_chosen(acc.accepted_idx)) {
            return {};
        }
        const std::array<storage_op, 1> ops{set_decided_index{acc.accepted_idx}};
        if (auto w = internal_storage_.write_atomically(ops); !w) [[unlikely]] {
            return stop_voting(w.error());
        }
        decided_idx_ = acc.accepted_idx;
        for (const auto f : leader_state_.get_promised_followers()) {
            outgoing_.push_back(
                {pid_, f, decide{leader_state_.n_leader, leader_state_.next_seq_num(f), decided_idx_}});
        }
        return {};
    }

    result handle_recovery(const recovery& rec, node_id from)
    {
        const bool primary = state_ == std::pair{role::leader, phase::accept};
        std::vector<entry> log;
        if (primary) {
            auto l = internal_storage_.get_entries(0, accepted_idx_);
            if (!l) [[unlikely]] {
                return stop_voting(l.error());
            }
            log = std::move(*l);
        }
        outgoing_.push_back({pid_, from,
                             recovery_response{rec.nonce, promise_, primary, primary ? accepted_round_ : ballot{},
                                               std::move(log), primary ? decided_idx_ : 0}});
        return {};
    }

    result handle_recovery_response(recovery_response resp, node_id from)
    {
        if (!recovering_ || resp.nonce != nonce_) {
            return {};
        }
        std::erase_if(recovery_responses_, [from](const auto& r) { return r.first == from; });
        recovery_responses_.emplace_back(from, std::move(resp));
        const auto highest = std::ranges::max(recovery_responses_ | std::views::values |
                                              std::views::transform(&recovery_response::promise));
        const auto l = std::ranges::find_if(recovery_responses_, [highest](const auto& r) {
            return r.second.primary && r.second.promise == highest && highest.pid == r.first;
        });
        if (recovery_responses_.size() < leader_state_.majority() || l == recovery_responses_.end()) {
            return {};
        }
        const auto pr = std::max(promise_, l->second.promise);
        const auto n_accepted = l->second.n_accepted;
        const auto decided_idx = std::max(decided_idx_, l->second.decided_idx);
        const auto accepted_idx = l->second.log.size();
        const std::array<storage_op, 4> ops{append_on_prefix{0, std::move(l->second.log)},
                                            set_accepted_round{n_accepted}, set_decided_index{decided_idx},
                                            set_promise{pr}};
        if (auto w = internal_storage_.write_atomically(ops); !w) [[unlikely]] {
            return stop_voting(w.error());
        }
        promise_ = pr;
        accepted_round_ = n_accepted;
        decided_idx_ = decided_idx;
        accepted_idx_ = accepted_idx;
        state_ = {role::follower, phase::recover};
        current_seq_num_ = {};
        cached_promise_message_.reset();
        buffered_proposals_.clear();
        leader_state_ = leader_state::with(pr, nodes_);
        recovering_ = false;
        recovery_responses_.clear();
        send_preparereq_to_all_peers();
        return {};
    }

    void handle_notaccepted(const not_accepted& not_acc, node_id from)
    {
        if (state_.first == role::leader && leader_state_.n_leader < not_acc.n) {
            leader_state_.lost_promise(from);
        }
    }

    result handle_forwarded_proposal(std::vector<entry> entries)
    {
        if (state_ == std::pair{role::leader, phase::prepare}) {
            buffered_proposals_.insert(buffered_proposals_.end(), std::make_move_iterator(entries.begin()),
                                       std::make_move_iterator(entries.end()));
            return {};
        }
        if (state_ == std::pair{role::leader, phase::accept}) {
            return accept_entries_leader(std::move(entries));
        }
        forward_proposals(std::move(entries));
        return {};
    }

    void start_read_round()
    {
        if (state_ != std::pair{role::leader, phase::accept} || read_pending_.empty() ||
            (!read_round_.reads.empty() && read_round_.n == leader_state_.n_leader)) {
            return;
        }
        read_round_ = {leader_state_.n_leader, read_round_.id + 1, std::exchange(read_pending_, {}), accepted_idx_,
                       {pid_}};
        for (const auto peer : peers_) {
            outgoing_.push_back({pid_, peer, heartbeat{read_round_.n, read_round_.id}});
        }
    }

    void handle_read_index_request(const read_index_request& req, const node_id from)
    {
        if (state_.first != role::leader) {
            return;
        }
        read_pending_.push_back({from, req.read});
        start_read_round();
    }

    void handle_heartbeat(const heartbeat& hb, const node_id from)
    {
        if (promise_ == hb.n) {
            outgoing_.push_back({pid_, from, heartbeat_ack{hb.n, hb.id}});
        }
    }

    void handle_heartbeat_ack(const heartbeat_ack& ack, const node_id from)
    {
        auto& rd = read_round_;
        if (state_ != std::pair{role::leader, phase::accept} || rd.reads.empty() || rd.n != ack.n ||
            rd.id != ack.id || leader_state_.n_leader != ack.n) {
            return;
        }
        if (!std::ranges::contains(rd.acks, from)) {
            rd.acks.push_back(from);
        }
        if (rd.acks.size() < leader_state_.majority()) {
            return;
        }
        for (const auto& r : rd.reads) {
            if (r.node == pid_) {
                indexed_reads_.push_back({r.read, rd.read_index, promise_});
            }
        }
        for (const auto peer : peers_) {
            std::vector<std::uint64_t> reads;
            for (const auto& r : rd.reads) {
                if (r.node == peer) {
                    reads.push_back(r.read);
                }
            }
            if (!reads.empty()) {
                outgoing_.push_back({pid_, peer, read_index_reply{std::move(reads), rd.read_index}});
            }
        }
        rd.reads.clear();
        rd.acks.clear();
        start_read_round();
    }

    void handle_read_index_reply(const read_index_reply& reply)
    {
        for (const auto r : reply.reads) {
            indexed_reads_.push_back({r, reply.read_index, promise_});
        }
    }

    node_id pid_;
    std::vector<node_id> peers_;
    std::vector<node_id> nodes_;
    S internal_storage_;
    std::pair<role, phase> state_{role::follower, phase::none};
    ballot promise_{};
    ballot accepted_round_{};
    std::uint64_t decided_idx_{};
    std::uint64_t accepted_idx_{};
    leader_state leader_state_{};
    sequence_number current_seq_num_{};
    struct applied_sync {
        ballot n;
        std::uint64_t session;
    } applied_sync_{};
    std::optional<konsens::promise> cached_promise_message_;
    std::vector<entry> buffered_proposals_;
    std::vector<paxos_message> outgoing_;
    std::optional<std::error_code> error_;
    bool recovering_{};
    std::uint64_t nonce_{};
    std::vector<std::pair<node_id, recovery_response>> recovery_responses_;
    std::vector<pending_read> read_pending_;
    konsens::read_round read_round_{};
    std::vector<indexed_read> indexed_reads_;
};

}
