//
// Copyright (c) 2022 alandefreitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//

#include "cli_config.hpp"
#include <boost/process.hpp>
#include <boost/program_options.hpp>
#include <fmt/color.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <fmt/ranges.h>
#include <algorithm>
#include <charconv>

#if FMT_VERSION >= 90000
template <>
struct fmt::formatter<boost::program_options::options_description>
    : ostream_formatter {};
template <>
struct fmt::formatter<std::filesystem::path> : ostream_formatter {};
#endif

namespace fs = std::filesystem;
namespace process = boost::process;

void
print_help(const boost::program_options::options_description &desc) {
    fmt::print("{}\n", desc);
}

boost::program_options::options_description &
program_description() {
    namespace fs = std::filesystem;
    namespace po = boost::program_options;
    static po::options_description desc("clang-unformat");
    // clang-format off
    const fs::path empty_path;
    if (desc.options().empty()) {
        desc.add_options()
        ("help", "produce help message")
        ("input", po::value<fs::path>()->default_value(empty_path), "input directory with source files")
        ("output", po::value<fs::path>()->default_value(empty_path), "output path for the clang-format file")
        ("temp", po::value<fs::path>()->default_value(empty_path), "temporary directory to formatted source files")
        ("clang-format", po::value<fs::path>()->default_value(empty_path), "path to the clang-format executable")
        ("parallel", po::value<std::size_t>()->default_value(std::min(std::thread::hardware_concurrency(), static_cast<unsigned int>(1))), "number of threads")
        ("require-influence", po::value<bool>()->default_value(false), "only include parameters that influence the output")
        ("extensions", po::value<std::vector<std::string>>(), "file extensions to format");
    }
    // clang-format on
    return desc;
}

cli_config
parse_cli(int argc, char **argv) {
    namespace fs = std::filesystem;
    namespace po = boost::program_options;
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, program_description()), vm);
    po::notify(vm);
    cli_config c;
    c.help = vm.count("help");
    c.input = vm["input"].as<fs::path>();
    c.output = vm["output"].as<fs::path>();
    c.temp = vm["temp"].as<fs::path>();
    c.clang_format = vm["clang-format"].as<fs::path>();
    if (vm.count("extensions")) {
        c.extensions = vm["extensions"].as<std::vector<std::string>>();
    }
    c.parallel = vm["parallel"].as<std::size_t>();
    c.require_influence = vm["require-influence"].as<bool>();
    return c;
}

bool
equal_directory_layout(const fs::path &temp, const fs::path &input) {
    auto begin = fs::recursive_directory_iterator(input);
    auto end = fs::recursive_directory_iterator{};
    for (auto it = begin; it != end; ++it) {
        fs::path input_relative = fs::relative(*it, input);
        fs::path output_relative = temp / input_relative;
        if (!fs::exists(output_relative)) {
            return false;
        }
    }
    return true;
}

bool
equal_subdirectory_layout(const fs::path &temp, const fs::path &input) {
    auto begin = fs::directory_iterator(temp);
    auto end = fs::directory_iterator{};
    for (auto it = begin; it != end; ++it) {
        auto const &p = *it;
        if (!fs::is_directory(p) || !equal_directory_layout(p, input)) {
            return false;
        }
    }
    return true;
}

bool
validate_input_dir(cli_config const &config) {
    fmt::print(fmt::fg(fmt::terminal_color::blue), "## Validating input\n");
    if (config.input.empty()) {
        fmt::print(
            fmt::fg(fmt::terminal_color::red),
            "Input directory not provided\n");
        return false;
    }
    if (!fs::exists(config.input)) {
        fmt::print(
            fmt::fg(fmt::terminal_color::red),
            "Input {} is does not exist",
            config.input);
        return false;
    }
    if (!fs::is_directory(config.input)) {
        fmt::print(
            fmt::fg(fmt::terminal_color::red),
            "Input {} is not a directory",
            config.input);
        return false;
    }
    fmt::print(
        fmt::fg(fmt::terminal_color::green),
        "config \"input\" {} OK!\n",
        config.input);
    fmt::print("\n");
    return true;
}

