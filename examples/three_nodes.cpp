#include <cerrno>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <expected>
#include <filesystem>
#include <format>
#include <iostream>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

#include <sys/types.h>
#include <unistd.h>

#include "konsens/node.hpp"
#include "konsens/persistent_storage.hpp"
#include "root_for_test.hpp"
#include "temporary_directories.hpp"

namespace {

template <typename Node>
std::expected<void, std::error_code> sync_the_store(Node& n, konsens::step_end end)
{
    while (end != konsens::step_end::nothing) {
        if (fdatasync(n.get_sequence_paxos().get_internal_storage().file_descriptor()) != 0) [[unlikely]] {
            throw konsens::sync_error(std::error_code{errno, std::generic_category()}, "fdatasync of the store failed");
        }
        const auto next = n.synced();
        if (!next) [[unlikely]] {
            return std::unexpected(next.error());
        }
        end = *next;
    }
    return {};
}

struct concatenation {
    static constexpr std::array key{std::byte{'v'}};

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
        return {value, {{{key.begin(), key.end()}, value}}};
    }

    std::vector<std::byte> query(std::span<const std::byte>, const konsens::state_view auto& view) const
    {
        return value_in(view);
    }
};

using node_t = konsens::node<konsens::persistent_storage, concatenation>;

constexpr std::size_t node_count = 3;
constexpr std::size_t stopped_node = 2;
constexpr std::size_t mebibyte = std::size_t{1} << 20;
constexpr std::size_t map_size = 64 * mebibyte;
constexpr std::uint64_t election_ticks = 5;
constexpr std::uint64_t resend_ticks = 3;
constexpr int round_limit = 200;
constexpr int first_entry_count = 3;
constexpr int second_entry_count = 2;
constexpr std::uint64_t register_call = 1;
constexpr std::uint64_t request_call = 2;
constexpr std::uint64_t query_call = 3;
constexpr std::uint64_t first_request_number = 1;

konsens::node_config config_of(const konsens::node_id pid)
{
    std::vector<konsens::node_id> peers;
    for (konsens::node_id p = 1; p <= node_count; ++p) {
        if (p != pid) {
            peers.push_back(p);
        }
    }
    return {1, pid, peers, 0, election_ticks, resend_ticks};
}

std::vector<std::byte> bytes_of(const std::string_view text)
{
    std::vector<std::byte> bytes;
    for (const char c : text) {
        bytes.push_back(static_cast<std::byte>(c));
    }
    return bytes;
}

konsens::entry entry_of(const std::string_view text)
{
    return {konsens::entry_kind::normal, bytes_of(text)};
}

std::string text_of(const std::span<const std::byte> bytes)
{
    std::string text;
    for (const auto b : bytes) {
        text.push_back(static_cast<char>(b));
    }
    return text;
}

std::string text_of(const konsens::entry& e)
{
    switch (e.kind) {
    case konsens::entry_kind::register_client:
        return std::format("RegisterClient of call {}", e.client_id);
    case konsens::entry_kind::client_request:
        return std::format("request {} of session {}: {}", e.request_number, e.session, text_of(e.payload));
    case konsens::entry_kind::expire_session:
        return std::format("ExpireSession of session {}", e.session);
    case konsens::entry_kind::results:
        return std::format("results of {} to {}", e.from_idx, e.to_idx);
    case konsens::entry_kind::rejected:
        return std::format("rejected {}", e.from_idx);
    case konsens::entry_kind::normal:
    case konsens::entry_kind::stop_sign:
        break;
    }
    return text_of(e.payload);
}

konsens::node_id to_of(const konsens::message& m)
{
    return std::visit([](const auto& x) { return x.to; }, m);
}

struct cluster {
    std::array<std::filesystem::path, node_count> directories;
    uid_t effective_user_id;
    std::array<std::optional<node_t>, node_count> nodes;
    std::vector<konsens::message> network;
    std::uint64_t nonce{};
    std::vector<konsens::client_reply> replies{};

    std::expected<void, std::error_code> start(const std::size_t i)
    {
        auto storage = konsens::persistent_storage::open(directories.at(i), map_size, effective_user_id);
        if (!storage) [[unlikely]] {
            return std::unexpected(storage.error());
        }
        const std::filesystem::path node_directory{directories.at(i).string() + "-node"};
        std::error_code created;
        std::filesystem::create_directories(node_directory, created);
        if (created) [[unlikely]] {
            return std::unexpected(created);
        }
        auto own = konsens::node_storage::open(node_directory, effective_user_id);
        if (!own) [[unlikely]] {
            return std::unexpected(own.error());
        }
        nonce += 1;
        auto n = node_t::with(config_of(i + 1), std::move(*storage), std::move(*own),
                              concatenation{}, nonce);
        if (!n) [[unlikely]] {
            return std::unexpected(n.error());
        }
        if (auto synced = sync_the_store(*n, n->get_step_end()); !synced) [[unlikely]] {
            return std::unexpected(synced.error());
        }
        nodes.at(i).emplace(std::move(*n));
        return {};
    }

