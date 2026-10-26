#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include <crc32c/crc32c.h>

#include "konsens/storage.hpp"
#include "konsens/verdict.hpp"

// Provisional names of Execute-Verify (doc/plan-execute-verify.md), made in
// this tree, for the owner to decide: entry_is_a_request, entry_is_done,
// request_of; the entry fields result, result_crc, out and rejected;
// request_result, indexed_result, chosen_result and their members;
// encode_result, decode_result, compute_result_crc, encode_chosen_results,
// decode_chosen_results; extend_base_crc; src/verdict.cpp.

namespace {

using konsens::chosen_result;
using konsens::entry;
using konsens::entry_kind;

std::vector<std::byte> bytes_of(const std::initializer_list<std::uint8_t> values)
{
    std::vector<std::byte> out;
    for (const auto v : values) {
        out.push_back(std::byte{v});
    }
    return out;
}

chosen_result chosen_at(const std::uint64_t index, const std::uint8_t value)
{
    const auto bytes = bytes_of({value});
    return {{index, bytes, konsens::compute_result_crc(index, bytes)}, 1, {}};
}

}

// Plan 2.1.1: register_client, client_request and expire_session are
// request entries; normal and stop_sign are not. Every member of entry_kind
// is checked.
TEST_CASE("each entry kind is a request entry or not")
{
    struct kind_class {
        entry_kind kind;
        bool request;
    };
    const std::array<kind_class, 5> table{{
        {entry_kind::normal, false},
        {entry_kind::stop_sign, false},
        {entry_kind::register_client, true},
        {entry_kind::client_request, true},
        {entry_kind::expire_session, true},
    }};
    for (const auto& [kind, is_request] : table) {
        CAPTURE(int{std::to_underlying(kind)});
        CHECK(konsens::entry_is_a_request(entry{kind}) == is_request);
    }
}

// Plan-one-environment 3.3: a request is done when it holds a result or is
// rejected. request_of gives the request without its result, as the work
// carried it.
TEST_CASE("a request is done when it holds a result or is rejected")
{
    const entry work{.kind = entry_kind::client_request, .payload = bytes_of({1}), .session = 2, .request_number = 3};
    CHECK_FALSE(konsens::entry_is_done(work));
    auto with_result = work;
    with_result.result = bytes_of({4});
    with_result.result_crc = 5;
    with_result.out = {6};
    CHECK(konsens::entry_is_done(with_result));
    CHECK(konsens::request_of(with_result) == work);
    auto rejected = work;
    rejected.rejected = true;
    CHECK(konsens::entry_is_done(rejected));
    CHECK(konsens::request_of(rejected) == work);
}

// Plan 2.2.1: a result is the reply and the mutations in CBOR. A result
// that goes into the log or onto the wire must come back the same, with an
// erase (no value) kept apart from an empty value.
TEST_CASE("a result and a list of chosen results come back from their encoding")
{
    const konsens::request_result r{
        0, bytes_of({1, 2}), {{bytes_of({'k'}), bytes_of({})}, {bytes_of({'e'}), std::nullopt}, {bytes_of({'v'}), bytes_of({9})}}};
    const auto encoded = konsens::encode_result(r);
    CHECK(konsens::decode_result(encoded) == r);
    const std::vector<chosen_result> list{{{3, encoded, 7}, 2, {1, 3}}, chosen_at(4, 5)};
    CHECK(konsens::decode_chosen_results(konsens::encode_chosen_results(list)) == list);
    CHECK_FALSE(konsens::decode_result(bytes_of({0xff})).has_value());
}

// Plan 2.1.2: the CRC32C of a result as the node that ran it computed it.
// The CRC also covers the index, so a result that a fault moved to another
// index does not check. The expectation is CRC32C over the index in 8 bytes
// little endian and then the encoded result, computed here by hand.
TEST_CASE("the CRC of a result covers its index and its bytes")
{
    const auto encoded = bytes_of({0xa0, 0x01});
    const std::array<std::uint8_t, 10> record{5, 0, 0, 0, 0, 0, 0, 0, 0xa0, 0x01};
    CHECK(konsens::compute_result_crc(5, encoded) == crc32c::Crc32c(record.data(), record.size()));
    CHECK(konsens::compute_result_crc(6, encoded) != konsens::compute_result_crc(5, encoded));
}

