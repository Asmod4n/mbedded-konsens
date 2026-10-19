#pragma once

#include <algorithm>
#include <cstdint>
#include <expected>
#include <span>
#include <system_error>
#include <variant>
#include <vector>

#include "konsens/entry_range.hpp"
#include "konsens/storage.hpp"

namespace konsens {

class memory_storage {
public:
    std::expected<void, std::error_code> write_atomically(std::span<const storage_op> ops)
    {
        for (const auto& op : ops) {
            std::visit([this](const auto& o) { apply(o); }, op);
        }
        return {};
    }

    std::expected<std::vector<entry>, std::error_code> get_entries(std::uint64_t from,
                                                                   std::uint64_t to) const
    {
        throw_if_from_after_to(from, to);
        if (from >= log_.size()) {
            return std::vector<entry>{};
        }
        const auto part = std::span(log_).subspan(from, std::min<std::uint64_t>(to, log_.size()) - from);
        return std::vector<entry>(part.begin(), part.end());
    }

    std::expected<std::uint64_t, std::error_code> get_log_len() const { return log_.size(); }
    std::expected<ballot, std::error_code> get_promise() const { return promise_; }
    std::expected<std::uint64_t, std::error_code> get_decided_idx() const { return decided_idx_; }
    std::expected<ballot, std::error_code> get_accepted_round() const { return accepted_round_; }

private:
    void apply(const append_entries& o) { log_.insert(log_.end(), o.entries.begin(), o.entries.end()); }

    void apply(const append_on_prefix& o)
    {
        if (o.from_idx < log_.size()) {
            log_.resize(o.from_idx);
        }
        log_.insert(log_.end(), o.entries.begin(), o.entries.end());
    }

    void apply(const set_promise& o) { promise_ = o.promise; }
    void apply(const set_decided_index& o) { decided_idx_ = o.decided_idx; }
    void apply(const set_accepted_round& o) { accepted_round_ = o.accepted_round; }

    std::vector<entry> log_;
    ballot promise_{};
    std::uint64_t decided_idx_{};
    ballot accepted_round_{};
};

}
