//
// Copyright (c) 2022 alandefreitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//

#ifndef CLANG_UNFORMAT_LEVENSHTEIN_HPP
#define CLANG_UNFORMAT_LEVENSHTEIN_HPP

#include <filesystem>
#include <string_view>

/// Levenshtein distance between two strings
std::size_t
levenshtein_distance(std::string_view s1, std::string_view s2);

/// Levenshtein distance between the contents of two files
std::size_t
levenshtein_distance(
    const std::filesystem::path &p1,
    const std::filesystem::path &p2);

#endif // CLANG_UNFORMAT_LEVENSHTEIN_HPP
