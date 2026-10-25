#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <exception>
#include <expected>
#include <filesystem>
#include <format>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <ranges>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include <sys/types.h>
#include <unistd.h>

#include "konsens/node.hpp"
#include "konsens/persistent_storage.hpp"
#include "konsens/wire.hpp"
#include "root_for_test.hpp"
#include "temporary_directories.hpp"

namespace {

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

constexpr std::size_t node_count = 3;
constexpr std::size_t channel_count = node_count * node_count;
constexpr std::uint64_t election_ticks = 5;
constexpr std::uint64_t resend_ticks = 3;
constexpr std::uint64_t permille = 1000;
constexpr std::size_t largest_batch = 8;
constexpr std::uint64_t call_timeout_rounds = 10;
constexpr std::uint64_t increments_between_reads = 10;
constexpr std::uint64_t rounds_without_progress_limit = 1000;
constexpr std::uint64_t agreement_round_limit = 1000;
constexpr std::uint64_t final_read_round_limit = 1000;
constexpr int usage_exit_code = 2;
constexpr std::size_t words_of_a_flag = 2;
constexpr std::size_t mebibyte = std::size_t{1} << 20;
constexpr std::size_t map_size = 64 * mebibyte;

constexpr std::uint64_t default_clients = 4;
constexpr std::uint64_t default_increments = 1000;
constexpr std::uint64_t default_seed = 1;
constexpr std::uint64_t default_loss = 10;
constexpr std::uint64_t default_duplicate = 10;
constexpr std::uint64_t default_restart = 5;
constexpr std::uint64_t default_power_loss = 2;

struct options {
    std::uint64_t clients{default_clients};
    std::uint64_t increments{default_increments};
    std::uint64_t seed{default_seed};
    std::uint64_t loss{default_loss};
    std::uint64_t duplicate{default_duplicate};
    std::uint64_t restart{default_restart};
    std::uint64_t power_loss{default_power_loss};
    bool calls_reconnected{true};
    bool cbor{false};
    bool allow_root_for_test{false};
};

struct flag {
    std::string_view name;
    std::uint64_t options::*member;
};

constexpr std::array flags{
    flag{"--clients", &options::clients},
    flag{"--increments", &options::increments},
    flag{"--seed", &options::seed},
    flag{"--loss", &options::loss},
    flag{"--duplicate", &options::duplicate},
    flag{"--restart", &options::restart},
    flag{"--power-loss", &options::power_loss},
};

constexpr std::string_view no_reconnected_flag = "--no-reconnected";
constexpr std::string_view cbor_flag = "--cbor";

constexpr std::string_view usage = "usage: counter_stress [--clients C] [--increments N] [--seed S] [--loss P] "
                                   "[--duplicate P] [--restart R] [--power-loss L] [--no-reconnected] [--cbor] "
                                   "[--allow-root-for-test]";

std::optional<std::uint64_t> number_of(const std::string_view text)
{
    std::uint64_t v{};
    const auto last = std::to_address(text.end());
    const auto [end, code] = std::from_chars(std::to_address(text.begin()), last, v);
    if (code != std::errc{} || end != last) [[unlikely]] {
        return std::nullopt;
    }
    return v;
}

std::expected<options, std::string_view> options_of(const std::span<char* const> args)
{
    options o{};
    for (auto rest = args.subspan(std::min(args.size(), std::size_t{1})); !rest.empty();) {
        const std::string_view name = rest.front();
        if (name == no_reconnected_flag) {
            o.calls_reconnected = false;
            rest = rest.subspan(1);
            continue;
        }
        if (name == cbor_flag) {
            o.cbor = true;
            rest = rest.subspan(1);
            continue;
        }
        if (name == allow_root_flag) {
            o.allow_root_for_test = true;
            rest = rest.subspan(1);
            continue;
        }
        const auto it = std::ranges::find(flags, name, &flag::name);
        if (it == flags.end()) [[unlikely]] {
            return std::unexpected("A flag is not known");
        }
        if (rest.size() < words_of_a_flag) [[unlikely]] {
            return std::unexpected("A flag has no value");
        }
        const auto value = number_of(rest.subspan(1).front());
        if (!value) [[unlikely]] {
            return std::unexpected("A value is not a number");
        }
        o.*(it->member) = *value;
        rest = rest.subspan(words_of_a_flag);
    }
    if (o.loss > permille || o.duplicate > permille || o.restart > permille || o.power_loss > permille) [[unlikely]] {
        return std::unexpected("A chance is above 1000 permille");
    }
    if (o.clients == 0 || o.increments == 0) [[unlikely]] {
        return std::unexpected("The values of --clients and --increments must not be 0");
    }
    if (o.increments > std::numeric_limits<std::uint64_t>::max() / o.clients) [[unlikely]] {
        return std::unexpected("The product of --clients and --increments is too large");
    }
    return o;
}

bool takes_defaults_apart_from_the_seed(const options& o)
{
    return o.clients == default_clients && o.increments == default_increments && o.loss == default_loss &&
           o.duplicate == default_duplicate && o.restart == default_restart && o.power_loss == default_power_loss &&
           o.calls_reconnected;
}

std::uint64_t value_of(const std::span<const std::byte> bytes)
{
    std::array<std::byte, sizeof(std::uint64_t)> b{};
    if (bytes.size() != b.size()) [[unlikely]] {
        throw std::logic_error("a counter reply has 8 bytes");
    }
    std::ranges::copy(bytes, b.begin());
    return std::bit_cast<std::uint64_t>(b);
}

struct lmdb_cluster {
    using storage_type = konsens::persistent_storage;

