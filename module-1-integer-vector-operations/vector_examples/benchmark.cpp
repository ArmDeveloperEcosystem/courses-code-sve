// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "vector_examples.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "benchmark/benchmark.h"

using std::int32_t;
using std::size_t;

using std::vector;

static void vector_examples_BM_VectorAddScalar(benchmark::State &state) {
    const size_t size = state.range(0);

    vector<int32_t> lhs(size, 1);
    vector<int32_t> rhs(size, 2);
    vector<int32_t> result(size);

    for (auto _ : state) {
        vector_add_scalar<int32_t>(lhs, rhs, result);

        benchmark::DoNotOptimize(result);
    }
}

BENCHMARK(vector_examples_BM_VectorAddScalar)
    ->RangeMultiplier(2)
    ->Range(1 << 8, 1 << 14);

#ifdef __ARM_FEATURE_SVE
static void vector_examples_BM_VectorAddSVE(benchmark::State &state) {
    const size_t size = state.range(0);

    vector<int32_t> lhs(size, 1);
    vector<int32_t> rhs(size, 2);
    vector<int32_t> result(size);

    for (auto _ : state) {
        vector_add_sve<int32_t>(lhs, rhs, result);

        benchmark::DoNotOptimize(result);
    }
}

BENCHMARK(vector_examples_BM_VectorAddSVE)
    ->RangeMultiplier(2)
    ->Range(1 << 8, 1 << 14);
#endif

static void vector_examples_BM_MinMaxScalar(benchmark::State &state) {
    // Use the vector size provided by Google Benchmark.
    const size_t size = state.range(0);

    vector<int32_t> values(size, 1);

    for (auto _ : state) {
        auto result = min_max_scalar<int32_t>(values);

        // Prevent the compiler from optimizing the result away.
        benchmark::DoNotOptimize(result);
    }
}

BENCHMARK(vector_examples_BM_MinMaxScalar)
    ->RangeMultiplier(2)
    ->Range(1 << 8, 1 << 14);

#ifdef __ARM_FEATURE_SVE
static void vector_examples_BM_MinMaxSVE(benchmark::State &state) {
    // Use the same vector size as the scalar benchmark.
    const size_t size = state.range(0);

    vector<int32_t> values(size, 1);

    for (auto _ : state) {
        auto result = min_max_sve<int32_t>(values);

        // Prevent the compiler from optimizing the result away.
        benchmark::DoNotOptimize(result);
    }
}

BENCHMARK(vector_examples_BM_MinMaxSVE)
    ->RangeMultiplier(2)
    ->Range(1 << 8, 1 << 14);
#endif
