//
// Copyright (c) 2022 alandefreitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//

#ifndef CLANG_UNFORMAT_APPLICATION_HPP
#define CLANG_UNFORMAT_APPLICATION_HPP

#include <clang_format.hpp>
#include <cli_config.hpp>
#include <boost/asio/thread_pool.hpp>
#include <filesystem>

class application {
public:
    /// Constructor
    application(int argc, char **argv);

    /// Run the application
    int
    run();

private:
    // Run local search on clang format parameters
    void
    clang_format_local_search();

    // Apply requirements to option
    void
    apply_requirements(
        bool &req_applied,
        clang_format_entry &prev_entry,
        clang_format_possible_values const &possible_values);

    // Print time stats
    void
    print_time_stats(
        std::chrono::steady_clock::duration total_evaluation_time,
        std::size_t total_neighbors_evaluated,
        std::size_t total_neighbors) const;

    // Run tasks to evaluate a given option
    void
    evaluate_option_values(
        const boost::asio::thread_pool::executor_type &ex,
        std::size_t &closest_edit_distance,
        std::size_t &total_neighbors_evaluated,
        std::string const &key,
        clang_format_possible_values const &possible_values);

    // Inherit undetermined values from options with the same prefix
    void
    inherit_undetermined_values();

    // Set default values for undetermined options
    void
    set_default_values();

    // Check if we should format the path according to the config options
    bool
    should_format(const std::filesystem::path &p);

    // Format the given temp directory according to the config options
    bool
    format_temp_directory(const std::filesystem::path &task_temp);

    // Calculate the distance from the formatted files to the original files
    std::size_t
    distance_formatted_files(const std::filesystem::path &task_temp);

    // Evaluate the edit distance for all files in specified directory
    std::size_t
    evaluate(const std::filesystem::path &task_temp);

    // Cmd-line configuration values
    cli_config config_;

    // The current list of clang-format entries
    std::vector<clang_format_entry> current_cf_;

    // Clang format options and their valid values
    std::vector<std::pair<std::string, clang_format_possible_values>> cf_opts_{
        generate_clang_format_options()
    };
};

#endif // CLANG_UNFORMAT_APPLICATION_HPP
