// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "sve_match.h"

#include <cstddef>
#include <cstdint>
#include <numeric>
#include <span>
#include <vector>

#include "gtest/gtest.h"

using std::size_t;
using std::uint16_t;
using std::uint8_t;

using std::vector;

namespace {

template <typename T> class sve_match_test : public testing::Test {};

using MatchTypes = testing::Types<uint8_t, uint16_t>;
TYPED_TEST_SUITE(sve_match_test, MatchTypes);

template <sve_match::MatchElement T>
void expect_all_implementations(std::span<const T> haystack,
                                std::span<const T> keys, bool expected) {
    EXPECT_EQ(sve_match::search_scalar<T>(haystack, keys), expected);
#ifdef __ARM_FEATURE_SVE2
    EXPECT_EQ(sve_match::search_sve2<T>(haystack, keys), expected);
    EXPECT_EQ(sve_match::search_sve2_unrolled<T>(haystack, keys), expected);
#endif
}
} // namespace

TYPED_TEST(sve_match_test, EmptyInputOrKeysDoesNotMatch) {
    using T = TypeParam;
    const vector<T> empty;
    const vector<T> haystack = {T{1}, T{2}, T{3}};
    const vector<T> keys = {T{2}};

    expect_all_implementations<T>(empty, keys, false);
    expect_all_implementations<T>(haystack, empty, false);
    expect_all_implementations<T>(empty, empty, false);
}

TYPED_TEST(sve_match_test, ReportsMatchAndNoMatch) {
    using T = TypeParam;
    const vector<T> haystack = {T{3}, T{8}, T{13}, T{21}, T{34}};
    const vector<T> matching_keys = {T{1}, T{21}, T{55}};
    const vector<T> absent_keys = {T{2}, T{5}, T{89}};

    expect_all_implementations<T>(haystack, matching_keys, true);
    expect_all_implementations<T>(haystack, absent_keys, false);
}

TYPED_TEST(sve_match_test, FindsMatchInVectorBody) {
    using T = TypeParam;
    vector<T> haystack(257, T{0});
    constexpr size_t second_segment_index = 16 / sizeof(T) + 1;
    haystack[second_segment_index] = T{42};
    const vector<T> keys = {T{1}, T{42}, T{99}};

    expect_all_implementations<T>(haystack, keys, true);
}

TYPED_TEST(sve_match_test, FindsMatchInScalarTail) {
    using T = TypeParam;
    // The architectural maximum is 256 bytes. An odd 257-element input leaves
    // a scalar tail for every valid SVE vector length and both element widths.
    vector<T> haystack(257, T{0});
    haystack.back() = T{42};
    const vector<T> keys = {T{42}};

    expect_all_implementations<T>(haystack, keys, true);
}

TYPED_TEST(sve_match_test, OversizedKeySetUsesScalarFallback) {
    using T = TypeParam;
    constexpr size_t keys_per_segment = 16 / sizeof(T);
    vector<T> keys(keys_per_segment + 1);
    std::iota(keys.begin(), keys.end(), T{1});
    const vector<T> haystack = {T{0}, keys.back()};

    expect_all_implementations<T>(haystack, keys, true);
}
