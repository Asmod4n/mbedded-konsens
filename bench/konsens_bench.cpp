#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <expected>
#include <filesystem>
#include <format>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

#include <unistd.h>

#include <crc32c/crc32c.h>

#include "konsens/memory_storage.hpp"
#include "konsens/node.hpp"
#include "konsens/persistent_storage.hpp"
#include "konsens/storage.hpp"
#include "konsens/wire.hpp"

namespace {

constexpr std::uint64_t warm_up_samples = 1;
constexpr std::size_t entries_per_accept_decide = 64;
constexpr std::size_t accept_decide_entry_bytes = 1024;
constexpr std::size_t batch_size = 64;
constexpr std::size_t crc_buffer_count = 16;
constexpr std::string_view crc32c_check_input = "123456789";
constexpr std::uint32_t crc32c_check_value = 0xe3069283;
constexpr std::array<std::size_t, 3> payload_sizes{64, 1024, 65536};
constexpr std::byte payload_byte{0x5a};
constexpr std::size_t node_count = 3;
constexpr std::uint32_t configuration_id = 1;
constexpr std::uint64_t election_ticks = 5;
constexpr std::uint64_t resend_ticks = 3;
constexpr std::uint64_t round_limit = 200;
constexpr std::uint64_t writes_before_read = 1;
constexpr konsens::node_id leader_pid = 1;
constexpr konsens::node_id follower_pid = 2;
constexpr std::uint32_t first_ballot_number = 1;
constexpr konsens::ballot wire_ballot{configuration_id, first_ballot_number, 0, leader_pid};
constexpr konsens::sequence_number wire_sequence_number{1, 1};
constexpr std::uint64_t wire_decided_idx = 1;
constexpr std::size_t lmdb_page_bytes = 4096;
constexpr std::size_t lmdb_record_overhead_bytes = 64;
constexpr std::size_t lmdb_copies_of_each_page = 2;
constexpr std::size_t lmdb_base_map_bytes = std::size_t{64} << 20;
constexpr std::uint64_t setup_entries = 16;

struct failure {
    std::string_view step;
    std::error_code code;
};

struct run_settings {
    std::uint64_t samples;
    std::uint64_t ops;
    std::size_t payload_bytes;
    std::filesystem::path directory;
};

struct case_result {
    std::vector<double> ns_per_op;
    std::string details;
};

using case_outcome = std::expected<case_result, failure>;

std::error_code check_failed()
{
    return std::make_error_code(std::errc::state_not_recoverable);
}

double ns_per_op_of(const std::chrono::steady_clock::time_point start, const std::chrono::steady_clock::time_point end,
                    const std::uint64_t ops)
{
    return std::chrono::duration<double, std::nano>(end - start).count() / static_cast<double>(ops);
}

double per_op(const std::uint64_t count, const run_settings& s)
{
    return static_cast<double>(count) / static_cast<double>(s.samples * s.ops);
}

std::size_t map_bytes_for(const std::uint64_t entry_count, const std::size_t payload_bytes)
{
    const std::size_t pages_per_entry = (payload_bytes + lmdb_record_overhead_bytes) / lmdb_page_bytes + 1;
    return lmdb_base_map_bytes + entry_count * pages_per_entry * lmdb_page_bytes * lmdb_copies_of_each_page;
}

struct counter {
    std::uint64_t value;

    static std::vector<std::byte> bytes_of(const std::uint64_t v)
    {
        const auto b = std::bit_cast<std::array<std::byte, sizeof v>>(v);
        return {b.begin(), b.end()};
    }

    std::vector<std::byte> apply([[maybe_unused]] const std::span<const std::byte> payload)
    {
        value += 1;
        return bytes_of(value);
    }

    std::vector<std::byte> query([[maybe_unused]] const std::span<const std::byte> payload) const
    {
        return bytes_of(value);
    }
};

std::optional<std::uint64_t> counter_value_of(const konsens::client_reply& reply)
{
    std::array<std::byte, sizeof(std::uint64_t)> b{};
    if (!reply.response || reply.response->size() != b.size()) [[unlikely]] {
        return std::nullopt;
    }
    std::ranges::copy(*reply.response, b.begin());
    return std::bit_cast<std::uint64_t>(b);
}

struct counting_storage {
    std::uint64_t payload_bytes;
    std::uint64_t log_len;
    konsens::ballot promise;
    std::uint64_t decided_idx;
    konsens::ballot accepted_round;

    std::expected<void, std::error_code> write_atomically(const std::span<const konsens::storage_op> ops)
    {
        for (const auto& op : ops) {
            std::visit([this](const auto& o) { apply(o); }, op);
        }
        return {};
    }

    std::expected<std::vector<konsens::entry>, std::error_code> get_entries([[maybe_unused]] const std::uint64_t from,
                                                                            [[maybe_unused]] const std::uint64_t to) const
    {
        return std::vector<konsens::entry>{};
    }

    std::expected<std::uint64_t, std::error_code> get_log_len() const { return log_len; }
    std::expected<konsens::ballot, std::error_code> get_promise() const { return promise; }
    std::expected<std::uint64_t, std::error_code> get_decided_idx() const { return decided_idx; }
    std::expected<konsens::ballot, std::error_code> get_accepted_round() const { return accepted_round; }
    std::expected<std::uint32_t, std::error_code> get_last_entry_crc() const { return 0; }

