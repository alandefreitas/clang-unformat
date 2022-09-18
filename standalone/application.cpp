//
// Copyright (c) 2022 alandefreitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//

#include "application.hpp"
#include <clang_format.hpp>
#include <cli_config.hpp>
#include <levenshtein.hpp>
#include <boost/process.hpp>
#include <fmt/chrono.h>
#include <fmt/color.h>
#include <fmt/format.h>
#include <futures/futures.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <numeric>
#include <optional>
#include <vector>
#include <string_view>

namespace fs = std::filesystem;
namespace process = boost::process;

application::application(int argc, char **argv)
    : config_(parse_cli(argc, argv)) {}

int
application::run() {
    if (config_.help || !validate_config(config_)) {
        print_help(program_description());
        return 1;
    }
    clang_format_local_search();
    inherit_undetermined_values();
    set_default_values();
    save(current_cf_, config_.output);
    return 0;
}

inline std::string
pretty_time(std::chrono::steady_clock::duration d) {
    auto mc = std::chrono::duration_cast<std::chrono::microseconds>(d);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(d);
    auto s = std::chrono::duration_cast<std::chrono::seconds>(d);
    auto m = std::chrono::duration_cast<std::chrono::minutes>(d);
    auto h = std::chrono::duration_cast<std::chrono::hours>(d);
    m = m - h;
    s = s - m - h;
    ms = ms - s - m - h;
    mc = mc - ms - s - m - h;
    if (h.count() > 1) {
        return fmt::format("{}:{}:{}", h, m, s);
    } else if (m.count() > 1) {
        return fmt::format("{}:{}:{}", m, s, ms);
    } else {
        return fmt::format("{}:{}:{}", s, ms, mc);
    }
}

bool
application::format_temp_directory(const fs::path &task_temp) {
    auto begin = fs::recursive_directory_iterator(task_temp);
    auto end = fs::recursive_directory_iterator{};
    for (auto it = begin; it != end; ++it) {
        fs::path p = it->path();
        if (!should_format(p)) {
            continue;
        }
        process::ipstream is;
        process::child
            c(config_.clang_format.c_str(),
              "-i",
              fs::absolute(p).c_str(),
              process::std_out > is,
              process::std_err > process::null);
        std::string line;
        bool first_error_line = true;
        while (c.running() && std::getline(is, line) && !line.empty()) {
            if (first_error_line) {
                fmt::print(
                    fmt::fg(fmt::terminal_color::red),
                    "clang-format error!\n");
                first_error_line = false;
            }
            fmt::print(fmt::fg(fmt::terminal_color::red), "{}\n", line);
        }
        c.wait();
        if (c.exit_code() != 0) {
            return false;
        }
    }
    return true;
}

std::size_t
application::distance_formatted_files(const fs::path &task_temp) {
    std::size_t total_distance = 0;
    auto begin = fs::recursive_directory_iterator(config_.input);
    auto end = fs::recursive_directory_iterator{};
    for (auto it = begin; it != end; ++it) {
        fs::path p = it->path();
        if (should_format(p)) {
            fs::path input_relative = fs::relative(p, config_.input);
            fs::path output_relative = task_temp / input_relative;
            total_distance += levenshtein_distance(p, output_relative);
        }
    }
    return total_distance;
}

std::size_t
application::evaluate(const fs::path &task_temp) {
    if (!format_temp_directory(task_temp)) {
        return std::size_t(-1);
    }
    return distance_formatted_files(task_temp);
}

// Apply requirements to option
void
application::apply_requirements(
    bool &req_applied,
    clang_format_entry &prev_entry,
    clang_format_possible_values const &possible_values) {
    if (!possible_values.requirements.first.empty()) {
        auto const &p = possible_values;
        auto current_cf_it = std::find_if(
            current_cf_.begin(),
            current_cf_.end(),
            [&](clang_format_entry const &entry) {
            return entry.key == p.requirements.first;
            });
        if (current_cf_it != current_cf_.end()) {
            req_applied = true;
            prev_entry = *current_cf_it;
            current_cf_it->value = p.requirements.second;
        }
    }
}