    std::array<std::filesystem::path, node_count> directories;
    uid_t effective_user_id;

    std::monostate stop(const konsens::node<konsens::persistent_storage, counter>&) const { return {}; }

    std::expected<konsens::persistent_storage, std::error_code> start(const std::size_t i, const std::monostate) const
    {
        return konsens::persistent_storage::open(directories.at(i), map_size, effective_user_id);
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
    return {1, pid, peers, 0, election_ticks, resend_ticks};
}

bool is_transient(const std::error_code& code)
{
    return code == konsens::client_errc::recovering || code == konsens::client_errc::retry;
}

using entry_fields =
    std::tuple<konsens::entry_kind, std::vector<std::byte>, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t>;

entry_fields fields_of(const konsens::entry& e)
{
    return {e.kind, e.payload, e.session, e.request_number, e.client_id, e.pid};
}

bool compatible(const std::span<const konsens::entry> a, const std::span<const konsens::entry> b)
{
    return std::ranges::starts_with(a, b) || std::ranges::starts_with(b, a);
}

struct failure {
    std::string_view what;
    std::optional<std::error_code> code;
};

using outcome = std::expected<void, failure>;

struct tally {
    std::uint64_t rounds;
    std::uint64_t messages;
    std::uint64_t lost;
    std::uint64_t duplicates;
    std::uint64_t restarts;
    std::uint64_t power_losses;
    std::uint64_t leader_changes;
};

std::size_t channel_of(const konsens::message& m)
{
    const auto from = std::visit([](const auto& x) { return x.from; }, m);
    const auto to = std::visit([](const auto& x) { return x.to; }, m);
    return static_cast<std::size_t>((from - 1) * node_count + (to - 1));
}

template <typename Cluster>
struct cluster {
    using node_t = konsens::node<typename Cluster::storage_type, counter>;

    Cluster storage;
    std::array<std::optional<node_t>, node_count> nodes;
    std::array<std::optional<konsens::transaction>, node_count> pending;
    std::array<std::deque<std::variant<konsens::message, std::string>>, channel_count> channels;
    std::vector<konsens::client_reply> replies;
    std::mt19937_64 random;
    std::uint64_t loss;
    std::uint64_t duplicate;
    std::uint64_t nonce;
    std::optional<std::size_t> last_leader;
    tally counts;
    std::vector<std::pair<konsens::node_id, konsens::node_id>> disconnected;
    bool calls_reconnected;
    bool cbor;
    std::array<std::uint64_t, konsens::kind_count> kinds;

    node_t& at(const std::size_t i) { return nodes.at(i).value(); }
    const node_t& at(const std::size_t i) const { return nodes.at(i).value(); }

    bool happens(const std::uint64_t chance)
    {
        return std::uniform_int_distribution<std::uint64_t>{0, permille - 1}(random) < chance;
    }

    std::size_t pick_node() { return std::uniform_int_distribution<std::size_t>{0, node_count - 1}(random); }

    void add(const std::size_t i, const konsens::transaction t)
    {
        if (pending.at(i)) {
            *pending.at(i) += t;
        } else {
            pending.at(i) = t;
        }
    }

    std::expected<void, std::error_code> add(const std::size_t i,
                                             const std::expected<konsens::transaction, std::error_code>& t)
    {
        if (!t) [[unlikely]] {
            return std::unexpected(t.error());
        }
        add(i, *t);
        return {};
    }

