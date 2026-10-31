#include "konsens/wire.hpp"

#include <memory>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <cbor/item_size.hpp>
#include <cbor/schema.hpp>
#include <cbor/structs.hpp>

namespace konsens {

namespace {

struct header {
    std::uint8_t method;
    std::uint8_t kind;
    node_id from;
    node_id to;
};

}

}

CBOR_DEFINE_TYPE(konsens::header, cbor::tag{35699}, CBOR_MEMBER(konsens::header, method),
                 CBOR_MEMBER(konsens::header, kind), CBOR_MEMBER(konsens::header, from),
                 CBOR_MEMBER(konsens::header, to));

namespace konsens {

namespace {

class wire_error_category final : public std::error_category {
public:
    const char* name() const noexcept override { return "konsens.wire"; }

    std::string message(const int code) const override
    {
        switch (static_cast<wire_errc>(code)) {
        case wire_errc::extraneous_data:
            return "Bytes follow the body of the message";
        case wire_errc::method_not_allowed:
            return "The method of the header is not POST";
        case wire_errc::unknown_kind:
            return "The kind of the header names no kind of message";
        case wire_errc::unknown_entry_kind:
            return "An entry has a kind that is not a value of entry_kind";
        case wire_errc::entry_field_of_another_kind:
            return "An entry has a field that its kind does not carry";
        }
        return "Unknown wire error";
    }
};

template <std::size_t I>
using body_of_kind = std::conditional_t<(I < paxos_kind_count), std::variant_alternative<I, paxos_msg>,
                                        std::variant_alternative<I - paxos_kind_count, heartbeat_msg>>::type;

std::error_code check_entries(const std::span<const entry> entries) noexcept
{
    for (const auto& e : entries) {
        if (!field_count_of(e.kind)) [[unlikely]] {
            return wire_errc::unknown_entry_kind;
        }
        if (entry_has_a_field_of_another_kind(e)) [[unlikely]] {
            return wire_errc::entry_field_of_another_kind;
        }
    }
    return {};
}

template <typename T>
std::error_code check_entries_of(const T&) noexcept
{
    return {};
}

std::error_code check_entries_of(const promise& p) noexcept
{
    if (!p.log_sync) {
        return {};
    }
    return check_entries(p.log_sync->suffix);
}

std::error_code check_entries_of(const accept_sync& a) noexcept
{
    return check_entries(a.log_sync.suffix);
}

std::error_code check_entries_of(const accept_decide& a) noexcept
{
    return check_entries(a.entries);
}

std::error_code check_entries_of(const proposal_forward& p) noexcept
{
    return check_entries(p.entries);
}

std::error_code check_entries_of(const build_response& b) noexcept
{
    return check_entries(b.entries);
}

std::error_code check_entries_of(const work& w) noexcept
{
    if (entry_is_done(w.request)) [[unlikely]] {
        return wire_errc::entry_field_of_another_kind;
    }
    if (const auto e = check_entries(w.base.suffix)) [[unlikely]] {
        return e;
    }
    return check_entries(std::span(&w.request, 1));
}

template <std::size_t I>
std::expected<received, std::error_code> decode_body(const std::shared_ptr<const std::string>& owner,
                                                     const std::string_view body, const header& h)
{
    using body_type = body_of_kind<I>;
    auto decoded = cbor::schema<body_type>::decode(owner, body);
    if (!decoded) [[unlikely]] {
        return std::unexpected(decoded.error());
    }
    if (const auto fault = check_entries_of(**decoded); fault) [[unlikely]] {
        return std::unexpected(fault);
    }
    return received{h.from, h.to, received_msg{std::in_place_index<I>, std::move(*decoded)}};
}

template <std::size_t... I>
std::expected<received, std::error_code> decode_body_of_kind(const std::shared_ptr<const std::string>& owner,
                                                             const std::string_view body, const header& h,
                                                             std::index_sequence<I...>)
{
    std::expected<received, std::error_code> result = std::unexpected(make_error_code(wire_errc::unknown_kind));
    static_cast<void>(((h.kind == I && (result = decode_body<I>(owner, body, h), true)) || ...));
    return result;
}

template <typename Body>
std::expected<std::string, std::error_code> encode_parts(const header& h, const Body& body)
{
    auto out = cbor::schema<header>::encode(h);
    if (!out) [[unlikely]] {
        return std::unexpected(out.error());
    }
    if (const auto appended = cbor::schema<Body>::encode(body, *out); !appended) [[unlikely]] {
        return std::unexpected(appended.error());
    }
    return std::move(*out);
}

}

const std::error_category& wire_category() noexcept
{
    static const wire_error_category category;
    return category;
}

std::error_code make_error_code(const wire_errc code) noexcept
{
    return {static_cast<int>(code), wire_category()};
}

std::uint8_t kind_of(const message& m) noexcept
{
    const auto index = std::visit(
        [](const auto& x) {
            if constexpr (std::is_same_v<std::remove_cvref_t<decltype(x)>, paxos_message>) {
                return x.msg.index();
            } else {
                return paxos_kind_count + x.msg.index();
            }
        },
        m);
    return static_cast<std::uint8_t>(index);
}

std::expected<std::string, std::error_code> encode(const message& m)
{
    return std::visit(
        [kind = kind_of(m)](const auto& x) {
            return std::visit(
                [&x, kind](const auto& body) { return encode_parts(header{post, kind, x.from, x.to}, body); }, x.msg);
        },
        m);
}

std::uint8_t kind_of(const received& r) noexcept
{
    return static_cast<std::uint8_t>(r.msg.index());
}

std::expected<received, std::error_code> decode(std::string bytes)
{
    const auto owner = std::make_shared<const std::string>(std::move(bytes));
    const std::string_view all = *owner;
    const auto header_size = cbor::item_size(all);
    if (!header_size) [[unlikely]] {
        return std::unexpected(header_size.error());
    }
    const auto body = all.substr(*header_size);
    const auto body_size = cbor::item_size(body);
    if (!body_size) [[unlikely]] {
        return std::unexpected(body_size.error());
    }
    if (*body_size != body.size()) [[unlikely]] {
        return std::unexpected(wire_errc::extraneous_data);
    }
    const auto decoded = cbor::schema<header>::decode(owner, all.substr(0, *header_size));
    if (!decoded) [[unlikely]] {
        return std::unexpected(decoded.error());
    }
    const header& h = **decoded;
    if (h.method != post) [[unlikely]] {
        return std::unexpected(wire_errc::method_not_allowed);
    }
    if (h.kind >= kind_count) [[unlikely]] {
        return std::unexpected(wire_errc::unknown_kind);
    }
    return decode_body_of_kind(owner, body, h, std::make_index_sequence<kind_count>{});
}

std::expected<std::vector<std::byte>, std::error_code> encode(const cluster_config& c)
{
    const auto out = cbor::structs<cluster_config>::encode(c);
    if (!out) [[unlikely]] {
        return std::unexpected(out.error());
    }
    const auto bytes = std::as_bytes(std::span(*out));
    return std::vector<std::byte>(bytes.begin(), bytes.end());
}

std::expected<cluster_config, std::error_code> decode_cluster_config(const std::span<const std::byte> payload)
{
    std::string bytes(payload.size(), '\0');
    std::ranges::transform(payload, bytes.begin(), [](const std::byte b) { return static_cast<char>(b); });
    const auto owner = std::make_shared<const std::string>(std::move(bytes));
    const auto size = cbor::item_size(*owner);
    if (!size) [[unlikely]] {
        return std::unexpected(size.error());
    }
    if (*size != owner->size()) [[unlikely]] {
        return std::unexpected(wire_errc::extraneous_data);
    }
    const auto decoded = cbor::structs<cluster_config>::decode(owner);
    if (!decoded) [[unlikely]] {
        return std::unexpected(decoded.error());
    }
    return **decoded;
}
}
