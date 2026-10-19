#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <stdexcept>
#include <system_error>
#include <vector>

#include "konsens/memory_storage.hpp"

namespace {

konsens::entry make_entry(std::byte b) { return {konsens::entry_kind::normal, {b}}; }

}

// SequencePaxos is a template on the concept storage. A memory_storage
// that does not satisfy it must not compile.
static_assert(konsens::storage<konsens::memory_storage>);

// A new memory_storage is the state of a node that never started:
// Ballot::default and an empty log, as in Init of SequencePaxos.tla.
TEST_CASE("a new memory_storage is empty") {
    const konsens::memory_storage s;
    CHECK(*s.get_log_len() == 0);
    CHECK(*s.get_promise() == konsens::ballot{});
    CHECK(*s.get_accepted_round() == konsens::ballot{});
    CHECK(*s.get_decided_idx() == 0);
}

// AppendOnPrefix in SequencePaxos.tla keeps the first k entries and
// appends the suffix. A follower relies on this to replace a log that
// diverges after k.
TEST_CASE("append_on_prefix truncates and appends") {
    konsens::memory_storage s;
    const std::array<konsens::storage_op, 1> first{konsens::append_entries{
        {make_entry(std::byte{1}), make_entry(std::byte{2}), make_entry(std::byte{3})}}};
    REQUIRE(s.write_atomically(first));
    const std::array<konsens::storage_op, 1> second{
        konsens::append_on_prefix{1, {make_entry(std::byte{9})}}};
    REQUIRE(s.write_atomically(second));
    const auto all = s.get_entries(0, 2);
    REQUIRE(all);
    CHECK(*all == std::vector{make_entry(std::byte{1}), make_entry(std::byte{9})});
}

// AppendOnPrefix with k at or beyond the log length keeps the whole log,
// as Vec::truncate does.
TEST_CASE("append_on_prefix beyond the log keeps the log") {
    konsens::memory_storage s;
    const std::array<konsens::storage_op, 2> ops{
        konsens::append_entries{{make_entry(std::byte{1})}},
        konsens::append_on_prefix{5, {make_entry(std::byte{2})}}};
    REQUIRE(s.write_atomically(ops));
    CHECK(*s.get_log_len() == 2);
}

// Each scalar op sets its value; the node reads them back after a crash.
TEST_CASE("scalar ops set their values") {
    konsens::memory_storage s;
    const konsens::ballot b{1, 2, 0, 3};
    const std::array<konsens::storage_op, 3> ops{konsens::set_promise{b}, konsens::set_accepted_round{b},
                                                 konsens::set_decided_index{0}};
    REQUIRE(s.write_atomically(ops));
    CHECK(*s.get_promise() == b);
    CHECK(*s.get_accepted_round() == b);
}

// The two storages follow one rule for get_entries, so that Sequence
// Paxos reads the same entries from each. A range that reaches past the
// log gives the stored part of it. A start at or past the end gives no
// entries. A start after the end of the range is a wrong use.
TEST_CASE("get_entries returns the stored part of the range") {
    konsens::memory_storage s;
    const std::array<konsens::storage_op, 1> ops{
        konsens::append_entries{{make_entry(std::byte{1}), make_entry(std::byte{2})}}};
    REQUIRE(s.write_atomically(ops));
    CHECK(*s.get_entries(0, 0) == std::vector<konsens::entry>{});
    CHECK(*s.get_entries(0, 1) == std::vector{make_entry(std::byte{1})});
    CHECK(*s.get_entries(1, 5) == std::vector{make_entry(std::byte{2})});
    CHECK(*s.get_entries(0, 5) == std::vector{make_entry(std::byte{1}), make_entry(std::byte{2})});
    CHECK(*s.get_entries(2, 5) == std::vector<konsens::entry>{});
    CHECK(*s.get_entries(3, 5) == std::vector<konsens::entry>{});
    CHECK_THROWS_AS((void)s.get_entries(1, 0), std::logic_error);
}

// memory_storage keeps each field of an entry. The same entries of each
// kind must come back from it as from persistent_storage, so that the
// simulation on each storage sees one log.
TEST_CASE("an entry of each kind comes back with its fields") {
    konsens::memory_storage s;
    const std::vector<konsens::entry> written{
        make_entry(std::byte{1}),
        {konsens::entry_kind::stop_sign, {}},
        {.kind = konsens::entry_kind::register_client, .client_id = 7, .pid = 3},
        {konsens::entry_kind::client_request, {std::byte{2}}, 1, 1},
        {.kind = konsens::entry_kind::expire_session, .session = 1}};
    const std::array<konsens::storage_op, 1> ops{konsens::append_entries{written}};
    REQUIRE(s.write_atomically(ops));
    CHECK(*s.get_entries(0, written.size()) == written);
}

// persistent_storage stores only the fields of the kind of an entry. A
// memory_storage that took more would accept a log that LMDB refuses, so
// both refuse the same wrong use.
TEST_CASE("an entry with a field of another kind is a wrong use") {
    konsens::memory_storage s;
    const std::array<konsens::storage_op, 1> ops{
        konsens::append_entries{{{konsens::entry_kind::normal, {}, 1}}}};
    CHECK_THROWS_AS((void)s.write_atomically(ops), std::logic_error);
    const std::array<konsens::storage_op, 1> on_prefix{
        konsens::append_on_prefix{0, {{.kind = konsens::entry_kind::expire_session, .client_id = 1}}}};
    CHECK_THROWS_AS((void)s.write_atomically(on_prefix), std::logic_error);
    CHECK(*s.get_log_len() == 0);
}
