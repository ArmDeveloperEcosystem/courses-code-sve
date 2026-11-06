// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "run_length_decoding.h"
#include "SVE-task-based-course.h"

#ifdef __aarch64__
#include <arm_sve.h>
#else
#warning Platform is not AArch64: no SVE-accelerated implementations will be available !
#endif

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <utility>

using std::size_t;
using std::uint8_t;

using std::string;

namespace {
INLINE size_t _calcDecodeLength(const char *encodedString) {
    //
    // calculate the total number of bytes required in decoded output
    //
    size_t len{0};
    while (*encodedString != '\0') {
        size_t count = 0;
        // Parse one or more digits.
        for (; *encodedString >= '0' && *encodedString <= '9';
             ++encodedString) {
            count = count * 10 + static_cast<size_t>(*encodedString - '0');
        }
        // Skip over the character being repeated.
        ++encodedString;
        len += count;
    }
    return len;
}

#ifdef __ARM_FEATURE_SVE
INLINE char *_sve_fill(char *output, char value, size_t count) {
    size_t i = 0;
    svuint8_t values = svdup_n_u8(static_cast<uint8_t>(value));

    while (i < count) {
        svbool_t pg = svwhilelt_b8(i, count);
        svst1_u8(pg, reinterpret_cast<uint8_t *>(output + i), values);
        i += svcntb();
    }

    return output + count;
}
#endif // __ARM_FEATURE_SVE
} // namespace

namespace runLengthDecode {

char *decodeScalarTwoPass(const char *encodedString) {
    if (encodedString == nullptr || *encodedString == '\0') {
        char *result = new char[1];
        result[0] = '\0';
        return result;
    }

    // Pass 1: determine the decoded length once and allocate to output
    size_t decodedLength = _calcDecodeLength(encodedString);
    char *output = new char[decodedLength + 1];
    char *result = output;

    // Pass 2: decode.
    const char *input = encodedString;

    while (*input != '\0') {

        size_t count = 0;

        // Parse the repeat count.
        for (; *input >= '0' && *input <= '9'; ++input) {
            count = count * 10 + static_cast<size_t>(*input - '0');
        }
        // Character to repeat.
        char value = *input++;
        // Write 'count' copies.

        /* NAIVE FOR LOOP
        for (size_t i = 0; i < count; ++i) {
            *output++ = value;
        }*/

        std::memset(output, value, count);
        output += count;
    }
    *output = '\0';
    return result;
}

char *decodeScalarSinglePass(const char *encodedString) {
    if (encodedString == nullptr || *encodedString == '\0') {
        char *result = new char[1];
        result[0] = '\0';
        return result;
    }

    constexpr size_t initialCapacity = 16;
    size_t capacity = initialCapacity;
    size_t decodedLength = 0;
    auto result = std::make_unique<char[]>(capacity);
    const char *input = encodedString;

    while (*input != '\0') {
        size_t count = 0;
        for (; *input >= '0' && *input <= '9'; ++input) {
            count = count * 10 + static_cast<size_t>(*input - '0');
        }

        if (*input == '\0') {
            break;
        }

        const char value = *input++;
        const size_t requiredCapacity = decodedLength + count + 1;
        if (requiredCapacity > capacity) {
            size_t newCapacity = capacity;
            while (newCapacity < requiredCapacity) {
                newCapacity *= 2;
            }

            auto expanded = std::make_unique<char[]>(newCapacity);
            std::memcpy(expanded.get(), result.get(), decodedLength);
            result = std::move(expanded);
            capacity = newCapacity;
        }

        std::memset(result.get() + decodedLength, value, count);
        decodedLength += count;
    }

    result[decodedLength] = '\0';
    return result.release();
}

string decodeScalarSinglePass(const string &encodedString) {
    string output;

    for (size_t input = 0; input < encodedString.size();) {
        size_t count = 0;

        for (; input < encodedString.size() && encodedString[input] >= '0' &&
               encodedString[input] <= '9';
             ++input) {
            count =
                count * 10 + static_cast<size_t>(encodedString[input] - '0');
        }

        if (input == encodedString.size()) {
            break;
        }

        output.append(count, encodedString[input]);
        ++input;
    }

    return output;
}

#ifdef __ARM_FEATURE_SVE
char *decodeSVETwoPass(const char *encodedString) {

    // scalar part
    if (encodedString == nullptr || *encodedString == '\0') {
        char *result = new char[1];
        result[0] = '\0';
        return result;
    }

    // Pass 1: determine the decoded length once and allocate to output
    size_t decodedLength = _calcDecodeLength(encodedString);
    char *output = new char[decodedLength + 1];
    char *result = output;

    const char *input = encodedString;

    while (*input != '\0') {
        size_t count = 0;

        for (; *input >= '0' && *input <= '9'; ++input) {
            count = count * 10 + static_cast<size_t>(*input - '0');
        }

        char value = *input++;
        output = _sve_fill(output, value, count);
    }
    *output = '\0';
    return result;
}

char *decodeSVESinglePass(const char *encodedString) {
    if (encodedString == nullptr || *encodedString == '\0') {
        char *result = new char[1];
        result[0] = '\0';
        return result;
    }

    constexpr size_t initialCapacity = 16;
    size_t capacity = initialCapacity;
    size_t decodedLength = 0;
    auto result = std::make_unique<char[]>(capacity);
    const char *input = encodedString;

    while (*input != '\0') {
        size_t count = 0;
        for (; *input >= '0' && *input <= '9'; ++input) {
            count = count * 10 + static_cast<size_t>(*input - '0');
        }

        if (*input == '\0') {
            break;
        }

        const char value = *input++;
        const size_t requiredCapacity = decodedLength + count + 1;
        if (requiredCapacity > capacity) {
            size_t newCapacity = capacity;
            while (newCapacity < requiredCapacity) {
                newCapacity *= 2;
            }

            auto expanded = std::make_unique<char[]>(newCapacity);
            std::memcpy(expanded.get(), result.get(), decodedLength);
            result = std::move(expanded);
            capacity = newCapacity;
        }

        _sve_fill(result.get() + decodedLength, value, count);
        decodedLength += count;
    }

    result[decodedLength] = '\0';
    return result.release();
}
#else
#warning SVE not available on this target: run_length_decoding will have no SVE implementation !
#endif // __ARM_FEATURE_SVE
} // namespace runLengthDecode
