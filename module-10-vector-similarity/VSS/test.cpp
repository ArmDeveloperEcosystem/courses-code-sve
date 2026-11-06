// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "VSS.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "gtest/gtest.h"

using std::int8_t;
using std::size_t;

using std::array;
using std::string;
using std::vector;

using VSS::Embedding;
using VSS::make_embedding;
using VSS::Metric;
using VSS::similarity_ref;

#ifdef __ARM_FEATURE_SVE
using VSS::similarity_sve;
#endif // __ARM_FEATURE_SVE

namespace {

enum class MetricBackend {
    Reference,
#ifdef __ARM_FEATURE_SVE
    SVE,
#endif // __ARM_FEATURE_SVE
};

template <size_t N>
Embedding<N, float> make_fp32_reference_embedding(float offset) {
    array<float, N> values{};
    for (size_t i = 0; i < N; ++i) {
        values[i] = offset + static_cast<float>(i % 17) * 0.25f;
    }
    return make_embedding<N, float>(values);
}

template <size_t N, typename Ty>
Embedding<N, Ty> make_typed_test_embedding(int offset) {
    array<Ty, N> values{};
    for (size_t i = 0; i < N; ++i) {
        if constexpr (std::is_same_v<Ty, int8_t>) {
            values[i] = static_cast<Ty>(offset + static_cast<int>(i % 17) - 8);
        } else {
            values[i] = static_cast<Ty>(static_cast<float>(offset) * 0.125f +
                                        static_cast<float>(i % 17) * 0.03125f);
        }
    }
    return make_embedding<N, Ty>(values);
}

struct MetricCase {
    const char *name;
    Metric::Kind metric;
    size_t dimensions;
    float a_offset;
    float b_offset;
};

struct ReferenceMetricCase {
    MetricCase metric_case;
    float expected;
};

const char *metric_name(Metric::Kind metric) {
    switch (metric) {
    case Metric::EUCLIDEAN_DISTANCE:
        return "euclidean_distance";
    case Metric::DOT_PRODUCT:
        return "dot_product";
    case Metric::COSINE_SIMILARITY:
        return "cosine_similarity";
    }
    throw std::invalid_argument("Unknown metric kind");
}

template <size_t N> constexpr const char *fp32_case_name() {
    if constexpr (N == 8)
        return "8_fp32";
    else if constexpr (N == 384)
        return "384_fp32";
    else if constexpr (N == 768)
        return "768_fp32";
    else if constexpr (N == 1024)
        return "1024_fp32";
    else if constexpr (N == 3072)
        return "3072_fp32";
    else
        static_assert(N == 8 || N == 384 || N == 768 || N == 1024 || N == 3072);
}

string test_name(const MetricCase &test_case) {
    return string(metric_name(test_case.metric)) + "_" + test_case.name;
}

float near_tolerance(float expected, float relative_tolerance) {
    return std::fabs(expected) * relative_tolerance + 1.0e-5f;
}

template <size_t N>
float expected_fp32_distance(float a_offset, float b_offset) noexcept {
    const auto diff = a_offset - b_offset;
    return static_cast<float>(N) * diff * diff;
}

template <size_t N>
float expected_fp32_dot_product(float a_offset, float b_offset) noexcept {
    float sum = 0;
    for (size_t i = 0; i < N; ++i) {
        const auto value = static_cast<float>(i % 17) * 0.25f;
        sum += (a_offset + value) * (b_offset + value);
    }
    return sum;
}

template <size_t N>
float expected_fp32_cosine_similarity(float a_offset, float b_offset) noexcept {
    const auto dot = expected_fp32_dot_product<N>(a_offset, b_offset);
    const auto a_norm = expected_fp32_dot_product<N>(a_offset, a_offset);
    const auto b_norm = expected_fp32_dot_product<N>(b_offset, b_offset);
    return dot / std::sqrt(a_norm * b_norm);
}

template <size_t N>
float expected_fp32_metric(Metric::Kind metric, float a_offset,
                           float b_offset) {
    switch (metric) {
    case Metric::EUCLIDEAN_DISTANCE:
        return expected_fp32_distance<N>(a_offset, b_offset);
    case Metric::DOT_PRODUCT:
        return expected_fp32_dot_product<N>(a_offset, b_offset);
    case Metric::COSINE_SIMILARITY:
        return expected_fp32_cosine_similarity<N>(a_offset, b_offset);
    }
    throw std::invalid_argument("Unknown metric kind");
}

template <size_t N>
MetricCase make_metric_case(Metric::Kind metric, float a_offset,
                            float b_offset) {
    return MetricCase{
        .name = fp32_case_name<N>(),
        .metric = metric,
        .dimensions = N,
        .a_offset = a_offset,
        .b_offset = b_offset,
    };
}

template <size_t N>
ReferenceMetricCase make_reference_metric_case(Metric::Kind metric,
                                               float a_offset, float b_offset) {
    return ReferenceMetricCase{
        .metric_case = make_metric_case<N>(metric, a_offset, b_offset),
        .expected = expected_fp32_metric<N>(metric, a_offset, b_offset),
    };
}

template <MetricBackend Backend, size_t N>
float compute_metric(const MetricCase &test_case) {
    const auto a = make_fp32_reference_embedding<N>(test_case.a_offset);
    const auto b = make_fp32_reference_embedding<N>(test_case.b_offset);
    switch (test_case.metric) {
    case Metric::EUCLIDEAN_DISTANCE:
        if constexpr (Backend == MetricBackend::Reference)
            return similarity_ref<N, float, Metric::EUCLIDEAN_DISTANCE>(a, b);
#ifdef __ARM_FEATURE_SVE
        else
            return similarity_sve<N, float, Metric::EUCLIDEAN_DISTANCE>(a, b);
#endif // __ARM_FEATURE_SVE
    case Metric::DOT_PRODUCT:
        if constexpr (Backend == MetricBackend::Reference)
            return similarity_ref<N, float, Metric::DOT_PRODUCT>(a, b);
#ifdef __ARM_FEATURE_SVE
        else
            return similarity_sve<N, float, Metric::DOT_PRODUCT>(a, b);
#endif // __ARM_FEATURE_SVE
    case Metric::COSINE_SIMILARITY:
        if constexpr (Backend == MetricBackend::Reference)
            return similarity_ref<N, float, Metric::COSINE_SIMILARITY>(a, b);
#ifdef __ARM_FEATURE_SVE
        else
            return similarity_sve<N, float, Metric::COSINE_SIMILARITY>(a, b);
#endif // __ARM_FEATURE_SVE
    }
    throw std::invalid_argument("Unknown metric kind");
}

template <MetricBackend Backend>
float compute_metric(const MetricCase &test_case) {
    switch (test_case.dimensions) {
    case 8:
        if constexpr (Backend == MetricBackend::Reference)
            return compute_metric<Backend, 8>(test_case);
        else
            throw std::invalid_argument("Unsupported SVE test dimension");
    case 384:
        return compute_metric<Backend, 384>(test_case);
    case 768:
        return compute_metric<Backend, 768>(test_case);
    case 1024:
        return compute_metric<Backend, 1024>(test_case);
    case 3072:
        return compute_metric<Backend, 3072>(test_case);
    default:
        throw std::invalid_argument("Unsupported dimension in compute_metric");
    }
}

const auto reference_cases = testing::Values(
    make_reference_metric_case<8>(Metric::EUCLIDEAN_DISTANCE, 0.0f, 7.0f),
    make_reference_metric_case<384>(Metric::EUCLIDEAN_DISTANCE, 0.25f, -1.5f),
    make_reference_metric_case<768>(Metric::EUCLIDEAN_DISTANCE, 0.5f, -0.125f),
    make_reference_metric_case<1024>(Metric::EUCLIDEAN_DISTANCE, -0.75f,
                                     0.375f),
    make_reference_metric_case<3072>(Metric::EUCLIDEAN_DISTANCE, 0.125f,
                                     -0.625f),
    make_reference_metric_case<8>(Metric::DOT_PRODUCT, -0.25f, 0.5f),
    make_reference_metric_case<384>(Metric::DOT_PRODUCT, 0.125f, -0.25f),
    make_reference_metric_case<768>(Metric::DOT_PRODUCT, -0.5f, 0.25f),
    make_reference_metric_case<1024>(Metric::DOT_PRODUCT, 0.75f, -0.125f),
    make_reference_metric_case<3072>(Metric::DOT_PRODUCT, -0.625f, 0.375f),
    make_reference_metric_case<8>(Metric::COSINE_SIMILARITY, -0.25f, 0.5f),
    make_reference_metric_case<384>(Metric::COSINE_SIMILARITY, 0.125f, -0.25f),
    make_reference_metric_case<768>(Metric::COSINE_SIMILARITY, -0.5f, 0.25f),
    make_reference_metric_case<1024>(Metric::COSINE_SIMILARITY, 0.375f,
                                     -0.625f),
    make_reference_metric_case<3072>(Metric::COSINE_SIMILARITY, -0.125f,
                                     0.75f));

string
reference_test_name(const testing::TestParamInfo<ReferenceMetricCase> &info) {
    return test_name(info.param.metric_case);
}

#ifdef __ARM_FEATURE_SVE
string metric_case_test_name(const testing::TestParamInfo<MetricCase> &info) {
    return test_name(info.param);
}
#endif // __ARM_FEATURE_SVE

class metric_ref : public testing::TestWithParam<ReferenceMetricCase> {};

TEST_P(metric_ref, matches_expected_value) {
    const auto &test_case = GetParam();
    EXPECT_NEAR(compute_metric<MetricBackend::Reference>(test_case.metric_case),
                test_case.expected,
                near_tolerance(test_case.expected, 1.0e-6f));
}

INSTANTIATE_TEST_SUITE_P(VSS, metric_ref, reference_cases, reference_test_name);

TEST(VSS, constructs_from_contiguous_sequences) {
    const float expected_values[] = {1.0f, 2.0f, 3.0f, 4.0f,
                                     5.0f, 6.0f, 7.0f, 8.0f};
    const auto expected = make_embedding<8, float>(expected_values);

    const auto from_literals = make_embedding<8, float>(
        {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f});

    EXPECT_FLOAT_EQ((similarity_ref<8, float, Metric::EUCLIDEAN_DISTANCE>(
                        from_literals, expected)),
                    0.0f);

    const array<float, 8> array_values = {1.0f, 2.0f, 3.0f, 4.0f,
                                          5.0f, 6.0f, 7.0f, 8.0f};
    const auto from_array = make_embedding<8, float>(array_values);

    EXPECT_FLOAT_EQ((similarity_ref<8, float, Metric::EUCLIDEAN_DISTANCE>(
                        from_array, expected)),
                    0.0f);

    const vector<float> vector_values = {1.0f, 2.0f, 3.0f, 4.0f,
                                         5.0f, 6.0f, 7.0f, 8.0f};
    const auto from_vector = make_embedding<8, float>(vector_values);

    EXPECT_FLOAT_EQ((similarity_ref<8, float, Metric::EUCLIDEAN_DISTANCE>(
                        from_vector, expected)),
                    0.0f);
}

TEST(VSS, rejects_wrong_sized_sequences) {
    const vector<float> values = {1.0f, 2.0f, 3.0f};

    EXPECT_THROW((make_embedding<8, float>(values)), std::invalid_argument);
}

TEST(VSS, exposes_elements) {
    auto embedding = make_embedding<8, float>(
        {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f});

    EXPECT_FLOAT_EQ(embedding[0], 1.0f);
    EXPECT_FLOAT_EQ(embedding.data()[7], 8.0f);

    embedding[0] = 9.0f;
    embedding.data()[1] = 10.0f;

    EXPECT_FLOAT_EQ(embedding[0], 9.0f);
    EXPECT_FLOAT_EQ(embedding[1], 10.0f);
    EXPECT_EQ(embedding.end() - embedding.begin(), 8);
}

TEST(VSS, supports_int8_metrics_ref) {
    static_assert(std::is_same_v<Metric::ResultTy, float>);

    const auto a = make_embedding<8, int8_t>({1, 2, 3, 4, 5, 6, 7, 8});
    const auto b = make_embedding<8, int8_t>({8, 7, 6, 5, 4, 3, 2, 1});

    EXPECT_FLOAT_EQ(
        (similarity_ref<8, int8_t, Metric::EUCLIDEAN_DISTANCE>(a, b)), 168.0f);
    EXPECT_FLOAT_EQ((similarity_ref<8, int8_t, Metric::DOT_PRODUCT>(a, b)),
                    120.0f);
    EXPECT_NEAR((similarity_ref<8, int8_t, Metric::COSINE_SIMILARITY>(a, b)),
                120.0f / std::sqrt(204.0f * 204.0f), 1.0e-6f);
}

TEST(VSS, supports_fp16_metrics_ref) {
    static_assert(std::is_same_v<Metric::ResultTy, float>);

    const auto a = make_embedding<8, _Float16>(
        {_Float16(1), _Float16(2), _Float16(3), _Float16(4), _Float16(5),
         _Float16(6), _Float16(7), _Float16(8)});
    const auto b = make_embedding<8, _Float16>(
        {_Float16(8), _Float16(7), _Float16(6), _Float16(5), _Float16(4),
         _Float16(3), _Float16(2), _Float16(1)});

    EXPECT_FLOAT_EQ(
        (similarity_ref<8, _Float16, Metric::EUCLIDEAN_DISTANCE>(a, b)),
        168.0f);
    EXPECT_FLOAT_EQ((similarity_ref<8, _Float16, Metric::DOT_PRODUCT>(a, b)),
                    120.0f);
    EXPECT_NEAR((similarity_ref<8, _Float16, Metric::COSINE_SIMILARITY>(a, b)),
                120.0f / std::sqrt(204.0f * 204.0f), 1.0e-6f);
}

#ifdef __ARM_FEATURE_SVE
template <typename Ty> constexpr float sve_relative_tolerance();
template <> constexpr float sve_relative_tolerance<float>() { return 1.0e-5f; }
template <> constexpr float sve_relative_tolerance<int8_t>() { return 1.0e-5f; }
template <> constexpr float sve_relative_tolerance<_Float16>() {
    return 5.0e-2f;
}

template <size_t N, typename Ty, Metric::Kind Kind>
void expect_sve_matches_ref_metric(const Embedding<N, Ty> &a,
                                   const Embedding<N, Ty> &b) {
    const auto expected = similarity_ref<N, Ty, Kind>(a, b);
    const auto actual = similarity_sve<N, Ty, Kind>(a, b);
    const auto tolerance =
        near_tolerance(expected, sve_relative_tolerance<Ty>());

    EXPECT_NEAR(actual, expected, tolerance)
        << "N=" << N << ", metric=" << metric_name(Kind);
}

template <size_t N, typename Ty> void expect_sve_matches_ref_for_dimension() {
    const auto a = make_typed_test_embedding<N, Ty>(1);
    const auto b = make_typed_test_embedding<N, Ty>(-2);

    expect_sve_matches_ref_metric<N, Ty, Metric::EUCLIDEAN_DISTANCE>(a, b);
    expect_sve_matches_ref_metric<N, Ty, Metric::DOT_PRODUCT>(a, b);
    expect_sve_matches_ref_metric<N, Ty, Metric::COSINE_SIMILARITY>(a, b);
}

template <typename Ty> void expect_sve_matches_ref_for_supported_dimensions() {
    expect_sve_matches_ref_for_dimension<384, Ty>();
    expect_sve_matches_ref_for_dimension<768, Ty>();
    expect_sve_matches_ref_for_dimension<1024, Ty>();
    expect_sve_matches_ref_for_dimension<3072, Ty>();
}

template <typename Ty, size_t RemainderN>
void expect_sve_matches_ref_for_remainder_paths() {
    expect_sve_matches_ref_for_dimension<3, Ty>();
    expect_sve_matches_ref_for_dimension<RemainderN, Ty>();
}

TEST(VSS, supports_int8_metrics_sve) {
    expect_sve_matches_ref_for_supported_dimensions<int8_t>();
}

TEST(VSS, supports_fp16_metrics_sve) {
    expect_sve_matches_ref_for_supported_dimensions<_Float16>();
}

TEST(VSS, exercises_fp32_remainder_paths_sve) {
    expect_sve_matches_ref_for_remainder_paths<float, 67>();
}

TEST(VSS, exercises_int8_remainder_paths_sve) {
    expect_sve_matches_ref_for_remainder_paths<int8_t, 259>();
}

TEST(VSS, exercises_fp16_remainder_paths_sve) {
    expect_sve_matches_ref_for_remainder_paths<_Float16, 131>();
}

const auto sve_cases = testing::Values(
    make_metric_case<384>(Metric::EUCLIDEAN_DISTANCE, 0.25f, -1.5f),
    make_metric_case<768>(Metric::EUCLIDEAN_DISTANCE, 0.5f, -0.125f),
    make_metric_case<1024>(Metric::EUCLIDEAN_DISTANCE, -0.75f, 0.375f),
    make_metric_case<3072>(Metric::EUCLIDEAN_DISTANCE, 0.125f, -0.625f),
    make_metric_case<384>(Metric::DOT_PRODUCT, 0.125f, -0.25f),
    make_metric_case<768>(Metric::DOT_PRODUCT, -0.5f, 0.25f),
    make_metric_case<1024>(Metric::DOT_PRODUCT, 0.75f, -0.125f),
    make_metric_case<3072>(Metric::DOT_PRODUCT, -0.625f, 0.375f),
    make_metric_case<384>(Metric::COSINE_SIMILARITY, 0.125f, -0.25f),
    make_metric_case<768>(Metric::COSINE_SIMILARITY, -0.5f, 0.25f),
    make_metric_case<1024>(Metric::COSINE_SIMILARITY, 0.375f, -0.625f),
    make_metric_case<3072>(Metric::COSINE_SIMILARITY, -0.125f, 0.75f));

class metric_sve : public testing::TestWithParam<MetricCase> {};

TEST_P(metric_sve, matches_reference_value) {
    const auto &test_case = GetParam();
    const auto expected = compute_metric<MetricBackend::Reference>(test_case);
    EXPECT_NEAR(compute_metric<MetricBackend::SVE>(test_case), expected,
                near_tolerance(expected, 1.0e-6f));
}

INSTANTIATE_TEST_SUITE_P(VSS, metric_sve, sve_cases, metric_case_test_name);
#endif // __ARM_FEATURE_SVE

} // namespace