// Print stats
void
application::print_time_stats(
    std::chrono::steady_clock::duration total_evaluation_time,
    std::size_t total_neighbors_evaluated,
    std::size_t total_neighbors) const {
    if (!current_cf_.empty() && total_evaluation_time > std::chrono::seconds(1))
    {
        fmt::print("==============================\n");
        fmt::print(
            fmt::fg(fmt::terminal_color::yellow),
            "# Current .clang-format:\n");
        print(current_cf_);
        fmt::print(
            "# Total evaluation time: {}\n",
            pretty_time(total_evaluation_time));
        auto avg_evaluation_time = total_evaluation_time
                                   / total_neighbors_evaluated;
        fmt::print(
            "# Average evaluation time: {} per parameter value\n",
            pretty_time(avg_evaluation_time));
        auto est_evaluation_time = avg_evaluation_time
                                   * (total_neighbors
                                      - total_neighbors_evaluated);
        fmt::print(
            "# Estimated time left: {}\n",
            pretty_time(est_evaluation_time));
        fmt::print("==============================\n\n");
    }
};

// Launch tasks to evaluate option
void
application::evaluate_option_values(
    const boost::asio::thread_pool::executor_type &ex,
    std::size_t &closest_edit_distance,
    std::size_t &total_neighbors_evaluated,
    std::string const &key,
    clang_format_possible_values const &possible_values) {
    // Options table header
    bool value_influenced_output = false;
    std::string improvement_value;
    const std::string empty_str;
    if (possible_values.options.size() > 1) {
        // Table header
        constexpr std::size_t first_col_w
            = std::max(sizeof("Edit distance"), sizeof("Value")) + 2;
        constexpr std::size_t min_col_w = 8;
        fmt::print("┌{0:─^{1}}", empty_str, first_col_w);
        for (const auto &option: possible_values.options) {
            fmt::print(
                "┬{0:─^{1}}",
                empty_str,
                (std::max)(option.size() + 2, min_col_w));
        }
        fmt::print("┐\n");
        fmt::print("│{0: ^{1}}", "Value", first_col_w);
        for (const auto &option: possible_values.options) {
            fmt::print(
                "│{0: ^{1}}",
                option,
                (std::max)(option.size() + 2, min_col_w));
        }
        fmt::print("│\n");

        // Launch evaluation tasks
        std::vector<futures::cfuture<std::size_t>> evaluation_tasks;
        for (std::size_t i = 0; i < possible_values.options.size(); ++i) {
            const auto &possible_value = *(
                possible_values.options.begin()
                + static_cast<std::vector<std::string>::difference_type>(i));
            evaluation_tasks.emplace_back(futures::async(
                ex,
                [this,
                 i,
                 current_cf = current_cf_,
                 possible_value,
                 key = key,
                 empty_str]() mutable {
                // Copy experiment files
                fs::path task_temp = config_.temp / fmt::format("temp_{}", i);
                fs::copy(
                    config_.input,
                    task_temp,
                    fs::copy_options::recursive
                        | fs::copy_options::overwrite_existing);

                // Emplace option in clang format
                current_cf.emplace_back(clang_format_entry{
                    key,
                    possible_value,
                    true,
                    0,
                    false,
                    empty_str });
                save(current_cf, task_temp / ".clang-format");

                // Evaluate
                std::size_t dist = evaluate(task_temp);
                return dist;
                }));
        }

        // Get and analyse results for parameter
        bool skipped_any = false;
        fmt::print("│{0: ^{1}}", "Edit distance", first_col_w);
        for (std::size_t i = 0; i < possible_values.options.size(); ++i) {
            const auto &possible_value = possible_values.options[i];

            // Message
            fmt::print("│");
            std::cout << std::flush;

            // Print some info
            std::size_t dist = evaluation_tasks[i].get();
            std::size_t col_w = (std::max)(possible_value.size() + 2, min_col_w);
            if (dist == std::size_t(-1)) {
                fmt::print(
                    fmt::fg(fmt::terminal_color::yellow),
                    "{0: ^{1}}",
                    "skip",
                    col_w);
                skipped_any = true;
            } else {
                const bool improved = dist < closest_edit_distance;
                const bool closest_is_concrete_value = closest_edit_distance
                                                       != std::size_t(-1);
                if (!value_influenced_output) {
                    value_influenced_output = dist != closest_edit_distance
                                              && closest_is_concrete_value;
                }
                if (improved && closest_is_concrete_value) {
                    fmt::print(
                        fmt::fg(fmt::terminal_color::green),
                        "{0: ^{1}}",
                        dist,
                        col_w);
                } else if (improved || dist == closest_edit_distance) {
                    fmt::print(
                        fmt::fg(fmt::terminal_color::blue),
                        "{0: ^{1}}",
                        dist,
                        col_w);
                } else {
                    fmt::print(
                        fmt::fg(fmt::terminal_color::bright_red),
                        "{0: ^{1}}",
                        dist,
                        col_w);
                }
                if (improved) {
                    closest_edit_distance = dist;
                    improvement_value = possible_value;
                }
            }
            std::cout << std::flush;
            ++total_neighbors_evaluated;
        }
        fmt::print("│\n");

        // table footer
        fmt::print("└{0:─^{1}}", empty_str, first_col_w);
        for (const auto &option: possible_values.options) {
            fmt::print(
                "┴{0:─^{1}}",
                empty_str,
                (std::max)(option.size() + 2, min_col_w));
        }
        fmt::print("┘\n");

        if (skipped_any) {
            fmt::print(
                fmt::fg(fmt::terminal_color::yellow),
                "Skipped option and value pairs not available in clang-format "
                "{}\n",
                config_.clang_format_version);
        }

        // Update the main file
        if (!improvement_value.empty() && value_influenced_output) {
            current_cf_.emplace_back(clang_format_entry{
                key,
                improvement_value,
                value_influenced_output,
                closest_edit_distance,
                closest_edit_distance == std::size_t(-1),
                empty_str });
        } else if (!value_influenced_output) {
            if (!config_.require_influence) {
                if (improvement_value.empty()) {
                    improvement_value = possible_values.options.front();
                }
                current_cf_.emplace_back(clang_format_entry{
                    key,
                    improvement_value,
                    value_influenced_output,
                    closest_edit_distance,
                    closest_edit_distance == std::size_t(-1),
                    empty_str });
                if (closest_edit_distance == std::size_t(-1)) {
                    current_cf_.back().failed = true;
                }
            }
            fmt::print(
                fmt::fg(fmt::terminal_color::yellow),
                "Parameter {} did not affect the output\n",
                key);
        }
    } else {
        fmt::print(
            fmt::fg(fmt::terminal_color::green),
            "Single option for {}: {}\n",
            key,
            possible_values.options.front());
        current_cf_.emplace_back(clang_format_entry{
            key,
            possible_values.options.front(),
            true,
            0,
            false,
            "single option" });
        ++total_neighbors_evaluated;
    }
    if (!current_cf_.empty()) {
        save(current_cf_, config_.output);
    }
    fmt::print("\n");
};

