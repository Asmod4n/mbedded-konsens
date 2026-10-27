#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <charconv>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <exception>
#include <expected>
#include <filesystem>
#include <format>
#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

#include <sys/resource.h>
#include <unistd.h>

#include <crc32c/crc32c.h>


#include "konsens/node.hpp"
#include "konsens/persistent_storage.hpp"
#include "konsens/sequence_paxos.hpp"
#include "konsens/state_machine.hpp"
#include "konsens/storage.hpp"
#include "konsens/wire.hpp"

#include "../examples/root_for_test.hpp"
#include "../test/temporary_directory.hpp"

namespace {

constexpr std::uint64_t warm_up_samples = 1;
constexpr std::size_t entries_per_accept_decide = 64;
constexpr std::size_t accept_decide_entry_bytes = 1024;
constexpr std::size_t batch_size = 64;
constexpr std::size_t batch_payload_bytes = 1024;
constexpr std::size_t crc_buffer_count = 16;
constexpr std::string_view crc32c_check_input = "123456789";
constexpr std::uint32_t crc32c_check_value = 0xe3069283;
constexpr std::byte payload_byte{0x5a};
constexpr std::size_t node_count = 3;
constexpr std::uint32_t configuration_id = 1;
constexpr std::uint64_t election_ticks = 5;
constexpr std::uint64_t resend_ticks = 3;
constexpr std::uint64_t max_in_flight = 16;
constexpr std::uint64_t compared_max_in_flight = 1;
constexpr std::uint64_t round_limit = 200;
constexpr std::uint64_t writes_before_read = 1;
constexpr konsens::node_id leader_pid = 1;
constexpr konsens::node_id follower_pid = 2;
constexpr std::uint32_t first_ballot_number = 1;
constexpr konsens::ballot wire_ballot{configuration_id, first_ballot_number, 0, leader_pid};
constexpr konsens::sequence_number wire_sequence_number{1, 1};
constexpr std::uint64_t wire_decided_idx = 1;
constexpr std::uint32_t wire_decided_chain_crc = 0;
constexpr std::size_t sqlite_page_bytes = 4096;
constexpr std::size_t sqlite_record_overhead_bytes = 64;
constexpr std::size_t sqlite_copies_of_each_page = 2;
constexpr std::size_t sqlite_base_bytes = std::size_t{64} << 20;
constexpr std::size_t state_max_bytes = std::size_t{256} << 20;
constexpr std::uint64_t setup_entries = 16;
constexpr std::uint64_t stored_key = 0;
constexpr std::chrono::seconds reply_timeout{20};
constexpr std::size_t puts_of_the_loop = 64;
constexpr std::uint64_t base_rate_per_s = 10000;
constexpr std::uint64_t double_rate_per_s = 2 * base_rate_per_s;
constexpr std::chrono::milliseconds no_delay{0};
constexpr std::chrono::milliseconds long_delay{20};
constexpr std::chrono::milliseconds pipeline_tick_period{100};
constexpr std::uint32_t bench_float_digits = 12;
constexpr double float_offset = 1.0 / 3;
constexpr std::array<double, node_count> float_error_of_node{0, 1e-14, -1e-14};
constexpr std::uint64_t scan_entries_per_transaction = 1000;
constexpr std::size_t scan_entry_bytes = 64;
constexpr std::uint8_t cbor_uint64_head = 0x1b;
constexpr std::uint8_t cbor_float64_head = 0xfb;
constexpr double per_mille = 1000;
constexpr double percent_p50 = 0.50;
constexpr double percent_p99 = 0.99;

template <typename E>
constexpr bool has_results_kind = requires { E::results; };

constexpr bool branch_has_execute_verify = has_results_kind<konsens::entry_kind>;

template <typename S>
constexpr bool has_scan_the_whole_log = requires(konsens::sequence_paxos<S>& sp) { sp.scan_the_whole_log(); };

struct failure {
    std::string_view step;
    std::error_code code;
};

struct run_settings {
    std::uint64_t samples;
    std::uint64_t ops;
    std::size_t payload_bytes;
    std::uint64_t parameter;
    std::filesystem::path directory;
    uid_t user_id;
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

double ops_of_all_samples(const run_settings& s)
{
    return static_cast<double>((warm_up_samples + s.samples) * s.ops);
}

double per_op(const std::uint64_t count, const run_settings& s)
{
    return static_cast<double>(count) / ops_of_all_samples(s);
}

double per_op(const std::chrono::nanoseconds spent, const run_settings& s)
{
    return std::chrono::duration<double, std::nano>(spent).count() / ops_of_all_samples(s);
}

std::size_t max_bytes_for(const std::uint64_t entry_count, const std::size_t payload_bytes)
{
    const std::size_t pages_per_entry = (payload_bytes + sqlite_record_overhead_bytes) / sqlite_page_bytes + 1;
    return sqlite_base_bytes + entry_count * pages_per_entry * sqlite_page_bytes * sqlite_copies_of_each_page;
}

double quantile_of(std::vector<double> values, const double q)
{
    if (values.empty()) [[unlikely]] {
        return 0;
    }
    std::ranges::sort(values);
    const auto at = static_cast<std::size_t>(q * static_cast<double>(values.size() - 1));
    return values.at(at);
}

std::vector<std::byte> bytes_of(const std::uint64_t v)
{
    const auto b = std::bit_cast<std::array<std::byte, sizeof v>>(v);
    return {b.begin(), b.end()};
}

std::uint64_t value_of_bytes(const std::span<const std::byte> bytes)
{
    std::array<std::byte, sizeof(std::uint64_t)> b{};
    std::ranges::copy(bytes | std::views::take(b.size()), b.begin());
    return std::bit_cast<std::uint64_t>(b);
}

std::vector<std::byte> cbor_item_of(const std::uint8_t head, const std::uint64_t bits)
{
    std::vector<std::byte> out{std::byte{head}};
    const auto ordered = std::endian::native == std::endian::big ? bits : std::byteswap(bits);
    const auto b = std::bit_cast<std::array<std::byte, sizeof ordered>>(ordered);
    out.insert(out.end(), b.begin(), b.end());
    return out;
}

std::optional<std::uint64_t> bits_of_cbor_item(const std::uint8_t head, const std::span<const std::byte> item)
{
    std::array<std::byte, sizeof(std::uint64_t)> b{};
    if (item.size() != b.size() + 1 || item.front() != std::byte{head}) [[unlikely]] {
        return std::nullopt;
    }
    std::ranges::copy(item.subspan(1), b.begin());
    const auto ordered = std::bit_cast<std::uint64_t>(b);
    return std::endian::native == std::endian::big ? ordered : std::byteswap(ordered);
}

using run_count = std::shared_ptr<std::atomic<std::uint64_t>>;

run_count new_run_count()
{
    return std::make_shared<std::atomic<std::uint64_t>>(0);
}

struct counter {
    static constexpr std::array key{std::byte{'c'}};

    run_count runs;

    static std::uint64_t value_in(const konsens::state_view auto& view)
    {
        const auto found = view.find(key);
        return found ? value_of_bytes(*found) : 0;
    }

    konsens::applied apply(std::span<const std::byte>, const konsens::state_view auto& view) const
    {
        runs->fetch_add(1, std::memory_order_relaxed);
        auto next = bytes_of(value_in(view) + 1);
        return {next, {{{key.begin(), key.end()}, next}}};
    }

    std::vector<std::byte> query(std::span<const std::byte>, const konsens::state_view auto& view) const
    {
        return bytes_of(value_in(view));
    }
};

struct looping {
    static constexpr std::array key{std::byte{'c'}};

    run_count runs;

    konsens::applied apply(std::span<const std::byte>, const konsens::state_view auto& view) const
    {
        runs->fetch_add(1, std::memory_order_relaxed);
        const auto found = view.find(key);
        auto next = bytes_of((found ? value_of_bytes(*found) : 0) + 1);
        std::vector<konsens::mutation> mutations{{{key.begin(), key.end()}, next}};
        for (std::size_t k = 1; k < puts_of_the_loop; ++k) {
            auto loop_key = bytes_of(k);
            loop_key.insert(loop_key.begin(), std::byte{'l'});
            mutations.push_back({std::move(loop_key), next});
        }
        return {next, std::move(mutations)};
    }

    std::vector<std::byte> query(std::span<const std::byte>, const konsens::state_view auto& view) const
    {
        const auto found = view.find(key);
        return found ? std::vector<std::byte>{found->begin(), found->end()} : std::vector<std::byte>{};
    }
};

template <bool with_float_rule>
struct float_fields {};

template <>
struct float_fields<true> {
    std::uint32_t float_digits = bench_float_digits;
};

struct floating : float_fields<branch_has_execute_verify> {
    static constexpr std::array count_key{std::byte{'n'}};
    static constexpr std::array value_key{std::byte{'f'}};

    run_count runs;
    double error;

    static std::uint64_t count_in(const konsens::state_view auto& view)
    {
        const auto found = view.find(count_key);
        return found ? bits_of_cbor_item(cbor_uint64_head, *found).value_or(0) : 0;
    }

    konsens::applied apply(std::span<const std::byte>, const konsens::state_view auto& view) const
    {
        runs->fetch_add(1, std::memory_order_relaxed);
        const auto count = count_in(view) + 1;
        const double value = (static_cast<double>(count) + float_offset) * (1 + error);
        auto item = cbor_item_of(cbor_float64_head, std::bit_cast<std::uint64_t>(value));
        return {item,
                {{{count_key.begin(), count_key.end()}, cbor_item_of(cbor_uint64_head, count)},
                 {{value_key.begin(), value_key.end()}, item}}};
    }