    std::expected<void, std::error_code> send(konsens::message m)
    {
        counts.messages += 1;
        if (happens(loss)) {
            counts.lost += 1;
            if (calls_reconnected) {
                std::visit([this](const auto& x) { disconnected.emplace_back(x.from, x.to); }, m);
            }
            return {};
        }
        auto& channel = channels.at(channel_of(m));
        std::variant<konsens::message, std::string> sent{std::move(m)};
        if (cbor) {
            auto bytes = konsens::encode(std::get<konsens::message>(sent));
            if (!bytes) [[unlikely]] {
                return std::unexpected(bytes.error());
            }
            sent = std::move(*bytes);
        }
        if (happens(duplicate)) {
            counts.duplicates += 1;
            channel.push_back(sent);
        }
        channel.push_back(std::move(sent));
        return {};
    }

    std::expected<void, std::error_code> deliver(const std::size_t to,
                                                 std::vector<std::variant<konsens::message, std::string>> batch)
    {
        if (!cbor) {
            std::vector<konsens::message> messages;
            for (auto& sent : batch) {
                messages.push_back(std::get<konsens::message>(std::move(sent)));
            }
            return add(to, at(to).handle_incoming(std::move(messages)));
        }
        std::vector<konsens::received> decoded;
        for (auto& sent : batch) {
            auto r = konsens::decode(std::get<std::string>(std::move(sent)));
            if (!r) [[unlikely]] {
                return std::unexpected(r.error());
            }
            kinds.at(konsens::kind_of(*r)) += 1;
            decoded.push_back(std::move(*r));
        }
        return add(to, at(to).handle_incoming(std::move(decoded)));
    }

    std::expected<void, std::error_code> collect()
    {
        for (const auto& [from, to] : std::exchange(disconnected, {})) {
            if (auto r = add(from - 1, at(from - 1).reconnected(to)); !r) [[unlikely]] {
                return r;
            }
            if (auto r = add(to - 1, at(to - 1).reconnected(from)); !r) [[unlikely]] {
                return r;
            }
        }
        for (std::size_t i = 0; i < node_count; ++i) {
            auto& n = nodes.at(i);
            if (const auto t = std::exchange(pending.at(i), std::nullopt)) {
                if (auto r = n.value().commit(*t); !r) [[unlikely]] {
                    return r;
                }
            }
            for (auto& m : n.value().take_outgoing_messages()) {
                if (auto r = send(std::move(m)); !r) [[unlikely]] {
                    return r;
                }
            }
            for (auto& r : n.value().take_client_replies()) {
                replies.push_back(std::move(r));
            }
        }
        return {};
    }

    std::expected<void, std::error_code> start(const std::size_t i,
                                               std::expected<typename Cluster::storage_type, std::error_code> opened)
    {
        if (!opened) [[unlikely]] {
            return std::unexpected(opened.error());
        }
        nonce += 1;
        auto n = node_t::with(config_of(i + 1), std::move(*opened), counter{}, nonce);
        if (!n) [[unlikely]] {
            return std::unexpected(n.error());
        }
        nodes.at(i).emplace(std::move(*n));
        return {};
    }

    std::expected<void, std::error_code> start(const std::size_t i) { return start(i, storage.start(i, {})); }

    std::expected<void, std::error_code> restart(const std::size_t i)
    {
        auto kept = storage.stop(at(i));
        pending.at(i).reset();
        nodes.at(i).reset();
        if (auto r = start(i, storage.start(i, std::move(kept))); !r) [[unlikely]] {
            return r;
        }
        counts.restarts += 1;
        return collect();
    }

    std::expected<void, std::error_code> lose_power()
    {
        for (std::size_t i = 0; i < node_count; ++i) {
            if (auto r = restart(i); !r) [[unlikely]] {
                return r;
            }
        }
        counts.power_losses += 1;
        return {};
    }

    bool recovers() const
    {
        return std::ranges::any_of(nodes, [](const auto& n) { return n.value().get_sequence_paxos().recovering(); });
    }

    std::optional<std::size_t> leader() const
    {
        for (std::size_t i = 0; i < node_count; ++i) {
            if (at(i).get_sequence_paxos().get_state() == std::pair{konsens::role::leader, konsens::phase::accept}) {
                return i;
            }
        }
        return std::nullopt;
    }