    void apply(const konsens::append_entries& o)
    {
        log_len += o.entries.size();
        add_payload_bytes(o.entries);
    }

    void apply(const konsens::append_on_prefix& o)
    {
        log_len = o.from_idx + o.entries.size();
        add_payload_bytes(o.entries);
    }

    void apply(const konsens::set_promise& o) { promise = o.promise; }
    void apply(const konsens::set_decided_index& o) { decided_idx = o.decided_idx; }
    void apply(const konsens::set_accepted_round& o) { accepted_round = o.accepted_round; }

    void add_payload_bytes(const std::span<const konsens::entry> entries)
    {
        for (const auto& e : entries) {
            payload_bytes += e.payload.size();
        }
    }
};

konsens::node_config config_of(const konsens::node_id pid)
{
    std::vector<konsens::node_id> peers;
    for (konsens::node_id p = 1; p <= node_count; ++p) {
        if (p != pid) {
            peers.push_back(p);
        }
    }
    return {configuration_id, pid, peers, 0, election_ticks, resend_ticks};
}

std::vector<konsens::entry> entries_of(const std::size_t count, const std::size_t payload_bytes)
{
    return std::vector<konsens::entry>(
        count, konsens::entry{konsens::entry_kind::normal, std::vector<std::byte>(payload_bytes, payload_byte)});
}

template <typename T>
bool holds_equal(const konsens::received& r, const konsens::node_id from, const konsens::node_id to, const T& body)
{
    const auto* held = std::get_if<cbor::ref<T>>(&r.msg);
    return r.from == from && r.to == to && held != nullptr && **held == body;
}

template <konsens::storage S>
struct cluster {
    using result = std::expected<void, std::error_code>;

    std::vector<konsens::node<S, counter>> nodes;
    std::vector<std::string> network;
    std::vector<konsens::client_reply> replies;
    std::uint64_t next_call{1};
    std::uint64_t rounds{};
    std::uint64_t messages{};
    std::vector<std::optional<konsens::transaction>> pending;

    result add(const std::size_t i, const std::expected<konsens::transaction, std::error_code>& t)
    {
        if (!t) [[unlikely]] {
            return std::unexpected(t.error());
        }
        pending.resize(nodes.size());
        if (pending.at(i)) {
            *pending.at(i) += *t;
        } else {
            pending.at(i) = *t;
        }
        return {};
    }

    result commit_and_collect()
    {
        pending.resize(nodes.size());
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            auto& n = nodes.at(i);
            if (const auto t = std::exchange(pending.at(i), std::nullopt)) {
                if (auto r = n.commit(*t); !r) [[unlikely]] {
                    return r;
                }
            }
            for (const auto& m : n.take_outgoing_messages()) {
                auto bytes = konsens::encode(m);
                if (!bytes) [[unlikely]] {
                    return std::unexpected(bytes.error());
                }
                network.push_back(std::move(*bytes));
            }
            for (auto& r : n.take_client_replies()) {
                replies.push_back(std::move(r));
            }
        }
        return {};
    }

    result run_round()
    {
        rounds += 1;
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            if (auto r = add(i, nodes.at(i).tick()); !r) [[unlikely]] {
                return r;
            }
        }
        if (auto c = commit_and_collect(); !c) [[unlikely]] {
            return c;
        }
        while (!network.empty()) {
            std::vector<std::vector<konsens::received>> arrived(nodes.size());
            for (auto& bytes : std::exchange(network, {})) {
                auto r = konsens::decode(std::move(bytes));
                if (!r) [[unlikely]] {
                    return std::unexpected(r.error());
                }
                messages += 1;
                arrived.at(r->to - 1).push_back(std::move(*r));
            }
            for (std::size_t i = 0; i < nodes.size(); ++i) {
                if (auto h = add(i, nodes.at(i).handle_incoming(std::move(arrived.at(i)))); !h) [[unlikely]] {
                    return h;
                }
            }
            if (auto c = commit_and_collect(); !c) [[unlikely]] {
                return c;
            }
        }
        return {};
    }

    std::optional<std::size_t> leader() const
    {
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            if (nodes.at(i).get_sequence_paxos().get_state() ==
                std::pair{konsens::role::leader, konsens::phase::accept}) {
                return i;
            }
        }
        return std::nullopt;
    }

    result elect()
    {
        for (std::uint64_t r = 0; r < round_limit; ++r) {
            if (leader()) {
                return {};
            }
            if (auto ran = run_round(); !ran) [[unlikely]] {
                return ran;
            }
        }
        return std::unexpected(std::make_error_code(std::errc::timed_out));
    }

    std::expected<std::vector<konsens::client_reply>, std::error_code> wait_for_replies(const std::uint64_t first_call,
                                                                                        const std::uint64_t count)
    {
        std::vector<konsens::client_reply> taken;
        for (std::uint64_t r = 0; r < round_limit; ++r) {
            std::erase_if(replies, [&taken, first_call, count](auto& reply) {
                if (reply.client_id < first_call || reply.client_id - first_call >= count) {
                    return false;
                }
                taken.push_back(std::move(reply));
                return true;
            });
            if (taken.size() == count) {
                return taken;
            }
            if (auto ran = run_round(); !ran) [[unlikely]] {
                return std::unexpected(ran.error());
            }
        }
        return std::unexpected(std::make_error_code(std::errc::timed_out));
    }

    std::expected<std::uint64_t, std::error_code> register_client(const std::size_t at)
    {
        const auto call = next_call++;
        if (auto r = add(at, nodes.at(at).register_client(call)); !r) [[unlikely]] {
            return std::unexpected(r.error());
        }
        const auto reply = wait_for_replies(call, 1);
        if (!reply) [[unlikely]] {
            return std::unexpected(reply.error());
        }
        return reply->front().session;
    }

    std::expected<konsens::client_reply, std::error_code> send_request(const std::size_t at, const std::uint64_t session,
                                                                       const std::uint64_t request_number,
                                                                       std::vector<std::byte> payload)
    {
        const auto call = next_call++;
        if (auto r = add(at, nodes.at(at).client_request(call, session, request_number, std::move(payload))); !r)
            [[unlikely]] {
            return std::unexpected(r.error());
        }
        auto reply = wait_for_replies(call, 1);
        if (!reply) [[unlikely]] {
            return std::unexpected(reply.error());
        }
        return std::move(reply->front());
    }

    std::uint64_t log_len_of(const std::size_t at) const
    {
        return nodes.at(at).get_sequence_paxos().get_internal_storage().get_log_len().value_or(0);
    }
};