    std::vector<std::byte> query(std::span<const std::byte>, const konsens::state_view auto& view) const
    {
        const auto found = view.find(value_key);
        return found ? std::vector<std::byte>{found->begin(), found->end()} : std::vector<std::byte>{};
    }
};

std::optional<std::uint64_t> counter_value_of(const konsens::client_reply& reply)
{
    if (!reply.response || reply.response->size() != sizeof(std::uint64_t)) [[unlikely]] {
        return std::nullopt;
    }
    return value_of_bytes(*reply.response);
}

std::optional<std::uint64_t> float_count_of(const konsens::client_reply& reply)
{
    if (!reply.response) [[unlikely]] {
        return std::nullopt;
    }
    const auto bits = bits_of_cbor_item(cbor_float64_head, *reply.response);
    if (!bits) [[unlikely]] {
        return std::nullopt;
    }
    const auto value = std::bit_cast<double>(*bits);
    return static_cast<std::uint64_t>(std::llround(value - float_offset));
}

std::optional<std::uint64_t> reply_count_of(const counter&, const konsens::client_reply& reply)
{
    return counter_value_of(reply);
}

std::optional<std::uint64_t> reply_count_of(const looping&, const konsens::client_reply& reply)
{
    return counter_value_of(reply);
}

std::optional<std::uint64_t> reply_count_of(const floating&, const konsens::client_reply& reply)
{
    return float_count_of(reply);
}

struct counted_storage {
    using read_only_transaction = konsens::persistent_storage::read_only_transaction;

    class transaction {
    public:
        transaction(konsens::persistent_storage::transaction t, std::uint64_t* const commits)
            : t_{std::move(t)}, commits_{commits}
        {
        }

        auto get_meta() const { return t_.get_meta(); }
        auto get_entries(const std::uint64_t from, const std::uint64_t to) const { return t_.get_entries(from, to); }
        auto get_last_entry() const { return t_.get_last_entry(); }
        auto get_entry_state(const std::uint64_t index) const { return t_.get_entry_state(index); }
        auto get_chain_crc(const std::uint64_t index) const { return t_.get_chain_crc(index); }
        auto get_config_first_idx() const { return t_.get_config_first_idx(); }
        auto find(const std::span<const std::byte> key) const { return t_.find(key); }
        auto find_session(const std::uint64_t session) const { return t_.find_session(session); }
        auto insert_or_assign(const std::span<const std::byte> key, const std::span<const std::byte> value)
        {
            return t_.insert_or_assign(key, value);
        }
        auto erase(const std::span<const std::byte> key) { return t_.erase(key); }
        auto insert_or_assign_session(const std::uint64_t session, const konsens::session_record& record)
        {
            return t_.insert_or_assign_session(session, record);
        }
        auto erase_session(const std::uint64_t session) { return t_.erase_session(session); }
        auto set_applied(const std::uint64_t index, const std::uint32_t crc)
        {
            return t_.set_applied(index, crc);
        }
        auto begin_nested() { return t_.begin_nested(); }
        bool has_writes() const { return t_.has_writes(); }
        auto append_entries(const std::span<const konsens::entry> entries) { return t_.append_entries(entries); }
        auto append_on_prefix(const std::uint64_t from_idx, const std::span<const konsens::entry> entries)
        {
            return t_.append_on_prefix(from_idx, entries);
        }
        auto set_promise(const konsens::ballot b) { return t_.set_promise(b); }
        auto set_accepted_round(const konsens::ballot b) { return t_.set_accepted_round(b); }
        auto set_decided_index(const std::uint64_t index) { return t_.set_decided_index(index); }
        auto set_config_first_idx(const std::uint64_t index) { return t_.set_config_first_idx(index); }
        auto write_repaired_log(const std::uint64_t from_idx, const std::span<const konsens::entry> entries,
                                const konsens::ballot promise, const konsens::ballot accepted_round,
                                const std::uint64_t decided_idx)
        {
            return t_.write_repaired_log(from_idx, entries, promise, accepted_round, decided_idx);
        }
        std::expected<void, std::error_code> commit() &&
        {
            *commits_ += 1;
            return std::move(t_).commit();
        }

    private:
        konsens::persistent_storage::transaction t_;
        std::uint64_t* commits_;
    };

    konsens::persistent_storage store;
    std::unique_ptr<std::uint64_t> commits = std::make_unique<std::uint64_t>(0);

    std::expected<transaction, std::error_code> begin()
    {
        return store.begin().transform(
            [this](konsens::persistent_storage::transaction t) { return transaction{std::move(t), commits.get()}; });
    }
    std::expected<read_only_transaction, std::error_code> begin_read_only() const { return store.begin_read_only(); }
};

static_assert(konsens::storage<counted_storage>);

template <typename C = konsens::node_config>
C config_of(const konsens::node_id pid, const std::uint64_t in_flight)
{
    C c{};
    c.configuration_id = configuration_id;
    c.pid = pid;
    for (konsens::node_id p = 1; p <= node_count; ++p) {
        if (p != pid) {
            c.peers.push_back(p);
        }
    }
    c.leader_priority = 0;
    c.election_tick_timeout = election_ticks;
    c.resend_message_tick_timeout = resend_ticks;
    if constexpr (requires { c.max_in_flight; }) {
        c.max_in_flight = in_flight;
    }
    if constexpr (requires { c.first_start; }) {
        c.first_start = true;
    }
    return c;
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

template <typename N>
bool is_leader(const N& node)
{
    return node.get_sequence_paxos().get_state() == std::pair{konsens::role::leader, konsens::phase::accept};
}

template <typename N>
std::optional<std::size_t> leader_of(const std::vector<N>& nodes)
{
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        if (is_leader(nodes.at(i))) {
            return i;
        }
    }
    return std::nullopt;
}

template <typename N>
std::uint64_t log_len_of(const std::vector<N>& nodes, const std::size_t at)
{
    const auto t = nodes.at(at).get_sequence_paxos().get_internal_storage().begin_read_only();
    if (!t) [[unlikely]] {
        return 0;
    }
    const auto m = t->get_meta();
    return m ? m->log_len : 0;
}

template <typename N>
bool holds_no_open_run(const N& n)
{
    if constexpr (requires { n.get_own_result_indexes(); }) {
        return n.get_own_result_indexes().empty();
    } else {
        return true;
    }
}

template <typename M>
using bench_node = konsens::node<counted_storage, M>;

template <typename M>
struct cluster {
    using result = std::expected<void, std::error_code>;

