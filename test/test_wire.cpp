#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <cbor/error.hpp>

#include "konsens/wire.hpp"

// Provisional names, made in this tree, for the owner to decide:
// wire_errc and its values unknown_kind, unknown_entry_kind and
// entry_field_of_another_kind; kind_of, kind_count and paxos_kind_count;
// in src/wire.cpp body_of_kind, check_entries, check_entries_of,
// decode_body, decode_body_of_kind and encode_parts; the flag --cbor of
// counter_stress. The names with a source: header (RFC 7252 section 3),
// post (RFC 7252 section 12.1.1), method_not_allowed (RFC 7252 section
// 5.9.2.6), extraneous_data (RFC 8949 section 2).
//
// Provisional form: decode copies each body once more out of cbor::ref,
// because cbor::ref gives its value only as a const reference.

namespace {

using konsens::ballot;
using konsens::entry;
using konsens::entry_kind;
using konsens::message;
using konsens::wire_errc;

// The major types and the additional information of RFC 8949 section 3.
constexpr std::uint8_t unsigned_integer = 0;
constexpr std::uint8_t negative_integer = 1;
constexpr std::uint8_t byte_string = 2;
constexpr std::uint8_t text_string = 3;
constexpr std::uint8_t array = 4;
constexpr std::uint8_t map = 5;
constexpr std::uint8_t tag = 6;
constexpr std::uint8_t simple_or_float = 7;
constexpr std::uint8_t simple_false = 20;
constexpr std::uint8_t simple_true = 21;
constexpr std::uint8_t major_type_shift = 5;
constexpr std::uint64_t one_byte_argument = 24;
constexpr std::uint64_t two_byte_argument = 25;
constexpr std::uint64_t four_byte_argument = 26;
constexpr std::uint64_t eight_byte_argument = 27;
constexpr std::uint64_t byte_bits = 8;
constexpr std::uint64_t one_byte_max = 0xff;
constexpr std::uint64_t two_byte_max = 0xffff;
constexpr std::uint64_t four_byte_max = 0xffffffff;
constexpr std::uint64_t reserved_initial_byte = 0x1c;

// RFC 8746 section 2.1: the tags of the typed arrays in little endian.
constexpr std::uint64_t typed_array_uint8 = 64;
constexpr std::uint64_t typed_array_uint64_little_endian = 71;

constexpr konsens::node_id paxos_from = 101;
constexpr konsens::node_id paxos_to = 102;
constexpr konsens::node_id ble_from = 103;
constexpr konsens::node_id ble_to = 104;
constexpr std::size_t uint8_values = std::size_t{std::numeric_limits<std::uint8_t>::max()} + 1;
constexpr std::size_t entry_kind_count = 5;

std::string head(const std::uint8_t major, const std::uint64_t argument)
{
    const auto initial = [major](const std::uint64_t info) {
        return static_cast<char>(static_cast<std::uint8_t>(major << major_type_shift) | static_cast<std::uint8_t>(info));
    };
    if (argument < one_byte_argument) {
        return std::string(1, initial(argument));
    }
    std::uint64_t info = eight_byte_argument;
    std::uint64_t length = sizeof(std::uint64_t);
    if (argument <= one_byte_max) {
        info = one_byte_argument;
        length = sizeof(std::uint8_t);
    } else if (argument <= two_byte_max) {
        info = two_byte_argument;
        length = sizeof(std::uint16_t);
    } else if (argument <= four_byte_max) {
        info = four_byte_argument;
        length = sizeof(std::uint32_t);
    }
    std::string out(1, initial(info));
    for (auto i = length; i-- > 0;) {
        out += static_cast<char>(static_cast<std::uint8_t>(argument >> (byte_bits * i)));
    }
    return out;
}

std::string integer(const std::uint64_t v) { return head(unsigned_integer, v); }

std::string text(const std::string_view s) { return head(text_string, s.size()) + std::string(s); }

std::string boolean(const bool v) { return head(simple_or_float, v ? simple_true : simple_false); }

std::string bytes_tagged(const std::uint64_t t, const std::string_view content)
{
    return head(tag, t) + head(byte_string, content.size()) + std::string(content);
}

std::string little_endian(const std::uint64_t v)
{
    std::string out;
    for (std::uint64_t i = 0; i < sizeof v; ++i) {
        out += static_cast<char>(static_cast<std::uint8_t>(v >> (byte_bits * i)));
    }
    return out;
}

// RFC 8949 section 4.2.1 sorts the keys of a map by their encoded bytes. A
// shorter text string has a smaller head, so the keys below stand in the order
// of their length first and of their letters second.
std::string header_bytes(const std::string& method, const std::string& kind, const konsens::node_id from,
                         const konsens::node_id to)
{
    return head(map, 4) + text("to") + integer(to) + text("from") + integer(from) + text("kind") + kind + text("method") +
           method;
}

std::string header_bytes(const std::uint64_t kind, const konsens::node_id from, const konsens::node_id to)
{
    return header_bytes(integer(konsens::post), integer(kind), from, to);
}

std::string ballot_bytes(const ballot& b)
{
    return head(map, 4) + text("n") + integer(b.n) + text("pid") + integer(b.pid) + text("priority") + integer(b.priority) +
           text("config_id") + integer(b.config_id);
}

std::string entry_bytes(const std::uint64_t kind, const std::string_view payload, const std::uint64_t session,
                        const std::uint64_t request_number, const std::uint64_t client_id, const std::uint64_t pid)
{
    return head(map, 6) + text("pid") + integer(pid) + text("kind") + integer(kind) + text("payload") +
           bytes_tagged(typed_array_uint8, payload) + text("session") + integer(session) + text("client_id") +
           integer(client_id) + text("request_number") + integer(request_number);
}

std::string proposal_forward_bytes(const std::string& one_entry)
{
    return head(map, 1) + text("entries") + head(array, 1) + one_entry;
}

ballot ballot_from(const std::uint32_t first)
{
    return {first, first + 1, first + 2, std::uint64_t{first} + 3};
}

std::vector<std::byte> payload_of(const std::vector<std::uint8_t>& values)
{
    std::vector<std::byte> out;
    for (const auto v : values) {
        out.push_back(std::byte{v});
    }
    return out;
}

std::vector<entry> five_entries()
{
    return {
        entry{entry_kind::normal, payload_of({1, 2, 3})},
        entry{entry_kind::stop_sign},
        entry{entry_kind::register_client, {}, 0, 0, 51, 52},
        entry{entry_kind::client_request, payload_of({4}), 53, 54},
        entry{entry_kind::expire_session, {}, 55},
    };
}

message paxos(konsens::paxos_msg m) { return konsens::paxos_message{paxos_from, paxos_to, std::move(m)}; }

message ble(konsens::heartbeat_msg m) { return konsens::ble_message{ble_from, ble_to, std::move(m)}; }

std::vector<message> every_kind()
{
    return {
        paxos(konsens::prepare_req{ballot_from(10)}),
        paxos(konsens::prepare{ballot_from(10), 20, ballot_from(30), 40}),
        paxos(konsens::promise{ballot_from(10), ballot_from(20), 30, 31, std::nullopt}),
        paxos(konsens::promise{ballot_from(10), ballot_from(20), 30, 31, konsens::log_sync{five_entries(), 32}}),
        paxos(konsens::accept_sync{ballot_from(10), {20, 21}, 22, konsens::log_sync{five_entries(), 23}}),
        paxos(konsens::accept_decide{ballot_from(10), {20, 21}, 22, five_entries()}),
        paxos(konsens::accepted{ballot_from(10), 20}),
        paxos(konsens::decide{ballot_from(10), {20, 21}, 22}),
        paxos(konsens::not_accepted{ballot_from(10)}),
        paxos(konsens::proposal_forward{five_entries()}),
        paxos(konsens::recovery{10}),
        paxos(konsens::recovery_response{10, ballot_from(20), true, ballot_from(30), five_entries(), 40}),
        paxos(konsens::recovery_response{10, ballot_from(20), false, ballot_from(30), five_entries(), 40}),
        paxos(konsens::read_index_request{10}),
        paxos(konsens::heartbeat{ballot_from(10), 20}),
        paxos(konsens::heartbeat_ack{ballot_from(10), 20}),
        paxos(konsens::read_index_reply{{10, 11, 12}, 20}),
        ble(konsens::heartbeat_request{10}),
        ble(konsens::heartbeat_reply{10, ballot_from(20), ballot_from(30), true}),
        ble(konsens::heartbeat_reply{10, ballot_from(20), ballot_from(30), false}),
    };
}

std::error_code error_of(const std::string& bytes)
{
    const auto decoded = konsens::decode(bytes);
    REQUIRE_FALSE(decoded.has_value());
    return decoded.error();
}

}