// Plan 2.5.2: base_crc is a CRC over the own results before an index. Each
// step takes the CRC before it, the index and the CRC of the result: 4, 8
// and 4 bytes little endian, as computed here by hand.
TEST_CASE("the base CRC folds the index and the CRC of each result")
{
    const std::array<std::uint8_t, 16> record{1, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0};
    CHECK(konsens::extend_base_crc(1, 2, 3) == crc32c::Crc32c(record.data(), record.size()));
}

namespace {

// RFC 8949 3.3: a double is the initial byte 0xfb and the 8 bytes of the
// binary64 value in network byte order.
std::vector<std::byte> double_item(const double value)
{
    const auto bits = std::bit_cast<std::uint64_t>(value);
    std::vector<std::byte> out{std::byte{0xfb}};
    for (int shift = 56; shift >= 0; shift -= 8) {
        out.push_back(static_cast<std::byte>(bits >> shift));
    }
    return out;
}

std::vector<std::byte> rounded(const std::vector<std::byte>& item, const std::uint32_t float_denominator)
{
    const auto r = konsens::round_each_float(item, float_denominator);
    REQUIRE(r.has_value());
    return *r;
}

constexpr konsens::float_rule twelve_digits{12, std::nullopt};
constexpr konsens::float_rule two_decimals{12, 2};

}

// Plan 2.2b: the library takes a reply or a value only as one encoded CBOR
// data item. A head without its content, and two items, are no such item.
TEST_CASE("holds_one_data_item takes exactly one well-formed item")
{
    CHECK(konsens::holds_one_data_item(bytes_of({0x01})));
    CHECK(konsens::holds_one_data_item(bytes_of({0x61, 0x61})));
    CHECK_FALSE(konsens::holds_one_data_item(bytes_of({0x61})));
    CHECK_FALSE(konsens::holds_one_data_item(bytes_of({0x01, 0x01})));
    CHECK_FALSE(konsens::holds_one_data_item({}));
}

// Plan 2.2a: round half to even. 0.125, 0.375, 2.5 and 3.5 are exact in
// binary, so each one is a tie. The written form is the shortest float of
// RFC 8949 4.2.2 that holds the value: 2.0 and 4.0 fit a half (RFC 8949
// Appendix A: 2.0 is not listed, 1.5 is 0xf93e00, 4.0 is 0xf94400 by the
// same binary16 layout).
TEST_CASE("a tie rounds to the even digit")
{
    CHECK(rounded(double_item(0.125), 1) == double_item(0.12));
    CHECK(rounded(double_item(0.375), 1) == double_item(0.38));
    CHECK(rounded(double_item(2.5), 0) == bytes_of({0xf9, 0x40, 0x00}));
    CHECK(rounded(double_item(3.5), 0) == bytes_of({0xf9, 0x44, 0x00}));
    CHECK(rounded(double_item(1.5), 1) == bytes_of({0xf9, 0x3e, 0x00}));
}

// Plan 2.2a: two nodes near an edge round to two neighbouring steps. 9.9949
// gives 9.99, and 9.9951 gives 1.00e+01. The compare allows one step of the
// coarser value, so the two agree. Two steps away from an edge do not.
TEST_CASE("two values one step apart near an edge agree")
{
    const auto low = rounded(double_item(9.9949), 2);
    const auto high = rounded(double_item(9.9951), 2);
    CHECK(low == double_item(9.99));
    CHECK(high == bytes_of({0xf9, 0x49, 0x00}));
    CHECK(konsens::data_items_agree(low, high, two_decimals));
    CHECK(konsens::floats_agree(9.99, 10.0, two_decimals));
    CHECK_FALSE(konsens::floats_agree(1.23, 1.25, two_decimals));
    CHECK_FALSE(konsens::floats_agree(9.99, 10.2, two_decimals));
    CHECK_FALSE(konsens::floats_agree(9.99, 10.0, twelve_digits));
}

// Plan 2.2.2: two floats agree when they are equal to float_digits
// significant digits.
TEST_CASE("floats agree to float_digits significant digits")
{
    CHECK(konsens::floats_agree(1.0, 1.0 + 1e-12, twelve_digits));
    CHECK(konsens::floats_agree(-3e200, -3e200 * (1 + 1e-13), twelve_digits));
    CHECK_FALSE(konsens::floats_agree(1.0, 1.0001, twelve_digits));
    CHECK_FALSE(konsens::floats_agree(1.0, -1.0, twelve_digits));
}

