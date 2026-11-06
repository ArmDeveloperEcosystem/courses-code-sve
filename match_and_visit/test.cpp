// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "match_and_visit.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <vector>

#include "gtest/gtest.h"

using std::size_t;
using std::uint32_t;

using std::vector;

namespace {

struct ScanResult {
    int status;
    vector<uint32_t> values;

    bool operator==(const ScanResult &) const = default;
};

template <typename ScanFunction>
ScanResult collectMatches(ScanFunction scan, const vector<uint32_t> &input,
                          uint32_t threshold) {
    ScanResult result{EXIT_SUCCESS, {}};
    const matchAndVisit::MatchCallback callback = [&result](uint32_t value) {
        result.values.push_back(value);
    };
    result.status = scan(input, threshold, callback);
    return result;
}

ScanResult collectScalarMatches(const vector<uint32_t> &input,
                                uint32_t threshold) {
    return collectMatches(matchAndVisit::matchAndVisitScalar, input, threshold);
}
} // namespace

TEST(match_and_visit, scalarInvokesCallbackForMatchesInInputOrder) {
    const vector<uint32_t> input{9, 3, 8, 2, 7, 1, 6, 0, 9, 2};
    const auto result = collectScalarMatches(input, 9);

    EXPECT_EQ(result.status, EXIT_SUCCESS);
    EXPECT_EQ(result.values, (vector<uint32_t>{3, 8, 2, 7, 1, 6, 0, 2}));
}

#ifdef __ARM_FEATURE_SVE
namespace {
ScanResult collectSveOptimizedMatches(const vector<uint32_t> &input,
                                      uint32_t threshold) {
    return collectMatches(matchAndVisit::matchAndVisitSVEOptimized, input,
                          threshold);
}

ScanResult collectSveBasicMatches(const vector<uint32_t> &input,
                                  uint32_t threshold) {
    return collectMatches(matchAndVisit::matchAndVisitSVEBasic, input,
                          threshold);
}

ScanResult collectSveCompactMatches(const vector<uint32_t> &input,
                                    uint32_t threshold) {
    return collectMatches(matchAndVisit::matchAndVisitSVECompact, input,
                          threshold);
}

void expectAllSveImplementationsMatchScalar(const vector<uint32_t> &input,
                                            uint32_t threshold) {
    const auto scalarResult = collectScalarMatches(input, threshold);

    EXPECT_EQ(scalarResult.status, EXIT_SUCCESS);
    EXPECT_EQ(collectSveOptimizedMatches(input, threshold), scalarResult);
    EXPECT_EQ(collectSveBasicMatches(input, threshold), scalarResult);
    EXPECT_EQ(collectSveCompactMatches(input, threshold), scalarResult);
}
} // namespace

TEST(match_and_visit, sveMatchesScalarForEmptyNoMatchAndAllMatchInputs) {
    expectAllSveImplementationsMatchScalar({}, 10);
    expectAllSveImplementationsMatchScalar({0, 1, 2, 3}, 0);
    expectAllSveImplementationsMatchScalar({10, 11, 12, 13}, 10);
    expectAllSveImplementationsMatchScalar({4, 1, 4, 2}, 5);
}

TEST(match_and_visit, svePreservesOrderAndDuplicateMatches) {
    const vector<uint32_t> input{9, 3, 8, 3, 7, 1, 6, 1, 9, 2};
    const vector<uint32_t> expected{3, 8, 3, 7, 1, 6, 1, 2};

    const auto scalarResult = collectScalarMatches(input, 9);
    ASSERT_EQ(scalarResult.values, expected);
    ASSERT_EQ(scalarResult.values.size(), 8);

    expectAllSveImplementationsMatchScalar(input, 9);
}

TEST(match_and_visit, sveMatchesScalarAcrossMultipleVectorsAndTail) {
    vector<uint32_t> input(257);
    for (size_t i = 0; i < input.size(); ++i) {
        input[i] = static_cast<uint32_t>((i * 37 + 11) % 101);
    }

    expectAllSveImplementationsMatchScalar(input, 43);
}

TEST(match_and_visit, sveUsesStrictUnsignedComparison) {
    const vector<uint32_t> input = {0, 1,
                                    std::numeric_limits<uint32_t>::max() - 1,
                                    std::numeric_limits<uint32_t>::max()};

    expectAllSveImplementationsMatchScalar(
        input, std::numeric_limits<uint32_t>::max());
}
#endif
