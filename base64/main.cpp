// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "base64.h"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

using std::size_t;
using std::uint8_t;

using std::cerr;
using std::cout;
using std::string;
using std::string_view;
using std::vector;

enum class Base64Mode {
    SCALAR,
    SVE,
};

bool parse_repetitions(string_view repetitionsArg, size_t itemSize,
                       size_t &repetitions, size_t &inputSize) {
    constexpr size_t maxInputSize = 64 * 1024 * 1024;
    const auto *begin = repetitionsArg.data();
    const auto *end = repetitionsArg.data() + repetitionsArg.size();
    const auto [ptr, ec] = std::from_chars(begin, end, repetitions);

    if (ec == std::errc::result_out_of_range) {
        cerr << "Invalid repetitions: " << repetitionsArg << '\n';
        cerr << "Repetitions value is too large" << '\n';
        return false;
    }

    if (ec != std::errc{} || ptr != end) {
        cerr << "Invalid repetitions: " << repetitionsArg << '\n';
        cerr << "Repetitions must be a positive integer" << '\n';
        return false;
    }

    if (repetitions == 0) {
        cerr << "Invalid repetitions: " << repetitionsArg << '\n';
        cerr << "Repetitions must be greater than zero" << '\n';
        return false;
    }

    if (repetitions > std::numeric_limits<size_t>::max() / itemSize) {
        cerr << "Invalid repetitions: " << repetitions << '\n';
        cerr << "Requested input size is too large" << '\n';
        return false;
    }

    inputSize = repetitions * itemSize;
    if (inputSize > maxInputSize) {
        cerr << "Invalid repetitions: " << repetitions << '\n';
        cerr << "Requested input size must be no more than " << maxInputSize
             << " bytes" << '\n';
        return false;
    }

    return true;
}

int main(int argc, char *argv[]) {

    if (argc != 3) {
        cerr << "Usage = " << argv[0] << " <scalar|sve> <repetitions>" << '\n';
        return EXIT_FAILURE;
    }

    const string_view modeArg = argv[1];
    Base64Mode mode;
    size_t repetitions = 0;
    size_t inputSize = 0;

    if (modeArg == "scalar") {
        mode = Base64Mode::SCALAR;
    } else if (modeArg == "sve") {
        mode = Base64Mode::SVE;
    } else {
        cerr << "Invalid argument: " << modeArg << '\n';
        cerr << "Valid arguments are: scalar, sve" << '\n';
        return EXIT_FAILURE;
    }

    const string sentence = "sveisamazing";

    if (!parse_repetitions(argv[2], sentence.size(), repetitions, inputSize)) {
        return EXIT_FAILURE;
    }

    vector<uint8_t> data;
    data.reserve(inputSize);

    for (size_t i = 0; i < repetitions; i++) {
        data.insert(data.end(), sentence.begin(), sentence.end());
    }

    // Encode Loop
    cout << "Encoded string " << sentence << " " << repetitions << " times"
         << '\n';
    const size_t encodedBufferSize = ((data.size() + 2) / 3) * 4;
    vector<char> encodedBuffer(encodedBufferSize);
    size_t encodedSize = 0;

    vector<uint8_t> decodedBuffer(data.size());
    switch (mode) {
    case Base64Mode::SCALAR:
        encodedSize = base64::base64_scalar_encode(data.data(), data.size(),
                                                   encodedBuffer.data());
        base64::base64_scalar_decode(encodedBuffer.data(), encodedSize,
                                     decodedBuffer.data());
        break;
    case Base64Mode::SVE:
#ifdef __ARM_FEATURE_SVE
        encodedSize = base64::base64_sve_encode(data.data(), data.size(),
                                                encodedBuffer.data());
        base64::base64_sve_decode(encodedBuffer.data(), encodedSize,
                                  decodedBuffer.data());
        break;
#else
        cerr << "SVE mode is not available on this target" << '\n';
        return EXIT_FAILURE;
#endif
    }

    cout << "Decoded " << data.size() << " bytes" << '\n';

    return EXIT_SUCCESS;
}
