#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <string>
#include <string_view>

#include "config.hpp"

using konsensd::config_errc;

namespace {

constexpr std::string_view whole = R"(
[cluster]
name = "home"
configuration_id = 2

[node]
pid = 1
data_directory = "/var/lib/konsens"
peer_address = "[::]:7151"
client_address = "[::]:7152"
leader_priority = 4
election_ticks = 7
resend_ticks = 2
tick_milliseconds = 50
max_frame_bytes = 4096
max_body_bytes = 512
max_store_bytes = 8192
build_from = [5, 6]

[[peers]]
pid = 2
address = "192.0.2.2:7151"

[[peers]]
pid = 3
address = "192.0.2.3:7151"
)";

constexpr std::string_view smallest = R"(
[cluster]
name = "home"
configuration_id = 1

[node]
pid = 1
data_directory = "/d"
peer_address = "[::]:7151"
client_address = "[::]:7152"

[[peers]]
pid = 2
address = "192.0.2.2:7151"
)";

std::string replaced(std::string_view text, const std::string_view from, const std::string_view to)
{
    std::string s{text};
    const auto at = s.find(from);
    REQUIRE(at != std::string::npos);
    s.replace(at, from.size(), to);
    return s;
}

config_errc error_of(const std::string& text)
{
    const auto c = konsensd::parse_config(text);
    REQUIRE(!c);
    return c.error().code;
}

}

// The config of konsensctl init carries every field, and each one must
// reach the program unchanged.
TEST_CASE("each field of a whole config is read")
{
    const auto c = konsensd::parse_config(whole);
    REQUIRE(c);
    CHECK(c->cluster_name == "home");
    CHECK(c->configuration_id == 2);
    CHECK(c->pid == 1);
    CHECK(c->data_directory == "/var/lib/konsens");
    CHECK(c->peer_address == "[::]:7151");
    CHECK(c->client_address == "[::]:7152");
    CHECK(c->leader_priority == 4);
    CHECK(c->election_ticks == 7);
    CHECK(c->resend_ticks == 2);
    CHECK(c->tick_milliseconds == 50);
    CHECK(c->max_frame_bytes == 4096);
    CHECK(c->max_body_bytes == 512);
    CHECK(c->max_store_bytes == 8192);
    CHECK(c->build_from == std::vector<konsens::node_id>{5, 6});
    CHECK(c->peers == std::vector<konsensd::peer>{{2, "192.0.2.2:7151"}, {3, "192.0.2.3:7151"}});
    const auto n = konsensd::node_config_of(*c, true);
    CHECK(n.configuration_id == 2);
    CHECK(n.pid == 1);
    CHECK(n.peers == std::vector<konsens::node_id>{2, 3});
    CHECK(n.leader_priority == 4);
    CHECK(n.election_tick_timeout == 7);
    CHECK(n.resend_message_tick_timeout == 2);
    CHECK(n.build_from == std::vector<konsens::node_id>{5, 6});
    CHECK(n.first_start);
}

// The optional fields have defaults, so a short config from a person
// works too.
TEST_CASE("the optional fields take their defaults")
{
    const auto c = konsensd::parse_config(smallest);
    REQUIRE(c);
    CHECK(c->leader_priority == 0);
    CHECK(c->election_ticks == konsensd::default_election_ticks);
    CHECK(c->resend_ticks == konsensd::default_resend_ticks);
    CHECK(c->tick_milliseconds == konsensd::default_tick_milliseconds);
    CHECK(c->max_frame_bytes == konsensd::default_max_frame_bytes);
    CHECK(c->max_body_bytes == konsensd::default_max_body_bytes);
    CHECK(c->max_store_bytes == konsensd::default_max_store_bytes);
    CHECK(c->build_from.empty());
    CHECK(!konsensd::node_config_of(*c, false).first_start);
}

// Each required field that is missing names itself, so the operator
// knows what to add.
TEST_CASE("a missing required field is refused with its name")
{
    for (const std::string_view line :
         {"name = \"home\"\n", "configuration_id = 1\n", "pid = 1\ndata", "data_directory = \"/d\"\n",
          "peer_address = \"[::]:7151\"\n", "client_address = \"[::]:7152\"\n", "address = \"192.0.2.2:7151\"\n"}) {
        const auto text = replaced(smallest, line, line.ends_with("data") ? "data" : "");
        const auto c = konsensd::parse_config(text);
        REQUIRE(!c);
        CHECK(c.error().code == config_errc::missing_key);
    }
    CHECK(error_of(replaced(smallest, "[[peers]]\npid = 2\naddress = \"192.0.2.2:7151\"\n", "")) ==
          config_errc::missing_key);
    CHECK(error_of(replaced(smallest, "[cluster]", "[other]")) == config_errc::unknown_key);
}

// The set of error codes is finite; each one is reached by the input
// that a person writes by mistake.
TEST_CASE("each error code is reached")
{
    CHECK(error_of(replaced(smallest, "name = \"home\"", "name = ")) == config_errc::syntax);
    CHECK(error_of(replaced(smallest, "pid = 1", "pid = \"1\"")) == config_errc::wrong_type);
    CHECK(error_of(replaced(smallest, "pid = 1", "pid = 0")) == config_errc::out_of_range);
    CHECK(error_of(replaced(smallest, "configuration_id = 1", "configuration_id = 4294967296")) ==
          config_errc::out_of_range);
    CHECK(error_of(replaced(smallest, "name = \"home\"", "name = \"\"")) == config_errc::out_of_range);
    CHECK(error_of(replaced(smallest, "pid = 1", "pid = 1\nport = 3")) == config_errc::unknown_key);
    CHECK(error_of(replaced(smallest, "pid = 2", "pid = 1")) == config_errc::duplicate_pid);
    CHECK(error_of(std::string{smallest} + "[[peers]]\npid = 2\naddress = \"x:1\"\n") == config_errc::duplicate_pid);
    CHECK(error_of(replaced(smallest, "[node]", "node = 3\n[x]")) == config_errc::unknown_key);
}

// A wrong type in each table, and in the list of pids, is caught where
// it stands, with its line.
TEST_CASE("a wrong type names its line")
{
    const auto c = konsensd::parse_config(replaced(smallest, "configuration_id = 1", "configuration_id = 1.5"));
    REQUIRE(!c);
    CHECK(c.error().code == config_errc::wrong_type);
    CHECK(c.error().key == "configuration_id");
    CHECK(c.error().line == 4);
    CHECK(error_of(replaced(smallest, "client_address = \"[::]:7152\"", "client_address = \"[::]:7152\"\nbuild_from = [\"a\"]")) ==
          config_errc::wrong_type);
    CHECK(error_of(replaced(smallest, "client_address = \"[::]:7152\"", "client_address = \"[::]:7152\"\nbuild_from = 3")) ==
          config_errc::wrong_type);
    CHECK(error_of("peers = 3\n" + replaced(smallest, "[[peers]]\npid = 2\naddress = \"192.0.2.2:7151\"", "")) ==
          config_errc::wrong_type);
}