    void stop(const std::size_t i) { nodes.at(i).reset(); }

    void collect()
    {
        for (auto& n : nodes) {
            if (n) {
                for (auto& m : n->take_outgoing_messages()) {
                    network.push_back(std::move(m));
                }
                for (auto& r : n->take_client_replies()) {
                    replies.push_back(std::move(r));
                }
            }
        }
    }

    std::expected<void, std::error_code> commit(const std::size_t i,
                                                const std::expected<konsens::transaction, std::error_code>& t)
    {
        if (!t) [[unlikely]] {
            return std::unexpected(t.error());
        }
        const auto end = nodes.at(i)->commit(*t);
        if (!end) [[unlikely]] {
            return std::unexpected(end.error());
        }
        return sync_the_store(*nodes.at(i), *end);
    }

    std::optional<konsens::client_reply> take_reply(const std::uint64_t call)
    {
        const auto it = std::ranges::find(replies, call, &konsens::client_reply::client_id);
        if (it == replies.end()) {
            return std::nullopt;
        }
        auto reply = std::move(*it);
        replies.erase(it);
        return reply;
    }

    std::expected<void, std::error_code> run_round()
    {
        for (std::size_t i = 0; i < node_count; ++i) {
            if (nodes.at(i)) {
                if (auto r = commit(i, nodes.at(i)->tick()); !r) [[unlikely]] {
                    return r;
                }
            }
        }
        collect();
        while (!network.empty()) {
            std::array<std::vector<konsens::message>, node_count> arrived;
            for (auto& m : std::exchange(network, {})) {
                arrived.at(to_of(m) - 1).push_back(std::move(m));
            }
            for (std::size_t i = 0; i < node_count; ++i) {
                if (nodes.at(i)) {
                    if (auto r = commit(i, nodes.at(i)->handle_incoming(std::move(arrived.at(i)))); !r) [[unlikely]] {
                        return r;
                    }
                }
            }
            collect();
        }
        return {};
    }

    std::optional<std::size_t> leader() const
    {
        for (std::size_t i = 0; i < node_count; ++i) {
            if (nodes.at(i) && nodes.at(i)->get_sequence_paxos().get_state() ==
                                   std::pair{konsens::role::leader, konsens::phase::accept}) {
                return i;
            }
        }
        return std::nullopt;
    }

    std::expected<std::uint64_t, std::error_code> decided_count(const std::size_t i) const
    {
        return nodes.at(i)->get_sequence_paxos().get_decided_idx();
    }

    bool all_decided_the_same() const
    {
        const auto first = nodes.front()->read_decided_suffix(0);
        for (const auto& n : nodes) {
            if (!n || n->read_decided_suffix(0) != first) [[unlikely]] {
                return false;
            }
        }
        return first.has_value();
    }

    bool all_running_decided(const std::uint64_t count) const
    {
        for (std::size_t i = 0; i < node_count; ++i) {
            if (!nodes.at(i)) {
                continue;
            }
            const auto decided = decided_count(i);
            if (!decided || *decided < count) {
                return false;
            }
        }
        return true;
    }
};

std::expected<void, std::error_code> run_until(cluster& c, const auto done)
{
    for (int r = 0; r < round_limit; ++r) {
        if (done()) {
            return {};
        }
        if (auto ran = c.run_round(); !ran) [[unlikely]] {
            return ran;
        }
    }
    if (done()) {
        return {};
    }
    return std::unexpected(std::make_error_code(std::errc::timed_out));
}

std::expected<void, std::error_code> print_decided(const cluster& c, const std::size_t i)
{
    const auto decided = c.nodes.at(i)->read_decided_suffix(0);
    if (!decided) [[unlikely]] {
        return std::unexpected(decided.error());
    }
    std::cout << std::format("Node {} has {} decided entries:", i + 1, decided->size()) << '\n';
    for (const auto& e : *decided) {
        std::cout << std::format("  {}", text_of(e)) << '\n';
    }
    return {};
}

