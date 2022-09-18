//
// Copyright (c) 2022 alandefreitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//

#ifndef CLANG_UNFORMAT_CLANG_FORMAT_HPP
#define CLANG_UNFORMAT_CLANG_FORMAT_HPP

#include <filesystem>
#include <string>
#include <vector>
#include <string_view>

/// Value for the specified clang format option
struct clang_format_entry {
    /// Option key
    /**
     * @note Options and sub-options are delimited by a '.'
     */
    std::string key;

    /// Option value
    std::string value;

    /// Whether the values for this option have affected the output during
    /// execution
    /**
     * A value is considered to have affected the output whenever at least one
     * of the values for this option has resulted in a different edit distance
     * between the files.
     */
    bool affected_output{ true };

    /// The edit distance achieved with this option value
    std::size_t score{ std::size_t(-1) };

    /// Whether clang-format has failed to evaluate this option during execution
    /**
     * This usually happens when the version of clang-format does not support
     * the option
     */
    bool failed{ false };

    /// A comment about the option
    /**
     * This comment is usually going to be appended to the final clang-format
     * file. It usually described the rationale for deciding on a value, be it
     * the score or inheriting values from similar options.
     */
    std::string comment;
};

/// Print a list of clang format entries
/**
 * This prints the list as if it were in a file. It's used during execution to
 * show partial results.
 */
void
print(const std::vector<clang_format_entry> &current_cf);

/// Save a list of clang format entries
/**
 * This save the list in a file. It's used at the end of execution.
 */
void
save(
    const std::vector<clang_format_entry> &current_cf,
    const std::filesystem::path &output);

/// Possible values for the specified clang format option
struct clang_format_possible_values {
    /// Constructor
    clang_format_possible_values(std::initializer_list<std::string_view> opts)
        : options(opts.begin(), opts.end()) {}

    /// Values we can use in this parameter
    std::vector<std::string> options;

    /// A required value another parameter needs for this parameter to work
    /**
     * Some options only make sense if a second option has a given value
     */
    std::pair<std::string, std::string> requirements;

    /// Prefix from which we should take the default values if this doesn't
    /// affect the output
    /**
     * If we cannot value a reasonable value for this option, the value is
     * undetermined.
     *
     * When the execution is over, we go through these undetermined values and
     * inherit their default values from similar options whose results are not
     * undetermined.
     *
     * Options with the specified prefix are considered similar options.
     */
    std::string default_value_from_prefix;

    /// Default value if there is no prefix and this doesn't affect the output
    /**
     * If no value can be determined or inherit, we use the default value below.
     */
    std::string default_value;
};

/// Generate a list of all clang format options and their possible values
std::vector<std::pair<std::string, clang_format_possible_values>>
generate_clang_format_options();

#endif // CLANG_UNFORMAT_CLANG_FORMAT_HPP
