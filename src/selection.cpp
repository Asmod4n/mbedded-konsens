#include "konsens/selection.hpp"

#include <algorithm>
#include <ranges>

namespace konsens {

selection selection::with(const ballot n, const std::span<const node_id> nodes, const std::uint64_t pointer,
                          const std::optional<float_rule> rule)
{
    selection s;
    s.n_ = n;
    s.nodes_.assign(nodes.begin(), nodes.end());
    std::ranges::sort(s.nodes_);
    s.pointer_ = pointer;
    s.rule_ = rule;
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

void selection::move_pointer(const std::uint64_t pointer, const std::span<const indexed_result> chosen)
{
    if (pointer <= pointer_) {
        return;
    }
    std::map<std::uint64_t, std::uint32_t> crcs;
    for (const auto& c : chosen) {
        crcs.emplace(c.index, compute_result_crc(c.index, c.result));
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
    if (a.pointer != pointer_ || log.pointer != pointer_ || !a.results.contains(index)) {
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
        counted, [this, index, result](const node_id j) { return results_agree(result_of(j, index), result, rule_); }));
}

std::vector<try_again_target> selection::select_results(const leader_log& log)
{
    std::vector<try_again_target> targets;
    if (log.pointer != pointer_) {
        return targets;
    }
    for (auto q = pointer_ + 1; q <= pointer_ + log.entries.size(); ++q) {
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
            if (!results_agree(result_of(j, q), chosen, rule_) && tried_.insert({j, q}).second) {
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
            !results_agree(result_of(x, index), chosen, rule_)) {
            out.push_back(x);
        }
    }
    return out;
}

std::optional<entry> selection::find_the_rejected_record(const leader_log& log, const std::uint64_t index) const
{
    std::vector<node_id> counted;
    for (const auto j : find_the_counted_nodes(log, index)) {
        if (!tried_.contains({j, index}) || again_.contains({j, index})) {
            counted.push_back(j);
        }
    }
    const auto uncounted = nodes_.size() - counted.size();
    if (uncounted >= majority()) {
        return std::nullopt;
    }
    for (const auto c : counted) {
        if (count_the_agreeing(counted, index, result_of(c, index)) + uncounted >= majority()) {
            return std::nullopt;
        }
    }
    std::vector<chosen_result> answers;
    for (const auto j : counted) {
        answers.push_back({answers_.at(j).results.at(index), j, {}});
    }
    entry e = log.entries[index - pointer_ - 1].get();
    e.rejected = true;
    e.result = encode_chosen_results(answers);
    e.result_crc = compute_result_crc(index, e.result);
    return e;
}

std::vector<entry> selection::find_the_done_records(const leader_log& log) const
{
    std::vector<entry> done;
    if (log.pointer != pointer_) {
        return done;
    }
    for (auto k = pointer_ + 1; k <= pointer_ + log.entries.size(); ++k) {
        const entry& task = log.entries[k - pointer_ - 1].get();
        if (!entry_is_a_request(task)) {
            done.push_back(task);
            continue;
        }
        if (!holds(log, k)) {
            if (done.empty() || std::ranges::none_of(done, entry_is_a_request)) {
                if (auto rejected = find_the_rejected_record(log, k)) {
                    done.push_back(std::move(*rejected));
                }
            }
            break;
        }
        const auto& s = selected_.at(k);
        auto e = task;
        e.result = s.chosen.result;
        e.result_crc = s.chosen.crc;
        e.out = find_the_nodes_out(log, k);
        done.push_back(std::move(e));
    }
    return done;
}

bool selection::asked_to_run_again(const node_id node, const std::uint64_t index) const
{
    return tried_.contains({node, index});
}

bool selection::ran_again(const node_id node, const std::uint64_t index) const
{
    return again_.contains({node, index});
}

void selection::count_a_refusal(const node_id node, const std::uint64_t index)
{
    if (std::ranges::contains(nodes_, node)) {
        refusals_.insert({index, node});
    }
}

void selection::count_an_appended_done_record(const std::uint64_t index)
{
    appended_.insert(index);
}

std::vector<std::uint64_t> selection::find_the_entries_that_a_majority_refused() const
{
    std::vector<std::uint64_t> refused;
    for (auto first = refusals_.begin(); first != refusals_.end();) {
        const auto index = first->first;
        const auto end = refusals_.lower_bound({index + 1, 0});
        if (appended_.contains(index) && static_cast<std::size_t>(std::distance(first, end)) >= majority()) {
            refused.push_back(index);
        }
        first = end;
    }
    return refused;
}

}