    std::vector<bench_node<M>> nodes;
    std::vector<std::string> network;
    std::vector<konsens::client_reply> replies;
    std::uint64_t next_call{1};
    std::uint64_t messages{};
    std::vector<std::optional<konsens::transaction>> pending;
    std::vector<std::chrono::nanoseconds> commit_time;

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
        commit_time.resize(nodes.size());
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            auto& n = nodes.at(i);
            if (const auto t = std::exchange(pending.at(i), std::nullopt)) {
                const auto start = std::chrono::steady_clock::now();
                const auto r = n.commit(*t);
                commit_time.at(i) += std::chrono::steady_clock::now() - start;
                if (!r) [[unlikely]] {
                    return std::unexpected(r.error());
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

    result elect()
    {
        for (std::uint64_t r = 0; r < round_limit; ++r) {
            if (leader_of(nodes)) {
                return {};
            }
            if (auto ran = run_round(); !ran) [[unlikely]] {
                return ran;
            }
        }
        return std::unexpected(std::make_error_code(std::errc::timed_out));
    }

    result settle()
    {
        for (std::uint64_t r = 0; r < round_limit; ++r) {
            if (std::ranges::all_of(nodes, [](const auto& n) { return holds_no_open_run(n); })) {
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

    std::expected<konsens::client_reply, std::error_code> send_request(const std::size_t at,
                                                                       const std::uint64_t session,
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

    std::expected<std::vector<konsens::client_reply>, std::error_code>
    send_requests(const std::size_t at, const std::uint64_t session, const std::uint64_t first_request,
                  const std::span<std::vector<std::byte>> payloads)
    {
        const auto first_call = next_call;
        for (auto& payload : payloads) {
            const auto request_number = first_request + (next_call - first_call);
            if (auto r = add(at, nodes.at(at).client_request(next_call++, session, request_number, std::move(payload)));
                !r) [[unlikely]] {
                return std::unexpected(r.error());
            }
        }
        return wait_for_replies(first_call, payloads.size());
    }

    std::expected<konsens::client_reply, std::error_code> send_query(const std::size_t at)
    {
        const auto call = next_call++;
        if (auto r = add(at, nodes.at(at).client_query(call, {})); !r) [[unlikely]] {
            return std::unexpected(r.error());
        }
        auto reply = wait_for_replies(call, 1);
        if (!reply) [[unlikely]] {
            return std::unexpected(reply.error());
        }
        return std::move(reply->front());
    }
};

template <typename M>
std::expected<cluster<M>, failure> start_cluster(const auto& open_node)
{
    cluster<M> c{};
    for (konsens::node_id pid = 1; pid <= node_count; ++pid) {
        auto n = open_node(pid);
        if (!n) [[unlikely]] {
            return std::unexpected(n.error());
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
        if (!std::ranges::all_of(encoded, [&reference](const auto& e) { return e && *e == *reference; })) [[unlikely]] {
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
            entries_of(entries_per_accept_decide, accept_decide_entry_bytes), wire_decided_chain_crc};
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
    return measure_encode(s, konsens::decide{wire_ballot, wire_sequence_number, wire_decided_idx, wire_decided_chain_crc});
}

case_outcome measure_decode_decide(const run_settings& s)
{
    return measure_decode(s, konsens::decide{wire_ballot, wire_sequence_number, wire_decided_idx, wire_decided_chain_crc});
}

std::expected<konsens::persistent_storage, failure> open_log(const std::filesystem::path& directory,
                                                             const std::size_t max_bytes, const uid_t user_id)
{
    std::filesystem::create_directories(directory);
    auto store = konsens::persistent_storage::open(directory, max_bytes, user_id);
    if (!store) [[unlikely]] {
        return std::unexpected(failure{"open a log store", store.error()});
    }
    return std::move(*store);
}

std::expected<std::uint64_t, failure> log_len_in(const konsens::persistent_storage& store)
{
    const auto t = store.begin_read_only();
    if (!t) [[unlikely]] {
        return std::unexpected(failure{"begin a read of the log", t.error()});
    }
    const auto m = t->get_meta();
    if (!m) [[unlikely]] {
        return std::unexpected(failure{"read the meta record", m.error()});
    }
    return m->log_len;
}

std::expected<void, failure> append_in_one_transaction(konsens::persistent_storage& store,
                                                       const std::span<const konsens::entry> entries)
{
    auto t = store.begin();
    if (!t) [[unlikely]] {
        return std::unexpected(failure{"begin a write of the log", t.error()});
    }
    if (auto a = t->append_entries(entries); !a) [[unlikely]] {
        return std::unexpected(failure{"append the entries", a.error()});
    }
    if (auto c = std::move(*t).commit(); !c) [[unlikely]] {
        return std::unexpected(failure{"commit the entries", c.error()});
    }
    return {};
}

std::expected<void, failure> set_the_ballot(konsens::persistent_storage& store)
{
    auto t = store.begin();
    if (!t) [[unlikely]] {
        return std::unexpected(failure{"begin a write of the log", t.error()});
    }
    if (auto p = t->set_promise(wire_ballot); !p) [[unlikely]] {
        return std::unexpected(failure{"set the promise", p.error()});
    }
    if (auto a = t->set_accepted_round(wire_ballot); !a) [[unlikely]] {
        return std::unexpected(failure{"set the accepted round", a.error()});
    }
    if (auto c = std::move(*t).commit(); !c) [[unlikely]] {
        return std::unexpected(failure{"commit the ballot", c.error()});
    }
    return {};
}

case_outcome measure_commit_lmdb(const run_settings& s)
{
    std::vector<double> ns;
    for (std::uint64_t sample = 0; sample < warm_up_samples + s.samples; ++sample) {
        auto store =
            open_log(s.directory / std::format("sample-{}", sample), max_bytes_for(s.ops, s.payload_bytes), s.user_id);
        if (!store) [[unlikely]] {
            return std::unexpected(store.error());
        }
        const auto entries = entries_of(s.ops, s.payload_bytes);
        const std::span<const konsens::entry> all(entries);
        const auto start = std::chrono::steady_clock::now();
        for (std::size_t k = 0; k < all.size(); ++k) {
            if (auto w = append_in_one_transaction(*store, all.subspan(k, 1)); !w) [[unlikely]] {
                return std::unexpected(w.error());
            }
        }
        const auto end = std::chrono::steady_clock::now();
        if (log_len_in(*store) != s.ops) [[unlikely]] {
            return std::unexpected(failure{"compare the log length with the number of commits", check_failed()});
        }
        if (sample >= warm_up_samples) {
            ns.push_back(ns_per_op_of(start, end, s.ops));
        }
    }
    return case_result{ns, std::string{}};
}

case_outcome measure_get_entries_lmdb(const run_settings& s)
{
    auto store = open_log(s.directory / "store", max_bytes_for(1, s.payload_bytes), s.user_id);
    if (!store) [[unlikely]] {
        return std::unexpected(store.error());
    }
    const auto entries = entries_of(1, s.payload_bytes);
    if (auto w = append_in_one_transaction(*store, entries); !w) [[unlikely]] {
        return std::unexpected(w.error());
    }
    const auto& payload = entries.front().payload;
    std::vector<double> ns;
    for (std::uint64_t sample = 0; sample < warm_up_samples + s.samples; ++sample) {
        const auto start = std::chrono::steady_clock::now();
        for (std::uint64_t k = 0; k < s.ops; ++k) {
            const auto t = store->begin_read_only();
            if (!t) [[unlikely]] {
                return std::unexpected(failure{"begin_read_only", t.error()});
            }
            const auto read = t->get_entries(stored_key, stored_key + 1);
            if (!read) [[unlikely]] {
                return std::unexpected(failure{"get_entries", read.error()});
            }
            if (read->size() != 1 || read->front().payload != payload) [[unlikely]] {
                return std::unexpected(failure{"compare the entry with the payload", check_failed()});
            }
        }
        const auto end = std::chrono::steady_clock::now();
        if (sample >= warm_up_samples) {
            ns.push_back(ns_per_op_of(start, end, s.ops));
        }
    }
    return case_result{ns, std::string{}};
}

template <typename M>
auto sqlite_nodes(const run_settings& s, const std::uint64_t entries_per_op, const std::uint64_t in_flight,
                  const auto& machine_of)
{
    const auto max_bytes =
        max_bytes_for((warm_up_samples + s.samples) * s.ops * entries_per_op + setup_entries, s.payload_bytes) +
        state_max_bytes;
    return [max_bytes, &s, in_flight,
            &machine_of](const konsens::node_id pid) -> std::expected<bench_node<M>, failure> {
        const auto directory = s.directory / std::format("node-{}", pid);
        auto log = open_log(directory / "log", max_bytes, s.user_id);
        if (!log) [[unlikely]] {
            return std::unexpected(log.error());
        }
        std::filesystem::create_directories(directory / "node");
        auto own = konsens::node_storage::open(directory / "node", s.user_id);
        if (!own) [[unlikely]] {
            return std::unexpected(failure{"open the node storage", own.error()});
        }
        auto n = bench_node<M>::with(config_of(pid, in_flight), counted_storage{std::move(*log)}, std::move(*own),
                                     machine_of(pid), pid);
        if (!n) [[unlikely]] {
            return std::unexpected(failure{"start a node", n.error()});
        }
        return std::move(*n);
    };
}

struct client_request_call {
    std::uint64_t call;
    std::uint64_t session;
    std::uint64_t request_number;
    std::vector<std::byte> payload;
};

struct client_query_call {
    std::uint64_t call;
};

struct tick_call {};

using inbox_element = std::variant<std::string, client_request_call, client_query_call, tick_call>;

struct timed_element {
    std::chrono::steady_clock::time_point due;
    inbox_element element;
};

struct node_inbox {
    std::mutex mutex;
    std::condition_variable_any ready;
    std::vector<inbox_element> elements;
    std::deque<timed_element> delayed;
};

struct reply_inbox {
    std::mutex mutex;
    std::condition_variable ready;
    std::vector<konsens::client_reply> replies;
    std::vector<std::pair<std::uint64_t, std::chrono::steady_clock::time_point>> released_at;
    std::optional<failure> failed;
};

struct elements_in_flight {
    std::mutex mutex;
    std::condition_variable none_left;
    std::uint64_t count;
};

struct thread_counters {
    std::uint64_t messages;
    std::chrono::nanoseconds commit_time;
    std::uint64_t leader_terms;
};

konsens::node_id destination_of(const konsens::message& m)
{
    return std::visit([](const auto& held) { return held.to; }, m);
}

bool is_reply_of(const konsens::client_reply& reply, const std::uint64_t first_call, const std::uint64_t count)
{
    return reply.client_id >= first_call && reply.client_id - first_call < count;
}

template <typename M>
struct threaded_cluster {
    using result = std::expected<void, failure>;

    std::vector<bench_node<M>> nodes;
    std::uint64_t next_call;
    std::chrono::milliseconds delay{no_delay};
    bool stamps_replies{false};
    std::chrono::milliseconds tick_period{no_delay};
    bool requires_one_leader{true};
    std::optional<std::size_t> leader_at_start = leader_of(nodes);
    std::vector<node_inbox> inboxes = std::vector<node_inbox>(nodes.size());
    std::vector<thread_counters> counters = std::vector<thread_counters>(nodes.size());
    reply_inbox replies{};
    elements_in_flight in_flight{};
    std::vector<std::jthread> threads{};
    std::jthread ticker{};

    void start()
    {
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            threads.emplace_back([this, i](const std::stop_token stop) { serve(stop, i); });
        }
        if (tick_period != no_delay) {
            ticker = std::jthread([this](const std::stop_token stop) { tick_until_stopped(stop); });
        }
    }

    void tick_until_stopped(const std::stop_token stop)
    {
        std::mutex m;
        std::condition_variable_any never;
        std::unique_lock lock(m);
        auto next = std::chrono::steady_clock::now() + tick_period;
        while (!never.wait_until(lock, stop, next, [] { return false; }) && !stop.stop_requested()) {
            for (std::size_t i = 0; i < nodes.size(); ++i) {
                std::vector<inbox_element> calls;
                calls.emplace_back(tick_call{});
                put_calls(i, std::move(calls));
            }
            next += tick_period;
        }
    }

    void serve(const std::stop_token stop, const std::size_t i)
    {
        thread_counters own{};
        try {
            if (auto r = serve_until_stopped(stop, i, own); !r) [[unlikely]] {
                std::cerr << std::format("Node {} failed at the step \"{}\": {}.", i + 1, r.error().step,
                                         r.error().code.message())
                          << '\n';
                report(r.error());
            }
        } catch (const std::exception& e) {
            std::cerr << std::format("Node {} ended with an exception: {}", i + 1, e.what()) << '\n';
            report(failure{"run a node thread", check_failed()});
        }
        counters.at(i) = own;
    }

    void report(const failure& f)
    {
        {
            const std::scoped_lock lock(replies.mutex);
            replies.failed = f;
        }
        replies.ready.notify_all();
    }

    std::vector<inbox_element> take_due(node_inbox& inbox, const std::stop_token stop)
    {
        std::unique_lock lock(inbox.mutex);
        while (true) {
            if (!inbox.ready.wait(lock, stop, [&inbox] { return !inbox.elements.empty() || !inbox.delayed.empty(); })) {
                return {};
            }
            std::vector<inbox_element> taken;
            const auto now_count = std::min(inbox.elements.size(), konsens::max_batch_size);
            const auto first = inbox.elements.begin();
            const auto last = std::next(first, static_cast<std::ptrdiff_t>(now_count));
            taken.assign(std::make_move_iterator(first), std::make_move_iterator(last));
            inbox.elements.erase(first, last);
            const auto now = std::chrono::steady_clock::now();
            while (!inbox.delayed.empty() && inbox.delayed.front().due <= now && taken.size() < konsens::max_batch_size) {
                taken.push_back(std::move(inbox.delayed.front().element));
                inbox.delayed.pop_front();
            }
            if (!taken.empty()) {
                return taken;
            }
            const auto due = inbox.delayed.front().due;
            inbox.ready.wait_until(lock, stop, due, [&inbox] { return !inbox.elements.empty(); });
            if (stop.stop_requested()) {
                return {};
            }
        }
    }

    result serve_until_stopped(const std::stop_token stop, const std::size_t i, thread_counters& own)
    {
        auto& inbox = inboxes.at(i);
        bool leads = is_leader(nodes.at(i));
        while (true) {
            auto taken = take_due(inbox, stop);
            if (taken.empty()) {
                return {};
            }
            if (auto r = handle(i, std::move(taken), own); !r) [[unlikely]] {
                return r;
            }
            const bool leads_now = is_leader(nodes.at(i));
            own.leader_terms += leads_now && !leads ? 1u : 0u;
            leads = leads_now;
        }
    }

    result handle(const std::size_t i, std::vector<inbox_element> taken, thread_counters& own)
    {
        auto& n = nodes.at(i);
        std::vector<konsens::received> batch;
        std::optional<konsens::transaction> t;
        const auto join = [&t](const konsens::transaction& more) {
            if (t) {
                *t += more;
            } else {
                t = more;
            }
        };
        for (auto& element : taken) {
            if (auto* bytes = std::get_if<std::string>(&element)) {
                auto r = konsens::decode(std::move(*bytes));
                if (!r) [[unlikely]] {
                    return std::unexpected(failure{"decode a message", r.error()});
                }
                own.messages += 1;
                batch.push_back(std::move(*r));
            } else if (auto* request = std::get_if<client_request_call>(&element)) {
                const auto called = n.client_request(request->call, request->session, request->request_number,
                                                     std::move(request->payload));
                if (!called) [[unlikely]] {
                    return std::unexpected(failure{"call client_request", called.error()});
                }
                join(*called);
            } else if (const auto* query = std::get_if<client_query_call>(&element)) {
                const auto called = n.client_query(query->call, {});
                if (!called) [[unlikely]] {
                    return std::unexpected(failure{"call client_query", called.error()});
                }
                join(*called);
            } else if (std::holds_alternative<tick_call>(element)) {
                const auto ticked = n.tick();
                if (!ticked) [[unlikely]] {
                    return std::unexpected(failure{"call tick", ticked.error()});
                }
                join(*ticked);
            }
        }
        const auto handled = n.handle_incoming(std::move(batch));
        if (!handled) [[unlikely]] {
            return std::unexpected(failure{"handle the incoming messages", handled.error()});
        }
        join(*handled);
        const auto start = std::chrono::steady_clock::now();
        const auto committed = n.commit(*t);
        own.commit_time += std::chrono::steady_clock::now() - start;
        if (!committed) [[unlikely]] {
            return std::unexpected(failure{"commit", committed.error()});
        }
        std::vector<std::vector<inbox_element>> outgoing(nodes.size());
        std::uint64_t sent = 0;
        for (const auto& m : n.take_outgoing_messages()) {
            auto bytes = konsens::encode(m);
            if (!bytes) [[unlikely]] {
                return std::unexpected(failure{"encode a message", bytes.error()});
            }
            outgoing.at(destination_of(m) - 1).emplace_back(std::move(*bytes));
            sent += 1;
        }
        add_in_flight(sent);
        for (std::size_t target = 0; target < outgoing.size(); ++target) {
            push(target, std::move(outgoing.at(target)), delay);
        }
        if (auto released = n.take_client_replies(); !released.empty()) {
            {
                const auto now = stamps_replies ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
                const std::scoped_lock lock(replies.mutex);
                if (stamps_replies) {
                    for (const auto& r : released) {
                        replies.released_at.emplace_back(r.client_id, now);
                    }
                }
                std::ranges::move(released, std::back_inserter(replies.replies));
            }
            replies.ready.notify_all();
        }
        remove_in_flight(taken.size());
        return {};
    }

    void add_in_flight(const std::uint64_t count)
    {
        const std::scoped_lock lock(in_flight.mutex);
        in_flight.count += count;
    }

    void remove_in_flight(const std::uint64_t count)
    {
        bool none_left = false;
        {
            const std::scoped_lock lock(in_flight.mutex);
            in_flight.count -= count;
            none_left = in_flight.count == 0;
        }
        if (none_left) {
            in_flight.none_left.notify_all();
        }
    }

    void push(const std::size_t target, std::vector<inbox_element> elements, const std::chrono::milliseconds after)
    {
        if (elements.empty()) {
            return;
        }
        auto& inbox = inboxes.at(target);
        {
            const std::scoped_lock lock(inbox.mutex);
            if (after == no_delay) {
                std::ranges::move(elements, std::back_inserter(inbox.elements));
            } else {
                const auto due = std::chrono::steady_clock::now() + after;
                for (auto& e : elements) {
                    inbox.delayed.push_back({due, std::move(e)});
                }
            }
        }
        inbox.ready.notify_one();
    }

    void put_calls(const std::size_t at, std::vector<inbox_element> calls)
    {
        add_in_flight(calls.size());
        push(at, std::move(calls), no_delay);
    }

    std::expected<std::vector<konsens::client_reply>, std::error_code> wait_for_replies(const std::uint64_t first_call,
                                                                                        const std::uint64_t count)
    {
        std::unique_lock lock(replies.mutex);
        const auto arrived = [this, first_call, count] {
            return replies.failed || static_cast<std::uint64_t>(
                                         std::ranges::count_if(replies.replies, [first_call, count](const auto& r) {
                                             return is_reply_of(r, first_call, count);
                                         })) == count;
        };
        if (!replies.ready.wait_for(lock, reply_timeout, arrived)) [[unlikely]] {
            return std::unexpected(std::make_error_code(std::errc::timed_out));
        }
        if (replies.failed) [[unlikely]] {
            return std::unexpected(replies.failed->code);
        }
        std::vector<konsens::client_reply> taken;
        std::erase_if(replies.replies, [&taken, first_call, count](auto& reply) {
            if (!is_reply_of(reply, first_call, count)) {
                return false;
            }
            taken.push_back(std::move(reply));
            return true;
        });
        return taken;
    }

    std::expected<konsens::client_reply, std::error_code> send_request(const std::size_t at,
                                                                       const std::uint64_t session,
                                                                       const std::uint64_t request_number,
                                                                       std::vector<std::byte> payload)
    {
        const auto call = next_call++;
        std::vector<inbox_element> calls;
        calls.emplace_back(client_request_call{call, session, request_number, std::move(payload)});
        put_calls(at, std::move(calls));
        auto reply = wait_for_replies(call, 1);
        if (!reply) [[unlikely]] {
            return std::unexpected(reply.error());
        }
        return std::move(reply->front());
    }

    std::expected<std::vector<konsens::client_reply>, std::error_code>
    send_requests(const std::size_t at, const std::uint64_t session, const std::uint64_t first_request,
                  const std::span<std::vector<std::byte>> payloads)
    {
        const auto first_call = next_call;
        std::vector<inbox_element> calls;
        for (auto& payload : payloads) {
            calls.emplace_back(
                client_request_call{next_call, session, first_request + (next_call - first_call), std::move(payload)});
            next_call += 1;
        }
        put_calls(at, std::move(calls));
        return wait_for_replies(first_call, payloads.size());
    }

    std::expected<konsens::client_reply, std::error_code> send_query(const std::size_t at)
    {
        const auto call = next_call++;
        std::vector<inbox_element> calls;
        calls.emplace_back(client_query_call{call});
        put_calls(at, std::move(calls));
        auto reply = wait_for_replies(call, 1);
        if (!reply) [[unlikely]] {
            return std::unexpected(reply.error());
        }
        return std::move(reply->front());
    }

    result finish()
    {
        ticker = {};
        {
            std::unique_lock lock(in_flight.mutex);
            if (!in_flight.none_left.wait_for(lock, reply_timeout, [this] { return in_flight.count == 0; }))
                [[unlikely]] {
                return std::unexpected(
                    failure{"wait until no message is in flight", std::make_error_code(std::errc::timed_out)});
            }
        }
        threads.clear();
        if (replies.failed) [[unlikely]] {
            return std::unexpected(*replies.failed);
        }
        if (requires_one_leader && leader_of(nodes) != leader_at_start) [[unlikely]] {
            return std::unexpected(failure{"keep one leader while the threads run", check_failed()});
        }
        const auto& first = nodes.front().get_sequence_paxos();
        if (!std::ranges::all_of(nodes, [&first, this](const auto& n) {
                return n.get_sequence_paxos().get_decided_idx() == first.get_decided_idx() &&
                       log_len_of(nodes, static_cast<std::size_t>(&n - nodes.data())) == log_len_of(nodes, 0);
            })) [[unlikely]] {
            return std::unexpected(
                failure{"compare the log length and the decided index of the nodes", check_failed()});
        }
        return {};
    }
};

struct traffic {
    std::uint64_t messages;
    std::uint64_t log_len;
    std::vector<std::uint64_t> commits;
    std::optional<std::size_t> leader;
    std::vector<std::chrono::nanoseconds> commit_time;
};

template <typename M>
std::vector<std::uint64_t> commits_of(const std::vector<bench_node<M>>& nodes)
{
    std::vector<std::uint64_t> calls;
    for (const auto& n : nodes) {
        calls.push_back(*n.get_sequence_paxos().get_internal_storage().commits);
    }
    return calls;
}

template <typename M>
traffic traffic_of(const cluster<M>& c, const std::size_t at)
{
    auto commit_time = c.commit_time;
    commit_time.resize(c.nodes.size());
    return {c.messages, log_len_of(c.nodes, at), commits_of(c.nodes), leader_of(c.nodes), commit_time};
}

template <typename M>
traffic traffic_of(const threaded_cluster<M>& c, const std::size_t at)
{
    std::uint64_t messages = 0;
    std::vector<std::chrono::nanoseconds> commit_time;
    for (const auto& counted : c.counters) {
        messages += counted.messages;
        commit_time.push_back(counted.commit_time);
    }
    return {messages, log_len_of(c.nodes, at), commits_of(c.nodes), leader_of(c.nodes), commit_time};
}

std::optional<std::string> commit_times_of(const traffic& first, const traffic& last, const run_settings& s)
{
    if (!first.leader || first.leader != last.leader || first.commit_time.size() != last.commit_time.size())
        [[unlikely]] {
        return std::nullopt;
    }
    std::chrono::nanoseconds leader{};
    std::chrono::nanoseconds slowest_follower{};
    for (std::size_t i = 0; i < last.commit_time.size(); ++i) {
        const auto spent = last.commit_time.at(i) - first.commit_time.at(i);
        if (i == *first.leader) {
            leader = spent;
        } else {
            slowest_follower = std::max(slowest_follower, spent);
        }
    }
    return std::format("leader_ns_per_op={:.0f} follower_ns_per_op={:.0f}", per_op(leader, s),
                       per_op(slowest_follower, s));
}

std::string commits_per_node_of(const traffic& first, const traffic& last, const run_settings& s)
{
    std::string text;
    for (std::size_t i = 0; i < last.commits.size(); ++i) {
        text += std::format("{}{:.2f}", i == 0 ? "" : "/", per_op(last.commits.at(i) - first.commits.at(i), s));
    }
    return text;
}

std::expected<std::string, failure> details_of(const traffic& first, const traffic& last, const run_settings& s)
{
    const auto commit_times = commit_times_of(first, last, s);
    if (!commit_times || first.commits.size() != last.commits.size()) [[unlikely]] {
        return std::unexpected(failure{"keep one leader through the samples", check_failed()});
    }
    const auto commits = std::ranges::fold_left(last.commits, std::uint64_t{0}, std::plus{}) -
                         std::ranges::fold_left(first.commits, std::uint64_t{0}, std::plus{});
    return std::format("messages_per_op={:.2f} commits_per_op={:.2f} commits_per_node_per_op={} {} "
                       "log_len_first={} log_len_last={}",
                       per_op(last.messages - first.messages, s), per_op(commits, s),
                       commits_per_node_of(first, last, s), *commit_times, first.log_len, last.log_len);
}

case_outcome outcome_of(std::vector<double> ns, const traffic& first, const traffic& last, const run_settings& s)
{
    auto details = details_of(first, last, s);
    if (!details) [[unlikely]] {
        return std::unexpected(details.error());
    }
    return case_result{std::move(ns), std::move(*details)};
}

template <typename M>
struct client_cluster {
    cluster<M> c;
    std::size_t at;
    std::uint64_t session;
};

template <typename M>
std::expected<client_cluster<M>, failure> start_client_cluster(const auto& open_node, const std::uint64_t writes_before,
                                                               const bool at_the_leader)
{
    auto c = start_cluster<M>(open_node);
    if (!c) [[unlikely]] {
        return std::unexpected(c.error());
    }
    const auto leader = *leader_of(c->nodes);
    const auto at = at_the_leader ? leader : follower_of(leader);
    const auto session = c->register_client(at);
    if (!session) [[unlikely]] {
        return std::unexpected(failure{"register the client", session.error()});
    }
    for (std::uint64_t k = 1; k <= writes_before; ++k) {
        if (const auto reply = c->send_request(at, *session, k, {}); !reply) [[unlikely]] {
            return std::unexpected(failure{"write before the samples", reply.error()});
        }
    }
    if (auto settled = c->settle(); !settled) [[unlikely]] {
        return std::unexpected(failure{"close the open runs before the samples", settled.error()});
    }
    return client_cluster<M>{std::move(*c), at, *session};
}

using timed_samples = std::expected<std::vector<double>, failure>;

constexpr auto time_writes = [](const run_settings& s, auto& c, const std::size_t at,
                                const std::uint64_t session) -> timed_samples {
    std::uint64_t request_number = 0;
    std::vector<double> ns;
    for (std::uint64_t sample = 0; sample < warm_up_samples + s.samples; ++sample) {
        std::vector<std::vector<std::byte>> payloads(s.ops, std::vector<std::byte>(s.payload_bytes, payload_byte));
        const auto start = std::chrono::steady_clock::now();
        for (auto& payload : payloads) {
            request_number += 1;
            const auto reply = c.send_request(at, session, request_number, std::move(payload));
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
    return ns;
};

constexpr auto time_reads = [](const run_settings& s, auto& c, const std::size_t at,
                               [[maybe_unused]] const std::uint64_t session) -> timed_samples {
    std::vector<double> ns;
    for (std::uint64_t sample = 0; sample < warm_up_samples + s.samples; ++sample) {
        const auto start = std::chrono::steady_clock::now();
        for (std::uint64_t k = 0; k < s.ops; ++k) {
            const auto reply = c.send_query(at);
            if (!reply) [[unlikely]] {
                return std::unexpected(failure{"read one query", reply.error()});
            }
            if (counter_value_of(*reply) != writes_before_read) [[unlikely]] {
                return std::unexpected(failure{"compare the counter reply with the number of writes", check_failed()});
            }
        }
        const auto end = std::chrono::steady_clock::now();
        if (sample >= warm_up_samples) {
            ns.push_back(ns_per_op_of(start, end, s.ops));
        }
    }
    return ns;
};

timed_samples time_batches_of(const std::size_t size, const run_settings& s, auto& c, const std::size_t at,
                              const std::uint64_t session)
{
    std::uint64_t request_number = 0;
    std::vector<double> ns;
    for (std::uint64_t sample = 0; sample < warm_up_samples + s.samples; ++sample) {
        std::vector<std::vector<std::byte>> payloads(s.ops, std::vector<std::byte>(s.payload_bytes, payload_byte));
        const auto start = std::chrono::steady_clock::now();
        for (std::size_t b = 0; b < payloads.size(); b += size) {
            const auto first_call = c.next_call;
            const auto first_request = request_number + 1;
            const auto replies = c.send_requests(at, session, first_request, std::span(payloads).subspan(b, size));
            if (!replies) [[unlikely]] {
                return std::unexpected(failure{"write one batch", replies.error()});
            }
            request_number += size;
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
    return ns;
}

constexpr auto time_write_batches = [](const run_settings& s, auto& c, const std::size_t at,
                                       const std::uint64_t session) -> timed_samples {
    return time_batches_of(batch_size, s, c, at, session);
};

constexpr auto time_write_batches_of_the_parameter = [](const run_settings& s, auto& c, const std::size_t at,
                                                        const std::uint64_t session) -> timed_samples {
    return time_batches_of(static_cast<std::size_t>(s.parameter), s, c, at, session);
};

const auto counter_of_node = [](const run_count& runs) {
    return [runs](const konsens::node_id) { return counter{runs}; };
};

case_outcome measure_cluster(const run_settings& s, const std::uint64_t entries_per_op,
                             const std::uint64_t writes_before, const auto& time)
{
    const auto machine_of = counter_of_node(new_run_count());
    auto p = start_client_cluster<counter>(sqlite_nodes<counter>(s, entries_per_op, max_in_flight, machine_of),
                                           writes_before, false);
    if (!p) [[unlikely]] {
        return std::unexpected(p.error());
    }
    const auto first = traffic_of(p->c, p->at);
    auto ns = time(s, p->c, p->at, p->session);
    if (!ns) [[unlikely]] {
        return std::unexpected(ns.error());
    }
    return outcome_of(std::move(*ns), first, traffic_of(p->c, p->at), s);
}

case_outcome measure_threaded_cluster(const run_settings& s, const std::uint64_t entries_per_op,
                                      const std::uint64_t writes_before, const auto& time)
{
    const auto machine_of = counter_of_node(new_run_count());
    auto p = start_client_cluster<counter>(sqlite_nodes<counter>(s, entries_per_op, max_in_flight, machine_of),
                                           writes_before, false);
    if (!p) [[unlikely]] {
        return std::unexpected(p.error());
    }
    threaded_cluster<counter> c{std::move(p->c.nodes), p->c.next_call};
    const auto first = traffic_of(c, p->at);
    c.start();
    auto ns = time(s, c, p->at, p->session);
    if (!ns) [[unlikely]] {
        return std::unexpected(ns.error());
    }
    if (auto f = c.finish(); !f) [[unlikely]] {
        return std::unexpected(f.error());
    }
    return outcome_of(std::move(*ns), first, traffic_of(c, p->at), s);
}

case_outcome measure_write_lmdb(const run_settings& s)
{
    return measure_cluster(s, 1, 0, time_writes);
}

case_outcome measure_read_lmdb(const run_settings& s)
{
    return measure_cluster(s, 0, writes_before_read, time_reads);
}

case_outcome measure_write_batch_lmdb(const run_settings& s)
{
    return measure_cluster(s, 1, 0, time_write_batches);
}

case_outcome measure_write_batch_size_lmdb(const run_settings& s)
{
    return measure_cluster(s, 1, 0, time_write_batches_of_the_parameter);
}

case_outcome measure_write_lmdb_threads(const run_settings& s)
{
    return measure_threaded_cluster(s, 1, 0, time_writes);
}

case_outcome measure_read_lmdb_threads(const run_settings& s)
{
    return measure_threaded_cluster(s, 0, writes_before_read, time_reads);
}

case_outcome measure_write_batch_lmdb_threads(const run_settings& s)
{
    return measure_threaded_cluster(s, 1, 0, time_write_batches);
}

std::chrono::nanoseconds processor_time()
{
    rusage usage{};
    getrusage(RUSAGE_SELF, &usage);
    const auto of = [](const timeval& t) {
        return std::chrono::seconds{t.tv_sec} + std::chrono::microseconds{t.tv_usec};
    };
    return std::chrono::duration_cast<std::chrono::nanoseconds>(of(usage.ru_utime) + of(usage.ru_stime));
}

template <typename M>
std::size_t reply_total_of(threaded_cluster<M>& c)
{
    const std::scoped_lock lock(c.replies.mutex);
    return c.replies.replies.size();
}

template <typename M>
std::uint64_t leader_changes_of(const threaded_cluster<M>& c)
{
    return std::ranges::fold_left(c.counters | std::views::transform(&thread_counters::leader_terms),
                                  std::uint64_t{}, std::plus{});
}

struct open_loop_record {
    std::vector<double> ns_per_op;
    std::vector<double> latency_ns;
    double processor_ns_per_op;
    std::uint64_t runs;
    std::uint64_t requests;
};

template <typename M>
std::expected<open_loop_record, failure> run_the_open_loop(const run_settings& s, threaded_cluster<M>& c,
                                                           const std::size_t at, const std::uint64_t session,
                                                           const M& machine, const run_count& runs,
                                                           const std::uint64_t rate_per_s)
{
    using clock = std::chrono::steady_clock;
    const auto total = (warm_up_samples + s.samples) * s.ops;
    const auto measured_from = warm_up_samples * s.ops;
    const auto interval = std::chrono::nanoseconds{std::chrono::seconds{1}} / static_cast<std::int64_t>(rate_per_s);
    const auto first_call = c.next_call;
    const auto runs_before = runs->load();
    const auto start = clock::now();
    const auto arrival = [start, interval](const std::uint64_t k) { return start + interval * static_cast<std::int64_t>(k); };
    auto processor_start = processor_time();
    for (std::uint64_t k = 0; k < total; ++k) {
        if (k == measured_from) {
            processor_start = processor_time();
        }
        std::this_thread::sleep_until(arrival(k));
        std::vector<inbox_element> calls;
        calls.emplace_back(client_request_call{c.next_call++, session, k + 1, {}});
        c.put_calls(at, std::move(calls));
    }
    const auto replies = c.wait_for_replies(first_call, total);
    if (!replies) [[unlikely]] {
        c.ticker = {};
        c.threads.clear();
        std::cerr << std::format("The open loop has {} of {} replies and {} leader changes.\n",
                                 reply_total_of(c), total, leader_changes_of(c));
        return std::unexpected(failure{"wait for the replies of the open loop", replies.error()});
    }
    const auto processor_end = processor_time();
    for (const auto& reply : *replies) {
        if (reply_count_of(machine, reply) != reply.client_id - first_call + 1) [[unlikely]] {
            return std::unexpected(failure{"compare each reply with its request number", check_failed()});
        }
    }
    std::vector<clock::time_point> replied(total);
    {
        const std::scoped_lock lock(c.replies.mutex);
        for (const auto& [call, when] : std::exchange(c.replies.released_at, {})) {
            if (call >= first_call && call - first_call < total) {
                replied.at(call - first_call) = when;
            }
        }
    }
    open_loop_record record{};
    for (std::uint64_t sample = warm_up_samples; sample < warm_up_samples + s.samples; ++sample) {
        const auto first = sample * s.ops;
        const auto last = std::ranges::max(std::span(replied).subspan(first, s.ops));
        record.ns_per_op.push_back(ns_per_op_of(arrival(first), last, s.ops));
    }
    for (std::uint64_t k = measured_from; k < total; ++k) {
        record.latency_ns.push_back(std::chrono::duration<double, std::nano>(replied.at(k) - arrival(k)).count());
    }
    record.processor_ns_per_op = std::chrono::duration<double, std::nano>(processor_end - processor_start).count() /
                                 static_cast<double>(total - measured_from);
    record.runs = runs->load() - runs_before;
    record.requests = total;
    return record;
}

template <typename M>
case_outcome measure_open_loop(const run_settings& s, const std::uint64_t in_flight,
                               const std::chrono::milliseconds delay, const std::uint64_t rate_per_s,
                               const auto& machine_of, const run_count& runs)
{
    auto p = start_client_cluster<M>(sqlite_nodes<M>(s, 1, in_flight, machine_of), 0, true);
    if (!p) [[unlikely]] {
        return std::unexpected(p.error());
    }
    const auto machine = machine_of(leader_pid);
    threaded_cluster<M> c{std::move(p->c.nodes), p->c.next_call, delay, true, pipeline_tick_period, false};
    c.start();
    auto r = run_the_open_loop(s, c, p->at, p->session, machine, runs, rate_per_s);
    if (!r) [[unlikely]] {
        return std::unexpected(r.error());
    }
    if (auto f = c.finish(); !f) [[unlikely]] {
        return std::unexpected(f.error());
    }
    const auto requests = static_cast<double>(r->requests);
    const auto runs_per_request = static_cast<double>(r->runs) / requests;
    const auto again = (static_cast<double>(r->runs) - static_cast<double>(node_count) * requests) / requests * per_mille;
    return case_result{
        std::move(r->ns_per_op),
        std::format("rate_per_s={} max_in_flight={} delay_ms={} tick_ms={} latency_p50_ns={:.0f} latency_p99_ns={:.0f} "
                    "processor_ns_per_op={:.0f} runs_per_request={:.3f} runs_again_per_1000={:.2f} leader_changes={}",
                    rate_per_s, in_flight, delay.count(), pipeline_tick_period.count(),
                    quantile_of(r->latency_ns, percent_p50),
                    quantile_of(r->latency_ns, percent_p99), r->processor_ns_per_op, runs_per_request, again,
                    leader_changes_of(c))};
}

template <typename M>
auto machine_with_runs(const run_count& runs)
{
    return [runs](const konsens::node_id) { return M{runs}; };
}

template <typename M>
case_outcome measure_pipeline(const run_settings& s, const std::chrono::milliseconds delay,
                              const std::uint64_t rate_per_s)
{
    const auto runs = new_run_count();
    return measure_open_loop<M>(s, s.parameter, delay, rate_per_s, machine_with_runs<M>(runs), runs);
}

case_outcome measure_pipeline_put_0ms_10000ps(const run_settings& s)
{
    return measure_pipeline<counter>(s, no_delay, base_rate_per_s);
}

case_outcome measure_pipeline_put_0ms_20000ps(const run_settings& s)
{
    return measure_pipeline<counter>(s, no_delay, double_rate_per_s);
}

case_outcome measure_pipeline_put_20ms_10000ps(const run_settings& s)
{
    return measure_pipeline<counter>(s, long_delay, base_rate_per_s);
}

case_outcome measure_pipeline_put_20ms_20000ps(const run_settings& s)
{
    return measure_pipeline<counter>(s, long_delay, double_rate_per_s);
}

case_outcome measure_pipeline_loop_0ms_10000ps(const run_settings& s)
{
    return measure_pipeline<looping>(s, no_delay, base_rate_per_s);
}

case_outcome measure_pipeline_loop_0ms_20000ps(const run_settings& s)
{
    return measure_pipeline<looping>(s, no_delay, double_rate_per_s);
}

case_outcome measure_pipeline_loop_20ms_10000ps(const run_settings& s)
{
    return measure_pipeline<looping>(s, long_delay, base_rate_per_s);
}

case_outcome measure_pipeline_loop_20ms_20000ps(const run_settings& s)
{
    return measure_pipeline<looping>(s, long_delay, double_rate_per_s);
}

case_outcome measure_execute_verify_int(const run_settings& s)
{
    const auto runs = new_run_count();
    return measure_open_loop<counter>(s, s.parameter, no_delay, base_rate_per_s, machine_with_runs<counter>(runs),
                                      runs);
}

case_outcome measure_execute_verify_float(const run_settings& s)
{
    const auto runs = new_run_count();
    const auto machine_of = [runs](const konsens::node_id pid) {
        floating f{};
        f.runs = runs;
        f.error = float_error_of_node.at(static_cast<std::size_t>(pid - 1));
        return f;
    };
    return measure_open_loop<floating>(s, s.parameter, no_delay, base_rate_per_s, machine_of, runs);
}

bool values_read_back(const std::span<const konsens::mutation> mutations, const auto& view)
{
    for (auto m = mutations.begin(); m != mutations.end(); ++m) {
        if (std::any_of(std::next(m), mutations.end(), [&m](const konsens::mutation& later) { return later.key == m->key; })) {
            continue;
        }
        const auto found = view.find(m->key);
        if (!found) [[unlikely]] {
            throw konsens::read_error(found.error(), "konsens_bench: the state storage failed to read");
        }
        if (found->has_value() != m->value.has_value() ||
            (*found && !std::ranges::equal(**found, *m->value))) [[unlikely]] {
            return false;
        }
    }
    return true;
}

struct key_value_machine {
    static constexpr std::array key{std::byte{'v'}};

    std::size_t value_bytes;

    konsens::applied apply(std::span<const std::byte> payload, const konsens::state_view auto&) const
    {
        return {{}, {{{key.begin(), key.end()}, std::vector<std::byte>(payload.begin(), payload.end())}}};
    }

    std::vector<std::byte> query(std::span<const std::byte>, const konsens::state_view auto& view) const
    {
        const auto found = view.find(key);
        return found ? std::vector<std::byte>{found->begin(), found->end()} : std::vector<std::byte>{};
    }
};

std::expected<void, failure> check_the_copy_after_a_step(const run_settings& s)
{
    const auto machine_of = [&s](const konsens::node_id) { return key_value_machine{s.payload_bytes}; };
    auto p = start_client_cluster<key_value_machine>(sqlite_nodes<key_value_machine>(s, 1, max_in_flight, machine_of),
                                                     0, false);
    if (!p) [[unlikely]] {
        return std::unexpected(p.error());
    }
    const std::vector<std::byte> payload(s.payload_bytes, payload_byte);
    if (const auto reply = p->c.send_request(p->at, p->session, 1, payload); !reply || !reply->response) [[unlikely]] {
        return std::unexpected(failure{"write the value in a step", check_failed()});
    }
    if (auto settled = p->c.settle(); !settled) [[unlikely]] {
        return std::unexpected(failure{"apply the step at each node", settled.error()});
    }
    const std::vector<konsens::mutation> written{{{key_value_machine::key.begin(), key_value_machine::key.end()}, payload}};
    std::vector<std::byte> other = payload;
    other.front() = ~other.front();
    const std::vector<konsens::mutation> changed{{{key_value_machine::key.begin(), key_value_machine::key.end()}, other}};
    for (const auto& n : p->c.nodes) {
        const auto t = n.get_sequence_paxos().get_internal_storage().begin_read_only();
        if (!t) [[unlikely]] {
            return std::unexpected(failure{"begin a read of the state", t.error()});
        }
        if (!values_read_back(written, *t) || values_read_back(changed, *t)) [[unlikely]] {
            return std::unexpected(failure{"compare the copy of the read back with the state after a step",
                                           check_failed()});
        }
    }
    return {};
}

case_outcome measure_read_back(const run_settings& s)
{
    if (auto checked = check_the_copy_after_a_step(s); !checked) [[unlikely]] {
        return std::unexpected(checked.error());
    }
    auto state = open_log(s.directory / "state-alone", state_max_bytes, s.user_id);
    if (!state) [[unlikely]] {
        return std::unexpected(state.error());
    }
    auto w = state->begin();
    if (!w) [[unlikely]] {
        return std::unexpected(failure{"begin a write of the state", w.error()});
    }
    const std::vector<konsens::mutation> mutations{
        {{key_value_machine::key.begin(), key_value_machine::key.end()},
         std::vector<std::byte>(s.payload_bytes, payload_byte)}};
    for (const auto& m : mutations) {
        if (auto put = w->insert_or_assign(m.key, *m.value); !put) [[unlikely]] {
            return std::unexpected(failure{"write the mutation", put.error()});
        }
    }
    std::vector<double> ns;
    std::uint64_t agreed = 0;
    for (std::uint64_t sample = 0; sample < warm_up_samples + s.samples; ++sample) {
        const auto start = std::chrono::steady_clock::now();
        for (std::uint64_t k = 0; k < s.ops; ++k) {
            agreed += values_read_back(mutations, *w) ? std::uint64_t{1} : std::uint64_t{0};
        }
        const auto end = std::chrono::steady_clock::now();
        if (sample >= warm_up_samples) {
            ns.push_back(ns_per_op_of(start, end, s.ops));
        }
    }
    if (agreed != (warm_up_samples + s.samples) * s.ops) [[unlikely]] {
        return std::unexpected(failure{"read back each written value", check_failed()});
    }
    return case_result{ns, std::format("mutations_per_op={}", mutations.size())};
}

template <typename S>
case_outcome measure_scan_the_whole_log_on([[maybe_unused]] const run_settings& s)
{
    if constexpr (has_scan_the_whole_log<S>) {
        auto store = open_log(s.directory / "log", max_bytes_for(s.parameter, scan_entry_bytes), s.user_id);
        if (!store) [[unlikely]] {
            return std::unexpected(store.error());
        }
        if (auto b = set_the_ballot(*store); !b) [[unlikely]] {
            return std::unexpected(b.error());
        }
        for (std::uint64_t written = 0; written < s.parameter; written += scan_entries_per_transaction) {
            const auto entries = entries_of(std::min(scan_entries_per_transaction, s.parameter - written), scan_entry_bytes);
            if (auto w = append_in_one_transaction(*store, entries); !w) [[unlikely]] {
                return std::unexpected(w.error());
            }
        }
        konsens::sequence_paxos_config config{1, leader_pid, {follower_pid, follower_pid + 1}};
        if constexpr (requires { config.start; }) {
            config.start = konsens::start_kind::clean;
        }
        auto sp = konsens::sequence_paxos<S>::with(config, std::move(*store), leader_pid);
        if (!sp) [[unlikely]] {
            return std::unexpected(failure{"start sequence_paxos on the log", sp.error()});
        }
        if (!sp->votes()) [[unlikely]] {
            return std::unexpected(failure{"vote before the scan", check_failed()});
        }
        std::vector<double> ns;
        for (std::uint64_t sample = 0; sample < warm_up_samples + s.samples; ++sample) {
            const auto start = std::chrono::steady_clock::now();
            for (std::uint64_t k = 0; k < s.ops; ++k) {
                if (auto r = sp->scan_the_whole_log(); !r) [[unlikely]] {
                    return std::unexpected(failure{"scan the whole log", r.error()});
                }
            }
            const auto end = std::chrono::steady_clock::now();
            if (!sp->votes()) [[unlikely]] {
                return std::unexpected(failure{"keep the vote after the scan", check_failed()});
            }
            if (sample >= warm_up_samples) {
                ns.push_back(ns_per_op_of(start, end, s.ops));
            }
        }
        return case_result{ns, std::format("log_len={} entry_bytes={}", s.parameter, scan_entry_bytes)};
    } else {
        return std::unexpected(failure{"find scan_the_whole_log at this commit", check_failed()});
    }
}

case_outcome measure_scan_the_whole_log(const run_settings& s)
{
    return measure_scan_the_whole_log_on<konsens::persistent_storage>(s);
}

using measure_function = case_outcome(const run_settings&);

struct parameter_ops {
    std::uint64_t parameter;
    std::uint64_t ops;
};

enum class argument_kind { payload_bytes, parameter };

struct bench_case {
    std::string_view name;
    measure_function* measure;
    std::span<const parameter_ops> ops_by_argument;
    argument_kind argument;
    bool needs_directory;
    bool exists_at_this_commit;
    bool runs_in_the_test_of_each_case{true};
};

constexpr std::array<parameter_ops, 3> crc32c_ops{{{64, 3400000}, {1024, 300000}, {65536, 3000}}};
constexpr std::array<parameter_ops, 1> encode_accept_decide_ops{{{0, 200}}};
constexpr std::array<parameter_ops, 1> decode_accept_decide_ops{{{0, 160}}};
constexpr std::array<parameter_ops, 1> encode_decide_ops{{{0, 150000}}};
constexpr std::array<parameter_ops, 1> decode_decide_ops{{{0, 40000}}};
constexpr std::array<parameter_ops, 3> commit_lmdb_ops{{{64, 30}, {1024, 24}, {65536, 16}}};
constexpr std::array<parameter_ops, 3> write_lmdb_ops{{{64, 5}, {1024, 5}, {65536, 3}}};
constexpr std::array<parameter_ops, 1> read_lmdb_ops{{{0, 1700}}};
constexpr std::array<parameter_ops, 3> write_batch_lmdb_ops{{{64, 64}, {1024, 64}, {65536, 64}}};
constexpr std::array<parameter_ops, 3> write_batch_size_lmdb_ops{{{1, 4}, {16, 32}, {256, 256}}};
constexpr std::array<parameter_ops, 3> get_entries_lmdb_ops{{{64, 75000}, {1024, 55000}, {65536, 2000}}};
constexpr std::array<parameter_ops, 3> write_lmdb_threads_ops{{{64, 5}, {1024, 5}, {65536, 3}}};
constexpr std::array<parameter_ops, 1> read_lmdb_threads_ops{{{0, 115}}};
constexpr std::array<parameter_ops, 3> write_batch_lmdb_threads_ops{{{64, 192}, {1024, 256}, {65536, 64}}};
constexpr std::array<parameter_ops, 4> pipeline_10000_ops{{{1, 1000}, {4, 1000}, {16, 1000}, {64, 1000}}};
constexpr std::array<parameter_ops, 4> pipeline_20000_ops{{{1, 2000}, {4, 2000}, {16, 2000}, {64, 2000}}};
constexpr std::array<parameter_ops, 4> execute_verify_ops_of_the_branch{{{1, 1000}, {4, 1000}, {16, 1000}, {64, 1000}}};
constexpr std::array<parameter_ops, 1> execute_verify_ops_without{{{compared_max_in_flight, 1000}}};
constexpr std::span<const parameter_ops> execute_verify_ops =
    branch_has_execute_verify ? std::span<const parameter_ops>(execute_verify_ops_of_the_branch)
                              : std::span<const parameter_ops>(execute_verify_ops_without);
constexpr std::array<parameter_ops, 3> read_back_ops{{{64, 100000}, {1024, 100000}, {65536, 20000}}};
constexpr std::array<parameter_ops, 3> scan_ops{{{1000, 20}, {10000, 2}, {100000, 1}}};

constexpr bool scan_exists = has_scan_the_whole_log<konsens::persistent_storage>;

constexpr auto cases = std::to_array<bench_case>({
    {"crc32c", measure_crc32c, crc32c_ops, argument_kind::payload_bytes, false, true},
    {"encode_accept_decide", measure_encode_accept_decide, encode_accept_decide_ops, argument_kind::payload_bytes,
     false, true},
    {"decode_accept_decide", measure_decode_accept_decide, decode_accept_decide_ops, argument_kind::payload_bytes,
     false, true},
    {"encode_decide", measure_encode_decide, encode_decide_ops, argument_kind::payload_bytes, false, true},
    {"decode_decide", measure_decode_decide, decode_decide_ops, argument_kind::payload_bytes, false, true},
    {"commit_lmdb", measure_commit_lmdb, commit_lmdb_ops, argument_kind::payload_bytes, true, true},
    {"write_lmdb", measure_write_lmdb, write_lmdb_ops, argument_kind::payload_bytes, true, true},
    {"read_lmdb", measure_read_lmdb, read_lmdb_ops, argument_kind::payload_bytes, true, true},
    {"write_batch_lmdb", measure_write_batch_lmdb, write_batch_lmdb_ops, argument_kind::payload_bytes, true, true},
    {"write_batch_size_lmdb", measure_write_batch_size_lmdb, write_batch_size_lmdb_ops, argument_kind::parameter, true,
     true},
    {"get_entries_lmdb", measure_get_entries_lmdb, get_entries_lmdb_ops, argument_kind::payload_bytes, true, true},
    {"write_lmdb_threads", measure_write_lmdb_threads, write_lmdb_threads_ops, argument_kind::payload_bytes, true,
     true},
    {"read_lmdb_threads", measure_read_lmdb_threads, read_lmdb_threads_ops, argument_kind::payload_bytes, true, true},
    {"write_batch_lmdb_threads", measure_write_batch_lmdb_threads, write_batch_lmdb_threads_ops,
     argument_kind::payload_bytes, true, true},
    {"pipeline_put_0ms_10000ps", measure_pipeline_put_0ms_10000ps, pipeline_10000_ops, argument_kind::parameter, true, true, false},
    {"pipeline_put_0ms_20000ps", measure_pipeline_put_0ms_20000ps, pipeline_20000_ops, argument_kind::parameter, true, true, false},
    {"pipeline_put_20ms_10000ps", measure_pipeline_put_20ms_10000ps, pipeline_10000_ops, argument_kind::parameter, true, true, false},
    {"pipeline_put_20ms_20000ps", measure_pipeline_put_20ms_20000ps, pipeline_20000_ops, argument_kind::parameter, true, true, false},
    {"pipeline_loop_0ms_10000ps", measure_pipeline_loop_0ms_10000ps, pipeline_10000_ops, argument_kind::parameter, true, true, false},
    {"pipeline_loop_0ms_20000ps", measure_pipeline_loop_0ms_20000ps, pipeline_20000_ops, argument_kind::parameter, true, true, false},
    {"pipeline_loop_20ms_10000ps", measure_pipeline_loop_20ms_10000ps, pipeline_10000_ops, argument_kind::parameter, true, true, false},
    {"pipeline_loop_20ms_20000ps", measure_pipeline_loop_20ms_20000ps, pipeline_20000_ops, argument_kind::parameter, true, true, false},
    {"execute_verify_int", measure_execute_verify_int, execute_verify_ops, argument_kind::parameter, true, true},
    {"execute_verify_float", measure_execute_verify_float, execute_verify_ops, argument_kind::parameter, true, true,
     false},
    {"read_back", measure_read_back, read_back_ops, argument_kind::payload_bytes, true, true},
    {"scan_the_whole_log", measure_scan_the_whole_log, scan_ops, argument_kind::parameter, true, scan_exists},
});

constexpr bool batches_are_whole()
{
    for (const auto& c : cases) {
        if (c.name.starts_with("write_batch_") && c.argument == argument_kind::payload_bytes &&
            !std::ranges::all_of(c.ops_by_argument, [](const parameter_ops& p) { return p.ops % batch_size == 0; })) {
            return false;
        }
        if (c.name == "write_batch_size_lmdb" &&
            !std::ranges::all_of(c.ops_by_argument, [](const parameter_ops& p) { return p.ops % p.parameter == 0; })) {
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
constexpr std::uint64_t self_test_samples = 1;
constexpr std::string_view list_flag = "--list";
constexpr std::string_view each_case_once_flag = "--each-case-once";

constexpr std::string_view usage =
    "Usage: konsens_bench <case> <samples> <argument> [<directory>] [--allow-root-for-test]\n"
    "       konsens_bench --list\n"
    "       konsens_bench --each-case-once [--allow-root-for-test]\n"
    "The argument of a case is a payload in bytes or a parameter; --list prints the cases and their arguments.\n"
    "A case whose name holds lmdb, pipeline, execute_verify, read_back or scan takes the directory of its stores.\n";

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

std::optional<std::uint64_t> ops_of(const bench_case& c, const std::uint64_t argument)
{
    const auto it = std::ranges::find(c.ops_by_argument, argument, &parameter_ops::parameter);
    if (it == c.ops_by_argument.end()) [[unlikely]] {
        return std::nullopt;
    }
    return it->ops;
}

int fail(const std::string_view name, const failure& f)
{
    std::cerr << std::format("The case {} failed at the step \"{}\": {}.", name, f.step, f.code.message()) << '\n';
    return EXIT_FAILURE;
}

run_settings settings_of(const bench_case& c, const std::uint64_t samples, const std::uint64_t argument,
                         const std::uint64_t ops, std::filesystem::path directory, const uid_t user_id)
{
    const bool is_payload = c.argument == argument_kind::payload_bytes;
    return {samples,
            ops,
            is_payload ? static_cast<std::size_t>(argument) : batch_payload_bytes,
            is_payload ? 0 : argument,
            std::move(directory),
            user_id};
}

void print_outcome(const bench_case& c, const run_settings& settings, case_result& outcome, const std::uint64_t argument)
{
    auto& ns = outcome.ns_per_op;
    std::ranges::sort(ns);
    const auto argument_name = c.argument == argument_kind::payload_bytes ? "payload_bytes" : "parameter";
    std::cout << std::format("case={} {}={} ops_per_sample={} samples={} ns_per_op_median={:.2f} "
                             "min={:.2f} max={:.2f}{}{}",
                             c.name, argument_name, argument, settings.ops, settings.samples, ns.at(ns.size() / 2),
                             ns.front(), ns.back(), outcome.details.empty() ? "" : " ", outcome.details)
              << '\n';
}

int list_cases()
{
    for (const auto& c : cases) {
        if (!c.exists_at_this_commit) {
            continue;
        }
        for (const auto& p : c.ops_by_argument) {
            std::cout << c.name << ' ' << p.parameter << '\n';
        }
    }
    return EXIT_SUCCESS;
}

int run_each_case_once(const uid_t user_id)
{
    const temporary_directory root;
    int status = EXIT_SUCCESS;
    for (const auto& c : cases) {
        if (!c.exists_at_this_commit || !c.runs_in_the_test_of_each_case) {
            continue;
        }
        for (const auto& p : c.ops_by_argument) {
            auto directory = root.path() / std::format("{}-{}", c.name, p.parameter);
            std::filesystem::create_directories(directory);
            const auto settings = settings_of(c, self_test_samples, p.parameter, p.ops, directory, user_id);
            auto outcome = c.measure(settings);
            if (!outcome) [[unlikely]] {
                status = fail(c.name, outcome.error());
                continue;
            }
            print_outcome(c, settings, *outcome, p.parameter);
            std::filesystem::remove_all(directory);
        }
    }
    return status;
}

int run(std::vector<std::string_view> args)
{
    if (args.size() == 2 && args.at(1) == list_flag) {
        return list_cases();
    }
    const bool allow_root = std::erase(args, allow_root_flag) > 0;
    const auto user_id = choose_the_user_id(geteuid(), allow_root);
    if (!user_id) [[unlikely]] {
        std::cerr << "konsens_bench refuses to run as root; give --allow-root-for-test in a test.\n";
        return exit_refused_root;
    }
    if (args.size() == 2 && args.at(1) == each_case_once_flag) {
        return run_each_case_once(*user_id);
    }
    if (args.size() != directory_argument && args.size() != directory_argument + 1) [[unlikely]] {
        std::cerr << usage;
        return EXIT_FAILURE;
    }
    const auto name = args.at(case_argument);
    const auto c = std::ranges::find(cases, name, &bench_case::name);
    const auto samples = number_of(args.at(samples_argument));
    const auto argument = number_of(args.at(payload_argument));
    const bool has_directory = args.size() > directory_argument;
    if (c == cases.end() || !c->exists_at_this_commit || !samples || *samples == 0 || !argument ||
        has_directory != c->needs_directory) [[unlikely]] {
        std::cerr << usage;
        return EXIT_FAILURE;
    }
    const auto ops = ops_of(*c, *argument);
    if (!ops) [[unlikely]] {
        std::cerr << usage;
        return EXIT_FAILURE;
    }
    const auto settings = settings_of(*c, *samples, *argument, *ops,
                                      has_directory ? std::filesystem::path(args.at(directory_argument))
                                                    : std::filesystem::path{},
                                      *user_id);
    auto outcome = c->measure(settings);
    if (!outcome) [[unlikely]] {
        return fail(name, outcome.error());
    }
    print_outcome(*c, settings, *outcome, *argument);
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
