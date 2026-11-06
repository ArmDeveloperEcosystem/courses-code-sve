// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "VSS.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "benchmark/benchmark.h"

using std::int64_t;
using std::int8_t;
using std::size_t;

using std::array;

using VSS::Embedding;
using VSS::make_embedding;
using VSS::Metric;
using VSS::similarity_ref;
#ifdef __ARM_FEATURE_SVE
using VSS::similarity_sve;
#endif //   __ARM_FEATURE_SVE

namespace {

template <size_t N, typename Ty>
Embedding<N, Ty> make_patterned_embedding(float offset) {
    array<Ty, N> values{};
    for (size_t i = 0; i < N; ++i) {
        if constexpr (std::is_same_v<Ty, int8_t>) {
            values[i] =
                static_cast<Ty>(static_cast<int>(offset) + int(i % 31) - 15);
        } else {
            values[i] =
                static_cast<Ty>(offset + static_cast<float>(i % 31) * 0.125f);
        }
    }
    return make_embedding<N, Ty>(values);
}

struct MetricCase {
    float a_offset;
    float b_offset;
    void (*reference)(benchmark::State &state, const MetricCase &bench_case);
#ifdef __ARM_FEATURE_SVE
    void (*sve)(benchmark::State &state, const MetricCase &bench_case);
#endif //   __ARM_FEATURE_SVE
};

template <size_t N, typename Ty, typename Compute>
void run_metric(benchmark::State &state, const MetricCase &bench_case,
                Compute compute) {
    const auto a = make_patterned_embedding<N, Ty>(bench_case.a_offset);
    const auto b = make_patterned_embedding<N, Ty>(bench_case.b_offset);
    for (auto _ : state) {
        auto result = compute(a, b);
        benchmark::DoNotOptimize(result);
    }

    state.counters["Metric/s"] = benchmark::Counter(
        static_cast<int64_t>(state.iterations()), benchmark::Counter::kIsRate);
}

template <size_t N, typename Ty, Metric::Kind metric>
void reference(benchmark::State &state, const MetricCase &bench_case) {
    using TestEmbedding = Embedding<N, Ty>;
    run_metric<N, Ty>(state, bench_case,
                      [](const TestEmbedding &a, const TestEmbedding &b) {
                          return similarity_ref<N, Ty, metric>(a, b);
                      });
}

#ifdef __ARM_FEATURE_SVE
template <size_t N, typename Ty, Metric::Kind metric>
void sve(benchmark::State &state, const MetricCase &bench_case) {
    using TestEmbedding = Embedding<N, Ty>;
    run_metric<N, Ty>(state, bench_case,
                      [](const TestEmbedding &a, const TestEmbedding &b) {
                          return similarity_sve<N, Ty, metric>(a, b);
                      });
}
#endif // __ARM_FEATURE_SVE

void VSS_euclidean_distance_ref(benchmark::State &state,
                                MetricCase bench_case) {
    bench_case.reference(state, bench_case);
}

void VSS_dot_product_ref(benchmark::State &state, MetricCase bench_case) {
    bench_case.reference(state, bench_case);
}

void VSS_cosine_similarity_ref(benchmark::State &state, MetricCase bench_case) {
    bench_case.reference(state, bench_case);
}

#ifdef __ARM_FEATURE_SVE
void VSS_euclidean_distance_sve(benchmark::State &state,
                                MetricCase bench_case) {
    bench_case.sve(state, bench_case);
}

void VSS_dot_product_sve(benchmark::State &state, MetricCase bench_case) {
    bench_case.sve(state, bench_case);
}

void VSS_cosine_similarity_sve(benchmark::State &state, MetricCase bench_case) {
    bench_case.sve(state, bench_case);
}
#endif // __ARM_FEATURE_SVE

template <size_t N, typename Ty, Metric::Kind metric>
constexpr MetricCase make_metric_case() {
    return MetricCase{
        .a_offset = 0.25f,
        .b_offset = -1.0f,
        .reference = reference<N, Ty, metric>,
#ifdef __ARM_FEATURE_SVE
        .sve = sve<N, Ty, metric>,
#endif // __ARM_FEATURE_SVE
    };
}

#define VSS_DEFINE_METRIC_CASES(SUFFIX, TYPE)                                  \
    constexpr auto euclidean_case_384_##SUFFIX =                               \
        make_metric_case<384, TYPE, Metric::EUCLIDEAN_DISTANCE>();             \
    constexpr auto euclidean_case_768_##SUFFIX =                               \
        make_metric_case<768, TYPE, Metric::EUCLIDEAN_DISTANCE>();             \
    constexpr auto euclidean_case_1024_##SUFFIX =                              \
        make_metric_case<1024, TYPE, Metric::EUCLIDEAN_DISTANCE>();            \
    constexpr auto euclidean_case_3072_##SUFFIX =                              \
        make_metric_case<3072, TYPE, Metric::EUCLIDEAN_DISTANCE>();            \
                                                                               \
    constexpr auto dot_product_case_384_##SUFFIX =                             \
        make_metric_case<384, TYPE, Metric::DOT_PRODUCT>();                    \
    constexpr auto dot_product_case_768_##SUFFIX =                             \
        make_metric_case<768, TYPE, Metric::DOT_PRODUCT>();                    \
    constexpr auto dot_product_case_1024_##SUFFIX =                            \
        make_metric_case<1024, TYPE, Metric::DOT_PRODUCT>();                   \
    constexpr auto dot_product_case_3072_##SUFFIX =                            \
        make_metric_case<3072, TYPE, Metric::DOT_PRODUCT>();                   \
                                                                               \
    constexpr auto cosine_similarity_case_384_##SUFFIX =                       \
        make_metric_case<384, TYPE, Metric::COSINE_SIMILARITY>();              \
    constexpr auto cosine_similarity_case_768_##SUFFIX =                       \
        make_metric_case<768, TYPE, Metric::COSINE_SIMILARITY>();              \
    constexpr auto cosine_similarity_case_1024_##SUFFIX =                      \
        make_metric_case<1024, TYPE, Metric::COSINE_SIMILARITY>();             \
    constexpr auto cosine_similarity_case_3072_##SUFFIX =                      \
        make_metric_case<3072, TYPE, Metric::COSINE_SIMILARITY>();

VSS_DEFINE_METRIC_CASES(fp32, float)
VSS_DEFINE_METRIC_CASES(int8, int8_t)
VSS_DEFINE_METRIC_CASES(fp16, _Float16)

#undef VSS_DEFINE_METRIC_CASES

} // namespace

