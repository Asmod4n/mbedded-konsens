#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cerrno>
#include <cstdint>
#include <stdexcept>

#include <liburing.h>

#include "multishot.hpp"

using konsensd::accept_outcome;
using konsensd::receive_outcome;

namespace {

constexpr std::uint32_t buffer_seven = IORING_CQE_F_BUFFER | (std::uint32_t{7} << IORING_CQE_BUFFER_SHIFT);

}

// The loop acts on these outcomes alone, so each combination of res and
// flags that the kernel gives must map to the right one. Without F_MORE
// the operation is ended, and the loop must arm it again or close.
TEST_CASE("each outcome of a multishot recv")
{
    CHECK(konsensd::receive_outcome_of(5, buffer_seven | IORING_CQE_F_MORE) == receive_outcome::bytes_and_armed);
    CHECK(konsensd::receive_outcome_of(5, buffer_seven) == receive_outcome::bytes_and_ended);
    CHECK(konsensd::receive_outcome_of(0, 0) == receive_outcome::end_of_stream);
    CHECK(konsensd::receive_outcome_of(-ENOBUFS, 0) == receive_outcome::no_buffer);
    CHECK(konsensd::receive_outcome_of(-ECONNRESET, 0) == receive_outcome::error);
}

// The kernel sets F_SOCK_NONEMPTY when more bytes wait in the socket. The
// loop reads F_BUFFER and F_MORE only, so this flag changes nothing.
TEST_CASE("F_SOCK_NONEMPTY changes no outcome of a multishot recv")
{
    CHECK(konsensd::receive_outcome_of(5, buffer_seven | IORING_CQE_F_MORE | IORING_CQE_F_SOCK_NONEMPTY) ==
          receive_outcome::bytes_and_armed);
    CHECK(konsensd::receive_outcome_of(5, buffer_seven | IORING_CQE_F_SOCK_NONEMPTY) ==
          receive_outcome::bytes_and_ended);
}

// These combinations do not come from the kernel. Bytes without a buffer
// would have no memory to read, and an armed recv without bytes would
// leave the count of pending operations wrong. EINVAL means that the
// kernel has no multishot recv; a close of each connection would hide
// that.
TEST_CASE("a multishot recv that the kernel does not give is a fault")
{
    CHECK_THROWS_AS(static_cast<void>(konsensd::receive_outcome_of(5, IORING_CQE_F_MORE)), std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(konsensd::receive_outcome_of(5, 0)), std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(konsensd::receive_outcome_of(0, IORING_CQE_F_MORE)), std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(konsensd::receive_outcome_of(-ENOBUFS, IORING_CQE_F_MORE)), std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(konsensd::receive_outcome_of(-EINVAL, 0)), std::logic_error);
}

// A multishot accept ends without F_MORE, and the loop must arm it again.
TEST_CASE("each outcome of a multishot accept")
{
    CHECK(konsensd::accept_outcome_of(9, IORING_CQE_F_MORE) == accept_outcome::descriptor_and_armed);
    CHECK(konsensd::accept_outcome_of(0, IORING_CQE_F_MORE) == accept_outcome::descriptor_and_armed);
    CHECK(konsensd::accept_outcome_of(9, 0) == accept_outcome::descriptor_and_ended);
    CHECK(konsensd::accept_outcome_of(-EMFILE, 0) == accept_outcome::error);
}

// An error ends a multishot accept. EINVAL means that the kernel has no
// multishot accept, and a new arm would give EINVAL again at once.
TEST_CASE("a multishot accept that the kernel does not give is a fault")
{
    CHECK_THROWS_AS(static_cast<void>(konsensd::accept_outcome_of(-EMFILE, IORING_CQE_F_MORE)), std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(konsensd::accept_outcome_of(-EINVAL, 0)), std::logic_error);
}