// A round trip shows that decode reads each member that encode writes. Each
// scalar member has a value that no other member of its message has, so a
// decode that exchanges two members of one type fails. Each bool takes both
// values, the optional is empty and set, and the entries hold each kind.
TEST_CASE("decode of encode gives the message, and a second encode gives the same bytes")
{
    std::set<std::uint8_t> kinds;
    for (const auto& m : every_kind()) {
        INFO("kind ", int{konsens::kind_of(m)});
        kinds.insert(konsens::kind_of(m));
        const auto bytes = konsens::encode(m);
        REQUIRE(bytes.has_value());
        const auto decoded = konsens::decode(*bytes);
        REQUIRE(decoded.has_value());
        CHECK(*decoded == m);
        CHECK(konsens::encode(*decoded) == bytes);
    }
    CHECK(kinds.size() == konsens::kind_count);
}

// A round trip does not show the form on the wire. These bytes are written
// from RFC 8949 and RFC 8746, so a change of a key, of the order of the keys
// or of the order of the kinds fails here.
TEST_CASE("an accept_decide with a payload has the bytes of RFC 8949 and RFC 8746")
{
    constexpr std::uint64_t accept_decide_kind = 4;
    const auto m = paxos(konsens::accept_decide{ballot_from(10), {20, 21}, 22,
                                                {entry{entry_kind::client_request, payload_of({0xaa}), 23, 24}}});
    const auto n = ballot_from(10);
    const auto expected =
        header_bytes(accept_decide_kind, paxos_from, paxos_to) + head(map, 4) + text("n") + ballot_bytes(n) +
        text("entries") + head(array, 1) +
        entry_bytes(std::to_underlying(entry_kind::client_request), "\xaa", 23, 24, 0, 0) + text("seq_num") +
        head(map, 2) + text("counter") + integer(21) + text("session") + integer(20) + text("decided_idx") + integer(22);
    CHECK(konsens::kind_of(m) == accept_decide_kind);
    CHECK(konsens::encode(m) == expected);
    CHECK(konsens::decode(expected) == m);
}

