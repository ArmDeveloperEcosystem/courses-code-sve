// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "histogram.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <vector>

#include "benchmark/benchmark.h"

using std::size_t;
using std::uint16_t;
using std::uint32_t;
using std::uint8_t;

using std::vector;

template <ValidSVEType T> void histogram_scalar(benchmark::State &state) {

    // data
    vector<T> data(state.range(0));
    std::iota(data.begin(), data.end(), 0);
    for (auto &val : data) {
        // keep values in [0,255] for a 256-bin histogram
        val = static_cast<T>(static_cast<uint32_t>(val) & 0xFFu);
    }
    // histogram
    vector<uint32_t> histogram(256, 0);
    // num of records
    for (auto _ : state) {
        histogram::calc_histogram<T>(histogram.data(), data.data(),
                                     data.size());
        benchmark::DoNotOptimize(histogram.data());
    }

    state.SetComplexityN(state.range(0));
}

template <ValidSVEType T>
void histogram_scalar_variable_bin(benchmark::State &state) {

    constexpr uint8_t bin_size = 16;
    constexpr size_t histogram_size = (256 + bin_size - 1) / bin_size;

    // data
    vector<T> data(state.range(0));
    std::iota(data.begin(), data.end(), 0);
    for (auto &val : data) {
        // keep values in [0,255] for a 256-bin histogram
        val = static_cast<T>(static_cast<uint32_t>(val) & 0xFFu);
    }
    // histogram
    vector<uint32_t> histogram(histogram_size, 0);
    // num of records
    for (auto _ : state) {
        histogram::calc_histogram<T>(histogram.data(), data.data(), data.size(),
                                     bin_size);
        benchmark::DoNotOptimize(histogram.data());
    }

    state.SetComplexityN(state.range(0));
}

BENCHMARK(histogram_scalar<uint8_t>)
    ->RangeMultiplier(4)
    ->Range((1 << 8), (1 << 18))
    ->Complexity(benchmark::oN);
BENCHMARK(histogram_scalar<uint16_t>)
    ->RangeMultiplier(4)
    ->Range((1 << 8), (1 << 18))
    ->Complexity(benchmark::oN);
BENCHMARK(histogram_scalar<uint32_t>)
    ->RangeMultiplier(4)
    ->Range((1 << 8), (1 << 18))
    ->Complexity(benchmark::oN);

BENCHMARK(histogram_scalar_variable_bin<uint8_t>)
    ->RangeMultiplier(4)
    ->Range((1 << 8), (1 << 18))
    ->Complexity(benchmark::oN);
BENCHMARK(histogram_scalar_variable_bin<uint16_t>)
    ->RangeMultiplier(4)
    ->Range((1 << 8), (1 << 18))
    ->Complexity(benchmark::oN);
BENCHMARK(histogram_scalar_variable_bin<uint32_t>)
    ->RangeMultiplier(4)
    ->Range((1 << 8), (1 << 18))
    ->Complexity(benchmark::oN);

#ifdef __ARM_FEATURE_SVE2
template <ValidSVEType T> void histogram_sve2(benchmark::State &state) {

    // data
    vector<T> data(state.range(0));
    std::iota(data.begin(), data.end(), 0);

    for (auto &val : data) {
        val = static_cast<T>(
            static_cast<uint32_t>(val) &
            0xFFu); // keep values in [0,255] for a 256-bin histogram
    }

    // histogram
    vector<uint32_t> histogram(256, 0);
    // num of records
    for (auto _ : state) {
        histogram::calc_histogram_sve2<T>(histogram.data(), data.data(),
                                          data.size());
        benchmark::DoNotOptimize(histogram.data());
    }
    state.SetComplexityN(state.range(0));
}

template <ValidSVEType T>
void histogram_sve2_unrolled(benchmark::State &state) {

    // data
    vector<T> data(state.range(0));
    std::iota(data.begin(), data.end(), 0);

    for (auto &val : data) {
        val = static_cast<T>(
            static_cast<uint32_t>(val) &
            0xFFu); // keep values in [0,255] for a 256-bin histogram
    }

    // histogram
    vector<uint32_t> histogram(256, 0);

    for (auto _ : state) {
        histogram::calc_histogram_sve2_unrolled<T>(histogram.data(),
                                                   data.data(), data.size());
        benchmark::DoNotOptimize(histogram.data());
    }
    state.SetComplexityN(state.range(0));
}

template <ValidSVEType T>
void histogram_sve2_variable_bin(benchmark::State &state) {

    constexpr uint8_t bin_size = 16;
    constexpr size_t histogram_size = (256 + bin_size - 1) / bin_size;

    // data
    vector<T> data(state.range(0));
    std::iota(data.begin(), data.end(), 0);

    for (auto &val : data) {
        val = static_cast<T>(
            static_cast<uint32_t>(val) &
            0xFFu); // keep values in [0,255] for a 256-bin histogram
    }

    // histogram
    vector<uint32_t> histogram(histogram_size, 0);
    // num of records
    for (auto _ : state) {
        histogram::calc_histogram_sve2<T>(histogram.data(), data.data(),
                                          data.size(), bin_size);
        benchmark::DoNotOptimize(histogram.data());
    }
    state.SetComplexityN(state.range(0));
}

