#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <map>
#include <optional>
#include <span>
#include <system_error>
#include <vector>

#include "konsens/storage.hpp"

namespace konsens {

template <typename T>
class write_set {
public:
    explicit write_set(const T& txn) noexcept : txn_(&txn) {}

    std::expected<std::optional<std::span<const std::byte>>, std::error_code>
    find(const std::span<const std::byte> key) const
    {
        if (const auto it = data_.find(key); it != data_.end()) {
            if (!it->second) {
                return std::optional<std::span<const std::byte>>{};
            }
            return std::optional<std::span<const std::byte>>{std::span<const std::byte>(*it->second)};
        }
        return txn_->find(key);
    }

    std::expected<std::optional<session_record>, std::error_code> find_session(const std::uint64_t session) const
    {
        if (const auto it = sessions_.find(session); it != sessions_.end()) {
            return it->second;
        }
        return txn_->find_session(session);
    }

    std::expected<void, std::error_code> insert_or_assign(const std::span<const std::byte> key,
                                                          const std::span<const std::byte> value)
    {
        std::vector<std::byte> v(value.begin(), value.end());
        if (const auto it = data_.find(key); it != data_.end()) {
            it->second = std::move(v);
        } else {
            data_.emplace(std::vector<std::byte>(key.begin(), key.end()), std::move(v));
        }
        return {};
    }

    std::expected<void, std::error_code> erase(const std::span<const std::byte> key)
    {
        if (const auto it = data_.find(key); it != data_.end()) {
            it->second.reset();
        } else {
            data_.emplace(std::vector<std::byte>(key.begin(), key.end()), std::nullopt);
        }
        return {};
    }

    std::expected<void, std::error_code> insert_or_assign_session(const std::uint64_t session,
                                                                  const session_record& record)
    {
        sessions_.insert_or_assign(session, record);
        return {};
    }

    std::expected<void, std::error_code> erase_session(const std::uint64_t session)
    {
        sessions_.insert_or_assign(session, std::nullopt);
        return {};
    }

private:
    struct bytes_less {
        using is_transparent = void;
        bool operator()(const std::span<const std::byte> a, const std::span<const std::byte> b) const
        {
            return std::ranges::lexicographical_compare(a, b);
        }
    };

    const T* txn_;
    std::map<std::vector<std::byte>, std::optional<std::vector<std::byte>>, bytes_less> data_;
    std::map<std::uint64_t, std::optional<session_record>> sessions_;
};

}
