#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <cbor/error.hpp>
#include <cbor/schema.hpp>

#include "konsens/wire.hpp"

// Provisional names, made in this tree, for the owner to decide:
// wire_errc and its values unknown_kind, unknown_entry_kind and
// entry_field_of_another_kind; kind_of, kind_count and paxos_kind_count;
// in src/wire.cpp body_of_kind, check_entries, check_entries_of,
// decode_body, decode_body_of_kind and encode_parts; the flag --cbor of
// counter_stress. The names with a source: header (RFC 7252 section 3),
// post (RFC 7252 section 12.1.1), method_not_allowed (RFC 7252 section
// 5.9.2.6), extraneous_data (RFC 8949 section 1.2).
//
// Provisional names: received and received_msg, the result of decode, and
// received_msg_of, which builds the variant of cbor::ref from the two
// variants of message.hpp.
//
// Provisional names of the recovery after a power loss
// (doc/plan-power-loss-code.md) that the code keeps without the recovery:
// compute_entry_crc, encode_entry, decode_entry and encoded_size_of of
// storage.hpp; in src/persistent_storage.cpp record_size_of, encode_record
// and decode_record; src/byte_order.hpp; in sequence_paxos.hpp paxos_errc
// and its value hardware_fault, paxos_error_category, paxos_category, the
// error value hardware_fault with its members index, pid, crc, other_pid
// and other_crc, and get_hardware_fault; check_decided_logs of
// counter_stress; ballot_above_promise of node.hpp.
//
// Provisional names of the removal of the recovery
// (doc/plan-remove-recovery.md item 8): in counter_stress the flag
// --restart-all, its member restart_all and default_restart_all,
// restart_every_node and restarts_of_every_node.
//
// Provisional names of the vote with the chain CRC
// (doc/plan-lmdb-only-source.md 2.5): the members decided_chain_crc,
// last_chain_crc and accepted_chain_crc of the messages; get_chain_crc of
// the storage; peer_seen, peer_step_back and take_peer_step_backs of
// sequence_paxos.
//
// Provisional form: decode gives each body in the cbor::ref that
// mbedded-cbor made, with no copy. node::handle_incoming takes a batch of
// received by value and keeps it until node::commit, because the storage
// ops of the batch hold spans of the entries in the bodies.
//
// Provisional names and forms of group commit (doc/plan-group-commit.md,
// section 9, and the decisions of the owner of 2026-10-21): the form B
// with a value, in which every call that collects returns a transaction and
// node::commit takes it; the class transaction (source: ISO SQL
// transaction, LMDB MDB_txn), its members commit_count_ and number_,
// throw_if_another_commit, and current_transaction and commit_count_ of
// node.hpp; the classes write_error (source: POSIX write) and sync_error
// (source: POSIX fsync, Java SyncFailedException) of storage.hpp, which a
// write and an LMDB commit that fail throw; max_batch_size and its value
// 1024; the private members waiting_ops_, incoming_, write_unfinished_,
// received_batch_, message_batch_, released_messages_ and
// released_replies_; write_waiting_ops_or_throw,
// get_storage_after_writing_waiting_ops, throw_if_a_write_did_not_finish,
// throw_if_ops_wait, drop_incoming_and_outgoing and
// log_or_buffered_proposals_contain of sequence_paxos.hpp; keep_and_handle,
// handle_one, batch_of, throw_if_batch_exceeds_max_batch_size and
// drop_held_messages_and_replies of node.hpp; mdb_txn (source: LMDB
// MDB_txn) and commit_or_throw of persistent_storage.cpp; batches::commit,
// batches::deliver and batches::tick_and_commit of test/batches.hpp;
// batches, largest_batch and batch_seed_offset of test/simulation.hpp;
// largest_batch of counter_stress. The names with a source: commit (ISO
// SQL COMMIT, mdb_txn_commit), handle_incoming (omni_paxos.rs:353),
// buffered_proposals (Omni-Paxos).

