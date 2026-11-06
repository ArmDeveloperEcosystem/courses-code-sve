// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on

#include "bloom.h"
#include "bloom_util.h"

#include <string>
#include <vector>

#include "gtest/gtest.h"

using namespace Bloom;

template <typename Filter> class bloom_filter_test : public ::testing::Test {};

using FilterTypes = ::testing::Types<
    Bloom::BloomFilterScalar64, Bloom::BloomFilterScalar32,
    Bloom::BloomFilterScalar64Partitioned, Bloom::BloomFilterScalar32Partitioned
#ifdef __ARM_FEATURE_SVE
    ,
    Bloom::BloomFilterSVE64, Bloom::BloomFilterSVE32,
    Bloom::BloomFilterSVE64Partitioned, Bloom::BloomFilterSVE32Partitioned
#endif
    >;

TYPED_TEST_SUITE(bloom_filter_test, FilterTypes);

TYPED_TEST(bloom_filter_test, InsertLookupAndClear) {
    TypeParam filter;

    const char *key1 = "key #1";
    EXPECT_FALSE(filter.maybe_contains(key1));
    filter.add(key1);
    EXPECT_TRUE(filter.maybe_contains(key1));

    const char *key2 = "key #2";
    // A Bloom filter may produce false positives, so checking that key2 is not
    // in the filter before adding it could fail (in theory).
    filter.add(key2);
    EXPECT_TRUE(filter.maybe_contains(key2));
    EXPECT_TRUE(filter.maybe_contains(key1));

    filter.clear();
    EXPECT_FALSE(filter.maybe_contains(key1));
    EXPECT_FALSE(filter.maybe_contains(key2));
}

TYPED_TEST(bloom_filter_test, ConstructFromKeys) {
    const std::vector<std::string> keys{"", "one", "two", "one"};
    TypeParam populated(keys);
    TypeParam reference;
    for (const auto &key : keys) {
        reference.add(key);
        EXPECT_TRUE(populated.maybe_contains(key));
    }
    EXPECT_TRUE(Filter::compare_words(populated, reference));

    TypeParam empty(std::vector<std::string>{});
    reference.clear();
    EXPECT_TRUE(Filter::compare_words(empty, reference));
}

#if defined(__ARM_FEATURE_SVE)
namespace {

const std::vector<std::string> keys =
    Bloom::build_keys(BLOOM_NUM_PRECALC_KEYS, BLOOM_MAX_KEY_LENGTH);

} // namespace

template <typename ScalarFilter, typename SVEFilter,
          Bloom::HashFn_t ScalarHash = nullptr,
          Bloom::HashFn_t SVEHash = nullptr>
struct ImplementationPair {
    using Scalar = ScalarFilter;
    using SVE = SVEFilter;

    static constexpr auto scalar_hash = ScalarHash;
    static constexpr auto sve_hash = SVEHash;

    static constexpr bool has_hash_functions =
        ScalarHash != nullptr && SVEHash != nullptr;
};

using ComparisonTypes = ::testing::Types<
    ImplementationPair<Bloom::BloomFilterScalar32, Bloom::BloomFilterSVE32,
                       Bloom::hashScalar32, Bloom::hashSVE32>,
    ImplementationPair<Bloom::BloomFilterScalar64, Bloom::BloomFilterSVE64,
                       Bloom::hashScalar64, Bloom::hashSVE64>,
    ImplementationPair<Bloom::BloomFilterScalar32Partitioned,
                       Bloom::BloomFilterSVE32Partitioned>,
    ImplementationPair<Bloom::BloomFilterScalar64Partitioned,
                       Bloom::BloomFilterSVE64Partitioned>>;

template <typename Pair>
class bloom_filter_comparison_test : public ::testing::Test {};

TYPED_TEST_SUITE(bloom_filter_comparison_test, ComparisonTypes);

TYPED_TEST(bloom_filter_comparison_test, HashesMatch) {
    if constexpr (TypeParam::has_hash_functions) {
        const auto count = std::min<std::size_t>(2048, keys.size());

        for (std::size_t i = 0; i < count; ++i) {
            SCOPED_TRACE(i);
            EXPECT_EQ(TypeParam::scalar_hash(keys[i]),
                      TypeParam::sve_hash(keys[i]));
        }
    } else {
        GTEST_SKIP() << "This pair has no standalone hash comparison";
    }
}

TYPED_TEST(bloom_filter_comparison_test, WordsMatch) {
    const auto count = std::min<std::size_t>(2048, keys.size());

    typename TypeParam::Scalar scalar;
    typename TypeParam::SVE sve;

    for (std::size_t i = 0; i < count; ++i) {
        SCOPED_TRACE(i);

        scalar.add(keys[i]);
        sve.add(keys[i]);

        EXPECT_TRUE(Bloom::Filter::compare_words(scalar, sve));
    }
}
#endif