// A std::vector<std::uint64_t> is an RFC 8746 typed array with tag 71, not a
// CBOR array. This test holds the bytes of that tag.
TEST_CASE("a read_index_reply has the bytes of RFC 8949 and RFC 8746")
{
    constexpr std::uint64_t read_index_reply_kind = 14;
    constexpr std::uint64_t first_read = 1;
    constexpr std::uint64_t second_read = 258;
    const auto m = paxos(konsens::read_index_reply{{first_read, second_read}, 30});
    const auto expected = header_bytes(read_index_reply_kind, paxos_from, paxos_to) + head(map, 2) + text("reads") +
                          bytes_tagged(typed_array_uint64_little_endian,
                                       little_endian(first_read) + little_endian(second_read)) +
                          text("read_index") + integer(30);
    CHECK(konsens::kind_of(m) == read_index_reply_kind);
    CHECK(konsens::encode(m) == expected);
    CHECK(konsens::decode(expected) == m);
}

// A heartbeat_reply carries two bools and two ballots; this test holds the
// bytes of a kind of the second variant, which follows the first variant.
TEST_CASE("a heartbeat_reply has the bytes of RFC 8949")
{
    constexpr std::uint64_t heartbeat_reply_kind = 16;
    const auto m = ble(konsens::heartbeat_reply{10, ballot_from(20), ballot_from(30), true});
    const auto expected = header_bytes(heartbeat_reply_kind, ble_from, ble_to) + head(map, 4) + text("happy") +
                          boolean(true) + text("round") + integer(10) + text("ballot") + ballot_bytes(ballot_from(20)) +
                          text("leader") + ballot_bytes(ballot_from(30));
    CHECK(konsens::encode(m) == expected);
    CHECK(konsens::decode(expected) == m);
}

