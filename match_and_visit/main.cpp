// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "match_and_visit.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

using std::size_t;
using std::uint32_t;

using std::cerr;
using std::cout;
using std::string_view;
using std::vector;

void printUsage(string_view executable) {
    cerr << "Usage: " << executable << " <scalar|sve>\n";
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    constexpr uint32_t threshold = 9;
    const vector<uint32_t> input{9, 3, 8, 2, 7, 1, 6, 0, 9, 4};
    size_t callbackCount = 0;
    const matchAndVisit::MatchCallback callback = [&callbackCount](uint32_t) {
        ++callbackCount;
    };

    const string_view implementation = argv[1];
    int status = EXIT_FAILURE;

    if (implementation == "scalar") {
        status = matchAndVisit::matchAndVisitScalar(input, threshold, callback);
    } else if (implementation == "sve") {
#ifdef __ARM_FEATURE_SVE
        status = matchAndVisit::matchAndVisitSVEOptimized(input, threshold,
                                                          callback);
#else
        cerr << "The sve implementation requires an SVE-capable build.\n";
        return EXIT_FAILURE;
#endif
    } else {
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    if (status != EXIT_SUCCESS) {
        cerr << "Match and visit produced an unexpected result.\n";
        return EXIT_FAILURE;
    }

    cout << "Callback count: " << callbackCount << '\n';

    return EXIT_SUCCESS;
}