template <konsens::storage S>
std::expected<cluster<S>, failure> start_cluster(const auto& open_storage)
{
    cluster<S> c{};
    for (konsens::node_id pid = 1; pid <= node_count; ++pid) {
        auto storage = open_storage(pid);
        if (!storage) [[unlikely]] {
            return std::unexpected(failure{"open the storage of a node", storage.error()});
        }
        auto n = konsens::node<S, counter>::with(config_of(pid), std::move(*storage), counter{}, pid);
        if (!n) [[unlikely]] {
            return std::unexpected(failure{"start a node", n.error()});
        }
        c.nodes.push_back(std::move(*n));
    }
    if (auto r = c.elect(); !r) [[unlikely]] {
        return std::unexpected(failure{"elect a leader", r.error()});
    }
    return c;
}

std::size_t follower_of(const std::size_t leader)
{
    return leader == 0 ? 1 : 0;
}

case_outcome measure_crc32c(const run_settings& s)
{
    if (crc32c::Crc32c(crc32c_check_input) != crc32c_check_value) [[unlikely]] {
        return std::unexpected(failure{"compare the CRC32C of the check input with the check value", check_failed()});
    }
    const std::vector<std::vector<std::byte>> buffers(crc_buffer_count,
                                                      std::vector<std::byte>(s.payload_bytes, payload_byte));
    std::vector<double> ns;
    std::uint32_t folded = 0;
    for (std::uint64_t sample = 0; sample < warm_up_samples + s.samples; ++sample) {
        const auto start = std::chrono::steady_clock::now();
        for (std::uint64_t k = 0; k < s.ops; ++k) {
            const auto& buffer = buffers.at(k % crc_buffer_count);
            folded ^= crc32c::Crc32c(buffer.data(), buffer.size());
        }
        const auto end = std::chrono::steady_clock::now();
        if (sample >= warm_up_samples) {
            ns.push_back(ns_per_op_of(start, end, s.ops));
        }
    }
    return case_result{ns, std::format("crc_folded={:08x}", folded)};
}

template <typename T>
case_outcome measure_encode(const run_settings& s, const T& body)
{
    const konsens::message m{konsens::paxos_message{leader_pid, follower_pid, body}};
    const auto reference = konsens::encode(m);
    if (!reference) [[unlikely]] {
        return std::unexpected(failure{"encode the message", reference.error()});
    }
    const auto decoded = konsens::decode(*reference);
    if (!decoded) [[unlikely]] {
        return std::unexpected(failure{"decode the encoded message", decoded.error()});
    }
    if (!holds_equal(*decoded, leader_pid, follower_pid, body)) [[unlikely]] {
        return std::unexpected(failure{"compare the decoded message with the encoded one", check_failed()});
    }
    std::vector<double> ns;
    for (std::uint64_t sample = 0; sample < warm_up_samples + s.samples; ++sample) {
        std::vector<std::expected<std::string, std::error_code>> encoded;
        encoded.reserve(s.ops);
        const auto start = std::chrono::steady_clock::now();
        for (std::uint64_t k = 0; k < s.ops; ++k) {
            encoded.push_back(konsens::encode(m));
        }
        const auto end = std::chrono::steady_clock::now();
        if (!std::ranges::all_of(encoded, [&reference](const auto& e) { return e && *e == *reference; }))
            [[unlikely]] {
            return std::unexpected(failure{"compare each encoded message with the first", check_failed()});
        }
        if (sample >= warm_up_samples) {
            ns.push_back(ns_per_op_of(start, end, s.ops));
        }
    }
    return case_result{ns, std::format("encoded_bytes={}", reference->size())};
}