    void count_leader_change()
    {
        const auto now = leader();
        if (!now || now == last_leader) {
            return;
        }
        if (last_leader) {
            counts.leader_changes += 1;
        }
        last_leader = now;
    }

    std::expected<void, std::error_code> run_round()
    {
        counts.rounds += 1;
        for (std::size_t i = 0; i < node_count; ++i) {
            if (auto r = add(i, at(i).tick()); !r) [[unlikely]] {
                return r;
            }
        }
        if (auto r = collect(); !r) [[unlikely]] {
            return r;
        }
        std::array<std::size_t, channel_count> busy{};
        for (;;) {
            const auto last = std::ranges::copy_if(std::views::iota(std::size_t{0}, channel_count), busy.begin(),
                                                   [this](const std::size_t i) { return !channels.at(i).empty(); })
                                  .out;
            const auto waiting = static_cast<std::size_t>(std::ranges::distance(busy.begin(), last));
            if (waiting == 0) {
                break;
            }
            const auto first = busy.at(std::uniform_int_distribution<std::size_t>{0, waiting - 1}(random));
            const auto to = first % node_count;
            auto size = std::uniform_int_distribution<std::size_t>{1, largest_batch}(random);
            std::vector<std::variant<konsens::message, std::string>> batch;
            for (auto from = first / node_count; size > 0; --size) {
                auto& channel = channels.at(from * node_count + to);
                batch.push_back(std::move(channel.front()));
                channel.pop_front();
                std::array<std::size_t, node_count> into{};
                const auto end = std::ranges::copy_if(std::views::iota(std::size_t{0}, node_count), into.begin(),
                                                      [this, to](const std::size_t f) {
                                                          return !channels.at(f * node_count + to).empty();
                                                      })
                                     .out;
                const auto senders = static_cast<std::size_t>(std::ranges::distance(into.begin(), end));
                if (senders == 0) {
                    break;
                }
                from = into.at(std::uniform_int_distribution<std::size_t>{0, senders - 1}(random));
            }
            if (auto r = deliver(to, std::move(batch)); !r) [[unlikely]] {
                return r;
            }
            if (auto r = collect(); !r) [[unlikely]] {
                return r;
            }
        }
        count_leader_change();
        return {};
    }

    bool agrees() const
    {
        const auto applied = at(0).get_applied_idx();
        return std::ranges::all_of(nodes, [applied](const auto& o) {
            const auto& n = o.value();
            const auto& sp = n.get_sequence_paxos();
            return !sp.recovering() && n.get_applied_idx() == applied && sp.get_decided_idx() == applied &&
                   sp.get_accepted_idx() == applied;
        });
    }
};

enum class step : std::uint8_t { registering, incrementing, reading, done };

struct client {
    step at;
    std::uint64_t session;
    std::uint64_t request_number;
    std::uint64_t read_floor;
    std::uint64_t latest_call;
    std::uint64_t sent_round;
    bool waiting;

    void advance_after_increment(const std::uint64_t increments, const std::uint64_t confirmed)
    {
        waiting = false;
        request_number += 1;
        if ((request_number - 1) % increments_between_reads == 0) {
            at = step::reading;
            read_floor = confirmed;
            return;
        }
        if (request_number > increments) {
            at = step::done;
        }
    }
};

struct call {
    std::size_t client;
    step kind;
    std::uint64_t request_number;
    std::uint64_t read_floor;
};

template <typename S>
std::expected<konsens::meta_record, std::error_code> read_meta(const S& storage)
{
    const auto t = storage.begin_read_only();
    if (!t) [[unlikely]] {
        return std::unexpected(t.error());
    }
    return t->get_meta();
}

template <typename Cluster>
struct stress {
    options given;
    cluster<Cluster> net;
    std::vector<client> clients;
    std::map<std::uint64_t, call> calls;
    std::map<std::pair<std::size_t, std::uint64_t>, std::uint64_t> first_replies;
    std::uint64_t confirmed;
    std::uint64_t reads_checked;
    std::uint64_t next_call;
    std::uint64_t round;
    std::uint64_t last_progress_round;
    std::vector<konsens::entry> decided_history;
    std::vector<konsens::ballot> decided_ballot;
    std::array<std::uint64_t, node_count> checked;
    std::array<std::uint64_t, node_count> last_decided;
    std::set<entry_fields> proposed;
    std::size_t proposed_checked;

