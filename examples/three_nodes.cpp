#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <exception>
#include <expected>
#include <iterator>
#include <filesystem>
#include <iostream>
#include <optional>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "konsens/node.hpp"
#include "konsens/persistent_storage.hpp"

namespace {

using node_t = konsens::node<konsens::persistent_storage>;

constexpr std::size_t node_count = 3;
constexpr std::size_t stopped_node = 2;
constexpr std::size_t map_size = std::size_t{64} << 20;
constexpr std::uint64_t election_ticks = 5;
constexpr std::uint64_t resend_ticks = 3;
constexpr int round_limit = 200;
constexpr int first_entry_count = 3;
constexpr int second_entry_count = 2;
constexpr uid_t root_user_id = 0;
constexpr uid_t test_user_id = 65534;
constexpr int exit_refused_root = 2;
constexpr std::string_view allow_root_flag = "--allow-root-for-test";

class temporary_directories {
public:
    temporary_directories() = default;
    temporary_directories(const temporary_directories&) = delete;
    temporary_directories& operator=(const temporary_directories&) = delete;
    ~temporary_directories()
    {
        for (const auto& p : paths_) {
            std::error_code ignored;
            std::filesystem::remove_all(p, ignored);
        }
    }

    std::filesystem::path create()
    {
        std::string name = (std::filesystem::temp_directory_path() / "konsens-three-nodes-XXXXXX").string();
        if (mkdtemp(name.data()) == nullptr) [[unlikely]] {
            throw std::system_error(errno, std::generic_category(), "mkdtemp");
        }
        paths_.emplace_back(name);
        return paths_.back();
    }

private:
    std::vector<std::filesystem::path> paths_;
};

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

konsens::entry entry_of(const std::string_view text)
{
    std::vector<std::byte> payload;
    for (const char c : text) {
        payload.push_back(static_cast<std::byte>(c));
    }
    return {konsens::entry_kind::normal, payload};
}

std::string text_of(const konsens::entry& e)
{
    std::string text;
    for (const auto b : e.payload) {
        text.push_back(static_cast<char>(b));
    }
    return text;
}

konsens::node_id to_of(const konsens::message& m)
{
    return std::visit([](const auto& x) { return x.to; }, m);
}

struct cluster {
    std::array<std::filesystem::path, node_count> directories;
    uid_t effective_user_id;
    std::array<std::optional<node_t>, node_count> nodes;
    std::deque<konsens::message> network;

    std::expected<void, std::error_code> start(const std::size_t i)
    {
        auto storage = konsens::persistent_storage::open(directories.at(i), map_size, effective_user_id);
        if (!storage) [[unlikely]] {
            return std::unexpected(storage.error());
        }
        auto n = node_t::with(config_of(i + 1), std::move(*storage));
        if (!n) [[unlikely]] {
            return std::unexpected(n.error());
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
            }
        }
    }

    std::expected<void, std::error_code> run_round()
    {
        for (auto& n : nodes) {
            if (n) {
                if (auto r = n->tick(); !r) [[unlikely]] {
                    return r;
                }
            }
        }
        collect();
        while (!network.empty()) {
            auto m = std::move(network.front());
            network.pop_front();
            auto& target = nodes.at(to_of(m) - 1);
            if (!target) {
                continue;
            }
            if (auto r = target->handle(std::move(m)); !r) [[unlikely]] {
                return r;
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

    std::uint64_t decided_count(const std::size_t i) const { return nodes.at(i)->get_sequence_paxos().get_decided_idx(); }

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
            if (nodes.at(i) && decided_count(i) < count) {
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
    for (int k = first; k < first + count; ++k) {
        const auto text = "entry " + std::to_string(k);
        if (auto r = c.nodes.at(*l)->append(entry_of(text)); !r) [[unlikely]] {
            return r;
        }
        std::cout << std::format("Node {} appends \"{}\".", *l + 1, text) << '\n';
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
        std::cerr << std::format("Usage: three_nodes [{}] [directory-1 directory-2 directory-3]", allow_root_flag) << '\n';
        return EXIT_FAILURE;
    }
    uid_t user_id = geteuid();
    if (user_id == root_user_id) [[unlikely]] {
        if (!allow_root) {
            std::cerr << std::format("The library refuses to run as root. Start the example as another user.") << '\n';
            return exit_refused_root;
        }
        std::cout << std::format("The process runs as root. The flag {} gives the library user id {}.", allow_root_flag,
                     test_user_id) << '\n';
        user_id = test_user_id;
    }
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
    if (auto r = append_at_leader(c, 1, first_entry_count); !r) [[unlikely]] {
        return fail("append the first entries", r.error());
    }
    const auto first_target = base + first_entry_count;
    if (auto r = run_until(c, [&c, first_target] { return c.all_running_decided(first_target); }); !r)
        [[unlikely]] {
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
    if (auto r = run_until(c, [&c, second_target] { return c.all_running_decided(second_target); }); !r)
        [[unlikely]] {
        return fail("decide with two nodes", r.error());
    }
    std::cout << std::format("The two running nodes decided {} entries.", second_target) << '\n';
    if (auto r = c.start(stopped_node); !r) [[unlikely]] {
        return fail("reopen the stopped node", r.error());
    }
    std::cout << std::format("Node {} is open again from its directory. It has {} decided entries from disk.", stopped_node + 1,
                 c.decided_count(stopped_node)) << '\n';
    if (auto r = run_until(c, [&c, second_target] { return c.all_running_decided(second_target); }); !r)
        [[unlikely]] {
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