#define VSS_REGISTER_REFERENCE_BENCHMARKS(DIMENSION, SUFFIX)                   \
    BENCHMARK_CAPTURE(VSS_euclidean_distance_ref, DIMENSION##_##SUFFIX,        \
                      euclidean_case_##DIMENSION##_##SUFFIX);                  \
    BENCHMARK_CAPTURE(VSS_dot_product_ref, DIMENSION##_##SUFFIX,               \
                      dot_product_case_##DIMENSION##_##SUFFIX);                \
    BENCHMARK_CAPTURE(VSS_cosine_similarity_ref, DIMENSION##_##SUFFIX,         \
                      cosine_similarity_case_##DIMENSION##_##SUFFIX);

#define VSS_REGISTER_REFERENCE_BENCHMARK_DIMENSION(DIMENSION)                  \
    VSS_REGISTER_REFERENCE_BENCHMARKS(DIMENSION, fp32)                         \
    VSS_REGISTER_REFERENCE_BENCHMARKS(DIMENSION, int8)                         \
    VSS_REGISTER_REFERENCE_BENCHMARKS(DIMENSION, fp16)

VSS_REGISTER_REFERENCE_BENCHMARK_DIMENSION(384)
VSS_REGISTER_REFERENCE_BENCHMARK_DIMENSION(768)
VSS_REGISTER_REFERENCE_BENCHMARK_DIMENSION(1024)
VSS_REGISTER_REFERENCE_BENCHMARK_DIMENSION(3072)

#undef VSS_REGISTER_REFERENCE_BENCHMARKS
#undef VSS_REGISTER_REFERENCE_BENCHMARK_DIMENSION

#ifdef __ARM_FEATURE_SVE
#define VSS_REGISTER_SVE_BENCHMARKS(DIMENSION, SUFFIX)                         \
    BENCHMARK_CAPTURE(VSS_euclidean_distance_sve, DIMENSION##_##SUFFIX,        \
                      euclidean_case_##DIMENSION##_##SUFFIX);                  \
    BENCHMARK_CAPTURE(VSS_dot_product_sve, DIMENSION##_##SUFFIX,               \
                      dot_product_case_##DIMENSION##_##SUFFIX);                \
    BENCHMARK_CAPTURE(VSS_cosine_similarity_sve, DIMENSION##_##SUFFIX,         \
                      cosine_similarity_case_##DIMENSION##_##SUFFIX);

#define VSS_REGISTER_SVE_BENCHMARK_DIMENSION(DIMENSION)                        \
    VSS_REGISTER_SVE_BENCHMARKS(DIMENSION, fp32)                               \
    VSS_REGISTER_SVE_BENCHMARKS(DIMENSION, int8)                               \
    VSS_REGISTER_SVE_BENCHMARKS(DIMENSION, fp16)

VSS_REGISTER_SVE_BENCHMARK_DIMENSION(384)
VSS_REGISTER_SVE_BENCHMARK_DIMENSION(768)
VSS_REGISTER_SVE_BENCHMARK_DIMENSION(1024)
VSS_REGISTER_SVE_BENCHMARK_DIMENSION(3072)

#undef VSS_REGISTER_SVE_BENCHMARKS
#undef VSS_REGISTER_SVE_BENCHMARK_DIMENSION
#endif // __ARM_FEATURE_SVE
