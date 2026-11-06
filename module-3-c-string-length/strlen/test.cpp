// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "strlen.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#ifdef __aarch64__
#include <arm_sve.h>
#else
#warning Platform is not AArch64: no SVE-accelerated implementations will be available !
#endif

using std::array;
using std::size_t;
using std::string;
using std::vector;

namespace {

using StrlenFunction = size_t (*)(const char *);

struct StrlenImplementation {
    const char *name;
    StrlenFunction function;
};

vector<StrlenImplementation> implementations() {
    vector<StrlenImplementation> result{
        {"scalar", str::strlen},
    };

#ifdef __ARM_FEATURE_SVE
    result.push_back({"sve_naive", str::strlenSVENaive});
    result.push_back({"sve_improved", str::strlenSVEImproved});
    result.push_back({"sve_simd_loops", str::strlenSVESimdLoops});
#endif

    return result;
}

class strlen_implementations_test
    : public testing::TestWithParam<StrlenImplementation> {
  protected:
    void expectLength(const char *input, size_t expected) const {
        EXPECT_EQ(GetParam().function(input), expected);
    }
};
} // namespace

TEST_P(strlen_implementations_test, HandlesEmptyAndOrdinaryStrings) {
    expectLength("", 0);
    expectLength("hello", 5);
    expectLength("SVE task based course", 21);
}

TEST_P(strlen_implementations_test, HandlesUnalignedStartingAddresses) {
    array<char, 128> storage{};

    for (size_t offset = 0; offset < 16; ++offset) {
        constexpr size_t length = 37;
        std::fill_n(storage.data() + offset, length, 'x');
        storage[offset + length] = '\0';
        EXPECT_EQ(GetParam().function(storage.data() + offset), length)
            << "offset " << offset;
    }
}

TEST_P(strlen_implementations_test, HandlesAlignmentAndVectorBoundaries) {
#ifdef __ARM_FEATURE_SVE
    const size_t vectorLength = svcntb();
#else
    constexpr size_t vectorLength = 16;
#endif
    const array<size_t, 11> lengths = {
        0,
        1,
        15,
        16,
        17,
        vectorLength - 1,
        vectorLength,
        vectorLength + 1,
        (2 * vectorLength) - 1,
        2 * vectorLength,
        (2 * vectorLength) + 1,
    };

    for (const size_t length : lengths) {
        const string input(length, 'x');
        EXPECT_EQ(GetParam().function(input.c_str()), length)
            << "length " << length;
    }
}

INSTANTIATE_TEST_SUITE_P(
    strlen, strlen_implementations_test, testing::ValuesIn(implementations()),
    [](const testing::TestParamInfo<StrlenImplementation> &info) {
        return info.param.name;
    });