bool
validate_output_dir(cli_config &config) {
    fmt::print(fmt::fg(fmt::terminal_color::blue), "## Validating output\n");
    if (config.output.empty()) {
        fmt::print("No output path set\n");
        config.output = config.input / ".clang-format";
        fmt::print(
            fmt::fg(fmt::terminal_color::yellow),
            "output path set to {}\n",
            config.output);
    }
    if (fs::exists(config.output)) {
        fmt::print("output path {} already exists\n", config.output);
        if (fs::is_directory(config.output)) {
            fmt::print(
                fmt::fg(fmt::terminal_color::yellow),
                "output {} is a directory\n",
                config.output);
            config.output /= ".clang-format";
            fmt::print(
                fmt::fg(fmt::terminal_color::yellow),
                "output set to {}\n",
                config.output);
            return true;
        }
        return false;
    }
    if (config.output.filename() != ".clang-format") {
        fmt::print(
            fmt::fg(fmt::terminal_color::red),
            "output file {} should be .clang-format\n",
            config.output);
        return false;
    }
    if (!fs::exists(config.output)) {
        fmt::print(
            fmt::fg(fmt::terminal_color::blue),
            "output file {} doesn't exist yet\n",
            config.output);
    }
    fmt::print(
        fmt::fg(fmt::terminal_color::green),
        "config \"output\" {} OK!\n",
        config.output);
    fmt::print("\n");
    return true;
}

bool
validate_temp_dir(cli_config &config) {
    fmt::print(fmt::fg(fmt::terminal_color::blue), "## Validating temp\n");
    if (config.temp.empty()) {
        fmt::print("No temp directory set\n");
        config.temp = fs::current_path() / "clang-unformat-temp";
        fmt::print(
            fmt::fg(fmt::terminal_color::yellow),
            "temp directory set to {}\n",
            config.temp);
    }
    if (fs::exists(config.temp)) {
        fmt::print(
            fmt::fg(fmt::terminal_color::yellow),
            "temp directory {} already exists\n",
            config.temp);
        if (!fs::is_directory(config.temp)) {
            fmt::print(
                fmt::fg(fmt::terminal_color::red),
                "temp {} is not a directory\n",
                config.temp);
            return false;
        }
        auto n_files = std::distance(
            fs::directory_iterator(config.temp),
            fs::directory_iterator{});
        if (n_files) {
            if (equal_directory_layout(config.temp, config.input)
                || equal_subdirectory_layout(config.temp, config.input))
            {
                fmt::print(
                    "temp directory {} is not empty but has an valid directory "
                    "layout\n",
                    config.temp);
            } else {
                fmt::print(
                    fmt::fg(fmt::terminal_color::red),
                    "temp directory {} cannot be used\n",
                    config.temp);
                return false;
            }
        }
    } else {
        fs::create_directories(config.temp);
        fmt::print(
            fmt::fg(fmt::terminal_color::green),
            "temp directory {} created\n",
            config.temp);
    }
    fmt::print(
        fmt::fg(fmt::terminal_color::green),
        "config \"temp\" {} OK!\n",
        config.temp);
    fmt::print("\n");
    return true;
}