namespace {

using konsens::ballot;
using konsens::entry;
using konsens::entry_kind;
using konsens::message;
using konsens::wire_errc;

// The major types and the additional information of RFC 8949 section 3.
constexpr std::uint8_t unsigned_integer = 0;
constexpr std::uint8_t array = 4;
constexpr std::uint8_t tag = 6;
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

// draft-ietf-cbor-packed-19 section 3.1: Basic-Packed-CBOR is tag 113 on an
// array of two elements, the packing tables and the rump.
constexpr std::uint64_t basic_packed_cbor = 113;
constexpr std::uint64_t basic_packed_cbor_elements = 2;

constexpr konsens::node_id paxos_from = 101;
constexpr konsens::node_id paxos_to = 102;
constexpr konsens::node_id ble_from = 103;
constexpr konsens::node_id ble_to = 104;
constexpr std::size_t uint8_values = std::size_t{std::numeric_limits<std::uint8_t>::max()} + 1;
constexpr std::size_t entry_kind_count = 5;

std::string head(const std::uint8_t major, const std::uint64_t argument)
{
    const auto initial = [major](const std::uint64_t info) {
        return static_cast<char>(static_cast<std::uint8_t>(major << major_type_shift) |
                                 static_cast<std::uint8_t>(info));
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

std::string integer(const std::uint64_t v)
{
    return head(unsigned_integer, v);
}

}

// The header of src/wire.cpp is private to that file. The test holds a
// struct with the same members, the same types and the same tag, so its
// packed form is the packed form of the header. A header with a wider
// method holds a value that std::uint8_t cannot hold.
struct test_header {
    std::uint8_t method;
    std::uint8_t kind;
    konsens::node_id from;
    konsens::node_id to;
};

struct wide_header {
    std::uint64_t method;
    std::uint8_t kind;
    konsens::node_id from;
    konsens::node_id to;
};

CBOR_DEFINE_TYPE(test_header, cbor::tag{35699}, CBOR_MEMBER(test_header, method), CBOR_MEMBER(test_header, kind),
                 CBOR_MEMBER(test_header, from), CBOR_MEMBER(test_header, to));
CBOR_DEFINE_TYPE(wide_header, cbor::tag{35699}, CBOR_MEMBER(wide_header, method), CBOR_MEMBER(wide_header, kind),
                 CBOR_MEMBER(wide_header, from), CBOR_MEMBER(wide_header, to));

namespace {

template <typename T>
std::string packed(const T& value)
{
    auto out = cbor::schema<T>::encode(value);
    REQUIRE(out.has_value());
    return std::move(*out);
}

std::string header_bytes(const std::uint8_t method, const std::uint8_t kind, const konsens::node_id from,
                         const konsens::node_id to)
{
    return packed(test_header{method, kind, from, to});
}

std::string header_bytes(const std::uint8_t kind, const konsens::node_id from, const konsens::node_id to)
{
    return header_bytes(konsens::post, kind, from, to);
}

std::string basic_packed_cbor_start()
{
    return head(tag, basic_packed_cbor) + head(array, basic_packed_cbor_elements);
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

message paxos(konsens::paxos_msg m)
{
    return konsens::paxos_message{paxos_from, paxos_to, std::move(m)};
}

message ble(konsens::heartbeat_msg m)
{
    return konsens::ble_message{ble_from, ble_to, std::move(m)};
}

std::vector<message> every_kind()
{
    return {
        paxos(konsens::prepare_req{ballot_from(10)}),
        paxos(konsens::prepare{ballot_from(10), 20, ballot_from(30), 40}),
        paxos(konsens::promise{ballot_from(10), ballot_from(20), 30, 31, std::nullopt, 0, 0}),
        paxos(konsens::promise{ballot_from(10), ballot_from(20), 30, 31, konsens::log_sync{five_entries(), 32}, 0, 0}),
        paxos(konsens::accept_sync{ballot_from(10), {20, 21}, 22, konsens::log_sync{five_entries(), 23}, 0}),
        paxos(konsens::accept_decide{ballot_from(10), {20, 21}, 22, five_entries(), 0}),
        paxos(konsens::accepted{ballot_from(10), 20, 0}),
        paxos(konsens::decide{ballot_from(10), {20, 21}, 22, 0}),
        paxos(konsens::not_accepted{ballot_from(10)}),
        paxos(konsens::proposal_forward{five_entries()}),
        paxos(konsens::read_index_request{10}),
        paxos(konsens::heartbeat{ballot_from(10), 20}),
        paxos(konsens::heartbeat_ack{ballot_from(10), 20}),
        paxos(konsens::read_index_reply{{10, 11, 12}, 20}),
        paxos(konsens::answer{ballot_from(10), 20, 21, 22, {{21, payload_of({0xa0}), 23}, {24, {}, 25}}, 0}),
        paxos(konsens::answer{ballot_from(10), 20, 21, 22, {}, 21}),
        paxos(konsens::try_again{ballot_from(10), 20}),
        paxos(konsens::refused{ballot_from(10), 20, konsens::refusal_reason::result_differs}),
        paxos(konsens::work{ballot_from(10),
                            20,
                            entry{entry_kind::client_request, payload_of({4}), 53, 54},
                            21,
                            22,
                            23,
                            {{entry{entry_kind::normal}}, 24}}),
        paxos(konsens::accept_decide{ballot_from(10),
                                     {20, 21},
                                     22,
                                     {{.kind = entry_kind::client_request,
                                       .payload = payload_of({4}),
                                       .session = 53,
                                       .request_number = 54,
                                       .result = payload_of({0xa0}),
                                       .result_crc = 55,
                                       .out = {56, 57},
                                       .rejected = true}},
                                     0}),
        paxos(konsens::build_request{10, 20}),
        paxos(konsens::build_response{10, 20, five_entries(), 30, 40, 50}),
        paxos(konsens::build_refused{10}),
        ble(konsens::heartbeat_request{10}),
        ble(konsens::heartbeat_reply{10, ballot_from(20), ballot_from(30), true}),
        ble(konsens::heartbeat_reply{10, ballot_from(20), ballot_from(30), false}),
    };
}

// The test copies a received message into a message, because only a message
// compares with ==.
message message_of(const konsens::received& r)
{
    return std::visit(
        [&r](const auto& body) -> message {
            using T = std::remove_cvref_t<decltype(*body)>;
            if constexpr (requires { konsens::paxos_msg{std::in_place_type<T>, *body}; }) {
                return konsens::paxos_message{r.from, r.to, konsens::paxos_msg{std::in_place_type<T>, *body}};
            } else {
                return konsens::ble_message{r.from, r.to, konsens::heartbeat_msg{std::in_place_type<T>, *body}};
            }
        },
        r.msg);
}

std::expected<message, std::error_code> decoded_message(const std::string& bytes)
{
    const auto r = konsens::decode(bytes);
    if (!r) [[unlikely]] {
        return std::unexpected(r.error());
    }
    return message_of(*r);
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
        CHECK(konsens::kind_of(*decoded) == konsens::kind_of(m));
        CHECK(message_of(*decoded) == m);
        CHECK(konsens::encode(message_of(*decoded)) == bytes);
    }
    CHECK(kinds.size() == konsens::kind_count);
}

// The kind byte is the only name of a message on the wire, and it is the
// index of the type in paxos_msg, then in heartbeat_msg. A new type in the
// middle of a variant moves every later kind, so that an old node and a new
// node no longer understand each other. This table fixes each kind byte, so
// that such a change fails here and is made on purpose.
TEST_CASE("each kind of message has its fixed kind byte")
{
    const std::vector<std::pair<message, std::uint8_t>> table{
        {paxos(konsens::prepare_req{ballot_from(10)}), 0},
        {paxos(konsens::prepare{ballot_from(10), 20, ballot_from(30), 40}), 1},
        {paxos(konsens::promise{ballot_from(10), ballot_from(20), 30, 31, std::nullopt, 0, 0}), 2},
        {paxos(konsens::accept_sync{ballot_from(10), {20, 21}, 22, konsens::log_sync{{}, 23}, 0}), 3},
        {paxos(konsens::accept_decide{ballot_from(10), {20, 21}, 22, {}, 0}), 4},
        {paxos(konsens::accepted{ballot_from(10), 20, 0}), 5},
        {paxos(konsens::decide{ballot_from(10), {20, 21}, 22, 0}), 6},
        {paxos(konsens::not_accepted{ballot_from(10)}), 7},
        {paxos(konsens::proposal_forward{{}}), 8},
        {paxos(konsens::read_index_request{10}), 9},
        {paxos(konsens::heartbeat{ballot_from(10), 20}), 10},
        {paxos(konsens::heartbeat_ack{ballot_from(10), 20}), 11},
        {paxos(konsens::read_index_reply{{10}, 20}), 12},
        {paxos(konsens::answer{ballot_from(10), 20, 21, 22, {}, 0}), 13},
        {paxos(konsens::try_again{ballot_from(10), 20}), 14},
        {paxos(konsens::refused{ballot_from(10), 20, konsens::refusal_reason::crc_mismatch}), 15},
        {paxos(konsens::work{
             ballot_from(10), 20, entry{entry_kind::normal}, 21, 22, 23, {{entry{entry_kind::normal}}, 24}}),
         16},
        {paxos(konsens::build_request{10, 20}), 17},
        {paxos(konsens::build_response{10, 20, {}, 30, 40, 50}), 18},
        {paxos(konsens::build_refused{10}), 19},
        {ble(konsens::heartbeat_request{10}), 20},
        {ble(konsens::heartbeat_reply{10, ballot_from(20), ballot_from(30), true}), 21},
    };
    CHECK(table.size() == konsens::kind_count);
    for (const auto& [m, kind] : table) {
        INFO("kind ", int{kind});
        CHECK(konsens::kind_of(m) == kind);
        const auto bytes = konsens::encode(m);
        REQUIRE(bytes.has_value());
        const auto [from, to] = std::visit([](const auto& x) { return std::pair{x.from, x.to}; }, m);
        CHECK(bytes->starts_with(header_bytes(kind, from, to)));
    }
}

// The header and the body are each one item of Basic-Packed-CBOR, so another
// decoder of draft-ietf-cbor-packed-19 reads both.
TEST_CASE("the header and the body of each kind start as Basic-Packed-CBOR")
{
    for (const auto& m : every_kind()) {
        INFO("kind ", int{konsens::kind_of(m)});
        const auto bytes = konsens::encode(m);
        REQUIRE(bytes.has_value());
        const auto [from, to] = std::visit([](const auto& x) { return std::pair{x.from, x.to}; }, m);
        const auto header = header_bytes(konsens::kind_of(m), from, to);
        CHECK(header.starts_with(basic_packed_cbor_start()));
        REQUIRE(bytes->starts_with(header));
        CHECK(bytes->substr(header.size()).starts_with(basic_packed_cbor_start()));
    }
}

// RFC 8949 section 3: an input that ends inside an item is not well-formed.
// Each length shorter than the message is tried, so a cut in the header, at
// its end, and in the body are all seen.
TEST_CASE("an empty input and each cut input give too_little_data")
{
    CHECK(error_of("") == cbor::error::too_little_data);
    const auto bytes = konsens::encode(paxos(konsens::accept_decide{ballot_from(10), {20, 21}, 22, five_entries(), 0}));
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
    const auto bytes = konsens::encode(paxos(konsens::prepare_req{ballot_from(10)}));
    REQUIRE(bytes.has_value());
    CHECK(error_of(*bytes + integer(0)) == wire_errc::extraneous_data);
    CHECK(error_of(*bytes + std::string(1, static_cast<char>(reserved_initial_byte))) == wire_errc::extraneous_data);
}

// The method is a finite set of 256 values. Only POST (RFC 7252 section
// 12.1.1) is a message between the nodes.
TEST_CASE("each method other than POST gives method_not_allowed")
{
    constexpr std::uint8_t prepare_req_kind = 0;
    const auto body = packed(konsens::prepare_req{ballot_from(10)});
    for (std::size_t method = 0; method < uint8_values; ++method) {
        CAPTURE(method);
        const auto bytes =
            header_bytes(static_cast<std::uint8_t>(method), prepare_req_kind, paxos_from, paxos_to) + body;
        if (method == konsens::post) {
            CHECK(decoded_message(bytes) == paxos(konsens::prepare_req{ballot_from(10)}));
        } else {
            CHECK(error_of(bytes) == wire_errc::method_not_allowed);
        }
    }
}

// The packed form carries the description of the header. A header whose
// method is wider than std::uint8_t has another description, and the decode
// refuses it with an error of mbedded-cbor.
TEST_CASE("a header with another description gives an error of mbedded-cbor")
{
    constexpr std::uint64_t outside = 256;
    const auto body = packed(konsens::prepare_req{ballot_from(10)});
    for (const auto method : {std::uint64_t{konsens::post}, outside}) {
        CAPTURE(method);
        const auto bytes = packed(wide_header{method, 0, paxos_from, paxos_to}) + body;
        CHECK(error_of(bytes).category() == cbor::category());
    }
}

// The kind is a finite set of 256 values. A kind past the last kind of
// message gives unknown_kind before the body is read.
TEST_CASE("each kind past the last kind of message gives unknown_kind")
{
    const auto body = packed(konsens::heartbeat_request{10});
    for (std::size_t kind = konsens::kind_count; kind < uint8_values; ++kind) {
        CAPTURE(kind);
        CHECK(error_of(header_bytes(static_cast<std::uint8_t>(kind), paxos_from, paxos_to) + body) ==
              wire_errc::unknown_kind);
    }
}

// The packed form of a body carries the tag of its struct. A body decodes
// only under the kind of its own struct, also where two structs have the
// same members, such as heartbeat and heartbeat_ack. Every pair of a kind and
// a body is tried.
TEST_CASE("a body decodes only under the kind of its own struct")
{
    std::vector<std::pair<std::uint8_t, std::string>> bodies;
    for (const auto& m : every_kind()) {
        const auto bytes = konsens::encode(m);
        REQUIRE(bytes.has_value());
        const auto [from, to] = std::visit([](const auto& x) { return std::pair{x.from, x.to}; }, m);
        bodies.emplace_back(konsens::kind_of(m), bytes->substr(header_bytes(konsens::kind_of(m), from, to).size()));
    }
    for (std::uint8_t kind = 0; kind < konsens::kind_count; ++kind) {
        for (const auto& [own, body] : bodies) {
            INFO("kind ", int{kind}, ", body of kind ", int{own});
            const auto decoded = konsens::decode(header_bytes(kind, paxos_from, paxos_to) + body);
            if (kind == own) {
                CHECK(decoded.has_value());
            } else {
                REQUIRE_FALSE(decoded.has_value());
                CHECK(decoded.error().category() == cbor::category());
            }
        }
    }
}

// mbedded-cbor reads an enum as its underlying integer and does not check the
// value. decode checks each entry with the same function that persistent
// storage calls, so an entry_kind that is not an enumerator never leaves
// decode. Each of the 256 values of the underlying type is tried.
TEST_CASE("an entry kind that is not a value of entry_kind gives unknown_entry_kind")
{
    constexpr std::uint8_t proposal_forward_kind = 8;
    for (std::size_t kind = 0; kind < uint8_values; ++kind) {
        CAPTURE(kind);
        const konsens::proposal_forward body{{entry{static_cast<entry_kind>(kind)}}};
        const auto bytes = header_bytes(proposal_forward_kind, paxos_from, paxos_to) + packed(body);
        if (kind < entry_kind_count) {
            CHECK(decoded_message(bytes) == paxos(body));
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
// session and request_number, expire_session carries session, and each
// request kind carries the result fields of its done record
// (plan-one-environment 3.3).
TEST_CASE("an entry with a field that its kind does not carry gives entry_field_of_another_kind")
{
    constexpr std::uint8_t proposal_forward_kind = 8;
    enum field : std::uint8_t {
        session,
        request_number,
        client_id,
        pid,
        result,
        result_crc,
        out,
        rejected,
        field_count
    };
    struct carried {
        entry_kind kind;
        std::array<bool, field_count> fields;
    };
    const std::array<carried, entry_kind_count> table{{
        {entry_kind::normal, {false, false, false, false, false, false, false, false}},
        {entry_kind::stop_sign, {false, false, false, false, false, false, false, false}},
        {entry_kind::register_client, {false, false, true, true, true, true, true, true}},
        {entry_kind::client_request, {true, true, false, false, true, true, true, true}},
        {entry_kind::expire_session, {true, false, false, false, true, true, true, true}},
    }};
    for (const auto& [kind, fields] : table) {
        for (std::uint8_t f = 0; f < field_count; ++f) {
            INFO("entry kind ", int{std::to_underlying(kind)}, ", field ", int{f});
            const auto one = [f](const field which) { return f == which ? std::uint64_t{1} : std::uint64_t{0}; };
            const entry e{.kind = kind,
                          .session = one(session),
                          .request_number = one(request_number),
                          .client_id = one(client_id),
                          .pid = one(pid),
                          .result = f == result ? payload_of({1}) : std::vector<std::byte>{},
                          .result_crc = static_cast<std::uint32_t>(one(result_crc)),
                          .out = f == out ? std::vector<konsens::node_id>{1} : std::vector<konsens::node_id>{},
                          .rejected = f == rejected};
            const auto bytes = header_bytes(proposal_forward_kind, paxos_from, paxos_to) +
                               packed(konsens::proposal_forward{{e}});
            if (fields.at(f)) {
                CHECK(konsens::decode(bytes).has_value());
            } else {
                CHECK(error_of(bytes) == wire_errc::entry_field_of_another_kind);
            }
        }
    }
}

// Plan-one-environment 2 item 2: work carries a request without its result.
// A request in work that holds a result or is rejected is a done record, and
// decode refuses it with entry_field_of_another_kind.
TEST_CASE("work with a done record gives entry_field_of_another_kind")
{
    constexpr std::uint8_t work_kind = 16;
    const auto work_bytes = [](const std::vector<std::byte>& result, const bool rejected) {
        return header_bytes(work_kind, paxos_from, paxos_to) +
               packed(konsens::work{ballot_from(10),
                                    20,
                                    entry{.kind = entry_kind::client_request,
                                          .session = 1,
                                          .request_number = 2,
                                          .result = result,
                                          .rejected = rejected},
                                    21,
                                    22,
                                    23,
                                    {{entry{entry_kind::normal}}, 24}});
    };
    CHECK(decoded_message(work_bytes({}, false)) == paxos(konsens::work{ballot_from(10),
                                                                        20,
                                                                        entry{entry_kind::client_request, {}, 1, 2},
                                                                        21,
                                                                        22,
                                                                        23,
                                                                        {{entry{entry_kind::normal}}, 24}}));
    CHECK(error_of(work_bytes(payload_of({1}), false)) == wire_errc::entry_field_of_another_kind);
    CHECK(error_of(work_bytes({}, true)) == wire_errc::entry_field_of_another_kind);
}

// Plan reconfiguration 2.1 and 4.3: a StopSign carries the next
// configuration as CBOR in its payload. A payload that is cut short or
// that has bytes after the item fails as a value.
TEST_CASE("a cluster_config passes the payload of a StopSign, and a broken payload fails as a value")
{
    const konsens::cluster_config next{2, {4, 5, 6}};
    const auto payload = konsens::encode(next);
    REQUIRE(payload.has_value());
    const auto decoded = konsens::decode_cluster_config(*payload);
    REQUIRE(decoded.has_value());
    CHECK(*decoded == next);
    for (std::size_t n = 0; n < payload->size(); ++n) {
        CAPTURE(n);
        CHECK_FALSE(konsens::decode_cluster_config(std::span(*payload).first(n)).has_value());
    }
    auto longer = *payload;
    longer.push_back(std::byte{0});
    CHECK_FALSE(konsens::decode_cluster_config(longer).has_value());
}

// Plan reconfiguration 2.7: a build_response carries entries of another
// node, so decode checks them as it checks the entries of accept_decide.
TEST_CASE("a build_response with an entry of a foreign field gives entry_field_of_another_kind")
{
    const auto m =
        paxos(konsens::build_response{10, 20, {entry{.kind = entry_kind::normal, .session = 1}}, 30, 40, 50});
    const auto bytes = konsens::encode(m);
    REQUIRE(bytes.has_value());
    CHECK(error_of(*bytes) == wire_errc::entry_field_of_another_kind);
}