// Plan 2.2.2: NaN agrees only with NaN. A rounded NaN is the half NaN
// 0xf97e00 of RFC 8949 4.2.2.
TEST_CASE("NaN agrees only with NaN")
{
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    CHECK(konsens::floats_agree(nan, -nan, twelve_digits));
    CHECK_FALSE(konsens::floats_agree(nan, 1.0, twelve_digits));
    CHECK_FALSE(konsens::floats_agree(0.0, nan, two_decimals));
    CHECK(rounded(double_item(nan), 2) == bytes_of({0xf9, 0x7e, 0x00}));
    CHECK(konsens::data_items_agree(bytes_of({0xf9, 0x7e, 0x00}), double_item(nan), twelve_digits));
}

// Plan 2.2.2: +0 and -0 agree. The rounding keeps the sign: RFC 8949
// Appendix A gives 0xf90000 for 0.0 and 0xf98000 for -0.0.
TEST_CASE("plus zero and minus zero agree and keep their sign")
{
    CHECK(konsens::floats_agree(0.0, -0.0, twelve_digits));
    CHECK(rounded(double_item(0.0), 2) == bytes_of({0xf9, 0x00, 0x00}));
    CHECK(rounded(double_item(-0.0), 2) == bytes_of({0xf9, 0x80, 0x00}));
}

// An infinity agrees only with the same infinity. RFC 8949 Appendix A
// gives 0xf97c00 and 0xf9fc00.
TEST_CASE("an infinity agrees only with the same infinity")
{
    const auto inf = std::numeric_limits<double>::infinity();
    CHECK(konsens::floats_agree(inf, inf, two_decimals));
    CHECK_FALSE(konsens::floats_agree(inf, -inf, two_decimals));
    CHECK_FALSE(konsens::floats_agree(inf, std::numeric_limits<double>::max(), two_decimals));
    CHECK(rounded(double_item(inf), 2) == bytes_of({0xf9, 0x7c, 0x00}));
    CHECK(rounded(double_item(-inf), 2) == bytes_of({0xf9, 0xfc, 0x00}));
}

// Plan 2.2.2 and 2.2a: the compare and the rounding walk into arrays, maps
// and the embedded item of a tag 24 (RFC 8949 3.4.5.1). Every item that is
// no float stays as it is, and agrees only byte for byte.
TEST_CASE("a float inside nested CBOR is rounded and compared")
{
    const auto f = double_item(1.23456);
    auto nested = bytes_of({0x82, 0x01, 0xa1, 0x61, 0x61});
    nested.insert(nested.end(), f.begin(), f.end());
    auto expected = bytes_of({0x82, 0x01, 0xa1, 0x61, 0x61});
    const auto r = double_item(1.23);
    expected.insert(expected.end(), r.begin(), r.end());
    CHECK(rounded(nested, 2) == expected);

    auto embedded = bytes_of({0xd8, 0x18, 0x49});
    embedded.insert(embedded.end(), f.begin(), f.end());
    auto embedded_expected = bytes_of({0xd8, 0x18, 0x49});
    embedded_expected.insert(embedded_expected.end(), r.begin(), r.end());
    CHECK(rounded(embedded, 2) == embedded_expected);

    auto close = bytes_of({0x82, 0x01, 0xa1, 0x61, 0x61});
    const auto g = double_item(1.23456 * (1 + 1e-13));
    close.insert(close.end(), g.begin(), g.end());
    CHECK(close != nested);
    CHECK(konsens::data_items_agree(nested, close, twelve_digits));
    auto other_key = bytes_of({0x82, 0x01, 0xa1, 0x61, 0x62});
    other_key.insert(other_key.end(), g.begin(), g.end());
    CHECK_FALSE(konsens::data_items_agree(nested, other_key, twelve_digits));
    CHECK_FALSE(konsens::data_items_agree(bytes_of({0x01}), bytes_of({0x18, 0x01}), twelve_digits));
}