    outcome check_decided_logs()
    {
        for (std::size_t i = 0; i < node_count; ++i) {
            const auto& sp = net.at(i).get_sequence_paxos();
            const auto m = read_meta(sp.get_internal_storage());
            if (!m) [[unlikely]] {
                return std::unexpected(failure{"read the meta record", m.error()});
            }
            const auto decided = m->decided_idx;
            if (decided < last_decided.at(i) || std::min(decided, m->log_len) < checked.at(i)) [[unlikely]] {
                return std::unexpected(failure{"S7: the decided index of a node fell", {}});
            }
            last_decided.at(i) = decided;
            const auto suffix = sp.read_decided_suffix(checked.at(i));
            if (!suffix) [[unlikely]] {
                return std::unexpected(failure{"read the decided log", suffix.error()});
            }
            for (const auto& e : *suffix) {
                const auto k = checked.at(i);
                if (k == decided_history.size()) {
                    decided_history.push_back(e);
                    decided_ballot.push_back(m->accepted_round);
                } else if (decided_history.at(k) != e) [[unlikely]] {
                    return std::unexpected(failure{"S1: two nodes decided different entries at one index", {}});
                }
                checked.at(i) = k + 1;
            }
        }
        return {};
    }

    outcome check_s2b() const
    {
        for (std::size_t i = 0; i < node_count; ++i) {
            const auto& sp = net.at(i).get_sequence_paxos();
            if (sp.get_state() != std::pair{konsens::role::leader, konsens::phase::accept}) {
                continue;
            }
            const auto n_leader = sp.get_leader_state().n_leader;
            const auto last = std::ranges::find_last_if(decided_ballot,
                                                        [n_leader](const konsens::ballot b) { return b <= n_leader; });
            const auto held = last.empty() ? 0 : decided_ballot.size() - last.size() + 1;
            const auto from = checked.at(i);
            if (from >= held) {
                continue;
            }
            const auto t = sp.get_internal_storage().begin_read_only();
            if (!t) [[unlikely]] {
                return std::unexpected(failure{"read the log", t.error()});
            }
            const auto log = t->get_entries(from, held);
            if (!log) [[unlikely]] {
                return std::unexpected(failure{"read the log", log.error()});
            }
            if (log->size() != held - from ||
                !std::ranges::equal(*log, std::span(decided_history).subspan(from, held - from))) [[unlikely]] {
                return std::unexpected(failure{"S2b: a leader misses a decided entry", {}});
            }
        }
        return {};
    }

    outcome check_s3()
    {
        for (; proposed_checked < decided_history.size(); ++proposed_checked) {
            if (!proposed.contains(fields_of(decided_history.at(proposed_checked)))) [[unlikely]] {
                return std::unexpected(failure{"S3: a decided entry was not proposed", {}});
            }
        }
        return {};
    }

    std::expected<std::vector<konsens::entry>, std::error_code> read_log_suffix(const std::size_t i,
                                                                                const std::uint64_t from) const
    {
        const auto t = net.at(i).get_sequence_paxos().get_internal_storage().begin_read_only();
        if (!t) [[unlikely]] {
            return std::unexpected(t.error());
        }
        const auto m = t->get_meta();
        if (!m) [[unlikely]] {
            return std::unexpected(m.error());
        }
        if (m->log_len < from) [[unlikely]] {
            return std::unexpected(std::make_error_code(std::errc::result_out_of_range));
        }
        return t->get_entries(from, m->log_len);
    }

    outcome check_s5() const
    {
        for (std::size_t i = 0; i < node_count; ++i) {
            for (std::size_t j = i + 1; j < node_count; ++j) {
                const auto n_accepted = net.at(i).get_sequence_paxos().get_accepted_round();
                if (n_accepted == konsens::ballot{} ||
                    n_accepted != net.at(j).get_sequence_paxos().get_accepted_round()) {
                    continue;
                }
                const auto from = std::min(checked.at(i), checked.at(j));
                const auto a = read_log_suffix(i, from);
                if (!a) [[unlikely]] {
                    return std::unexpected(failure{"read the log", a.error()});
                }
                const auto b = read_log_suffix(j, from);
                if (!b) [[unlikely]] {
                    return std::unexpected(failure{"read the log", b.error()});
                }
                if (!compatible(*a, *b)) [[unlikely]] {
                    return std::unexpected(failure{"S5: two logs with one accepted round differ", {}});
                }
            }
        }
        return {};
    }