template <ValidSVEType T>
void histogram_sve2_unrolled_variable_bin(benchmark::State &state) {

    constexpr uint8_t bin_size = 16;
    constexpr size_t histogram_size = (256 + bin_size - 1) / bin_size;

    // data
    vector<T> data(state.range(0));
    std::iota(data.begin(), data.end(), 0);

    for (auto &val : data) {
        val = static_cast<T>(
            static_cast<uint32_t>(val) &
            0xFFu); // keep values in [0,255] for a 256-bin histogram
    }

    // histogram
    vector<uint32_t> histogram(histogram_size, 0);

    for (auto _ : state) {
        histogram::calc_histogram_sve2_unrolled<T>(
            histogram.data(), data.data(), data.size(), bin_size);
        benchmark::DoNotOptimize(histogram.data());
    }
    state.SetComplexityN(state.range(0));
}

BENCHMARK(histogram_sve2<uint8_t>)
    ->RangeMultiplier(4)
    ->Range((1 << 8), (1 << 18))
    ->Complexity(benchmark::oN);
BENCHMARK(histogram_sve2<uint16_t>)
    ->RangeMultiplier(4)
    ->Range((1 << 8), (1 << 18))
    ->Complexity(benchmark::oN);
BENCHMARK(histogram_sve2<uint32_t>)
    ->RangeMultiplier(4)
    ->Range((1 << 8), (1 << 18))
    ->Complexity(benchmark::oN);

BENCHMARK(histogram_sve2_variable_bin<uint8_t>)
    ->RangeMultiplier(4)
    ->Range((1 << 8), (1 << 18))
    ->Complexity(benchmark::oN);
BENCHMARK(histogram_sve2_variable_bin<uint16_t>)
    ->RangeMultiplier(4)
    ->Range((1 << 8), (1 << 18))
    ->Complexity(benchmark::oN);
BENCHMARK(histogram_sve2_variable_bin<uint32_t>)
    ->RangeMultiplier(4)
    ->Range((1 << 8), (1 << 18))
    ->Complexity(benchmark::oN);

BENCHMARK(histogram_sve2_unrolled<uint8_t>)
    ->RangeMultiplier(4)
    ->Range((1 << 8), (1 << 18))
    ->Complexity(benchmark::oN);
BENCHMARK(histogram_sve2_unrolled<uint16_t>)
    ->RangeMultiplier(4)
    ->Range((1 << 8), (1 << 18))
    ->Complexity(benchmark::oN);
BENCHMARK(histogram_sve2_unrolled<uint32_t>)
    ->RangeMultiplier(4)
    ->Range((1 << 8), (1 << 18))
    ->Complexity(benchmark::oN);

BENCHMARK(histogram_sve2_unrolled_variable_bin<uint8_t>)
    ->RangeMultiplier(4)
    ->Range((1 << 8), (1 << 18))
    ->Complexity(benchmark::oN);
BENCHMARK(histogram_sve2_unrolled_variable_bin<uint16_t>)
    ->RangeMultiplier(4)
    ->Range((1 << 8), (1 << 18))
    ->Complexity(benchmark::oN);
BENCHMARK(histogram_sve2_unrolled_variable_bin<uint32_t>)
    ->RangeMultiplier(4)
    ->Range((1 << 8), (1 << 18))
    ->Complexity(benchmark::oN);
#endif

#ifdef __ARM_FEATURE_SVE
template <ValidSVEType T> void histogram_sve(benchmark::State &state) {

    // data
    vector<T> data(state.range(0));
    std::iota(data.begin(), data.end(), 0);

    for (auto &val : data) {
        val = static_cast<T>(
            static_cast<uint32_t>(val) &
            0xFFu); // keep values in [0,255] for a 256-bin histogram
    }

    // histogram
    vector<uint32_t> histogram(256, 0);
    // num of records
    for (auto _ : state) {
        histogram::calc_histogram_sve<T>(histogram.data(), data.data(),
                                         data.size());
        benchmark::DoNotOptimize(histogram.data());
    }
    state.SetComplexityN(state.range(0));
}

template <ValidSVEType T>
void histogram_sve_scalar_update(benchmark::State &state) {

    // data
    vector<T> data(state.range(0));
    std::iota(data.begin(), data.end(), 0);

    for (auto &val : data) {
        val = static_cast<T>(
            static_cast<uint32_t>(val) &
            0xFFu); // keep values in [0,255] for a 256-bin histogram
    }

    // histogram
    vector<uint32_t> histogram(256, 0);
    // num of records
    for (auto _ : state) {
        histogram::calc_histogram_sve_scalar_update<T>(
            histogram.data(), data.data(), data.size());
        benchmark::DoNotOptimize(histogram.data());
    }
    state.SetComplexityN(state.range(0));
}

/*
BENCHMARK(histogram_sve<uint8_t>)
    ->RangeMultiplier(4)
    ->Range((1 << 8), (1 << 18))
    ->Complexity(benchmark::oN);
BENCHMARK(histogram_sve<uint16_t>)
    ->RangeMultiplier(4)
    ->Range((1 << 8), (1 << 18))
    ->Complexity(benchmark::oN);
BENCHMARK(histogram_sve<uint32_t>)
    ->RangeMultiplier(4)
    ->Range((1 << 8), (1 << 18))
    ->Complexity(benchmark::oN);
*/

BENCHMARK(histogram_sve_scalar_update<uint8_t>)
    ->RangeMultiplier(4)
    ->Range((1 << 8), (1 << 18))
    ->Complexity(benchmark::oN);
BENCHMARK(histogram_sve_scalar_update<uint16_t>)
    ->RangeMultiplier(4)
    ->Range((1 << 8), (1 << 18))
    ->Complexity(benchmark::oN);
BENCHMARK(histogram_sve_scalar_update<uint32_t>)
    ->RangeMultiplier(4)
    ->Range((1 << 8), (1 << 18))
    ->Complexity(benchmark::oN);

#endif