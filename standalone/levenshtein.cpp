//
// Copyright (c) 2022 alandefreitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//

#include "levenshtein.hpp"
#include <edlib.h>
#include <fstream>

std::size_t
levenshtein_distance(std::string_view s1, std::string_view s2) {
    return edlibAlign(
               s1.data(),
               s1.size(),
               s2.data(),
               s2.size(),
               edlibDefaultAlignConfig())
        .editDistance;
}

std::size_t
levenshtein_distance(
    const std::filesystem::path &p1,
    const std::filesystem::path &p2) {
    std::ifstream f1(p1);
    std::ifstream f2(p2);
    std::string
        s1((std::istreambuf_iterator<char>(f1)),
           std::istreambuf_iterator<char>());
    std::string
        s2((std::istreambuf_iterator<char>(f2)),
           std::istreambuf_iterator<char>());
    return levenshtein_distance(std::string_view(s1), std::string_view(s2));
}
