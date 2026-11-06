// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "histogram.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <stdexcept>
#include <vector>

#include "gtest/gtest.h"

using std::size_t;
using std::uint16_t;
using std::uint32_t;
using std::uint8_t;

using std::vector;

using HistogramTypes = ::testing::Types<uint8_t, uint16_t, uint32_t>;

template <ValidSVEType T> class histogramTest : public ::testing::Test {
  protected:
    // Flat Histogram
    static constexpr size_t HISTOGRAM_SIZE = (1u << 8); // 256 elements
    static constexpr uint8_t BIN_SIZE = 16;
    static constexpr size_t VARIABLE_HISTOGRAM_SIZE = HISTOGRAM_SIZE / BIN_SIZE;
    vector<T> records = vector<T>(HISTOGRAM_SIZE);
    vector<uint32_t> histogram = vector<uint32_t>(HISTOGRAM_SIZE);
    vector<uint32_t> answer = vector<uint32_t>(
        HISTOGRAM_SIZE); // each bin should have a cumulative sum of 1.
    vector<uint32_t> variable_histogram =
        vector<uint32_t>(VARIABLE_HISTOGRAM_SIZE);
    vector<uint32_t> variable_answer =
        vector<uint32_t>(VARIABLE_HISTOGRAM_SIZE);

    // Small histogram with dataset less than SVE vector length
    static constexpr size_t SMALL_HISTOGRAM_SIZE =
        3; // minimum SVL length is 128 bits (i.e. 4 ints)
    vector<T> small_records = vector<T>(SMALL_HISTOGRAM_SIZE);
    vector<uint32_t> small_histogram = vector<uint32_t>(SMALL_HISTOGRAM_SIZE);
    vector<uint32_t> small_answer = vector<uint32_t>(SMALL_HISTOGRAM_SIZE);
    vector<uint32_t> small_variable_histogram = vector<uint32_t>(1);
    vector<uint32_t> small_variable_answer = vector<uint32_t>(1);

    void SetUp() override {
        std::iota(records.begin(), records.end(), 0);
        std::ranges::fill(histogram, 0);
        std::ranges::fill(answer, 1);
        std::ranges::fill(variable_histogram, 0);
        std::ranges::fill(variable_answer, BIN_SIZE);

        std::iota(small_records.begin(), small_records.end(), 0);
        std::ranges::fill(small_histogram, 0);
        std::ranges::fill(small_answer, 1);
        std::ranges::fill(small_variable_histogram, 0);
        std::ranges::fill(small_variable_answer, SMALL_HISTOGRAM_SIZE);
    }

    void TearDown() override {}

    void testCountingDuplicates() {
        SetUp();
        records[100] = 42;
        answer[42] += 1;
        answer[100] -= 1;
    }

    void testCountingVariableBinDuplicates() {
        SetUp();
        records[100] = 42;
        variable_answer[42 / BIN_SIZE] += 1;
        variable_answer[100 / BIN_SIZE] -= 1;
    }
};

TYPED_TEST_SUITE(histogramTest, HistogramTypes);

TYPED_TEST(histogramTest, scalar_typed_test) {
    histogram::calc_histogram<TypeParam>(
        this->histogram.data(), this->records.data(), this->records.size());
    histogram::calc_histogram<TypeParam>(this->small_histogram.data(),
                                         this->small_records.data(),
                                         this->small_records.size());
    EXPECT_EQ(this->answer, this->histogram);
    EXPECT_EQ(this->small_answer, this->small_histogram);

    this->testCountingDuplicates();
    histogram::calc_histogram<TypeParam>(
        this->histogram.data(), this->records.data(), this->records.size());
    EXPECT_EQ(this->answer, this->histogram);
};

