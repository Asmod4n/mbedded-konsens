#include "konsens/selection.hpp"

#include <algorithm>
#include <ranges>
#include <stdexcept>

namespace konsens {

selection selection::with(const ballot n, const std::span<const node_id> nodes, const std::uint64_t pointer,
                          const std::optional<float_rule> rule, const std::span<const entry> taken)
{
    selection s;
    s.n_ = n;
    s.nodes_.assign(nodes.begin(), nodes.end());
    std::ranges::sort(s.nodes_);
    s.pointer_ = pointer;
    s.rule_ = rule;
    s.taken_.assign(taken.begin(), taken.end());
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
        for (const auto& [index, result_crc] : held.result_crcs) {
            if (index >= a.from_idx) {
                break;
            }
            crc = extend_base_crc(crc, index, result_crc);
        }
        if (crc == a.base_crc) {
            held.results.erase(held.results.lower_bound(a.from_idx), held.results.end());
            held.result_crcs.erase(held.result_crcs.lower_bound(a.from_idx), held.result_crcs.end());
            for (const auto& r : a.results) {
                held.results.emplace(r.index, r);
                held.result_crcs.emplace(r.index, compute_result_crc(r.index, r.result));
            }
            return;
        }
    }
    counted_answer c{a.pointer, a.from_idx, a.base_crc, {}, {}};
    for (const auto& r : a.results) {
        c.results.emplace(r.index, r);
        c.result_crcs.emplace(r.index, compute_result_crc(r.index, r.result));
    }
    answers_.insert_or_assign(from, std::move(c));
}

void selection::move_pointer(const std::uint64_t pointer, const std::span<const indexed_result> chosen)
{
    if (pointer <= pointer_) {
        return;
    }
    std::uint32_t chain = 0;
    std::uint64_t last = 0;
    for (const auto& c : chosen) {
        if (c.index <= last) [[unlikely]] {
            throw std::logic_error("konsens: move_pointer takes the chosen results in ascending order");
        }
        last = c.index;
        if (c.index > pointer_ && c.index <= pointer) {
            chain = extend_base_crc(chain, c.index, compute_result_crc(c.index, c.result));
        }
    }
    std::uint32_t rebased = 0;
    for (auto it = selected_.begin(); it != selected_.end();) {
        if (it->first <= pointer) {
            it = selected_.erase(it);
            continue;
        }
        const auto index = it->first;
        const auto result_crc = it->second.result_crc;
        const bool holds = it->second.base_crc == chain;
        if (holds) {
            it->second.base_crc = rebased;
        }
        chain = extend_base_crc(chain, index, result_crc);
        rebased = extend_base_crc(rebased, index, result_crc);
        it = holds ? std::next(it) : selected_.erase(it);
    }
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

const entry* selection::taken_at(const std::uint64_t index) const
{
    if (index <= pointer_ || index - pointer_ > taken_.size()) {
        return nullptr;
    }
    return &taken_[index - pointer_ - 1];
}

const selected_result* selection::selection_at(const std::uint64_t index) const
{
    const auto it = selected_.find(index);
    return it == selected_.end() ? nullptr : &it->second;
}

selection::count_walk selection::begin_the_count(const leader_log& log, const node_id node) const
{
    const auto it = answers_.find(node);
    if (it == answers_.end() || it->second.pointer != pointer_ || log.pointer != pointer_) {
        return {nullptr, 0, false};
    }
    return {&it->second, 0, true};
}

void selection::count_walk::count_the_selection(const std::uint64_t index, const selected_result* const selected)
{
    if (!agrees) {
        return;
    }
    if (selected == nullptr) {
        agrees = false;
        return;
    }
    if (index < answer->from_idx) {
        base_crc = extend_base_crc(base_crc, index, selected->result_crc);
        return;
    }
    const auto r = answer->results.find(index);
    agrees = r != answer->results.end() && r->second.result == selected->chosen.result;
}

bool selection::count_walk::counts(const std::uint64_t index) const
{
    return agrees && answer->results.contains(index) && base_crc == answer->base_crc;
}

bool selection::counts(const leader_log& log, const node_id node, const std::uint64_t index) const
{
    auto w = begin_the_count(log, node);
    for (auto k = pointer_ + 1; k < index && w.agrees; ++k) {
        if (holds_a_request(log, k)) {
            w.count_the_selection(k, selection_at(k));
        }
    }
    return w.counts(index);
}

bool selection::holds_on(const std::uint64_t index, const std::uint32_t base_crc) const
{
    const auto* s = selection_at(index);
    return s != nullptr && s->base_crc == base_crc;
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
    std::vector<count_walk> walks;
    walks.reserve(nodes_.size());
    for (const auto j : nodes_) {
        walks.push_back(begin_the_count(log, j));
    }
    std::uint32_t chain = 0;
    for (auto q = pointer_ + 1; q <= pointer_ + log.entries.size(); ++q) {
        if (!holds_a_request(log, q)) {
            continue;
        }
        if (!holds_on(q, chain)) {
            std::vector<node_id> counted;
            for (std::size_t i = 0; i < nodes_.size(); ++i) {
                if (walks[i].counts(q)) {
                    counted.push_back(nodes_[i]);
                }
            }
            if (counted.empty()) {
                break;
            }
            const auto* taken = taken_at(q);
            if (taken != nullptr && entry_is_a_request(*taken) && !taken->rejected &&
                count_the_agreeing(counted, q, taken->result) >= majority()) {
                selected_.insert_or_assign(
                    q, selected_result{
                           {q, taken->result, taken->result_crc}, compute_result_crc(q, taken->result), 0, chain});
                kept_.insert(q);
                chain = extend_base_crc(chain, q, selected_.at(q).result_crc);
                for (auto& w : walks) {
                    w.count_the_selection(q, selection_at(q));
                }
                continue;
            }
            kept_.erase(q);
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
            const auto& a = answers_.at(candidate);
            selected_.insert_or_assign(q, selected_result{a.results.at(q), a.result_crcs.at(q), candidate, chain});
        }
        const auto* s = selection_at(q);
        chain = extend_base_crc(chain, q, s->result_crc);
        for (auto& w : walks) {
            w.count_the_selection(q, s);
        }
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
    std::uint32_t chain = 0;
    for (auto k = pointer_ + 1; k <= pointer_ + log.entries.size(); ++k) {
        const entry& task = log.entries[k - pointer_ - 1].get();
        if (!entry_is_a_request(task)) {
            done.push_back(task);
            continue;
        }
        if (!holds_on(k, chain)) {
            if (auto rejected = find_the_rejected_record(log, k)) {
                const auto* taken = taken_at(k);
                done.push_back(taken != nullptr && taken->rejected ? *taken : std::move(*rejected));
            }
            break;
        }
        const auto& s = selected_.at(k);
        chain = extend_base_crc(chain, k, s.result_crc);
        if (kept_.contains(k)) {
            done.push_back(*taken_at(k));
            continue;
        }
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
}
