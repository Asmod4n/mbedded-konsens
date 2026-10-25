#include "konsens/verdict.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <stdfloat>
#include <string>
#include <string_view>

#include <cbor/decode.hpp>
#include <cbor/encode.hpp>
#include <cbor/item_size.hpp>
#include <cbor/structs.hpp>
#include <crc32c/crc32c.h>

#include "byte_order.hpp"

namespace konsens {

namespace {

template <typename T>
std::vector<std::byte> encode_or_throw(const T& value)
{
    const auto encoded = cbor::structs<T>::encode(value);
    if (!encoded) [[unlikely]] {
        throw std::logic_error("konsens: a result or a list of results has no CBOR encoding");
    }
    const auto bytes = std::as_bytes(std::span(*encoded));
    return {bytes.begin(), bytes.end()};
}

template <typename T>
std::optional<T> decode_bytes(const std::span<const std::byte> encoded)
{
    auto owner = std::make_shared<const std::string>(reinterpret_cast<const char*>(encoded.data()), encoded.size());
    const std::string_view view = *owner;
    const auto decoded = cbor::structs<T>::decode(std::shared_ptr<void const>(std::move(owner)), view);
    if (!decoded) [[unlikely]] {
        return std::nullopt;
    }
    return **decoded;
}

bool holds_a_request(const std::uint64_t first_index, const std::span<const entry> entries, const std::uint64_t index)
{
    return index >= first_index && index - first_index < entries.size() &&
           entry_is_a_request(entries[index - first_index]);
}

bool holds_a_request_in(const std::uint64_t first_index, const std::span<const entry> entries,
                        const std::uint64_t from, const std::uint64_t to)
{
    for (auto index = std::max(from, first_index); index <= to; ++index) {
        if (holds_a_request(first_index, entries, index)) {
            return true;
        }
    }
    return false;
}

}

std::vector<std::byte> encode_result(const request_result& r)
{
    return encode_or_throw(r);
}

std::optional<request_result> decode_result(const std::span<const std::byte> encoded)
{
    return decode_bytes<request_result>(encoded);
}

std::uint32_t compute_result_crc(const std::uint64_t index, const std::span<const std::byte> encoded) noexcept
{
    const auto index_bytes = to_little_endian(index);
    return crc32c::Extend(crc32c::Crc32c(reinterpret_cast<const std::uint8_t*>(index_bytes.data()), index_bytes.size()),
                          reinterpret_cast<const std::uint8_t*>(encoded.data()), encoded.size());
}

std::vector<std::byte> encode_chosen_results(const std::span<const chosen_result> list)
{
    return encode_or_throw(std::vector<chosen_result>(list.begin(), list.end()));
}

std::optional<std::vector<chosen_result>> decode_chosen_results(const std::span<const std::byte> payload)
{
    return decode_bytes<std::vector<chosen_result>>(payload);
}

verdicts find_the_verdicts(const std::uint64_t pointer, const std::uint64_t first_index,
                           const std::span<const entry> entries)
{
    if (first_index == 0 || pointer >= first_index) [[unlikely]] {
        throw std::logic_error("konsens: find_the_verdicts takes a first index above the pointer");
    }
    verdicts v{pointer, {}, {}, 0};
    for (std::uint64_t k = 0; k < entries.size(); ++k) {
        const auto& e = entries[k];
        const auto m = first_index + k;
        if (e.kind == entry_kind::results && e.from_idx == v.pointer + 1 && e.to_idx < m &&
            holds_a_request_in(first_index, entries, e.from_idx, e.to_idx)) {
            const auto list = decode_chosen_results(e.payload);
            if (!list) [[unlikely]] {
                continue;
            }
            std::uint64_t last = 0;
            for (const auto& item : *list) {
                const auto q = item.chosen.index;
                if (q > last && q >= e.from_idx && q <= e.to_idx && holds_a_request(first_index, entries, q)) {
                    v.chosen.push_back(item);
                    last = q;
                }
            }
            v.pointer = e.to_idx;
        } else if (e.kind == entry_kind::rejected && e.from_idx > v.pointer && e.from_idx < m &&
                   holds_a_request(first_index, entries, e.from_idx) &&
                   !holds_a_request_in(first_index, entries, v.pointer + 1, e.from_idx - 1)) {
            v.voids.push_back(e.from_idx);
            v.pointer = e.from_idx;
        }
    }
    v.applied_idx = first_index - 1 + entries.size();
    for (std::uint64_t k = 0; k < entries.size(); ++k) {
        if (first_index + k > v.pointer && entry_is_a_request(entries[k])) {
            v.applied_idx = first_index + k - 1;
            break;
        }
    }
    return v;
}

std::uint32_t extend_base_crc(const std::uint32_t base_crc, const std::uint64_t index,
                              const std::uint32_t result_crc) noexcept
{
    std::array<std::byte, sizeof base_crc + sizeof index + sizeof result_crc> head{};
    std::ranges::copy(to_little_endian(result_crc),
                      std::ranges::copy(to_little_endian(index),
                                        std::ranges::copy(to_little_endian(base_crc), head.begin()).out)
                          .out);
    return crc32c::Crc32c(reinterpret_cast<const std::uint8_t*>(head.data()), head.size());
}

namespace {

constexpr std::size_t float_chars_size = 64;
constexpr std::uint64_t encoded_cbor_data_item_tag = 24;
constexpr std::size_t data_items_per_map_entry = 2;
constexpr std::uint64_t first_float_typed_array_tag = 80;
constexpr std::uint64_t last_float_typed_array_tag = 87;
constexpr std::uint64_t little_endian_field = 0b100;
constexpr std::uint64_t length_field = 0b11;
constexpr std::size_t smallest_float_size = 2;

std::string_view chars_of(const std::span<const std::byte> bytes)
{
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

bool is_a_float(const cbor::item& n)
{
    return n.type == cbor::type::half_float || n.type == cbor::type::single_float ||
           n.type == cbor::type::double_float;
}

double float_of(const cbor::item& n)
{
    switch (n.type) {
    case cbor::type::half_float:
        return *n.get<cbor::type::half_float>();
    case cbor::type::single_float:
        return *n.get<cbor::type::single_float>();
    default:
        return *n.get<cbor::type::double_float>();
    }
}

std::span<const cbor::item> children_of(const cbor::item& n)
{
    return n.type == cbor::type::array ? *n.get<cbor::type::array>() : *n.get<cbor::type::map>();
}

const cbor::item* embedded_in(const cbor::owning<cbor::item>& tree, const cbor::item& n)
{
    const auto tagged = n.get<cbor::type::tag>();
    if (!tagged || tagged->first != encoded_cbor_data_item_tag) {
        return nullptr;
    }
    const auto embedded = cbor::encoded_cbor_data_item(tree, n);
    return embedded ? &embedded->get() : nullptr;
}

bool is_a_map_key(const cbor::item& container, const std::size_t position)
{
    return container.type == cbor::type::map && position % data_items_per_map_entry == 0;
}

bool is_a_float_typed_array(const cbor::item& n)
{
    if (n.type != cbor::type::typed_array) {
        return false;
    }
    const auto number = n.get<cbor::type::typed_array>()->first;
    return number >= first_float_typed_array_tag && number <= last_float_typed_array_tag;
}

template <typename F>
double element_as(const std::span<const std::byte> element, const bool little_endian)
{
    std::array<std::byte, sizeof(F)> bytes{};
    std::ranges::copy(element, bytes.begin());
    if (little_endian != (std::endian::native == std::endian::little)) {
        std::ranges::reverse(bytes);
    }
    return static_cast<double>(std::bit_cast<F>(bytes));
}

template <typename F>
void append_element_as(const double value, const bool little_endian, std::vector<std::byte>& out)
{
    auto bytes = std::bit_cast<std::array<std::byte, sizeof(F)>>(static_cast<F>(value));
    if (little_endian != (std::endian::native == std::endian::little)) {
        std::ranges::reverse(bytes);
    }
    out.insert(out.end(), bytes.begin(), bytes.end());
}

std::size_t element_size_of(const std::uint64_t number)
{
    return smallest_float_size << (number & length_field);
}

std::vector<double> floats_in(const cbor::item& typed_array)
{
    const auto [number, content] = *typed_array.get<cbor::type::typed_array>();
    const auto bytes = *content->get<cbor::type::byte_string>();
    const auto size = element_size_of(number);
    const bool little = (number & little_endian_field) != 0;
    std::vector<double> values;
    for (std::size_t at = 0; at + size <= bytes.size(); at += size) {
        const auto element = bytes.subspan(at, size);
        switch (size) {
        case sizeof(std::float16_t):
            values.push_back(element_as<std::float16_t>(element, little));
            break;
        case sizeof(float):
            values.push_back(element_as<float>(element, little));
            break;
        case sizeof(double):
            values.push_back(element_as<double>(element, little));
            break;
        default:
            values.push_back(element_as<std::float128_t>(element, little));
            break;
        }
    }
    return values;
}

std::vector<std::byte> typed_array_bytes_of(const std::uint64_t number, const std::span<const double> values)
{
    const bool little = (number & little_endian_field) != 0;
    std::vector<std::byte> out;
    for (const auto v : values) {
        switch (element_size_of(number)) {
        case sizeof(std::float16_t):
            append_element_as<std::float16_t>(v, little, out);
            break;
        case sizeof(float):
            append_element_as<float>(v, little, out);
            break;
        case sizeof(double):
            append_element_as<double>(v, little, out);
            break;
        default:
            append_element_as<std::float128_t>(v, little, out);
            break;
        }
    }
    return out;
}

bool holds_a_float(const cbor::owning<cbor::item>& tree, const cbor::item& n)
{
    if (is_a_float(n) || is_a_float_typed_array(n)) {
        return true;
    }
    switch (n.type) {
    case cbor::type::array:
    case cbor::type::map: {
        const auto children = children_of(n);
        for (std::size_t k = 0; k < children.size(); ++k) {
            if (!is_a_map_key(n, k) && holds_a_float(tree, children[k])) {
                return true;
            }
        }
        return false;
    }
    case cbor::type::tag:
        if (const auto* const e = embedded_in(tree, n)) {
            return holds_a_float(tree, *e);
        }
        return holds_a_float(tree, *n.get<cbor::type::tag>()->second);
    default:
        return false;
    }
}

std::optional<cbor::owning<cbor::item>> decode_one_data_item(const std::span<const std::byte> encoded)
{
    if (!holds_one_data_item(encoded)) {
        return std::nullopt;
    }
    auto tree = cbor::decode(chars_of(encoded));
    if (!tree) [[unlikely]] {
        return std::nullopt;
    }
    return std::move(*tree);
}

std::vector<std::byte> encoded_bytes_of(const cbor::item& n)
{
    std::vector<std::byte> out;
    if (!cbor::encode(n, out)) [[unlikely]] {
        throw std::logic_error("konsens: a decoded CBOR item has no encoding");
    }
    return out;
}

template <typename Write>
void write_with_an_encoder(std::vector<std::byte>& out, const Write write)
{
    cbor::encoder e(out);
    if (!write(e) || !e.finish()) [[unlikely]] {
        throw std::logic_error("konsens: the CBOR encoder refused an item");
    }
}

double power_of_ten(const int exponent)
{
    const auto text = "1e" + std::to_string(exponent);
    double value = exponent < 0 ? 0.0 : std::numeric_limits<double>::infinity();
    static_cast<void>(std::from_chars(text.data(), text.data() + text.size(), value));
    return value;
}

double round_half_to_even(const double value, const std::uint32_t float_denominator)
{
    if (!std::isfinite(value) || float_denominator > std::numeric_limits<double>::max_digits10) {
        return value;
    }
    std::array<char, float_chars_size> text{};
    const auto written = std::to_chars(text.data(), text.data() + text.size(), value, std::chars_format::scientific,
                                       static_cast<int>(float_denominator));
    if (written.ec != std::errc{}) [[unlikely]] {
        throw std::logic_error("konsens: a float has no scientific form in the buffer");
    }
    double rounded = value;
    static_cast<void>(std::from_chars(text.data(), written.ptr, rounded));
    return rounded;
}

int decimal_exponent_of(const double value, const std::uint32_t float_denominator)
{
    std::array<char, float_chars_size> text{};
    const auto written = std::to_chars(text.data(), text.data() + text.size(), value, std::chars_format::scientific,
                                       static_cast<int>(float_denominator));
    if (written.ec != std::errc{}) [[unlikely]] {
        throw std::logic_error("konsens: a float has no scientific form in the buffer");
    }
    const std::string_view form(text.data(), written.ptr);
    const auto e = form.find('e');
    const auto sign = form[e + 1] == '+' ? e + 2 : e + 1;
    int exponent = 0;
    static_cast<void>(std::from_chars(form.data() + sign, form.data() + form.size(), exponent));
    return exponent;
}

double unit_in_the_last_place(const double value)
{
    const auto magnitude = std::abs(value);
    return std::nextafter(magnitude, std::numeric_limits<double>::infinity()) - magnitude;
}

void write_the_float(const double value, std::vector<std::byte>& out)
{
    write_with_an_encoder(out, [value](auto& e) {
        if (std::isnan(value)) {
            return e.floating_point(std::numeric_limits<std::float16_t>::quiet_NaN());
        }
        if (static_cast<double>(static_cast<std::float16_t>(value)) == value) {
            return e.floating_point(static_cast<std::float16_t>(value));
        }
        if (static_cast<double>(static_cast<float>(value)) == value) {
            return e.floating_point(static_cast<float>(value));
        }
        return e.floating_point(value);
    });
}

void write_rounded(const cbor::owning<cbor::item>& tree, const cbor::item& n, const std::uint32_t float_denominator,
                   std::vector<std::byte>& out)
{
    if (!holds_a_float(tree, n)) {
        const auto bytes = encoded_bytes_of(n);
        out.insert(out.end(), bytes.begin(), bytes.end());
        return;
    }
    if (is_a_float(n)) {
        write_the_float(round_half_to_even(float_of(n), float_denominator), out);
        return;
    }
    if (is_a_float_typed_array(n)) {
        const auto number = n.get<cbor::type::typed_array>()->first;
        auto values = floats_in(n);
        for (auto& v : values) {
            v = round_half_to_even(v, float_denominator);
        }
        const auto bytes = typed_array_bytes_of(number, values);
        write_with_an_encoder(out, [number, &bytes](auto& e) {
            return e.tag(number).and_then([&e, &bytes] { return e.byte_string(std::span<const std::byte>(bytes)); });
        });
        return;
    }
    if (n.type == cbor::type::array || n.type == cbor::type::map) {
        const auto children = children_of(n);
        write_with_an_encoder(out, [&n, &children](auto& e) {
            return n.type == cbor::type::array ? e.array(children.size()) : e.map(children.size() / data_items_per_map_entry);
        });
        for (std::size_t k = 0; k < children.size(); ++k) {
            if (is_a_map_key(n, k)) {
                const auto key = encoded_bytes_of(children[k]);
                out.insert(out.end(), key.begin(), key.end());
            } else {
                write_rounded(tree, children[k], float_denominator, out);
            }
        }
        return;
    }
    const auto tagged = *n.get<cbor::type::tag>();
    if (const auto* const embedded = embedded_in(tree, n)) {
        std::vector<std::byte> inner;
        write_rounded(tree, *embedded, float_denominator, inner);
        write_with_an_encoder(out, [&tagged, &inner](auto& e) {
            return e.tag(tagged.first).and_then([&e, &inner] { return e.byte_string(std::span<const std::byte>(inner)); });
        });
        return;
    }
    write_with_an_encoder(out, [&tagged](auto& e) { return e.tag(tagged.first); });
    write_rounded(tree, *tagged.second, float_denominator, out);
}

bool items_agree(const cbor::owning<cbor::item>& ta, const cbor::item& a, const cbor::owning<cbor::item>& tb,
                 const cbor::item& b, const float_rule& rule)
{
    if (is_a_float(a) && is_a_float(b)) {
        return floats_agree(float_of(a), float_of(b), rule);
    }
    if (!holds_a_float(ta, a) && !holds_a_float(tb, b)) {
        return encoded_bytes_of(a) == encoded_bytes_of(b);
    }
    if (a.type != b.type) {
        return false;
    }
    if (is_a_float_typed_array(a) && is_a_float_typed_array(b)) {
        const auto fa = floats_in(a);
        const auto fb = floats_in(b);
        return a.get<cbor::type::typed_array>()->first == b.get<cbor::type::typed_array>()->first &&
               std::ranges::equal(fa, fb, [&rule](const double x, const double y) { return floats_agree(x, y, rule); });
    }
    if (a.type == cbor::type::array || a.type == cbor::type::map) {
        const auto ca = children_of(a);
        const auto cb = children_of(b);
        if (ca.size() != cb.size()) {
            return false;
        }
        for (std::size_t k = 0; k < ca.size(); ++k) {
            const bool agree = is_a_map_key(a, k) ? encoded_bytes_of(ca[k]) == encoded_bytes_of(cb[k])
                                                  : items_agree(ta, ca[k], tb, cb[k], rule);
            if (!agree) {
                return false;
            }
        }
        return true;
    }
    if (a.type != cbor::type::tag || a.get<cbor::type::tag>()->first != b.get<cbor::type::tag>()->first) {
        return false;
    }
    const auto* const ea = embedded_in(ta, a);
    const auto* const eb = embedded_in(tb, b);
    if (ea != nullptr && eb != nullptr) {
        return items_agree(ta, *ea, tb, *eb, rule);
    }
    return ea == nullptr && eb == nullptr &&
           items_agree(ta, *a.get<cbor::type::tag>()->second, tb, *b.get<cbor::type::tag>()->second, rule);
}

}

bool holds_one_data_item(const std::span<const std::byte> encoded)
{
    const auto size = cbor::item_size(chars_of(encoded));
    return size && *size == encoded.size();
}

std::optional<std::vector<std::byte>> round_each_float(const std::span<const std::byte> data_item,
                                                       const std::uint32_t float_denominator)
{
    const auto tree = decode_one_data_item(data_item);
    if (!tree) [[unlikely]] {
        return std::nullopt;
    }
    std::vector<std::byte> out;
    write_rounded(*tree, **tree, float_denominator, out);
    return out;
}

bool floats_agree(const double a, const double b, const float_rule& rule)
{
    if (std::isnan(a) || std::isnan(b)) {
        return std::isnan(a) && std::isnan(b);
    }
    if (a == b) {
        return true;
    }
    if (!std::isfinite(a) || !std::isfinite(b)) {
        return false;
    }
    const auto larger = std::max(std::abs(a), std::abs(b));
    auto allowed = larger * power_of_ten(1 - static_cast<int>(rule.float_digits));
    if (rule.float_denominator && *rule.float_denominator <= std::numeric_limits<double>::max_digits10) {
        const auto step =
            power_of_ten(decimal_exponent_of(larger, *rule.float_denominator) - static_cast<int>(*rule.float_denominator));
        allowed = std::max(allowed, step + unit_in_the_last_place(a) + unit_in_the_last_place(b));
    }
    return std::abs(a - b) <= allowed;
}

bool data_items_agree(const std::span<const std::byte> a, const std::span<const std::byte> b, const float_rule& rule)
{
    const auto ta = decode_one_data_item(a);
    const auto tb = decode_one_data_item(b);
    if (!ta || !tb) {
        return std::ranges::equal(a, b);
    }
    return items_agree(*ta, **ta, *tb, **tb, rule);
}

bool results_agree(const std::span<const std::byte> a, const std::span<const std::byte> b,
                   const std::optional<float_rule>& rule)
{
    if (!rule || std::ranges::equal(a, b)) {
        return std::ranges::equal(a, b);
    }
    const auto ra = decode_result(a);
    const auto rb = decode_result(b);
    if (!ra || !rb || ra->error != rb->error || ra->mutations.size() != rb->mutations.size() ||
        !data_items_agree(ra->reply, rb->reply, *rule)) {
        return false;
    }
    return std::ranges::equal(ra->mutations, rb->mutations, [&rule](const mutation& x, const mutation& y) {
        return x.key == y.key && x.value.has_value() == y.value.has_value() &&
               (!x.value || data_items_agree(*x.value, *y.value, *rule));
    });
}

}