    outcome check_invariants()
    {
        if (auto c = check_decided_logs(); !c) [[unlikely]] {
            return c;
        }
        if (auto c = check_s2b(); !c) [[unlikely]] {
            return c;
        }
        if (auto c = check_s3(); !c) [[unlikely]] {
            return c;
        }
        return check_s5();
    }

    outcome send_due_calls()
    {
        for (std::size_t i = 0; i < clients.size(); ++i) {
            auto& c = clients.at(i);
            if (c.at == step::done || (c.waiting && round - c.sent_round < call_timeout_rounds)) {
                continue;
            }
            const auto id = next_call++;
            calls.emplace(id, call{i, c.at, c.request_number, c.read_floor});
            c.latest_call = id;
            c.sent_round = round;
            c.waiting = true;
            const auto to = net.pick_node();
            if (c.at == step::registering) {
                proposed.insert(fields_of({konsens::entry_kind::register_client, {}, 0, 0, id, to + 1}));
            } else if (c.at == step::incrementing) {
                proposed.insert(fields_of({konsens::entry_kind::client_request, {}, c.session, c.request_number}));
            }
            auto& n = net.at(to);
            const auto sent = [&n, &c, id] {
                if (c.at == step::registering) {
                    return n.register_client(id);
                }
                if (c.at == step::incrementing) {
                    return n.client_request(id, c.session, c.request_number, {});
                }
                return n.client_query(id, {});
            }();
            if (!sent) [[unlikely]] {
                if (!is_transient(sent.error())) [[unlikely]] {
                    return std::unexpected(failure{"a node refused a call", sent.error()});
                }
                c.waiting = false;
            } else {
                net.add(to, *sent);
            }
        }
        return {};
    }

    outcome take_reply(const konsens::client_reply& r)
    {
        const auto& k = calls.at(r.client_id);
        auto& c = clients.at(k.client);
        const bool current = c.at == k.kind && c.request_number == k.request_number;
        if (!r.response) [[unlikely]] {
            const auto e = r.response.error();
            if (e == konsens::client_errc::session_expired) [[unlikely]] {
                return std::unexpected(failure{"a call got the reply session_expired", e});
            }
            if (e == konsens::client_errc::stale_request) {
                if (current && k.kind == step::incrementing) [[unlikely]] {
                    return std::unexpected(failure{"the current request got the reply stale_request", e});
                }
                return {};
            }
            if (!is_transient(e)) [[unlikely]] {
                return std::unexpected(failure{"a reply holds an error", e});
            }
            if (current && r.client_id == c.latest_call) {
                c.waiting = false;
            }
            return {};
        }
        if (k.kind == step::registering) {
            if (current) {
                c.session = r.session;
                c.at = step::incrementing;
                c.waiting = false;
            }
            return {};
        }
        const auto v = value_of(*r.response);
        if (k.kind == step::reading) {
            if (v < k.read_floor) [[unlikely]] {
                return std::unexpected(failure{"check 4: a read during the faults misses a confirmed increment", {}});
            }
            reads_checked += 1;
            if (current) {
                last_progress_round = round;
                c.waiting = false;
                c.at = c.request_number > given.increments ? step::done : step::incrementing;
            }
            return {};
        }
        const auto [it, first] = first_replies.try_emplace({k.client, k.request_number}, v);
        if (!first) {
            if (it->second != v) [[unlikely]] {
                return std::unexpected(failure{"check 1: one increment got two different replies", {}});
            }
            return {};
        }
        if (!current) [[unlikely]] {
            return std::unexpected(failure{"check 1: a reply came for a request that is not current", {}});
        }
        confirmed += 1;
        last_progress_round = round;
        c.advance_after_increment(given.increments, confirmed);
        return {};
    }

    outcome take_replies()
    {
        for (const auto& r : std::exchange(net.replies, {})) {
            if (auto t = take_reply(r); !t) [[unlikely]] {
                return t;
            }
        }
        return {};
    }

    outcome run_round()
    {
        if (auto r = net.run_round(); !r) [[unlikely]] {
            return std::unexpected(failure{"run a round", r.error()});
        }
        round += 1;
        if (auto c = check_invariants(); !c) [[unlikely]] {
            return c;
        }
        return take_replies();
    }

    bool clients_are_done() const
    {
        return std::ranges::all_of(clients, [](const client& c) { return c.at == step::done; });
    }

