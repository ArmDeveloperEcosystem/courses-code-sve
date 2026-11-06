// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "run_length_decoding.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string_view>

using std::cerr;
using std::cout;
using std::string_view;

void printUsage(const char *program) {
    cerr << "Usage: " << program << " <scalar|sve> <encoded-input>\n";
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    const string_view implementation{argv[1]};
    const char *const encodedInput = argv[2];
    std::unique_ptr<char[]> decoded;

    if (implementation == "scalar") {
        decoded.reset(runLengthDecode::decodeScalarTwoPass(encodedInput));
#ifdef __ARM_FEATURE_SVE
    } else if (implementation == "sve") {
        decoded.reset(runLengthDecode::decodeSVETwoPass(encodedInput));
#endif // __ARM_FEATURE_SVE
    } else {
        cerr << "Unknown implementation: " << implementation << '\n';
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    cout << decoded.get() << '\n';
    return EXIT_SUCCESS;
}