void
application::clang_format_local_search() {
    std::chrono::steady_clock::duration total_evaluation_time(0);
    const std::size_t total_neighbors = std::accumulate(
        cf_opts_.begin(),
        cf_opts_.end(),
        std::size_t(0),
        [](std::size_t x, auto const &p) {
        return x + p.second.options.size();
        });
    std::size_t total_neighbors_evaluated = 0;
    futures::asio::thread_pool pool(config_.parallel);
    auto ex = pool.executor();

    for (const auto &[key, possible_values]: cf_opts_) {
        bool req_applied = false;
        clang_format_entry prev_entry;
        apply_requirements(req_applied, prev_entry, possible_values);
        print_time_stats(
            total_evaluation_time,
            total_neighbors_evaluated,
            total_neighbors);
        fmt::print("Parameter ");
        fmt::print(fmt::fg(fmt::terminal_color::green), "{}\n", key);
        auto closest_edit_distance = std::size_t(-1);
        auto evaluation_start = std::chrono::steady_clock::now();
        evaluate_option_values(
            ex,
            closest_edit_distance,
            total_neighbors_evaluated,
            key,
            possible_values);
        // Undo requirements, unless the new score is already better anyway
        if (req_applied && prev_entry.score < closest_edit_distance) {
            auto has_prev_key = [&](clang_format_entry const &e) {
                return e.key == prev_entry.key;
            };
            auto it = std::
                find_if(current_cf_.begin(), current_cf_.end(), has_prev_key);
            if (it != current_cf_.end()) {
                *it = prev_entry;
            }
        }
        // Update time estimate
        auto evaluation_end = std::chrono::steady_clock::now();
        auto evaluation_time = evaluation_end - evaluation_start;
        total_evaluation_time += evaluation_time;
    }
}

