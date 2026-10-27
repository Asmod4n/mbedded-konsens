#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <map>
#include <optional>
#include <span>
#include <vector>

#include "key_value.hpp"

namespace {

struct map_view {
    std::map<std::vector<std::byte>, std::vector<std::byte>> values;

    std::optional<std::span<const std::byte>> find(const std::span<const std::byte> key) const
    {
        const auto it = values.find({key.begin(), key.end()});
        if (it == values.end()) {
            return std::nullopt;
        }
        return std::span<const std::byte>{it->second};
    }
};

std::vector<std::byte> bytes_of(const std::string_view s)
{
    const auto b = std::as_bytes(std::span(s));
    return {b.begin(), b.end()};
}

}

// A put gives one mutation with the value; a delete gives one mutation
// with no value, which the library turns into an erase.
TEST_CASE("put and delete give their mutations")
{
    const konsensd::key_value m;
    const map_view view;
    const auto put = m.apply(konsensd::encode_write({konsensd::key_value_op::put, bytes_of("k"), bytes_of("v")}), view);
    REQUIRE(put.mutations.size() == 1);
    CHECK(put.mutations[0].key == bytes_of("k"));
    CHECK(put.mutations[0].value == bytes_of("v"));
    CHECK(put.reply.empty());
    const auto erased = m.apply(konsensd::encode_write({konsensd::key_value_op::erase, bytes_of("k"), {}}), view);
    REQUIRE(erased.mutations.size() == 1);
    CHECK(!erased.mutations[0].value);
    CHECK(m.apply(bytes_of("not cbor"), view).mutations.empty());
}

// An empty value is a value, so the read must tell it from a key that is
// not there.
TEST_CASE("a read tells an empty value from a missing key")
{
    const konsensd::key_value m;
    map_view view;
    view.values[bytes_of("empty")] = {};
    view.values[bytes_of("full")] = bytes_of("x");
    CHECK(konsensd::decode_read(m.query(bytes_of("empty"), view)) == konsensd::key_value_read{true, {}});
    CHECK(konsensd::decode_read(m.query(bytes_of("full"), view)) == konsensd::key_value_read{true, bytes_of("x")});
    CHECK(konsensd::decode_read(m.query(bytes_of("none"), view)) == konsensd::key_value_read{false, {}});
}

// Each op round trips; a number that names no op is refused.
TEST_CASE("each op round trips, and an unknown op is refused")
{
    for (const auto op : {konsensd::key_value_op::put, konsensd::key_value_op::erase}) {
        const konsensd::key_value_write w{op, bytes_of("k"), bytes_of("v")};
        CHECK(konsensd::decode_write(konsensd::encode_write(w)) == w);
    }
    CHECK(!konsensd::decode_write(konsensd::encode_write({static_cast<konsensd::key_value_op>(3), {}, {}})));
    CHECK(!konsensd::decode_write(konsensd::encode_write({static_cast<konsensd::key_value_op>(0), {}, {}})));
}