std::expected<void, std::error_code> append_at_leader(cluster& c, const int first, const int count)
{
    const auto l = c.leader();
    if (!l) [[unlikely]] {
        return std::unexpected(std::make_error_code(std::errc::resource_unavailable_try_again));
    }
    auto& n = *c.nodes.at(*l);
    std::optional<konsens::transaction> t;
    for (int k = first; k < first + count; ++k) {
        const auto text = "entry " + std::to_string(k);
        const auto appended = n.append(entry_of(text));
        if (!appended) [[unlikely]] {
            return std::unexpected(appended.error());
        }
        if (t) {
            *t += *appended;
        } else {
            t = *appended;
        }
        std::cout << std::format("Node {} appends \"{}\".", *l + 1, text) << '\n';
    }
    if (!t) {
        return {};
    }
    const auto end = n.commit(*t);
    if (!end) [[unlikely]] {
        return std::unexpected(end.error());
    }
    if (auto synced = sync_the_store(n, *end); !synced) [[unlikely]] {
        return synced;
    }
    c.collect();
    std::cout << std::format("Node {} writes the {} entries in one commit.", *l + 1, count) << '\n';
    return {};
}

std::expected<konsens::client_reply, std::error_code> call_and_wait(cluster& c, const std::size_t at,
                                                                    const std::uint64_t call, const auto give)
{
    if (auto r = c.commit(at, give(*c.nodes.at(at))); !r) [[unlikely]] {
        return std::unexpected(r.error());
    }
    c.collect();
    std::optional<konsens::client_reply> reply;
    if (auto r = run_until(c,
                           [&c, &reply, call] {
                               reply = c.take_reply(call);
                               return reply.has_value();
                           });
        !r) [[unlikely]] {
        return std::unexpected(r.error());
    }
    if (!reply->response) [[unlikely]] {
        return std::unexpected(reply->response.error());
    }
    return std::move(*reply);
}

std::expected<void, std::error_code> run_client(cluster& c)
{
    const auto l = c.leader();
    if (!l) [[unlikely]] {
        return std::unexpected(std::make_error_code(std::errc::resource_unavailable_try_again));
    }
    const auto writer = (*l + 1) % node_count;
    const auto reader = (*l + 2) % node_count;
    const auto registered =
        call_and_wait(c, writer, register_call, [](node_t& w) { return w.register_client(register_call); });
    if (!registered) [[unlikely]] {
        return std::unexpected(registered.error());
    }
    const auto session = registered->session;
    std::cout << std::format("A client registers at node {}. Its session is {}.", writer + 1, session) << '\n';
    const std::string_view text = "client text";
    const auto written = call_and_wait(c, writer, request_call, [session, text](node_t& w) {
        return w.client_request(request_call, session, first_request_number, bytes_of(text));
    });
    if (!written) [[unlikely]] {
        return std::unexpected(written.error());
    }
    std::cout << std::format("The client writes \"{}\" at node {} with request number {}. The reply is \"{}\".", text,
                             writer + 1, first_request_number, text_of(*written->response))
              << '\n';
    const auto read = call_and_wait(c, reader, query_call, [](node_t& r) { return r.client_query(query_call, {}); });
    if (!read) [[unlikely]] {
        return std::unexpected(read.error());
    }
    std::cout << std::format("The client reads at node {} through the read index. The value is \"{}\".", reader + 1,
                             text_of(*read->response))
              << '\n';
    if (*read->response != bytes_of(text)) [[unlikely]] {
        return std::unexpected(std::make_error_code(std::errc::state_not_recoverable));
    }
    return {};
}

int fail(const std::string_view step, const std::error_code& code)
{
    std::cerr << std::format("The step \"{}\" failed: {}.", step, code.message()) << '\n';
    return EXIT_FAILURE;
}