void
application::inherit_undetermined_values() {
    // Fix the ones that failed or did not affect the output
    // Fix with common prefix
    for (clang_format_entry &entry: current_cf_) {
        if (entry.failed || !entry.affected_output) {
            if (entry.failed) {
                fmt::print(
                    fmt::fg(fmt::terminal_color::yellow),
                    "Inheriting failed entry {}\n",
                    entry.key);
            } else {
                fmt::print(
                    fmt::fg(fmt::terminal_color::yellow),
                    "Inheriting innocuous entry {}\n",
                    entry.key);
            }
            auto opts_it = std::
                find_if(cf_opts_.begin(), cf_opts_.end(), [&](auto &p) {
                    return p.first == entry.key;
                });
            auto &opts = opts_it->second;
            // Try to inherit from other similar prefixes
            constexpr auto starts_with =
                [](std::string_view str, std::string_view substr) {
                return str.substr(0, substr.size()) == substr;
            };
            if (!opts.default_value_from_prefix.empty()) {
                std::vector<std::pair<std::string, std::size_t>>
                    other_entries_count;
                for (auto &other_entry: current_cf_) {
                    bool inherit_value = !other_entry.failed
                                         && other_entry.affected_output
                                         && starts_with(
                                             other_entry.key,
                                             opts.default_value_from_prefix);
                    if (inherit_value) {
                        auto it = std::find_if(
                            other_entries_count.begin(),
                            other_entries_count.end(),
                            [&](std::pair<std::string, std::size_t> const &oe) {
                            return oe.first == other_entry.value;
                            });
                        if (it != other_entries_count.end()) {
                            ++it->second;
                        } else {
                            other_entries_count
                                .emplace_back(other_entry.value, 1);
                        }
                    }
                }
                std::sort(
                    other_entries_count.begin(),
                    other_entries_count.end(),
                    [](auto &a, auto &b) { return a.second > b.second; });
                for (auto const &[value, count]: other_entries_count) {
                    auto it = std::
                        find(opts.options.begin(), opts.options.end(), value);
                    if (bool option_is_valid = it != opts.options.end();
                        option_is_valid)
                    {
                        entry.value = value;
                        entry.affected_output = true;
                        entry.failed = false;
                        entry.comment = fmt::format(
                            "inherited from prefix {}",
                            opts.default_value_from_prefix);
                        fmt::print(
                            fmt::fg(fmt::terminal_color::green),
                            "    Inheriting value {} from prefix {} for {}\n",
                            value,
                            opts.default_value_from_prefix,
                            entry.key);
                        break;
                    }
                }
            }
        }
    }
}

void
application::set_default_values() {
    // Fix with default values
    for (clang_format_entry &entry: current_cf_) {
        if (entry.failed || !entry.affected_output) {
            if (entry.failed) {
                fmt::print(
                    fmt::fg(fmt::terminal_color::yellow),
                    "Finding default for failed entry {}\n",
                    entry.key);
            } else {
                fmt::print(
                    fmt::fg(fmt::terminal_color::yellow),
                    "Finding default for innocuous entry {}\n",
                    entry.key);
            }
            auto opts_it = std::
                find_if(cf_opts_.begin(), cf_opts_.end(), [&](auto &p) {
                    return p.first == entry.key;
                });
            auto &opts = opts_it->second;
            if (!opts.default_value.empty()) {
                entry.value = opts.default_value;
                entry.affected_output = true;
                entry.failed = false;
                entry.comment = fmt::
                    format("Using default value {}", opts.default_value);
                fmt::print(
                    fmt::fg(fmt::terminal_color::green),
                    "    Using default value {} for {}\n",
                    opts.default_value,
                    entry.key);
            }
        }
    }
}

bool
application::should_format(const fs::path &p) {
    if (fs::is_regular_file(p) && p.has_extension()) {
        auto file_ext_str = p.extension().string();
        std::string_view file_ext_view = file_ext_str.c_str();
        auto it = std::find_if(
            config_.extensions.begin(),
            config_.extensions.end(),
            [file_ext_view](auto ext) {
            return file_ext_view == ext
                   || (file_ext_view.front() == '.'
                       && file_ext_view.substr(1) == ext);
            });
        return it != config_.extensions.end();
    }
    return false;
}
