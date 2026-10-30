#pragma once

#include <concepts>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include "konsens/persistent_storage.hpp"
#include "konsens/storage.hpp"
#include "konsens/write_set.hpp"

namespace konsens {

struct mutation {
    std::vector<std::byte> key;
    std::optional<std::vector<std::byte>> value;

    friend bool operator==(const mutation&, const mutation&) = default;
};

struct applied {
    std::vector<std::byte> reply;
    std::vector<mutation> mutations;

    friend bool operator==(const applied&, const applied&) = default;
};

template <typename V>
concept state_view = requires(const V& v, std::span<const std::byte> key) {
    { v.find(key) } -> std::same_as<std::optional<std::span<const std::byte>>>;
};

template <typename T>
class transaction_view {
public:
    explicit transaction_view(const T& txn) noexcept : txn_(&txn) {}

    std::optional<std::span<const std::byte>> find(const std::span<const std::byte> key) const
    {
        auto found = txn_->find(key);
        if (!found) [[unlikely]] {
            throw read_error(found.error(), "konsens: the storage failed to read the state");
        }
        return *found;
    }

private:
    const T* txn_;
};

using write_view = transaction_view<write_set<persistent_storage::transaction>>;
using read_view = transaction_view<persistent_storage::read_only_transaction>;

static_assert(state_view<write_view> && state_view<read_view>);

template <typename M>
concept state_machine = std::movable<M> && requires(const M& m, std::span<const std::byte> payload, const write_view& w,
                                                    const read_view& r) {
    { m.apply(payload, w) } -> std::same_as<applied>;
    { m.query(payload, r) } -> std::same_as<std::vector<std::byte>>;
};

}
