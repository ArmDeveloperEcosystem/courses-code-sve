// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "vector_examples.h"

#include <cstdint>
#include <vector>

#include "gtest/gtest.h"

using std::int16_t;
using std::int32_t;
using std::int8_t;
using std::uint16_t;
using std::uint32_t;
using std::uint8_t;

using std::vector;

// Integer types supported by the scalar and SVE implementations.
using IntegerTypes =
    ::testing::Types<int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t>;

// ------------------------------------------------------------
// Vector Add Scalar
// ------------------------------------------------------------

template <typename T>
class vector_examples_VectorAddScalarTest : public ::testing::Test {};

TYPED_TEST_SUITE(vector_examples_VectorAddScalarTest, IntegerTypes);

TYPED_TEST(vector_examples_VectorAddScalarTest, AddsVectors) {
    using T = TypeParam;

    vector<T> lhs{1, 2, 3, 4};
    vector<T> rhs{5, 6, 7, 8};
    vector<T> result(lhs.size());

    vector_add_scalar<T>(lhs, rhs, result);

    vector<T> expected{6, 8, 10, 12};

    EXPECT_EQ(result, expected);
}

// ------------------------------------------------------------
// Vector Add SVE
// ------------------------------------------------------------

#ifdef __ARM_FEATURE_SVE
template <typename T>
class vector_examples_VectorAddSVETest : public ::testing::Test {};

TYPED_TEST_SUITE(vector_examples_VectorAddSVETest, IntegerTypes);

TYPED_TEST(vector_examples_VectorAddSVETest, MatchesScalar) {
    using T = TypeParam;

    vector<T> lhs{1, 2, 3, 4};
    vector<T> rhs{5, 6, 7, 8};

    vector<T> scalar_result(lhs.size());
    vector<T> sve_result(lhs.size());

    vector_add_scalar<T>(lhs, rhs, scalar_result);
    vector_add_sve<T>(lhs, rhs, sve_result);

    EXPECT_EQ(scalar_result, sve_result);
}
#endif

// ------------------------------------------------------------
// Min/Max Scalar
// ------------------------------------------------------------

template <typename T>
class vector_examples_MinMaxScalarTest : public ::testing::Test {};

TYPED_TEST_SUITE(vector_examples_MinMaxScalarTest, IntegerTypes);

TYPED_TEST(vector_examples_MinMaxScalarTest, FindsMinimumAndMaximum) {
    using T = TypeParam;

    vector<T> values{7, 2, 9, 4, 5};

    auto [minimum, maximum] = min_max_scalar<T>(values);

    EXPECT_EQ(minimum, static_cast<T>(2));
    EXPECT_EQ(maximum, static_cast<T>(9));
}

// ------------------------------------------------------------
// Min/Max SVE
// ------------------------------------------------------------

#ifdef __ARM_FEATURE_SVE
template <typename T>
class vector_examples_MinMaxSVETest : public ::testing::Test {};

TYPED_TEST_SUITE(vector_examples_MinMaxSVETest, IntegerTypes);

TYPED_TEST(vector_examples_MinMaxSVETest, MatchesScalar) {
    using T = TypeParam;

    vector<T> values{7, 2, 9, 4, 5};

    auto [scalar_min, scalar_max] = min_max_scalar<T>(values);

    auto [sve_min, sve_max] = min_max_sve<T>(values);

    EXPECT_EQ(scalar_min, sve_min);
    EXPECT_EQ(scalar_max, sve_max);
}
#endif