// RFC 8949 section 3: an input that ends inside an item is not well-formed.
// Each length shorter than the message is tried, so a cut in the header, at
// its end, and in the body are all seen.
TEST_CASE("an empty input and each cut input give too_little_data")
{
    CHECK(error_of("") == cbor::error::too_little_data);
    const auto bytes = konsens::encode(paxos(konsens::accept_decide{ballot_from(10), {20, 21}, 22, five_entries()}));
    REQUIRE(bytes.has_value());
    for (std::size_t length = 0; length < bytes->size(); ++length) {
        CAPTURE(length);
        CHECK(error_of(bytes->substr(0, length)) == cbor::error::too_little_data);
    }
}

// RFC 8742 lets a sequence hold more items; a message holds two. A third item
// gives extraneous_data whether it is well-formed or not, because decode does
// not read it.
TEST_CASE("bytes after the body give extraneous_data")
{
    const auto bytes = konsens::encode(paxos(konsens::recovery{10}));
    REQUIRE(bytes.has_value());
    CHECK(error_of(*bytes + integer(0)) == wire_errc::extraneous_data);
    CHECK(error_of(*bytes + std::string(1, static_cast<char>(reserved_initial_byte))) == wire_errc::extraneous_data);
}

// The method is a finite set of 256 values. Only POST (RFC 7252 section
// 12.1.1) is a message between the nodes.
TEST_CASE("each method other than POST gives method_not_allowed")
{
    constexpr std::uint64_t recovery_kind = 9;
    const auto body = head(map, 1) + text("nonce") + integer(10);
    for (std::size_t method = 0; method < uint8_values; ++method) {
        CAPTURE(method);
        const auto bytes = header_bytes(integer(method), integer(recovery_kind), paxos_from, paxos_to) + body;
        if (method == konsens::post) {
            CHECK(konsens::decode(bytes) == paxos(konsens::recovery{10}));
        } else {
            CHECK(error_of(bytes) == wire_errc::method_not_allowed);
        }
    }
}

// The header holds the method and the kind as std::uint8_t. Errors of
// mbedded-cbor pass through decode unchanged. Its documentation
// (doc/advanced/structs.md section 9) gives number_out_of_range for an integer
// that the C++ type cannot hold, 256 and -1 alike, and incorrect_type for an
// item that is not an integer.
TEST_CASE("a method or a kind outside std::uint8_t gives the error of mbedded-cbor")
{
    constexpr std::uint64_t outside = 256;
    const auto body = head(map, 0);
    const auto minus_one = head(negative_integer, 0);
    const auto not_an_integer = text("2");
    for (const auto& [wrong, expected] : {std::pair{integer(outside), cbor::error::number_out_of_range},
                                          std::pair{minus_one, cbor::error::number_out_of_range},
                                          std::pair{not_an_integer, cbor::error::incorrect_type}}) {
        CHECK(error_of(header_bytes(wrong, integer(0), paxos_from, paxos_to) + body) == expected);
        CHECK(error_of(header_bytes(integer(konsens::post), wrong, paxos_from, paxos_to) + body) == expected);
    }
}

// The kind is a finite set of 256 values. Each kind of message has a member
// that is not optional, so an empty body under a known kind gives
// key_not_found, and an empty body under an unknown kind gives unknown_kind.
TEST_CASE("each kind past the last kind of message gives unknown_kind")
{
    const auto body = head(map, 0);
    for (std::size_t kind = 0; kind < uint8_values; ++kind) {
        CAPTURE(kind);
        const auto bytes = header_bytes(kind, paxos_from, paxos_to) + body;
        if (kind < konsens::kind_count) {
            CHECK(error_of(bytes) == cbor::error::key_not_found);
        } else {
            CHECK(error_of(bytes) == wire_errc::unknown_kind);
        }
    }
}