// Plan 2.2.2: with the float rule, two results agree when their replies and
// values agree as CBOR items. Without it, they agree only byte for byte.
TEST_CASE("results agree through their data items under the float rule")
{
    const std::vector<std::byte> key{std::byte{'k'}};
    const konsens::request_result a{0, double_item(2.0 / 3), {{key, double_item(1.0 / 3)}}};
    const konsens::request_result b{0, double_item(2.0 / 3 * (1 + 1e-14)), {{key, double_item(1.0 / 3)}}};
    const konsens::request_result c{0, double_item(2.0 / 3), {{key, std::nullopt}}};
    const auto ea = konsens::encode_result(a);
    const auto eb = konsens::encode_result(b);
    CHECK(konsens::results_agree(ea, eb, twelve_digits));
    CHECK_FALSE(konsens::results_agree(ea, eb, std::nullopt));
    CHECK_FALSE(konsens::results_agree(ea, konsens::encode_result(c), twelve_digits));
}

namespace {

// RFC 8746 2: a typed array is a tag over one byte string that holds the
// elements one after the other.
std::vector<std::byte> typed_array(const std::uint8_t tag, const std::vector<std::byte>& elements)
{
    auto out = bytes_of({0xd8, tag, static_cast<std::uint8_t>(0x40 + elements.size())});
    out.insert(out.end(), elements.begin(), elements.end());
    return out;
}

std::vector<std::byte> big_endian_doubles(const std::initializer_list<double> values)
{
    std::vector<std::byte> out;
    for (const auto v : values) {
        const auto item = double_item(v);
        out.insert(out.end(), std::next(item.begin()), item.end());
    }
    return out;
}

std::vector<std::byte> little_endian_floats(const std::initializer_list<float> values)
{
    std::vector<std::byte> out;
    for (const auto v : values) {
        const auto bits = std::bit_cast<std::uint32_t>(v);
        for (int shift = 0; shift < 32; shift += 8) {
            out.push_back(static_cast<std::byte>(bits >> shift));
        }
    }
    return out;
}

}

// A map key identifies a value and is no measured value. The rounding keeps
// it, and the compare takes it byte for byte, so the key order of RFC 8949
// 4.2.1 stays as the state machine wrote it.
TEST_CASE("a map key is not rounded and agrees only byte for byte")
{
    const auto key = double_item(1.23456);
    const auto value = double_item(1.23456);
    auto map = bytes_of({0xa1});
    map.insert(map.end(), key.begin(), key.end());
    map.insert(map.end(), value.begin(), value.end());
    auto expected = bytes_of({0xa1});
    expected.insert(expected.end(), key.begin(), key.end());
    const auto r = double_item(1.23);
    expected.insert(expected.end(), r.begin(), r.end());
    CHECK(rounded(map, 2) == expected);

    auto near = bytes_of({0xa1});
    const auto near_key = double_item(1.23456 * (1 + 1e-15));
    near.insert(near.end(), near_key.begin(), near_key.end());
    near.insert(near.end(), value.begin(), value.end());
    CHECK_FALSE(konsens::data_items_agree(map, near, twelve_digits));
}

// RFC 8746 2.1: tags 80 to 87 hold IEEE 754 floats; tag 82 is binary64
// big endian, tag 85 is binary32 little endian. Each element is rounded and
// compared as any other float, and keeps the type of its array.
TEST_CASE("the floats of a typed array are rounded and compared")
{
    constexpr std::uint8_t binary64_big = 82;
    constexpr std::uint8_t binary32_little = 85;
    CHECK(rounded(typed_array(binary64_big, big_endian_doubles({1.23456, 9.9951})), 2) ==
          typed_array(binary64_big, big_endian_doubles({1.23, 10.0})));
    CHECK(rounded(typed_array(binary32_little, little_endian_floats({1.5f, 0.125f})), 1) ==
          typed_array(binary32_little, little_endian_floats({1.5f, static_cast<float>(0.12)})));

    const auto a = typed_array(binary64_big, big_endian_doubles({1.0 / 3, 2.0}));
    const auto b = typed_array(binary64_big, big_endian_doubles({1.0 / 3 * (1 + 1e-14), 2.0}));
    CHECK(a != b);
    CHECK(konsens::data_items_agree(a, b, twelve_digits));
    CHECK_FALSE(konsens::data_items_agree(a, typed_array(binary64_big, big_endian_doubles({1.0 / 3})), twelve_digits));
    CHECK_FALSE(konsens::data_items_agree(a, typed_array(86, big_endian_doubles({1.0 / 3, 2.0})), twelve_digits));
}
