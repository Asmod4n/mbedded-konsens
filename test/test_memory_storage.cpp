#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <array>
#include <cstddef>
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

// A read outside the log is a wrong use. It gives an error value and
// reads no memory outside the vector.
TEST_CASE("get_entries outside the log is an error") {
    konsens::memory_storage s;
    CHECK(s.get_entries(0, 0));
    CHECK(s.get_entries(0, 1).error() == std::make_error_code(std::errc::invalid_argument));
    CHECK(s.get_entries(1, 0).error() == std::make_error_code(std::errc::invalid_argument));
}
