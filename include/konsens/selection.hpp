#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <utility>
#include <vector>

#include "konsens/message.hpp"
#include "konsens/storage.hpp"
#include "konsens/verdict.hpp"

namespace konsens {

struct counted_answer {
    std::uint64_t pointer;
    std::uint64_t from_idx;
    std::uint32_t base_crc;
    std::map<std::uint64_t, indexed_result> results;

    friend bool operator==(const counted_answer&, const counted_answer&) = default;
};

struct selected_result {
    indexed_result chosen;
    node_id node;
    std::map<std::uint64_t, std::uint32_t> base;

    friend bool operator==(const selected_result&, const selected_result&) = default;
};

struct leader_log {
    std::uint64_t pointer;
    std::span<const std::reference_wrapper<const entry>> entries;
};

struct try_again_target {
    node_id node;
    std::uint64_t index;

    friend bool operator==(const try_again_target&, const try_again_target&) = default;
};

class selection {
public:
    static selection with(ballot n, std::span<const node_id> nodes, std::uint64_t pointer,
                          std::optional<float_rule> rule);

    ballot get_ballot() const { return n_; }
    std::uint64_t get_pointer() const { return pointer_; }
    const std::map<std::uint64_t, selected_result>& get_selected() const { return selected_; }
    const std::map<node_id, counted_answer>& get_answers() const { return answers_; }

    void count_answer(node_id from, const answer& a);
    void move_pointer(std::uint64_t pointer, std::span<const indexed_result> chosen);
    [[nodiscard]] std::vector<try_again_target> select_results(const leader_log& log);
    [[nodiscard]] std::vector<entry> find_the_done_records(const leader_log& log) const;
    [[nodiscard]] bool asked_to_run_again(node_id node, std::uint64_t index) const;
    [[nodiscard]] bool ran_again(node_id node, std::uint64_t index) const;
    void count_a_refusal(node_id node, std::uint64_t index);
    void count_an_appended_done_record(std::uint64_t index);
    [[nodiscard]] std::vector<std::uint64_t> find_the_entries_that_a_majority_refused() const;

private:
    [[nodiscard]] bool holds_a_request(const leader_log& log, std::uint64_t index) const;
    [[nodiscard]] bool counts(const leader_log& log, node_id node, std::uint64_t index) const;
    [[nodiscard]] bool holds(const leader_log& log, std::uint64_t index) const;
    [[nodiscard]] std::vector<node_id> find_the_counted_nodes(const leader_log& log, std::uint64_t index) const;
    [[nodiscard]] std::size_t count_the_agreeing(std::span<const node_id> counted, std::uint64_t index,
                                                 std::span<const std::byte> result) const;
    [[nodiscard]] std::vector<node_id> find_the_nodes_out(const leader_log& log, std::uint64_t index) const;
    [[nodiscard]] std::optional<entry> find_the_rejected_record(const leader_log& log, std::uint64_t index) const;
    [[nodiscard]] std::span<const std::byte> result_of(node_id node, std::uint64_t index) const;
    [[nodiscard]] std::size_t majority() const { return nodes_.size() / 2 + 1; }

    ballot n_;
    std::vector<node_id> nodes_;
    std::uint64_t pointer_;
    std::optional<float_rule> rule_;
    std::map<node_id, counted_answer> answers_;
    std::set<std::pair<node_id, std::uint64_t>> tried_;
    std::set<std::pair<node_id, std::uint64_t>> again_;
    std::map<std::uint64_t, selected_result> selected_;
    std::set<std::uint64_t> appended_;
    std::set<std::pair<std::uint64_t, node_id>> refusals_;
};

}
