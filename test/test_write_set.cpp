#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <unistd.h>

#include "konsens/persistent_storage.hpp"
#include "konsens/state_machine.hpp"
#include "konsens/write_set.hpp"
#include "temporary_directory.hpp"

namespace {

// The base of Execute-Verify lives in a write set in memory over the open
// transaction (doc/plan-write-path.md, section 23, the simple form). These
// tests hold the write set to the reads that a SAVEPOINT over the same
// transaction gives, so that every request sees the same base as before.

constexpr uid_t non_root_user_id = 1000;
constexpr std::size_t max_size = std::size_t{16} << 20;

std::vector<std::byte> bytes_of(const std::string_view text)
{
    std::vector<std::byte> result;
    for (const char c : text) {
        result.push_back(static_cast<std::byte>(c));
    }
    return result;
}

enum class op_kind { insert, erase, insert_session, erase_session, write_back };

struct op {
    op_kind kind;
    std::string_view key;
    std::string_view value;
    std::uint64_t session;
    std::uint64_t request_number;
};

struct reads {
    std::vector<std::optional<std::vector<std::byte>>> values;
    std::vector<std::optional<konsens::session_record>> sessions;

    friend bool operator==(const reads&, const reads&) = default;
};

constexpr std::array keys{std::string_view{"parent"}, std::string_view{"a"}, std::string_view{"b"},
                          std::string_view{""}};
constexpr std::array sessions{std::uint64_t{1}, std::uint64_t{2}, std::uint64_t{3}};

template <typename T>
reads read_all(const T& t)
{
    reads r;
    for (const auto k : keys) {
        const auto found = t.find(bytes_of(k));
        REQUIRE(found.has_value());
        r.values.push_back(*found ? std::optional{std::vector<std::byte>((*found)->begin(), (*found)->end())}
                                  : std::nullopt);
    }
    for (const auto s : sessions) {
        const auto found = t.find_session(s);
        REQUIRE(found.has_value());
        r.sessions.push_back(*found);
    }
    return r;
}

template <typename T>
void apply(T& t, const op& o)
{
    const auto key = bytes_of(o.key);
    switch (o.kind) {
    case op_kind::insert:
        REQUIRE(t.insert_or_assign(key, bytes_of(o.value)));
        break;
    case op_kind::erase:
        REQUIRE(t.erase(key));
        break;
    case op_kind::insert_session:
        REQUIRE(t.insert_or_assign_session(o.session, {o.request_number, bytes_of(o.value)}));
        break;
    case op_kind::erase_session:
        REQUIRE(t.erase_session(o.session));
        break;
    case op_kind::write_back: {
        const auto found = t.find(key);
        REQUIRE(found.has_value());
        REQUIRE(found->has_value());
        REQUIRE(t.insert_or_assign(key, **found));
        break;
    }
    }
}

konsens::persistent_storage storage_with_a_parent_state(const std::filesystem::path& directory)
{
    auto opened = konsens::persistent_storage::open(directory, max_size, non_root_user_id);
    REQUIRE(opened.has_value());
    auto w = opened->begin();
    REQUIRE(w.has_value());
    REQUIRE(w->insert_or_assign(bytes_of("parent"), bytes_of("committed")));
    REQUIRE(w->insert_or_assign_session(2, {7, bytes_of("reply")}));
    REQUIRE(std::move(*w).commit());
    return std::move(*opened);
}

std::pair<std::vector<reads>, std::vector<reads>> run_both(const std::vector<op>& ops)
{
    const temporary_directory dir;
    auto storage = storage_with_a_parent_state(dir.path());
    auto w = storage.begin();
    REQUIRE(w.has_value());
    std::vector<reads> from_the_set;
    std::vector<reads> from_the_savepoint;
    {
        konsens::write_set<konsens::persistent_storage::transaction> s{*w};
        from_the_set.push_back(read_all(s));
        for (const auto& o : ops) {
            apply(s, o);
            from_the_set.push_back(read_all(s));
        }
    }
    const auto parent_before = read_all(*w);
    {
        auto n = w->begin_nested();
        REQUIRE(n.has_value());
        from_the_savepoint.push_back(read_all(*n));
        for (const auto& o : ops) {
            apply(*n, o);
            from_the_savepoint.push_back(read_all(*n));
        }
    }
    CHECK(read_all(*w) == parent_before);
    CHECK(from_the_set.front() == parent_before);
    return {from_the_set, from_the_savepoint};
}

}

// Each sequence is one way a base can write a key or a session: the
// writes of done records and own results, in log order, and the reads of
// the request that runs on top of them.
TEST_CASE("a write set gives the reads of a savepoint over the same transaction")
{
    const std::vector<std::vector<op>> cases{
        {{op_kind::insert, "a", "1", 0, 0}, {op_kind::erase, "a", "", 0, 0}},
        {{op_kind::erase, "b", "", 0, 0}},
        {{op_kind::erase, "a", "", 0, 0}, {op_kind::insert, "a", "2", 0, 0}},
        {{op_kind::insert, "a", "1", 0, 0}, {op_kind::insert, "a", "22", 0, 0}},
        {{op_kind::erase, "parent", "", 0, 0}},
        {{op_kind::insert, "parent", "new", 0, 0}, {op_kind::erase, "parent", "", 0, 0},
         {op_kind::insert, "parent", "again", 0, 0}},
        {{op_kind::insert, "b", "", 0, 0}},
        {{op_kind::insert, "", "empty key", 0, 0}, {op_kind::erase, "", "", 0, 0}},
        {{op_kind::write_back, "parent", "", 0, 0}},
        {{op_kind::insert, "a", "value", 0, 0}, {op_kind::write_back, "a", "", 0, 0}},
        {{op_kind::insert_session, "", "", 1, 0}, {op_kind::insert_session, "", "r1", 1, 1},
         {op_kind::erase_session, "", "", 1, 0}},
        {{op_kind::erase_session, "", "", 2, 0}},
        {{op_kind::erase_session, "", "", 3, 0}},
        {{op_kind::erase_session, "", "", 2, 0}, {op_kind::insert_session, "", "", 2, 0}},
        {{op_kind::insert_session, "", "r8", 2, 8}},
    };
    for (const auto& ops : cases) {
        const auto [from_the_set, from_the_savepoint] = run_both(ops);
        CHECK(from_the_set == from_the_savepoint);
    }
}

// A value that the write set gives out stays the same bytes while later
// writes go to other keys, because the machine may hold it while it reads on.
TEST_CASE("a value read from a write set stays while other keys are written")
{
    const temporary_directory dir;
    auto storage = storage_with_a_parent_state(dir.path());
    auto w = storage.begin();
    REQUIRE(w.has_value());
    konsens::write_set<konsens::persistent_storage::transaction> s{*w};
    REQUIRE(s.insert_or_assign(bytes_of("a"), bytes_of("held")));
    const auto found = s.find(bytes_of("a"));
    REQUIRE(found.has_value());
    REQUIRE(found->has_value());
    for (const auto k : {"b", "c", "d", "e"}) {
        REQUIRE(s.insert_or_assign(bytes_of(k), bytes_of(k)));
    }
    CHECK(std::vector<std::byte>((*found)->begin(), (*found)->end()) == bytes_of("held"));
}

// The machines take the view that run_request gives them. The concept
// state_machine checks write_view, so write_view is the view of a write set.
TEST_CASE("write_view is the view of a write set over the persistent transaction")
{
    CHECK(std::same_as<konsens::write_view,
                       konsens::transaction_view<konsens::write_set<konsens::persistent_storage::transaction>>>);
}
