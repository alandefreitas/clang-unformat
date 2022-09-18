//
// Copyright (c) 2022 alandefreitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//

#include "levenshtein.hpp"
#include <fstream>
#include <vector>

std::size_t
levenshtein_distance(std::string_view s1, std::string_view s2) {
    // trivial cases
    const std::size_t m(s1.size());
    const std::size_t n(s2.size());
    if (m == 0) {
        return n;
    }
    if (n == 0) {
        return m;
    }

    // init costs
    std::vector<std::size_t> costs(n + 1);
    for (std::size_t k = 0; k <= n; k++) {
        costs[k] = k;
    }

    // iterate s1
    std::size_t i{ 0 };
    for (char const &c1: s1) {
        costs[0] = i + 1;
        std::size_t corner{ i };
        std::size_t j{ 0 };
        for (char const &c2: s2) {
            std::size_t upper{ costs[j + 1] };
            if (c1 == c2) {
                costs[j + 1] = corner;
            } else {
                std::size_t t(upper < corner ? upper : corner);
                costs[j + 1] = (costs[j] < t ? costs[j] : t) + 1;
            }
            corner = upper;
            j++;
        }
        i++;
    }
    return costs[n];
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