    outcome run_increments()
    {
        while (!clients_are_done()) {
            if (round - last_progress_round > rounds_without_progress_limit) [[unlikely]] {
                return std::unexpected(failure{"the phase \"increments\" reached its round limit", {}});
            }
            if (!net.recovers() && net.happens(given.restart)) {
                if (auto r = net.restart(net.pick_node()); !r) [[unlikely]] {
                    return std::unexpected(failure{"restart a node", r.error()});
                }
            } else if (!net.recovers() && net.happens(given.power_loss)) {
                if (auto r = net.lose_power(); !r) [[unlikely]] {
                    return std::unexpected(failure{"restart every node", r.error()});
                }
            }
            if (auto s = send_due_calls(); !s) [[unlikely]] {
                return s;
            }
            if (auto r = run_round(); !r) [[unlikely]] {
                return r;
            }
        }
        return {};
    }

    outcome run_agreement()
    {
        net.loss = 0;
        net.duplicate = 0;
        for (std::uint64_t r = 0; r < agreement_round_limit; ++r) {
            if (net.agrees()) {
                return {};
            }
            if (auto ran = run_round(); !ran) [[unlikely]] {
                return ran;
            }
        }
        return std::unexpected(failure{"the phase \"agreement\" reached its round limit", {}});
    }