template <typename T>
case_outcome measure_decode(const run_settings& s, const T& body)
{
    const konsens::message m{konsens::paxos_message{leader_pid, follower_pid, body}};
    const auto reference = konsens::encode(m);
    if (!reference) [[unlikely]] {
        return std::unexpected(failure{"encode the message", reference.error()});
    }
    std::vector<double> ns;
    for (std::uint64_t sample = 0; sample < warm_up_samples + s.samples; ++sample) {
        std::vector<std::string> copies(s.ops, *reference);
        std::vector<std::expected<konsens::received, std::error_code>> decoded;
        decoded.reserve(s.ops);
        const auto start = std::chrono::steady_clock::now();
        for (auto& bytes : copies) {
            decoded.push_back(konsens::decode(std::move(bytes)));
        }
        const auto end = std::chrono::steady_clock::now();
        if (!std::ranges::all_of(decoded, [&body](const auto& d) {
                return d && holds_equal(*d, leader_pid, follower_pid, body);
            })) [[unlikely]] {
            return std::unexpected(failure{"compare each decoded message with the encoded one", check_failed()});
        }
        if (sample >= warm_up_samples) {
            ns.push_back(ns_per_op_of(start, end, s.ops));
        }
    }
    return case_result{ns, std::format("encoded_bytes={}", reference->size())};
}

konsens::accept_decide wire_accept_decide()
{
    return {wire_ballot, wire_sequence_number, wire_decided_idx,
            entries_of(entries_per_accept_decide, accept_decide_entry_bytes)};
}

case_outcome measure_encode_accept_decide(const run_settings& s)
{
    return measure_encode(s, wire_accept_decide());
}

case_outcome measure_decode_accept_decide(const run_settings& s)
{
    return measure_decode(s, wire_accept_decide());
}

case_outcome measure_encode_decide(const run_settings& s)
{
    return measure_encode(s, konsens::decide{wire_ballot, wire_sequence_number, wire_decided_idx});
}

case_outcome measure_decode_decide(const run_settings& s)
{
    return measure_decode(s, konsens::decide{wire_ballot, wire_sequence_number, wire_decided_idx});
}

struct accepting_follower {
    cluster<counting_storage> c;
    std::size_t leader;
    std::size_t follower;
    konsens::accept_decide first;
};

std::expected<konsens::received, std::error_code> received_of(const konsens::message& m)
{
    auto bytes = konsens::encode(m);
    if (!bytes) [[unlikely]] {
        return std::unexpected(bytes.error());
    }
    return konsens::decode(std::move(*bytes));
}

std::expected<accepting_follower, failure> start_accepting_follower()
{
    auto c = start_cluster<counting_storage>(
        []([[maybe_unused]] const konsens::node_id pid) -> std::expected<counting_storage, std::error_code> {
            return counting_storage{};
        });
    if (!c) [[unlikely]] {
        return std::unexpected(c.error());
    }
    const auto leader = *c->leader();
    const auto follower = follower_of(leader);
    const auto appended = c->nodes.at(leader).append(konsens::entry{konsens::entry_kind::normal});
    if (!appended) [[unlikely]] {
        return std::unexpected(failure{"append an entry at the leader", appended.error()});
    }
    if (auto r = c->nodes.at(leader).commit(*appended); !r) [[unlikely]] {
        return std::unexpected(failure{"commit the entry at the leader", r.error()});
    }
    const auto to = static_cast<konsens::node_id>(follower + 1);
    for (const auto& m : c->nodes.at(leader).take_outgoing_messages()) {
        const auto* p = std::get_if<konsens::paxos_message>(&m);
        if (p == nullptr || p->to != to) {
            continue;
        }
        const auto* acc = std::get_if<konsens::accept_decide>(&p->msg);
        if (acc == nullptr) {
            continue;
        }
        auto r = received_of(m);
        if (!r) [[unlikely]] {
            return std::unexpected(failure{"pass the first AcceptDecide through the wire", r.error()});
        }
        const auto before = c->nodes.at(follower).get_sequence_paxos().get_accepted_idx();
        std::vector<konsens::received> batch;
        batch.push_back(std::move(*r));
        const auto handled = c->nodes.at(follower).handle_incoming(std::move(batch));
        if (!handled) [[unlikely]] {
            return std::unexpected(failure{"handle the first AcceptDecide", handled.error()});
        }
        if (auto h = c->nodes.at(follower).commit(*handled); !h) [[unlikely]] {
            return std::unexpected(failure{"commit the first AcceptDecide", h.error()});
        }
        std::ignore = c->nodes.at(follower).take_outgoing_messages();
        if (c->nodes.at(follower).get_sequence_paxos().get_state() !=
                std::pair{konsens::role::follower, konsens::phase::accept} ||
            c->nodes.at(follower).get_sequence_paxos().get_accepted_idx() != before + acc->entries.size())
            [[unlikely]] {
            return std::unexpected(failure{"bring the follower to (Follower, Accept)", check_failed()});
        }
        return accepting_follower{std::move(*c), leader, follower, *acc};
    }
    return std::unexpected(failure{"find the first AcceptDecide to the follower", check_failed()});
}

