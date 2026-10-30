#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include <sys/socket.h>
#include <unistd.h>

#include "address.hpp"
#include "listener.hpp"

using konsensd::listener_errc;

namespace {

const std::filesystem::path test_directory{"test_listener.d"};

std::string address_in_test_directory(const std::string& name)
{
    return "unix:" + (test_directory / name).string();
}

void make_the_test_directory()
{
    std::filesystem::remove_all(test_directory);
    std::filesystem::create_directory(test_directory);
}

}

// A TCP listener is the form that konsensd had before the unix: form.
TEST_CASE("a listener on TCP")
{
    const auto l = konsensd::listener_on("127.0.0.1:0");
    REQUIRE(l);
    CHECK(l->socket_file.empty());
    ::close(l->fd);
}

// The first start: bind makes the socket file.
TEST_CASE("a listener on a unix: path with no file")
{
    make_the_test_directory();
    const auto l = konsensd::listener_on(address_in_test_directory("a.sock"));
    REQUIRE(l);
    CHECK(l->socket_file == test_directory / "a.sock");
    CHECK(std::filesystem::is_socket(l->socket_file));
    ::close(l->fd);
}

// A node that was killed leaves its socket file. Nothing accepts on it,
// so a connect gives ECONNREFUSED, and the next start removes the file
// and binds. Without this, a restart after SIGKILL fails.
TEST_CASE("a listener on a unix: path with a stale socket file")
{
    make_the_test_directory();
    const std::string address = address_in_test_directory("stale.sock");
    const auto a = konsensd::resolve(address);
    REQUIRE(a);
    const int stale = ::socket(AF_UNIX, SOCK_STREAM, 0);
    REQUIRE(stale >= 0);
    REQUIRE(::bind(stale, reinterpret_cast<const sockaddr*>(&a->storage), a->length) == 0);
    ::close(stale);
    REQUIRE(std::filesystem::is_socket(test_directory / "stale.sock"));
    const auto l = konsensd::listener_on(address);
    REQUIRE(l);
    ::close(l->fd);
}

// A second node with the same address must not take the socket of a
// node that runs.
TEST_CASE("a listener on a unix: path where a process accepts")
{
    make_the_test_directory();
    const std::string address = address_in_test_directory("busy.sock");
    const auto first = konsensd::listener_on(address);
    REQUIRE(first);
    const auto second = konsensd::listener_on(address);
    REQUIRE(!second);
    CHECK(second.error() == listener_errc::in_use);
    CHECK(std::filesystem::is_socket(test_directory / "busy.sock"));
    ::close(first->fd);
}

// A path that names a file of another kind is a fault of the
// configuration. konsensd removes no file that is not a socket.
TEST_CASE("a listener on a unix: path that names a regular file")
{
    make_the_test_directory();
    std::ofstream{test_directory / "plain"} << "data";
    const auto l = konsensd::listener_on(address_in_test_directory("plain"));
    REQUIRE(!l);
    CHECK(l.error() == listener_errc::not_a_socket);
    CHECK(std::filesystem::is_regular_file(test_directory / "plain"));
}

// konsensd makes no directory for a socket file.
TEST_CASE("a listener on a unix: path in a directory that does not exist")
{
    make_the_test_directory();
    const auto l = konsensd::listener_on(address_in_test_directory("missing/a.sock"));
    REQUIRE(!l);
    CHECK(l.error() == listener_errc::no_listener);
}

// An address that resolve refuses gives no listener.
TEST_CASE("a listener on an address that does not resolve")
{
    const auto l = konsensd::listener_on("unix:");
    REQUIRE(!l);
    CHECK(l.error() == listener_errc::does_not_resolve);
}

// Each error of a listener has a text for the log.
TEST_CASE("each error of a listener has a reason")
{
    for (const auto e : {listener_errc::does_not_resolve, listener_errc::in_use, listener_errc::not_a_socket,
                         listener_errc::no_listener}) {
        CHECK(konsensd::reason_of(e) != "unknown");
    }
}