// The header alone names the kind. A body that lacks a member of that kind
// fails. A body of another kind with the same keys decodes as the kind of the
// header: heartbeat and heartbeat_ack, and prepare_req and not_accepted, have
// the same keys. This is a known limit of the form with a header and a body.
TEST_CASE("a body of another kind decodes only when it has the keys of the kind of the header")
{
    constexpr std::uint64_t prepare_req_kind = 0;
    constexpr std::uint64_t decide_kind = 6;
    constexpr std::uint64_t not_accepted_kind = 7;
    constexpr std::uint64_t heartbeat_kind = 12;
    constexpr std::uint64_t heartbeat_ack_kind = 13;
    const auto heartbeat_request_body = head(map, 1) + text("round") + integer(10);
    CHECK(error_of(header_bytes(decide_kind, paxos_from, paxos_to) + heartbeat_request_body) ==
          cbor::error::key_not_found);

    const auto heartbeat_body = head(map, 2) + text("n") + ballot_bytes(ballot_from(10)) + text("id") + integer(20);
    CHECK(konsens::decode(header_bytes(heartbeat_kind, paxos_from, paxos_to) + heartbeat_body) ==
          paxos(konsens::heartbeat{ballot_from(10), 20}));
    CHECK(konsens::decode(header_bytes(heartbeat_ack_kind, paxos_from, paxos_to) + heartbeat_body) ==
          paxos(konsens::heartbeat_ack{ballot_from(10), 20}));

    const auto n_body = head(map, 1) + text("n") + ballot_bytes(ballot_from(10));
    CHECK(konsens::decode(header_bytes(prepare_req_kind, paxos_from, paxos_to) + n_body) ==
          paxos(konsens::prepare_req{ballot_from(10)}));
    CHECK(konsens::decode(header_bytes(not_accepted_kind, paxos_from, paxos_to) + n_body) ==
          paxos(konsens::not_accepted{ballot_from(10)}));
}

// mbedded-cbor reads an enum as its underlying integer and does not check the
// value. decode checks each entry with the same function that persistent
// storage calls, so an entry_kind that is not an enumerator never leaves
// decode. Each of the 256 values of the underlying type is tried.
TEST_CASE("an entry kind that is not a value of entry_kind gives unknown_entry_kind")
{
    constexpr std::uint64_t proposal_forward_kind = 8;
    for (std::size_t kind = 0; kind < uint8_values; ++kind) {
        CAPTURE(kind);
        const auto bytes = header_bytes(proposal_forward_kind, paxos_from, paxos_to) +
                           proposal_forward_bytes(entry_bytes(kind, "", 0, 0, 0, 0));
        if (kind < entry_kind_count) {
            const auto decoded = konsens::decode(bytes);
            const auto expected = paxos(konsens::proposal_forward{{entry{static_cast<entry_kind>(kind)}}});
            CHECK(decoded == expected);
        } else {
            CHECK(error_of(bytes) == wire_errc::unknown_entry_kind);
        }
    }
}

// Storage ends the process when an entry carries a field that its kind does
// not carry, because that is a wrong use. From the wire it is a value of
// another node, so decode refuses it before it reaches storage. The fields of
// each kind come from the definition of entry: normal and stop_sign carry
// none, register_client carries client_id and pid, client_request carries
// session and request_number, expire_session carries session.
TEST_CASE("an entry with a field that its kind does not carry gives entry_field_of_another_kind")
{
    constexpr std::uint64_t proposal_forward_kind = 8;
    enum field : std::uint8_t { session, request_number, client_id, pid, field_count };
    struct carried {
        entry_kind kind;
        std::array<bool, field_count> fields;
    };
    const std::array<carried, entry_kind_count> table{{
        {entry_kind::normal, {false, false, false, false}},
        {entry_kind::stop_sign, {false, false, false, false}},
        {entry_kind::register_client, {false, false, true, true}},
        {entry_kind::client_request, {true, true, false, false}},
        {entry_kind::expire_session, {true, false, false, false}},
    }};
    for (const auto& [kind, fields] : table) {
        for (std::uint8_t f = 0; f < field_count; ++f) {
            INFO("entry kind ", int{std::to_underlying(kind)}, ", field ", int{f});
            const auto one = [f](const field which) { return f == which ? std::uint64_t{1} : std::uint64_t{0}; };
            const auto bytes = header_bytes(proposal_forward_kind, paxos_from, paxos_to) +
                               proposal_forward_bytes(entry_bytes(std::to_underlying(kind), "", one(session),
                                                                  one(request_number), one(client_id), one(pid)));
            if (fields.at(f)) {
                CHECK(konsens::decode(bytes).has_value());
            } else {
                CHECK(error_of(bytes) == wire_errc::entry_field_of_another_kind);
            }
        }
    }
}
