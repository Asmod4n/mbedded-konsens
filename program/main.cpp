#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <print>
#include <random>
#include <span>
#include <sstream>
#include <string>
#include <string_view>

#include "config.hpp"
#include "serve.hpp"

namespace {

constexpr std::string_view usage = "usage: konsensd --config FILE [--first-start]";
constexpr int nonce_halves_shift = 32;

std::string_view name_of(const konsensd::config_errc e)
{
    switch (e) {
    case konsensd::config_errc::syntax:
        return "a syntax error";
    case konsensd::config_errc::missing_key:
        return "a missing key";
    case konsensd::config_errc::wrong_type:
        return "a wrong type";
    case konsensd::config_errc::out_of_range:
        return "a value out of range";
    case konsensd::config_errc::unknown_key:
        return "an unknown key";
    case konsensd::config_errc::duplicate_pid:
        return "a pid that stands twice";
    }
    return "an unknown error";
}

int run(const std::span<char*> args)
{
    std::string path;
    bool first_start = false;
    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string_view a = args[i];
        if (a == "--config" && i + 1 < args.size()) {
            path = args[++i];
        } else if (a == "--first-start") {
            first_start = true;
        } else {
            std::println(std::cerr, "{}", usage);
            return konsensd::exit_failed;
        }
    }
    if (path.empty()) {
        std::println(std::cerr, "{}", usage);
        return konsensd::exit_failed;
    }
    std::ifstream in{path};
    if (!in) {
        std::println(std::cerr, "konsensd: the config {} does not open", path);
        return konsensd::exit_failed;
    }
    std::stringstream text;
    text << in.rdbuf();
    const auto c = konsensd::parse_config(text.str());
    if (!c) {
        std::println(std::cerr, "konsensd: the config {} has {} at line {}, column {} (key \"{}\")", path,
                     name_of(c.error().code), c.error().line, c.error().column, c.error().key);
        return konsensd::exit_failed;
    }
    std::random_device random;
    const std::uint64_t nonce = (std::uint64_t{random()} << nonce_halves_shift) | random();
    return konsensd::serve(*c, {first_start, nonce});
}

}

int main(int argc, char** argv)
{
    try {
        return run({argv, static_cast<std::size_t>(argc)});
    } catch (const std::exception& e) {
        std::println(std::cerr, "konsensd: {}", e.what());
        return konsensd::exit_failed;
    }
}