    std::expected<std::uint64_t, failure> final_read(const std::size_t i)
    {
        std::optional<std::uint64_t> pending;
        std::uint64_t sent_round = 0;
        for (std::uint64_t r = 0; r < final_read_round_limit; ++r) {
            if (!pending || r - sent_round >= call_timeout_rounds) {
                const auto id = next_call++;
                pending.reset();
                if (auto s = net.at(i).client_query(id, {}); !s) [[unlikely]] {
                    if (!is_transient(s.error())) [[unlikely]] {
                        return std::unexpected(failure{"a node refused the final read", s.error()});
                    }
                } else {
                    net.add(i, *s);
                    pending = id;
                    sent_round = r;
                }
            }
            if (auto ran = net.run_round(); !ran) [[unlikely]] {
                return std::unexpected(failure{"run a round", ran.error()});
            }
            round += 1;
            if (auto c = check_invariants(); !c) [[unlikely]] {
                return std::unexpected(c.error());
            }
            std::optional<std::uint64_t> value;
            for (const auto& reply : std::exchange(net.replies, {})) {
                if (calls.contains(reply.client_id)) {
                    if (auto t = take_reply(reply); !t) [[unlikely]] {
                        return std::unexpected(t.error());
                    }
                    continue;
                }
                if (reply.client_id != pending) {
                    continue;
                }
                if (!reply.response) [[unlikely]] {
                    if (!is_transient(reply.response.error())) [[unlikely]] {
                        return std::unexpected(failure{"the final read got an error", reply.response.error()});
                    }
                    pending.reset();
                    continue;
                }
                value = value_of(*reply.response);
            }
            if (value) {
                return *value;
            }
        }
        return std::unexpected(failure{"the phase \"final read\" reached its round limit", {}});
    }
};

int fail(const std::uint64_t seed, const failure& f)
{
    if (f.code) {
        std::cerr << std::format("Seed {}: {}: {}.", seed, f.what, f.code->message()) << '\n';
    } else {
        std::cerr << std::format("Seed {}: {}.", seed, f.what) << '\n';
    }
    return EXIT_FAILURE;
}

std::uint64_t count_requests_decided_twice(const std::span<const konsens::entry> log)
{
    std::map<std::pair<std::uint64_t, std::uint64_t>, std::uint64_t> copies;
    for (const auto& e : log) {
        if (e.kind == konsens::entry_kind::client_request) {
            copies[{e.session, e.request_number}] += 1;
        }
    }
    return static_cast<std::uint64_t>(std::ranges::count_if(copies, [](const auto& c) { return c.second > 1; }));
}

template <typename Cluster>
int run(const options& given, Cluster storage)
{
    stress<Cluster> s{given,
                      cluster<Cluster>{std::move(storage),
                                       {},
                                       {},
                                       {},
                                       {},
                                       std::mt19937_64{given.seed},
                                       given.loss,
                                       given.duplicate,
                                       0,
                                       {},
                                       {},
                                       {},
                                       given.calls_reconnected,
                                       given.cbor,
                                       {}},
                      {},
                      {},
                      {},
                      0,
                      0,
                      1,
                      0,
                      0,
                      {},
                      {},
                      {},
                      {},
                      {},
                      0};
    for (std::size_t i = 0; i < node_count; ++i) {
        if (auto r = s.net.start(i); !r) [[unlikely]] {
            return fail(given.seed, {"start the nodes", r.error()});
        }
    }
    for (std::uint64_t i = 0; i < given.clients; ++i) {
        s.clients.push_back({step::registering, 0, 1, 0, 0, 0, false});
    }
    if (auto r = s.run_increments(); !r) [[unlikely]] {
        return fail(given.seed, r.error());
    }
    const auto load_rounds = s.round;
    if (auto r = s.run_agreement(); !r) [[unlikely]] {
        return fail(given.seed, r.error());
    }

    const auto total = given.clients * given.increments;
    std::vector<std::uint64_t> replies;
    for (const auto& [pair, v] : s.first_replies) {
        replies.push_back(v);
    }
    std::ranges::sort(replies);
    if (!std::ranges::equal(replies, std::views::iota(std::uint64_t{1}, total + 1))) [[unlikely]] {
        return fail(given.seed, {"check 1: the replies are not the numbers 1 to C*N, each once", {}});
    }
    std::cout << std::format("Check 1 (each increment executed once): passed. The replies are the numbers 1 to {}.",
                             total)
              << '\n';

    for (std::size_t i = 0; i < node_count; ++i) {
        const auto v = s.final_read(i);
        if (!v) [[unlikely]] {
            return fail(given.seed, v.error());
        }
        if (*v != total) [[unlikely]] {
            return fail(given.seed, {"check 2: a final read does not give C*N", {}});
        }
    }
    std::cout << std::format("Check 2 (final read): passed. The read at each node gives {}.", total) << '\n';

    const auto log = s.net.at(0).read_decided_suffix(0);
    if (!log) [[unlikely]] {
        return fail(given.seed, {"read the decided log", log.error()});
    }
    for (std::size_t i = 1; i < node_count; ++i) {
        const auto other = s.net.at(i).read_decided_suffix(0);
        if (!other) [[unlikely]] {
            return fail(given.seed, {"read the decided log", other.error()});
        }
        if (*other != *log) [[unlikely]] {
            return fail(given.seed, {"check 3: the decided logs are not equal", {}});
        }
    }
    std::cout << std::format("Check 3 (equal decided logs): passed. Each log holds {} entries.", log->size()) << '\n';

    std::cout << std::format("Check 4 (reads during the faults): passed. {} reads gave at least the count of "
                             "increments that were confirmed before the read started.",
                             s.reads_checked)
              << '\n';

    const auto twice = count_requests_decided_twice(*log);
    std::cout << std::format("Requests that the decided log holds more than once: {}.", twice) << '\n';
    if (twice == 0 && takes_defaults_apart_from_the_seed(given)) [[unlikely]] {
        return fail(given.seed, {"no request was decided twice, so check 1 showed nothing", {}});
    }

    const auto& t = s.net.counts;
    std::cout << std::format("Seed {}: {} rounds ({} with faults), {} messages, {} lost, {} duplicates, {} restarts, "
                             "{} restarts of every node, {} changes of leader.",
                             given.seed, t.rounds, load_rounds, t.messages, t.lost, t.duplicates, t.restarts,
                             t.power_losses, t.leader_changes)
              << '\n';
    if (given.cbor) {
        std::cout << "Messages through CBOR by kind:";
        for (std::size_t k = 0; k < s.net.kinds.size(); ++k) {
            std::cout << std::format(" {}:{}", k, s.net.kinds.at(k));
        }
        std::cout << '\n';
    }
    return EXIT_SUCCESS;
}

int run_on_lmdb(const options& given)
{
    const auto user_id = choose_the_user_id(geteuid(), given.allow_root_for_test);
    if (!user_id) [[unlikely]] {
        std::cerr << "The library refuses to run as root. Start the program as another user." << '\n';
        return exit_refused_root;
    }
    temporary_directories temporary;
    lmdb_cluster storage{{}, *user_id};
    for (auto& d : storage.directories) {
        d = temporary.create();
    }
    return run(given, std::move(storage));
}

}

int main(const int argc, char** const argv)
{
    try {
        const auto given = options_of(std::span<char* const>(argv, static_cast<std::size_t>(argc)));
        if (!given) [[unlikely]] {
            std::cerr << std::format("{}.\n{}", given.error(), usage) << '\n';
            return usage_exit_code;
        }
        return run_on_lmdb(*given);
    } catch (const std::exception& e) {
        std::cerr << std::format("The program ended with an exception: {}", e.what()) << '\n';
        return EXIT_FAILURE;
    }
}
