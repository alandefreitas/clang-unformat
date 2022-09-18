//
// Copyright (c) 2022 alandefreitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//

#ifndef CLANG_UNFORMAT_CLI_CONFIG_HPP
#define CLANG_UNFORMAT_CLI_CONFIG_HPP

#include <boost/program_options/options_description.hpp>
#include <filesystem>
#include <thread>

/// The command line options
struct cli_config {
    bool help{ false };
    std::filesystem::path input;
    std::filesystem::path output;
    std::filesystem::path temp;
    std::filesystem::path clang_format;
    std::size_t clang_format_version{ 0 };
    std::vector<std::string> extensions;
    std::size_t parallel{ std::thread::hardware_concurrency() };
    bool require_influence{ false };
};

/// Print the config options
void
print_help(const boost::program_options::options_description &desc);

/// Get options description
boost::program_options::options_description &
program_description();

/// Parse the command line options
cli_config
parse_cli(int argc, char **argv);

bool
validate_config(cli_config &config);

#endif // CLANG_UNFORMAT_CLI_CONFIG_HPP
