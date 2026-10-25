#include "konsens/selection.hpp"

#include <algorithm>
#include <ranges>

namespace konsens {

selection selection::with(const ballot n, const std::span<const node_id> nodes, const std::uint64_t pointer)
{
    selection s;
    s.n_ = n;
    s.nodes_.assign(nodes.begin(), nodes.end());
    std::ranges::sort(s.nodes_);
    s.pointer_ = pointer;
    return s;
}

void selection::count_answer(const node_id from, const answer& a)
{
    if (a.n != n_ || a.from_idx <= a.pointer || !std::ranges::contains(nodes_, from)) [[unlikely]] {
        return;
    }
    std::uint64_t last = a.from_idx - 1;
    for (const auto& r : a.results) {
        if (r.index <= last) [[unlikely]] {
            return;
        }
        last = r.index;
    }
    if (a.second_run > 0) {
        again_.insert({from, a.second_run});
    }
    if (const auto it = answers_.find(from);
        it != answers_.end() && it->second.pointer == a.pointer && a.from_idx >= it->second.from_idx) {
        auto& held = it->second;
        auto crc = held.base_crc;
        for (const auto& [index, r] : held.results) {
            if (index >= a.from_idx) {
                break;
            }
            crc = extend_base_crc(crc, index, compute_result_crc(index, r.result));
        }
        if (crc == a.base_crc) {
            held.results.erase(held.results.lower_bound(a.from_idx), held.results.end());
            for (const auto& r : a.results) {
                held.results.emplace(r.index, r);
            }
            return;
        }
    }
    counted_answer c{a.pointer, a.from_idx, a.base_crc, {}};
    for (const auto& r : a.results) {
        c.results.emplace(r.index, r);
    }
    answers_.insert_or_assign(from, std::move(c));
}

void selection::move_pointer(const std::uint64_t pointer, const std::span<const chosen_result> chosen)
{
    if (pointer <= pointer_) {
        return;
    }
    std::map<std::uint64_t, std::uint32_t> crcs;
    for (const auto& c : chosen) {
        crcs.emplace(c.chosen.index, compute_result_crc(c.chosen.index, c.chosen.result));
    }
    std::erase_if(selected_, [pointer, &crcs](auto& s) {
        if (s.first <= pointer) {
            return true;
        }
        auto& base = s.second.base;
        for (auto it = base.begin(); it != base.end() && it->first <= pointer; it = base.erase(it)) {
            const auto c = crcs.find(it->first);
            if (c == crcs.end() || c->second != it->second) {
                return true;
            }
        }
        return false;
    });
    std::erase_if(answers_, [pointer](const auto& a) { return a.second.pointer < pointer; });
    std::erase_if(tried_, [pointer](const auto& t) { return t.second <= pointer; });
    std::erase_if(again_, [pointer](const auto& t) { return t.second <= pointer; });
    pointer_ = pointer;
}

bool selection::holds_a_request(const leader_log& log, const std::uint64_t index) const
{
    return index > log.pointer && index - log.pointer <= log.entries.size() &&
           entry_is_a_request(log.entries[index - log.pointer - 1]);
}

std::span<const std::byte> selection::result_of(const node_id node, const std::uint64_t index) const
{
    return answers_.at(node).results.at(index).result;
}

bool selection::counts(const leader_log& log, const node_id node, const std::uint64_t index) const
{
    const auto it = answers_.find(node);
    if (it == answers_.end()) {
        return false;
    }
    const auto& a = it->second;
    if (a.pointer != pointer_ || log.pointer != pointer_ || index > log.decided_idx || !a.results.contains(index)) {
        return false;
    }
    std::uint32_t crc = 0;
    for (auto k = pointer_ + 1; k < index; ++k) {
        if (!holds_a_request(log, k)) {
            continue;
        }
        const auto s = selected_.find(k);
        if (s == selected_.end()) {
            return false;
        }
        if (k < a.from_idx) {
            crc = extend_base_crc(crc, k, compute_result_crc(k, s->second.chosen.result));
            continue;
        }
        const auto r = a.results.find(k);
        if (r == a.results.end() || r->second.result != s->second.chosen.result) {
            return false;
        }
    }
    return crc == a.base_crc;
}

bool selection::holds(const leader_log& log, const std::uint64_t index) const
{
    const auto it = selected_.find(index);
    if (it == selected_.end() || log.pointer != pointer_) {
        return false;
    }
    std::size_t requests = 0;
    for (auto k = pointer_ + 1; k < index; ++k) {
        if (!holds_a_request(log, k)) {
            continue;
        }
        const auto s = selected_.find(k);
        const auto b = it->second.base.find(k);
        if (s == selected_.end() || b == it->second.base.end() ||
            b->second != compute_result_crc(k, s->second.chosen.result)) {
            return false;
        }
        requests += 1;
    }
    return requests == it->second.base.size();
}

std::vector<node_id> selection::find_the_counted_nodes(const leader_log& log, const std::uint64_t index) const
{
    std::vector<node_id> counted;
    for (const auto j : nodes_) {
        if (counts(log, j, index)) {
            counted.push_back(j);
        }
    }
    return counted;
}

std::size_t selection::count_the_agreeing(const std::span<const node_id> counted, const std::uint64_t index,
                                          const std::span<const std::byte> result) const
{
    return static_cast<std::size_t>(std::ranges::count_if(
        counted, [this, index, result](const node_id j) { return results_agree(result_of(j, index), result); }));
}

std::vector<try_again_target> selection::select_results(const leader_log& log)
{
    std::vector<try_again_target> targets;
    if (log.pointer != pointer_) {
        return targets;
    }
    for (auto q = pointer_ + 1; q <= std::min(log.decided_idx, pointer_ + log.entries.size()); ++q) {
        if (!holds_a_request(log, q) || holds(log, q)) {
            continue;
        }
        const auto counted = find_the_counted_nodes(log, q);
        if (counted.empty()) {
            break;
        }
        node_id candidate = counted.front();
        std::size_t most = 0;
        for (const auto c : counted) {
            const auto agreeing = count_the_agreeing(counted, q, result_of(c, q));
            if (agreeing > most) {
                most = agreeing;
                candidate = c;
            }
        }
        if (most < majority()) {
            break;
        }
        const auto chosen = result_of(candidate, q);
        for (const auto j : counted) {
            if (!results_agree(result_of(j, q), chosen) && tried_.insert({j, q}).second) {
                targets.push_back({j, q});
            }
        }
        selected_result s{answers_.at(candidate).results.at(q), candidate, {}};
        for (auto k = pointer_ + 1; k < q; ++k) {
            if (holds_a_request(log, k)) {
                s.base.emplace(k, compute_result_crc(k, selected_.at(k).chosen.result));
            }
        }
        selected_.insert_or_assign(q, std::move(s));
    }
    return targets;
}

std::vector<node_id> selection::find_the_nodes_out(const leader_log& log, const std::uint64_t index) const
{
    std::vector<node_id> out;
    const auto& chosen = selected_.at(index).chosen.result;
    for (const auto x : nodes_) {
        if (tried_.contains({x, index}) && again_.contains({x, index}) && counts(log, x, index) &&
            !results_agree(result_of(x, index), chosen)) {
            out.push_back(x);
        }
    }
    return out;
}

std::optional<entry> selection::find_the_verdict_entry(const leader_log& log) const
{
    if (log.pointer != pointer_) {
        return std::nullopt;
    }
    const auto end = pointer_ + log.entries.size();
    std::vector<std::uint64_t> requests;
    for (auto k = pointer_ + 1; k <= end; ++k) {
        if (k > log.decided_idx && entry_is_a_verdict(log.entries[k - pointer_ - 1])) {
            return std::nullopt;
        }
        if (holds_a_request(log, k)) {
            requests.push_back(k);
        }
    }
    if (requests.empty()) {
        return std::nullopt;
    }
    std::vector<chosen_result> list;
    for (const auto q : requests) {
        if (!holds(log, q)) {
            break;
        }
        list.push_back({selected_.at(q).chosen, selected_.at(q).node, find_the_nodes_out(log, q)});
    }
    if (!list.empty()) {
        return entry{.kind = entry_kind::results,
                     .payload = encode_chosen_results(list),
                     .from_idx = pointer_ + 1,
                     .to_idx = list.back().chosen.index};
    }
    const auto q0 = requests.front();
    std::vector<node_id> counted;
    for (const auto j : find_the_counted_nodes(log, q0)) {
        if (!tried_.contains({j, q0}) || again_.contains({j, q0})) {
            counted.push_back(j);
        }
    }
    const auto uncounted = nodes_.size() - counted.size();
    if (uncounted >= majority()) {
        return std::nullopt;
    }
    for (const auto c : counted) {
        if (count_the_agreeing(counted, q0, result_of(c, q0)) + uncounted >= majority()) {
            return std::nullopt;
        }
    }
    std::vector<chosen_result> answers;
    for (const auto j : counted) {
        answers.push_back({answers_.at(j).results.at(q0), j, {}});
    }
    return entry{.kind = entry_kind::rejected, .payload = encode_chosen_results(answers), .from_idx = q0};
}

bool selection::asked_to_run_again(const node_id node, const std::uint64_t index) const
{
    return tried_.contains({node, index});
}

bool selection::ran_again(const node_id node, const std::uint64_t index) const
{
    return again_.contains({node, index});
}

}