case_outcome measure_handle_accept_decide(const run_settings& s)
{
    auto f = start_accepting_follower();
    if (!f) [[unlikely]] {
        return std::unexpected(f.error());
    }
    auto& follower = f->c.nodes.at(f->follower);
    const auto from = static_cast<konsens::node_id>(f->leader + 1);
    const auto to = static_cast<konsens::node_id>(f->follower + 1);
    const auto entries = entries_of(entries_per_accept_decide, accept_decide_entry_bytes);
    const std::uint64_t bytes_per_message = entries_per_accept_decide * accept_decide_entry_bytes;
    auto counter_of_message = f->first.seq_num.counter;
    std::vector<double> ns;
    for (std::uint64_t sample = 0; sample < warm_up_samples + s.samples; ++sample) {
        std::vector<std::vector<konsens::received>> batches;
        for (std::uint64_t k = 0; k < s.ops; ++k) {
            counter_of_message += 1;
            auto r = received_of(konsens::paxos_message{
                from, to,
                konsens::accept_decide{f->first.n, {f->first.seq_num.session, counter_of_message},
                                       f->first.decided_idx, entries}});
            if (!r) [[unlikely]] {
                return std::unexpected(failure{"pass an AcceptDecide through the wire", r.error()});
            }
            if (k % konsens::max_batch_size == 0) {
                batches.emplace_back();
            }
            batches.back().push_back(std::move(*r));
        }
        const auto counted_before = follower.get_sequence_paxos().get_internal_storage().payload_bytes;
        const auto start = std::chrono::steady_clock::now();
        for (auto& batch : batches) {
            const auto handled = follower.handle_incoming(std::move(batch));
            if (!handled) [[unlikely]] {
                return std::unexpected(failure{"handle a batch of AcceptDecide messages", handled.error()});
            }
            if (auto h = follower.commit(*handled); !h) [[unlikely]] {
                return std::unexpected(failure{"commit a batch of AcceptDecide messages", h.error()});
            }
        }
        const auto end = std::chrono::steady_clock::now();
        std::ignore = follower.take_outgoing_messages();
        if (follower.get_sequence_paxos().get_internal_storage().payload_bytes - counted_before !=
            s.ops * bytes_per_message) [[unlikely]] {
            return std::unexpected(failure{"compare the bytes counted with the bytes sent", check_failed()});
        }
        if (sample >= warm_up_samples) {
            ns.push_back(ns_per_op_of(start, end, s.ops));
        }
    }
    return case_result{ns, std::format("payload_bytes_counted={}",
                                       follower.get_sequence_paxos().get_internal_storage().payload_bytes)};
}

case_outcome measure_handle_decide(const run_settings& s)
{
    auto f = start_accepting_follower();
    if (!f) [[unlikely]] {
        return std::unexpected(f.error());
    }
    auto& follower = f->c.nodes.at(f->follower);
    const auto from = static_cast<konsens::node_id>(f->leader + 1);
    const auto to = static_cast<konsens::node_id>(f->follower + 1);
    auto counter_of_message = f->first.seq_num.counter;
    auto decided = follower.get_sequence_paxos().get_decided_idx();
    std::vector<double> ns;
    for (std::uint64_t sample = 0; sample < warm_up_samples + s.samples; ++sample) {
        counter_of_message += 1;
        auto filled = received_of(konsens::paxos_message{
            from, to,
            konsens::accept_decide{f->first.n, {f->first.seq_num.session, counter_of_message}, decided,
                                   entries_of(s.ops, 0)}});
        if (!filled) [[unlikely]] {
            return std::unexpected(
                failure{"pass the AcceptDecide of the empty entries through the wire", filled.error()});
        }
        std::vector<konsens::received> fill;
        fill.push_back(std::move(*filled));
        const auto filled_in = follower.handle_incoming(std::move(fill));
        if (!filled_in) [[unlikely]] {
            return std::unexpected(failure{"handle the AcceptDecide of the empty entries", filled_in.error()});
        }
        if (auto h = follower.commit(*filled_in); !h) [[unlikely]] {
            return std::unexpected(failure{"commit the AcceptDecide of the empty entries", h.error()});
        }
        std::ignore = follower.take_outgoing_messages();
        if (follower.get_sequence_paxos().get_accepted_idx() < decided + s.ops) [[unlikely]] {
            return std::unexpected(failure{"accept an entry for each Decide", check_failed()});
        }
        std::vector<std::vector<konsens::received>> batches;
        for (std::uint64_t k = 0; k < s.ops; ++k) {
            counter_of_message += 1;
            decided += 1;
            auto r = received_of(konsens::paxos_message{
                from, to, konsens::decide{f->first.n, {f->first.seq_num.session, counter_of_message}, decided}});
            if (!r) [[unlikely]] {
                return std::unexpected(failure{"pass a Decide through the wire", r.error()});
            }
            if (k % konsens::max_batch_size == 0) {
                batches.emplace_back();
            }
            batches.back().push_back(std::move(*r));
        }
        const auto start = std::chrono::steady_clock::now();
        for (auto& batch : batches) {
            const auto handled = follower.handle_incoming(std::move(batch));
            if (!handled) [[unlikely]] {
                return std::unexpected(failure{"handle a batch of Decide messages", handled.error()});
            }
            if (auto h = follower.commit(*handled); !h) [[unlikely]] {
                return std::unexpected(failure{"commit a batch of Decide messages", h.error()});
            }
        }
        const auto end = std::chrono::steady_clock::now();
        std::ignore = follower.take_outgoing_messages();
        if (follower.get_sequence_paxos().get_decided_idx() != decided) [[unlikely]] {
            return std::unexpected(failure{"compare the decided index with the last Decide", check_failed()});
        }
        if (sample >= warm_up_samples) {
            ns.push_back(ns_per_op_of(start, end, s.ops));
        }
    }
    return case_result{ns, std::format("decided_idx={}", decided)};
}