// store the clang format version for future error messages
// we run clang-format --version with boost::process to extract the version
bool
set_clang_format_version(cli_config &config) {
    fmt::print(
        fmt::fg(fmt::terminal_color::yellow),
        "default to {}\n",
        config.clang_format);
    process::ipstream is;
    process::child
        c(config.clang_format.c_str(), "--version", process::std_out > is);
    std::string line;

    while (c.running() && std::getline(is, line) && !line.empty()) {
        fmt::print(fmt::fg(fmt::terminal_color::green), "{}\n", line);
        std::string_view line_view(line);
        // find line with version
        if (line_view.substr(0, 21) != "clang-format version ")
            continue;

        // find version major in the line
        std::string_view::size_type major_end = line_view.find_first_of('.', 21);
        if (major_end == std::string_view::npos) {
            fmt::print(
                fmt::fg(fmt::terminal_color::red),
                "Cannot find major version in \"{}\"\n",
                line);
            return false;
        }

        // convert the major string to major int
        auto clang_version_view = line_view.substr(21, major_end - 21);
        std::size_t major = 0;
        auto res = std::from_chars(
            clang_version_view.data(),
            clang_version_view.data() + clang_version_view.size(),
            major,
            10);
        if (res.ec != std::errc{}) {
            fmt::print(
                fmt::fg(fmt::terminal_color::red),
                "Cannot parse major version \"{}\" as integer\n",
                clang_version_view);
            return false;
        }

        config.clang_format_version = major;
        if (major < 13) {
            fmt::print(
                fmt::fg(fmt::terminal_color::red),
                "You might want to update clang-format from {} for this to "
                "work properly\n",
                major);
        }
    }
    return true;
}

bool
validate_clang_format_executable(cli_config &config) {
    fmt::print(
        fmt::fg(fmt::terminal_color::blue),
        "## Validating clang-format\n");
    if (config.clang_format.empty()) {
        fmt::print("no clang-format path set\n");
        config.clang_format = process::search_path("clang-format").c_str();
        if (fs::exists(config.clang_format)) {
            set_clang_format_version(config);
        } else {
            fmt::print(
                fmt::fg(fmt::terminal_color::red),
                "cannot find clang-format in PATH\n");
            return false;
        }
    } else if (!fs::exists(config.clang_format)) {
        fmt::print(
            fmt::fg(fmt::terminal_color::red),
            "cannot find clang-format in path {}\n",
            config.clang_format);
        return false;
    }
    fmt::print(
        fmt::fg(fmt::terminal_color::green),
        "config \"clang_format\" {} OK!\n",
        config.clang_format);
    fmt::print("\n");
    return true;
}

bool
validate_file_extensions(cli_config &config) {
    fmt::print(
        fmt::fg(fmt::terminal_color::blue),
        "## Validating file extensions\n");
    if (config.extensions.empty()) {
        fmt::print("no file extensions set\n");
        config.extensions = { "h", "hpp", "cpp", "ipp" };
        fmt::print(
            fmt::fg(fmt::terminal_color::yellow),
            "default to: {}\n",
            config.extensions);
    }
    fmt::print(
        fmt::fg(fmt::terminal_color::green),
        "config \"extensions\" {} OK!\n",
        config.extensions);
    fmt::print("\n");
    return true;
}

bool
validate_threads(cli_config &config) {
    fmt::print(fmt::fg(fmt::terminal_color::blue), "## Validating threads\n");
    if (config.parallel == 0) {
        fmt::print(
            fmt::fg(fmt::terminal_color::yellow),
            "Cannot execute with {} threads\n",
            config.parallel);
        config.parallel = std::
            min(std::thread::hardware_concurrency(),
                static_cast<unsigned int>(1));
        fmt::print(
            fmt::fg(fmt::terminal_color::yellow),
            "Defaulting to {} threads\n",
            config.parallel);
    }
    fmt::print(
        fmt::fg(fmt::terminal_color::green),
        "config \"parallel\" {} OK!\n",
        config.parallel);
    fmt::print("\n");
    return true;
}

bool
validate_config(cli_config &config) {
    namespace fs = std::filesystem;
#define CHECK(expr) \
    if (!(expr))    \
    return false
    CHECK(validate_input_dir(config));
    CHECK(validate_output_dir(config));
    CHECK(validate_temp_dir(config));
    CHECK(validate_clang_format_executable(config));
    CHECK(validate_file_extensions(config));
    CHECK(validate_threads(config));
#undef CHECK
    fmt::print("=============================\n\n");
    return true;
}