int run(const std::span<const std::string_view> args)
{
    if (!konsens::can_use_crc32c_instruction()) [[unlikely]] {
        std::cerr << "Warning: the CPU has no CRC32C instruction. Each CRC32C runs without it and is slower.\n";
    }
    if (std::ranges::contains(args, help_flag)) {
        std::cout << std::format("Usage: three_nodes [{}] [directory-1 directory-2 directory-3]", allow_root_flag)
                  << '\n';
        return EXIT_SUCCESS;
    }
    bool allow_root = false;
    std::vector<std::filesystem::path> given;
    for (const auto a : args) {
        if (a == allow_root_flag) {
            allow_root = true;
        } else {
            given.emplace_back(a);
        }
    }
    if (!given.empty() && given.size() != node_count) [[unlikely]] {
        std::cerr << std::format("Usage: three_nodes [{}] [directory-1 directory-2 directory-3]", allow_root_flag)
                  << '\n';
        return EXIT_FAILURE;
    }
    const uid_t effective_user_id = geteuid();
    const auto chosen = choose_the_user_id(effective_user_id, allow_root);
    if (!chosen) [[unlikely]] {
        std::cerr << std::format("The library refuses to run as root. Start the example as another user.") << '\n';
        return exit_refused_root;
    }
    if (*chosen != effective_user_id) {
        std::cout << std::format("The process runs as root. The flag {} gives the library user id {}.", allow_root_flag,
                                 *chosen)
                  << '\n';
    }
    const uid_t user_id = *chosen;
    temporary_directories temporary;
    cluster c{{}, user_id, {}, {}};
    for (std::size_t i = 0; i < node_count; ++i) {
        c.directories.at(i) = given.empty() ? temporary.create() : given.at(i);
        std::cout << std::format("Node {} keeps its data in {}.", i + 1, c.directories.at(i).string()) << '\n';
    }
    for (std::size_t i = 0; i < node_count; ++i) {
        if (auto r = c.start(i); !r) [[unlikely]] {
            return fail("start the nodes", r.error());
        }
    }
    if (auto r = run_until(c, [&c] { return c.leader().has_value(); }); !r) [[unlikely]] {
        return fail("elect a leader", r.error());
    }
    std::cout << std::format("Node {} is the leader.", *c.leader() + 1) << '\n';
    const auto base = c.decided_count(*c.leader());
    if (!base) [[unlikely]] {
        return fail("read the decided index of the leader", base.error());
    }
    if (auto r = append_at_leader(c, 1, first_entry_count); !r) [[unlikely]] {
        return fail("append the first entries", r.error());
    }
    const auto first_target = *base + first_entry_count;
    if (auto r = run_until(c, [&c, first_target] { return c.all_running_decided(first_target); }); !r) [[unlikely]] {
        return fail("decide the first entries", r.error());
    }
    for (std::size_t i = 0; i < node_count; ++i) {
        if (auto r = print_decided(c, i); !r) [[unlikely]] {
            return fail("read the decided entries", r.error());
        }
    }
    c.stop(stopped_node);
    std::cout << std::format("Node {} is stopped. Its LMDB environment is closed.", stopped_node + 1) << '\n';
    if (auto r = run_until(c, [&c] { return c.leader().has_value(); }); !r) [[unlikely]] {
        return fail("elect a leader of two nodes", r.error());
    }
    std::cout << std::format("Node {} is the leader of the two running nodes.", *c.leader() + 1) << '\n';
    if (auto r = append_at_leader(c, first_entry_count + 1, second_entry_count); !r) [[unlikely]] {
        return fail("append with two nodes", r.error());
    }
    const auto second_target = first_target + second_entry_count;
    if (auto r = run_until(c, [&c, second_target] { return c.all_running_decided(second_target); }); !r) [[unlikely]] {
        return fail("decide with two nodes", r.error());
    }
    std::cout << std::format("The two running nodes decided {} entries.", second_target) << '\n';
    if (auto r = c.start(stopped_node); !r) [[unlikely]] {
        return fail("reopen the stopped node", r.error());
    }
    const auto reopened = c.decided_count(stopped_node);
    if (!reopened) [[unlikely]] {
        return fail("read the decided index of the reopened node", reopened.error());
    }
    std::cout << std::format("Node {} is open again from its directory. It has {} decided entries from disk.",
                             stopped_node + 1, *reopened)
              << '\n';
    if (auto r = run_until(c, [&c, second_target] { return c.all_running_decided(second_target); }); !r) [[unlikely]] {
        return fail("let the reopened node catch up", r.error());
    }
    std::cout << std::format("Node {} caught up.", stopped_node + 1) << '\n';
    if (auto r = print_decided(c, stopped_node); !r) [[unlikely]] {
        return fail("read the decided entries of the reopened node", r.error());
    }
    if (!c.all_decided_the_same()) [[unlikely]] {
        return fail("compare the decided entries", std::make_error_code(std::errc::state_not_recoverable));
    }
    std::cout << std::format("All three nodes have the same {} decided entries.", second_target) << '\n';
    if (auto r = run_client(c); !r) [[unlikely]] {
        return fail("register, write and read as a client", r.error());
    }
    return EXIT_SUCCESS;
}

}

int main(const int argc, const char* const* const argv)
{
    try {
        const auto all = std::span(argv, static_cast<std::size_t>(argc));
        const std::vector<std::string_view> args(std::next(all.begin()), all.end());
        return run(args);
    } catch (const std::exception& e) {
        std::cerr << std::format("The example ended with an exception: {}", e.what()) << '\n';
        return EXIT_FAILURE;
    }
}