template <konsens::storage S>
case_outcome measure_commits(const run_settings& s, const auto& open_storage)
{
    std::vector<double> ns;
    for (std::uint64_t sample = 0; sample < warm_up_samples + s.samples; ++sample) {
        auto store = open_storage(sample);
        if (!store) [[unlikely]] {
            return std::unexpected(failure{"open a fresh store", store.error()});
        }
        const auto entries = entries_of(s.ops, s.payload_bytes);
        const std::span<const konsens::entry> all(entries);
        const auto start = std::chrono::steady_clock::now();
        for (std::size_t k = 0; k < all.size(); ++k) {
            const std::array<konsens::storage_op, 1> ops{konsens::append_entries{all.subspan(k, 1)}};
            if (auto w = store->write_atomically(ops); !w) [[unlikely]] {
                return std::unexpected(failure{"commit one entry", w.error()});
            }
        }
        const auto end = std::chrono::steady_clock::now();
        if (store->get_log_len() != s.ops) [[unlikely]] {
            return std::unexpected(failure{"compare the log length with the number of commits", check_failed()});
        }
        if (sample >= warm_up_samples) {
            ns.push_back(ns_per_op_of(start, end, s.ops));
        }
    }
    return case_result{ns, std::string{}};
}

case_outcome measure_commit_memory(const run_settings& s)
{
    return measure_commits<konsens::memory_storage>(
        s, []([[maybe_unused]] const std::uint64_t sample) -> std::expected<konsens::memory_storage, std::error_code> {
            return konsens::memory_storage{};
        });
}

case_outcome measure_commit_lmdb(const run_settings& s)
{
    return measure_commits<konsens::persistent_storage>(s, [&s](const std::uint64_t sample) {
        const auto directory = s.directory / std::format("sample-{}", sample);
        std::filesystem::create_directories(directory);
        return konsens::persistent_storage::open(directory, map_bytes_for(s.ops, s.payload_bytes), geteuid());
    });
}

auto memory_nodes()
{
    return []([[maybe_unused]] const konsens::node_id pid) -> std::expected<konsens::memory_storage, std::error_code> {
        return konsens::memory_storage{};
    };
}

auto lmdb_nodes(const run_settings& s, const std::uint64_t entries_per_op)
{
    const auto map_bytes =
        map_bytes_for((warm_up_samples + s.samples) * s.ops * entries_per_op + setup_entries, s.payload_bytes);
    return [map_bytes, &s](const konsens::node_id pid) {
        const auto directory = s.directory / std::format("node-{}", pid);
        std::filesystem::create_directories(directory);
        return konsens::persistent_storage::open(directory, map_bytes, geteuid());
    };
}

struct traffic {
    std::uint64_t rounds;
    std::uint64_t messages;
    std::uint64_t log_len;
};

template <konsens::storage S>
traffic traffic_of(const cluster<S>& c, const std::size_t at)
{
    return {c.rounds, c.messages, c.log_len_of(at)};
}

std::string details_of(const traffic& first, const traffic& last, const run_settings& s)
{
    return std::format("rounds_per_op={:.2f} messages_per_op={:.2f} log_len_first={} log_len_last={}",
                       per_op(last.rounds - first.rounds, s), per_op(last.messages - first.messages, s), first.log_len,
                       last.log_len);
}

template <konsens::storage S>
case_outcome measure_writes(const run_settings& s, const auto& open_storage)
{
    auto c = start_cluster<S>(open_storage);
    if (!c) [[unlikely]] {
        return std::unexpected(c.error());
    }
    const auto at = follower_of(*c->leader());
    const auto session = c->register_client(at);
    if (!session) [[unlikely]] {
        return std::unexpected(failure{"register the client", session.error()});
    }
    std::uint64_t request_number = 0;
    traffic first{};
    std::vector<double> ns;
    for (std::uint64_t sample = 0; sample < warm_up_samples + s.samples; ++sample) {
        std::vector<std::vector<std::byte>> payloads(s.ops, std::vector<std::byte>(s.payload_bytes, payload_byte));
        if (sample == warm_up_samples) {
            first = traffic_of(*c, at);
        }
        const auto start = std::chrono::steady_clock::now();
        for (auto& payload : payloads) {
            request_number += 1;
            const auto reply = c->send_request(at, *session, request_number, std::move(payload));
            if (!reply) [[unlikely]] {
                return std::unexpected(failure{"write one request", reply.error()});
            }
            if (counter_value_of(*reply) != request_number) [[unlikely]] {
                return std::unexpected(failure{"compare the counter reply with the number of writes", check_failed()});
            }
        }
        const auto end = std::chrono::steady_clock::now();
        if (sample >= warm_up_samples) {
            ns.push_back(ns_per_op_of(start, end, s.ops));
        }
    }
    return case_result{ns, details_of(first, traffic_of(*c, at), s)};
}