TYPED_TEST(histogramTest, scalar_variable_bin_typed_test) {
    histogram::calc_histogram<TypeParam>(this->variable_histogram.data(),
                                         this->records.data(),
                                         this->records.size(), this->BIN_SIZE);
    histogram::calc_histogram<TypeParam>(
        this->small_variable_histogram.data(), this->small_records.data(),
        this->small_records.size(), this->BIN_SIZE);
    EXPECT_EQ(this->variable_answer, this->variable_histogram);
    EXPECT_EQ(this->small_variable_answer, this->small_variable_histogram);

    this->testCountingVariableBinDuplicates();
    histogram::calc_histogram<TypeParam>(this->variable_histogram.data(),
                                         this->records.data(),
                                         this->records.size(), this->BIN_SIZE);
    EXPECT_EQ(this->variable_answer, this->variable_histogram);

    EXPECT_THROW(histogram::calc_histogram<TypeParam>(
                     this->variable_histogram.data(), this->records.data(),
                     this->records.size(), 0),
                 std::invalid_argument);
};

#ifdef __ARM_FEATURE_SVE
TYPED_TEST(histogramTest, sve_typed_test) {
    histogram::calc_histogram_sve<TypeParam>(
        this->histogram.data(), this->records.data(), this->records.size());
    histogram::calc_histogram_sve<TypeParam>(this->small_histogram.data(),
                                             this->small_records.data(),
                                             this->small_records.size());
    EXPECT_EQ(this->answer, this->histogram);
    EXPECT_EQ(this->small_answer, this->small_histogram);

    this->testCountingDuplicates();
    histogram::calc_histogram_sve<TypeParam>(
        this->histogram.data(), this->records.data(), this->records.size());
    EXPECT_EQ(this->answer, this->histogram);
};

TYPED_TEST(histogramTest, sve_same_vector_duplicates_typed_test) {
    vector<TypeParam> records(64, static_cast<TypeParam>(42));
    vector<uint32_t> histogram(this->HISTOGRAM_SIZE, 0);
    vector<uint32_t> answer(this->HISTOGRAM_SIZE, 0);
    answer[42] = records.size();

    histogram::calc_histogram_sve<TypeParam>(histogram.data(), records.data(),
                                             records.size());
    EXPECT_EQ(answer, histogram);
};

TYPED_TEST(histogramTest, sve_repeated_accumulation_typed_test) {
    vector<TypeParam> records = {
        static_cast<TypeParam>(7), static_cast<TypeParam>(7),
        static_cast<TypeParam>(8), static_cast<TypeParam>(9),
        static_cast<TypeParam>(9), static_cast<TypeParam>(9),
    };
    vector<uint32_t> histogram(this->HISTOGRAM_SIZE, 1);
    vector<uint32_t> answer(this->HISTOGRAM_SIZE, 1);
    answer[7] += 4;
    answer[8] += 2;
    answer[9] += 6;

    histogram::calc_histogram_sve<TypeParam>(histogram.data(), records.data(),
                                             records.size());
    histogram::calc_histogram_sve<TypeParam>(histogram.data(), records.data(),
                                             records.size());
    EXPECT_EQ(answer, histogram);
};

TYPED_TEST(histogramTest, sve_scalar_update_typed_test) {
    histogram::calc_histogram_sve_scalar_update<TypeParam>(
        this->histogram.data(), this->records.data(), this->records.size());
    histogram::calc_histogram_sve_scalar_update<TypeParam>(
        this->small_histogram.data(), this->small_records.data(),
        this->small_records.size());
    EXPECT_EQ(this->answer, this->histogram);
    EXPECT_EQ(this->small_answer, this->small_histogram);

    vector<TypeParam> records(64, static_cast<TypeParam>(42));
    vector<uint32_t> histogram(this->HISTOGRAM_SIZE, 1);
    vector<uint32_t> answer(this->HISTOGRAM_SIZE, 1);
    answer[42] += records.size();

    histogram::calc_histogram_sve_scalar_update<TypeParam>(
        histogram.data(), records.data(), records.size());
    EXPECT_EQ(answer, histogram);
};
#endif

