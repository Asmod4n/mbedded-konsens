#define TOML_EXCEPTIONS 0
#define TOML_ENABLE_FORMATTERS 0
#include <toml++/toml.hpp>

#include <algorithm>
#include <array>
#include <limits>
#include <optional>
#include <span>
#include <utility>

#include "config.hpp"

namespace konsensd {

namespace {

constexpr std::array cluster_keys{std::string_view{"name"}, std::string_view{"configuration_id"}};
constexpr std::array node_keys{std::string_view{"pid"},
                               std::string_view{"data_directory"},
                               std::string_view{"peer_address"},
                               std::string_view{"client_address"},
                               std::string_view{"leader_priority"},
                               std::string_view{"election_ticks"},
                               std::string_view{"resend_ticks"},
                               std::string_view{"tick_milliseconds"},
                               std::string_view{"max_frame_bytes"},
                               std::string_view{"max_body_bytes"},
                               std::string_view{"max_store_bytes"},
                               std::string_view{"build_from"},
                               std::string_view{"sync_by_the_caller"}};
constexpr std::array peer_keys{std::string_view{"pid"}, std::string_view{"address"}};
constexpr std::array top_keys{std::string_view{"cluster"}, std::string_view{"node"}, std::string_view{"peers"}};

config_error error_at(const config_errc code, const std::string_view key, const toml::source_region& where)
{
    return {code, key, where.begin.line, where.begin.column};
}

std::expected<void, config_error> refuse_unknown_keys(const toml::table& t, const std::span<const std::string_view> known)
{
    for (const auto& [k, v] : t) {
        if (std::ranges::find(known, k.str()) == known.end()) [[unlikely]] {
            return std::unexpected(error_at(config_errc::unknown_key, {}, k.source()));
        }
    }
    return {};
}

std::expected<const toml::table*, config_error> table_in(const toml::table& t, const std::string_view key)
{
    const toml::node* n = t.get(key);
    if (n == nullptr) [[unlikely]] {
        return std::unexpected(error_at(config_errc::missing_key, key, t.source()));
    }
    const toml::table* found = n->as_table();
    if (found == nullptr) [[unlikely]] {
        return std::unexpected(error_at(config_errc::wrong_type, key, n->source()));
    }
    return found;
}

template <typename T>
std::expected<T, config_error> number_in(const toml::table& t, const std::string_view key, const std::optional<T> fallback,
                                         const std::int64_t lowest)
{
    const toml::node* n = t.get(key);
    if (n == nullptr) {
        if (!fallback) [[unlikely]] {
            return std::unexpected(error_at(config_errc::missing_key, key, t.source()));
        }
        return *fallback;
    }
    const auto* v = n->as_integer();
    if (v == nullptr) [[unlikely]] {
        return std::unexpected(error_at(config_errc::wrong_type, key, n->source()));
    }
    const std::int64_t value = v->get();
    if (value < lowest || std::cmp_greater(value, std::numeric_limits<T>::max())) [[unlikely]] {
        return std::unexpected(error_at(config_errc::out_of_range, key, n->source()));
    }
    return static_cast<T>(value);
}

std::expected<bool, config_error> flag_in(const toml::table& t, const std::string_view key, const bool fallback)
{
    const toml::node* n = t.get(key);
    if (n == nullptr) {
        return fallback;
    }
    const auto* v = n->as_boolean();
    if (v == nullptr) [[unlikely]] {
        return std::unexpected(error_at(config_errc::wrong_type, key, n->source()));
    }
    return v->get();
}

std::expected<std::string, config_error> text_in(const toml::table& t, const std::string_view key)
{
    const toml::node* n = t.get(key);
    if (n == nullptr) [[unlikely]] {
        return std::unexpected(error_at(config_errc::missing_key, key, t.source()));
    }
    const auto* v = n->as_string();
    if (v == nullptr) [[unlikely]] {
        return std::unexpected(error_at(config_errc::wrong_type, key, n->source()));
    }
    if (v->get().empty()) [[unlikely]] {
        return std::unexpected(error_at(config_errc::out_of_range, key, n->source()));
    }
    return v->get();
}

std::expected<std::vector<konsens::node_id>, config_error> pids_in(const toml::table& t, const std::string_view key)
{
    std::vector<konsens::node_id> pids;
    const toml::node* n = t.get(key);
    if (n == nullptr) {
        return pids;
    }
    const toml::array* a = n->as_array();
    if (a == nullptr) [[unlikely]] {
        return std::unexpected(error_at(config_errc::wrong_type, key, n->source()));
    }
    for (const toml::node& item : *a) {
        const auto* v = item.as_integer();
        if (v == nullptr) [[unlikely]] {
            return std::unexpected(error_at(config_errc::wrong_type, key, item.source()));
        }
        if (v->get() < 1) [[unlikely]] {
            return std::unexpected(error_at(config_errc::out_of_range, key, item.source()));
        }
        pids.push_back(static_cast<konsens::node_id>(v->get()));
    }
    return pids;
}

std::expected<std::vector<peer>, config_error> peers_in(const toml::table& t)
{
    std::vector<peer> peers;
    const toml::node* n = t.get("peers");
    if (n == nullptr) [[unlikely]] {
        return std::unexpected(error_at(config_errc::missing_key, "peers", t.source()));
    }
    const toml::array* a = n->as_array();
    if (a == nullptr || !a->is_array_of_tables()) [[unlikely]] {
        return std::unexpected(error_at(config_errc::wrong_type, "peers", n->source()));
    }
    for (const toml::node& item : *a) {
        const toml::table& p = *item.as_table();
        if (auto r = refuse_unknown_keys(p, peer_keys); !r) [[unlikely]] {
            return std::unexpected(r.error());
        }
        const auto pid = number_in<konsens::node_id>(p, "pid", std::nullopt, 1);
        if (!pid) [[unlikely]] {
            return std::unexpected(pid.error());
        }
        auto address = text_in(p, "address");
        if (!address) [[unlikely]] {
            return std::unexpected(address.error());
        }
        if (std::ranges::contains(peers, *pid, &peer::pid)) [[unlikely]] {
            return std::unexpected(error_at(config_errc::duplicate_pid, "pid", p.source()));
        }
        peers.push_back({*pid, std::move(*address)});
    }
    return peers;
}

}

std::expected<config, config_error> parse_config(const std::string_view text)
{
    const toml::parse_result parsed = toml::parse(text);
    if (!parsed) [[unlikely]] {
        return std::unexpected(error_at(config_errc::syntax, {}, parsed.error().source()));
    }
    const toml::table& top = parsed.table();
    if (auto r = refuse_unknown_keys(top, top_keys); !r) [[unlikely]] {
        return std::unexpected(r.error());
    }
    const auto cluster = table_in(top, "cluster");
    if (!cluster) [[unlikely]] {
        return std::unexpected(cluster.error());
    }
    const auto node = table_in(top, "node");
    if (!node) [[unlikely]] {
        return std::unexpected(node.error());
    }
    if (auto r = refuse_unknown_keys(**cluster, cluster_keys); !r) [[unlikely]] {
        return std::unexpected(r.error());
    }
    if (auto r = refuse_unknown_keys(**node, node_keys); !r) [[unlikely]] {
        return std::unexpected(r.error());
    }
    const toml::table& c = **cluster;
    const toml::table& n = **node;
    auto name = text_in(c, "name");
    if (!name) [[unlikely]] {
        return std::unexpected(name.error());
    }
    const auto configuration_id = number_in<std::uint32_t>(c, "configuration_id", std::nullopt, 1);
    if (!configuration_id) [[unlikely]] {
        return std::unexpected(configuration_id.error());
    }
    const auto pid = number_in<konsens::node_id>(n, "pid", std::nullopt, 1);
    if (!pid) [[unlikely]] {
        return std::unexpected(pid.error());
    }
    auto data_directory = text_in(n, "data_directory");
    if (!data_directory) [[unlikely]] {
        return std::unexpected(data_directory.error());
    }
    auto peer_address = text_in(n, "peer_address");
    if (!peer_address) [[unlikely]] {
        return std::unexpected(peer_address.error());
    }
    auto client_address = text_in(n, "client_address");
    if (!client_address) [[unlikely]] {
        return std::unexpected(client_address.error());
    }
    const auto leader_priority = number_in<std::uint32_t>(n, "leader_priority", 0u, 0);
    if (!leader_priority) [[unlikely]] {
        return std::unexpected(leader_priority.error());
    }
    const auto election_ticks = number_in<std::uint64_t>(n, "election_ticks", default_election_ticks, 1);
    if (!election_ticks) [[unlikely]] {
        return std::unexpected(election_ticks.error());
    }
    const auto resend_ticks = number_in<std::uint64_t>(n, "resend_ticks", default_resend_ticks, 1);
    if (!resend_ticks) [[unlikely]] {
        return std::unexpected(resend_ticks.error());
    }
    const auto tick_milliseconds = number_in<std::uint64_t>(n, "tick_milliseconds", default_tick_milliseconds, 1);
    if (!tick_milliseconds) [[unlikely]] {
        return std::unexpected(tick_milliseconds.error());
    }
    const auto max_frame_bytes = number_in<std::uint64_t>(n, "max_frame_bytes", default_max_frame_bytes, 1);
    if (!max_frame_bytes) [[unlikely]] {
        return std::unexpected(max_frame_bytes.error());
    }
    const auto max_body_bytes = number_in<std::uint64_t>(n, "max_body_bytes", default_max_body_bytes, 0);
    if (!max_body_bytes) [[unlikely]] {
        return std::unexpected(max_body_bytes.error());
    }
    const auto max_store_bytes = number_in<std::uint64_t>(n, "max_store_bytes", default_max_store_bytes, 1);
    if (!max_store_bytes) [[unlikely]] {
        return std::unexpected(max_store_bytes.error());
    }
    auto build_from = pids_in(n, "build_from");
    if (!build_from) [[unlikely]] {
        return std::unexpected(build_from.error());
    }
    const auto sync_by_the_caller = flag_in(n, "sync_by_the_caller", false);
    if (!sync_by_the_caller) [[unlikely]] {
        return std::unexpected(sync_by_the_caller.error());
    }
    auto peers = peers_in(top);
    if (!peers) [[unlikely]] {
        return std::unexpected(peers.error());
    }
    if (std::ranges::contains(*peers, *pid, &peer::pid)) [[unlikely]] {
        return std::unexpected(error_at(config_errc::duplicate_pid, "pid", n.source()));
    }
    return config{std::move(*name),
                  *configuration_id,
                  *pid,
                  std::move(*data_directory),
                  std::move(*peer_address),
                  std::move(*client_address),
                  *leader_priority,
                  *election_ticks,
                  *resend_ticks,
                  *tick_milliseconds,
                  *max_frame_bytes,
                  *max_body_bytes,
                  *max_store_bytes,
                  std::move(*build_from),
                  std::move(*peers),
                  *sync_by_the_caller};
}

konsens::node_config node_config_of(const config& c, const bool first_start)
{
    std::vector<konsens::node_id> peers;
    for (const auto& p : c.peers) {
        peers.push_back(p.pid);
    }
    return {c.configuration_id, c.pid,         std::move(peers), c.leader_priority,
            c.election_ticks,   c.resend_ticks, c.build_from,     first_start, c.sync_by_the_caller};
}

}