template <konsens::storage S>
case_outcome measure_reads(const run_settings& s, const auto& open_storage)
{
    auto c = start_cluster<S>(open_storage);
    if (!c) [[unlikely]] {
        return std::unexpected(c.error());
    }
    const auto at = follower_of(*c->leader());
    const auto session = c->register_client(at);
    if (!session) [[unlikely]] {
        return std::unexpected(failure{"register the client", session.error()});
    }
    for (std::uint64_t k = 1; k <= writes_before_read; ++k) {
        if (const auto reply = c->send_request(at, *session, k, {}); !reply) [[unlikely]] {
            return std::unexpected(failure{"write before the reads", reply.error()});
        }
    }
    traffic first{};
    std::vector<double> ns;
    for (std::uint64_t sample = 0; sample < warm_up_samples + s.samples; ++sample) {
        if (sample == warm_up_samples) {
            first = traffic_of(*c, at);
        }
        const auto start = std::chrono::steady_clock::now();
        for (std::uint64_t k = 0; k < s.ops; ++k) {
            const auto call = c->next_call++;
            if (auto r = c->add(at, c->nodes.at(at).client_query(call, {})); !r) [[unlikely]] {
                return std::unexpected(failure{"send one query", r.error()});
            }
            const auto reply = c->wait_for_replies(call, 1);
            if (!reply) [[unlikely]] {
                return std::unexpected(failure{"wait for the reply of one query", reply.error()});
            }
            if (counter_value_of(reply->front()) != writes_before_read) [[unlikely]] {
                return std::unexpected(failure{"compare the counter reply with the number of writes", check_failed()});
            }
        }
        const auto end = std::chrono::steady_clock::now();
        if (sample >= warm_up_samples) {
            ns.push_back(ns_per_op_of(start, end, s.ops));
        }
    }
    return case_result{ns, details_of(first, traffic_of(*c, at), s)};
}

template <konsens::storage S>
case_outcome measure_write_batches(const run_settings& s, const auto& open_storage)
{
    auto c = start_cluster<S>(open_storage);
    if (!c) [[unlikely]] {
        return std::unexpected(c.error());
    }
    const auto at = follower_of(*c->leader());
    const auto session = c->register_client(at);
    if (!session) [[unlikely]] {
        return std::unexpected(failure{"register the client", session.error()});
    }
    std::uint64_t request_number = 0;
    traffic first{};
    std::vector<double> ns;
    for (std::uint64_t sample = 0; sample < warm_up_samples + s.samples; ++sample) {
        std::vector<std::vector<std::byte>> payloads(s.ops, std::vector<std::byte>(s.payload_bytes, payload_byte));
        if (sample == warm_up_samples) {
            first = traffic_of(*c, at);
        }
        const auto start = std::chrono::steady_clock::now();
        for (std::size_t b = 0; b < payloads.size(); b += batch_size) {
            const auto first_call = c->next_call;
            const auto first_request = request_number + 1;
            for (auto& payload : std::span(payloads).subspan(b, batch_size)) {
                request_number += 1;
                if (auto r = c->add(at, c->nodes.at(at).client_request(c->next_call++, *session, request_number,
                                                                       std::move(payload)));
                    !r) [[unlikely]] {
                    return std::unexpected(failure{"send one request of a batch", r.error()});
                }
            }
            const auto replies = c->wait_for_replies(first_call, batch_size);
            if (!replies) [[unlikely]] {
                return std::unexpected(failure{"wait for the replies of a batch", replies.error()});
            }
            if (!std::ranges::all_of(*replies, [first_call, first_request](const auto& reply) {
                    return counter_value_of(reply) == first_request + (reply.client_id - first_call);
                })) [[unlikely]] {
                return std::unexpected(failure{"compare each counter reply with its request number", check_failed()});
            }
        }
        const auto end = std::chrono::steady_clock::now();
        if (sample >= warm_up_samples) {
            ns.push_back(ns_per_op_of(start, end, s.ops));
        }
    }
    return case_result{ns, details_of(first, traffic_of(*c, at), s)};
}

case_outcome measure_write_memory(const run_settings& s)
{
    return measure_writes<konsens::memory_storage>(s, memory_nodes());
}

case_outcome measure_write_lmdb(const run_settings& s)
{
    return measure_writes<konsens::persistent_storage>(s, lmdb_nodes(s, 1));
}

case_outcome measure_read_memory(const run_settings& s)
{
    return measure_reads<konsens::memory_storage>(s, memory_nodes());
}

case_outcome measure_read_lmdb(const run_settings& s)
{
    return measure_reads<konsens::persistent_storage>(s, lmdb_nodes(s, 0));
}

case_outcome measure_write_batch_memory(const run_settings& s)
{
    return measure_write_batches<konsens::memory_storage>(s, memory_nodes());
}

case_outcome measure_write_batch_lmdb(const run_settings& s)
{
    return measure_write_batches<konsens::persistent_storage>(s, lmdb_nodes(s, 1));
}

using measure_function = case_outcome(const run_settings&);

struct bench_case {
    std::string_view name;
    measure_function* measure;
    std::array<std::uint64_t, payload_sizes.size()> ops_by_payload;
    std::uint64_t ops_without_payload;
    bool needs_directory;
};