#ifdef __ARM_FEATURE_SVE2
TYPED_TEST(histogramTest, sve2_typed_test) {
    histogram::calc_histogram_sve2<TypeParam>(
        this->histogram.data(), this->records.data(), this->records.size());
    histogram::calc_histogram_sve2<TypeParam>(this->small_histogram.data(),
                                              this->small_records.data(),
                                              this->small_records.size());
    EXPECT_EQ(this->answer, this->histogram);
    EXPECT_EQ(this->small_answer, this->small_histogram);

    this->testCountingDuplicates();
    histogram::calc_histogram_sve2<TypeParam>(
        this->histogram.data(), this->records.data(), this->records.size());
    EXPECT_EQ(this->answer, this->histogram);
};

TYPED_TEST(histogramTest, sve2_variable_bin_typed_test) {
    histogram::calc_histogram_sve2<TypeParam>(
        this->variable_histogram.data(), this->records.data(),
        this->records.size(), this->BIN_SIZE);
    histogram::calc_histogram_sve2<TypeParam>(
        this->small_variable_histogram.data(), this->small_records.data(),
        this->small_records.size(), this->BIN_SIZE);
    EXPECT_EQ(this->variable_answer, this->variable_histogram);
    EXPECT_EQ(this->small_variable_answer, this->small_variable_histogram);

    this->testCountingVariableBinDuplicates();
    histogram::calc_histogram_sve2<TypeParam>(
        this->variable_histogram.data(), this->records.data(),
        this->records.size(), this->BIN_SIZE);
    EXPECT_EQ(this->variable_answer, this->variable_histogram);

    EXPECT_THROW(histogram::calc_histogram_sve2<TypeParam>(
                     this->variable_histogram.data(), this->records.data(),
                     this->records.size(), 0),
                 std::invalid_argument);
};

TYPED_TEST(histogramTest, sve2_unrolled_typed_test) {
    histogram::calc_histogram_sve2_unrolled<TypeParam>(
        this->histogram.data(), this->records.data(), this->records.size());
    histogram::calc_histogram_sve2_unrolled<TypeParam>(
        this->small_histogram.data(), this->small_records.data(),
        this->small_records.size());
    EXPECT_EQ(this->answer, this->histogram);
    EXPECT_EQ(this->small_answer, this->small_histogram);

    this->testCountingDuplicates();
    histogram::calc_histogram_sve2_unrolled<TypeParam>(
        this->histogram.data(), this->records.data(), this->records.size());
    EXPECT_EQ(this->answer, this->histogram);
};

TYPED_TEST(histogramTest, sve2_unrolled_variable_bin_typed_test) {
    const auto num_records = static_cast<uint32_t>(this->records.size());
    const auto small_num_records =
        static_cast<uint32_t>(this->small_records.size());
    const uint8_t bin_size = this->BIN_SIZE;

    histogram::calc_histogram_sve2_unrolled<TypeParam>(
        this->variable_histogram.data(), this->records.data(), num_records,
        bin_size);
    histogram::calc_histogram_sve2_unrolled<TypeParam>(
        this->small_variable_histogram.data(), this->small_records.data(),
        small_num_records, bin_size);
    EXPECT_EQ(this->variable_answer, this->variable_histogram);
    EXPECT_EQ(this->small_variable_answer, this->small_variable_histogram);

    this->testCountingVariableBinDuplicates();
    histogram::calc_histogram_sve2_unrolled<TypeParam>(
        this->variable_histogram.data(), this->records.data(), num_records,
        bin_size);
    EXPECT_EQ(this->variable_answer, this->variable_histogram);

    EXPECT_THROW(histogram::calc_histogram_sve2_unrolled<TypeParam>(
                     this->variable_histogram.data(), this->records.data(),
                     num_records, uint8_t{0}),
                 std::invalid_argument);
};
#endif