constexpr auto cases = std::to_array<bench_case>({
    {"crc32c", measure_crc32c, {3400000, 300000, 3000}, 0, false},
    {"encode_accept_decide", measure_encode_accept_decide, {}, 200, false},
    {"decode_accept_decide", measure_decode_accept_decide, {}, 160, false},
    {"encode_decide", measure_encode_decide, {}, 150000, false},
    {"decode_decide", measure_decode_decide, {}, 40000, false},
    {"handle_accept_decide", measure_handle_accept_decide, {}, 4000, false},
    {"handle_decide", measure_handle_decide, {}, 400000, false},
    {"commit_memory", measure_commit_memory, {200000, 24000, 300}, 0, false},
    {"commit_lmdb", measure_commit_lmdb, {30, 24, 16}, 0, true},
    {"write_memory", measure_write_memory, {1700, 1100, 38}, 0, false},
    {"write_lmdb", measure_write_lmdb, {5, 5, 3}, 0, true},
    {"read_memory", measure_read_memory, {}, 2200, false},
    {"read_lmdb", measure_read_lmdb, {}, 1700, true},
    {"write_batch_memory", measure_write_batch_memory, {2048, 768, 64}, 0, false},
    {"write_batch_lmdb", measure_write_batch_lmdb, {64, 64, 64}, 0, true},
});

constexpr bool batches_are_whole()
{
    for (const auto& c : cases) {
        if (c.name.starts_with("write_batch_") &&
            !std::ranges::all_of(c.ops_by_payload, [](const std::uint64_t ops) { return ops % batch_size == 0; })) {
            return false;
        }
    }
    return true;
}

static_assert(batches_are_whole());

constexpr std::size_t case_argument = 1;
constexpr std::size_t samples_argument = 2;
constexpr std::size_t payload_argument = 3;
constexpr std::size_t directory_argument = 4;

constexpr std::string_view usage =
    "Usage: konsens_bench <case> <samples> <payload_bytes> [<directory>]\n"
    "A case with a payload takes 64, 1024 or 65536 bytes; every other case takes 0.\n"
    "A case whose name ends in _lmdb takes the directory of its stores; no other case takes one.\n";

std::optional<std::uint64_t> number_of(const std::string_view text)
{
    std::uint64_t value{};
    const auto end = std::to_address(text.end());
    const auto [stop, code] = std::from_chars(std::to_address(text.begin()), end, value);
    if (code != std::errc{} || stop != end) [[unlikely]] {
        return std::nullopt;
    }
    return value;
}

std::optional<std::uint64_t> ops_of(const bench_case& c, const std::uint64_t payload_bytes)
{
    if (c.ops_without_payload != 0) {
        return payload_bytes == 0 ? std::optional{c.ops_without_payload} : std::nullopt;
    }
    const auto it = std::ranges::find(payload_sizes, payload_bytes);
    if (it == payload_sizes.end()) [[unlikely]] {
        return std::nullopt;
    }
    return c.ops_by_payload.at(static_cast<std::size_t>(std::ranges::distance(payload_sizes.begin(), it)));
}

int fail(const std::string_view name, const failure& f)
{
    std::cerr << std::format("The case {} failed at the step \"{}\": {}.", name, f.step, f.code.message()) << '\n';
    return EXIT_FAILURE;
}

int run(const std::vector<std::string_view>& args)
{
    if (args.size() != directory_argument && args.size() != directory_argument + 1) [[unlikely]] {
        std::cerr << usage;
        return EXIT_FAILURE;
    }
    const auto name = args.at(case_argument);
    const auto c = std::ranges::find(cases, name, &bench_case::name);
    const auto samples = number_of(args.at(samples_argument));
    const auto payload_bytes = number_of(args.at(payload_argument));
    const bool has_directory = args.size() > directory_argument;
    if (c == cases.end() || !samples || *samples == 0 || !payload_bytes || has_directory != c->needs_directory)
        [[unlikely]] {
        std::cerr << usage;
        return EXIT_FAILURE;
    }
    const auto ops = ops_of(*c, *payload_bytes);
    if (!ops) [[unlikely]] {
        std::cerr << usage;
        return EXIT_FAILURE;
    }
    const run_settings settings{*samples, *ops, *payload_bytes,
                                has_directory ? std::filesystem::path(args.at(directory_argument)) : std::filesystem::path{}};
    auto outcome = c->measure(settings);
    if (!outcome) [[unlikely]] {
        return fail(name, outcome.error());
    }
    auto& ns = outcome->ns_per_op;
    std::ranges::sort(ns);
    std::cout << std::format("case={} payload_bytes={} ops_per_sample={} samples={} ns_per_op_median={:.2f} "
                             "min={:.2f} max={:.2f}{}{}",
                             name, *payload_bytes, *ops, *samples, ns.at(ns.size() / 2), ns.front(), ns.back(),
                             outcome->details.empty() ? "" : " ", outcome->details)
              << '\n';
    return EXIT_SUCCESS;
}

}

int main(const int argc, char** const argv)
{
    try {
        const std::span<char* const> words(argv, static_cast<std::size_t>(argc));
        return run(std::vector<std::string_view>(words.begin(), words.end()));
    } catch (const std::exception& e) {
        std::cerr << std::format("The bench ended with an exception: {}", e.what()) << '\n';
        return EXIT_FAILURE;
    }
}
