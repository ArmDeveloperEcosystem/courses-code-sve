// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "base64.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <ranges>
#include <vector>

#include "benchmark/benchmark.h"

using std::size_t;
using std::uint8_t;

using std::vector;

class base64_benchmark_fixture : public benchmark::Fixture {
  protected:
    size_t N;
    size_t N_encode;
    vector<uint8_t> data;
    vector<char> encodedData;

  public:
    void SetUp(const ::benchmark::State &state) override {
        N = state.range(0);
        data.resize(N);
        std::ranges::fill(data, 3);
        N_encode = ((N + 2) / 3) * 4;
        encodedData.resize(N_encode);
        base64::base64_scalar_encode(data.data(), data.size(),
                                     encodedData.data());
    }
    void TearDown(const ::benchmark::State &state) override {}
};

BENCHMARK_DEFINE_F(base64_benchmark_fixture,
                   base64_ScalarEncodeBenchmark)(benchmark::State &state) {
    for (auto _ : state) {
        base64::base64_scalar_encode(data.data(), data.size(),
                                     encodedData.data());
        benchmark::DoNotOptimize(data);
        benchmark::DoNotOptimize(encodedData);
    }
    state.SetComplexityN(state.range(0));

    state.counters["CharactersEncoded/s"] =
        benchmark::Counter(static_cast<double>(state.iterations()) *
                               static_cast<double>(data.size()),
                           benchmark::Counter::kIsRate);
};

BENCHMARK_DEFINE_F(base64_benchmark_fixture,
                   base64_ScalarDecodeBenchmark)(benchmark::State &state) {
    for (auto _ : state) {
        base64::base64_scalar_decode(encodedData.data(), N_encode, data.data());
        benchmark::DoNotOptimize(encodedData);
        benchmark::DoNotOptimize(data);
    }
    state.SetComplexityN(state.range(0));

    state.counters["CharactersDecoded/s"] =
        benchmark::Counter(static_cast<double>(state.iterations()) *
                               static_cast<double>(encodedData.size()),
                           benchmark::Counter::kIsRate);
}

BENCHMARK_REGISTER_F(base64_benchmark_fixture, base64_ScalarEncodeBenchmark)
    ->RangeMultiplier(4)
    ->Range((1 << 6), (1 << 14))
    ->Complexity(benchmark::oN);
BENCHMARK_REGISTER_F(base64_benchmark_fixture, base64_ScalarDecodeBenchmark)
    ->RangeMultiplier(4)
    ->Range((1 << 6), (1 << 14))
    ->Complexity(benchmark::oN);

#ifdef __ARM_FEATURE_SVE
BENCHMARK_DEFINE_F(base64_benchmark_fixture,
                   base64_SVEEncodeBenchmark)(benchmark::State &state) {
    for (auto _ : state) {
        base64::base64_sve_encode(data.data(), data.size(), encodedData.data());
        benchmark::DoNotOptimize(data);
        benchmark::DoNotOptimize(encodedData);
    }
    state.SetComplexityN(state.range(0));

    state.counters["CharactersEncoded/s"] =
        benchmark::Counter(static_cast<double>(state.iterations()) *
                               static_cast<double>(data.size()),
                           benchmark::Counter::kIsRate);
};

BENCHMARK_DEFINE_F(base64_benchmark_fixture,
                   base64_SVEEncodeNaiveBenchmark)(benchmark::State &state) {
    for (auto _ : state) {
        base64::base64_sve_encode_naive(data.data(), data.size(),
                                        encodedData.data());
        benchmark::DoNotOptimize(data);
        benchmark::DoNotOptimize(encodedData);
    }
    state.SetComplexityN(state.range(0));

    state.counters["CharactersEncoded/s"] =
        benchmark::Counter(static_cast<double>(state.iterations()) *
                               static_cast<double>(data.size()),
                           benchmark::Counter::kIsRate);
};

BENCHMARK_DEFINE_F(base64_benchmark_fixture,
                   base64_SVEDecodeBenchmark)(benchmark::State &state) {
    for (auto _ : state) {
        base64::base64_sve_decode(encodedData.data(), encodedData.size(),
                                  data.data());
        benchmark::DoNotOptimize(data);
        benchmark::DoNotOptimize(encodedData);
    }
    state.SetComplexityN(state.range(0));

    state.counters["CharactersDecoded/s"] =
        benchmark::Counter(static_cast<double>(state.iterations()) *
                               static_cast<double>(encodedData.size()),
                           benchmark::Counter::kIsRate);
};

BENCHMARK_DEFINE_F(base64_benchmark_fixture,
                   base64_SVEDecodeNaiveBenchmark)(benchmark::State &state) {
    for (auto _ : state) {
        base64::base64_sve_decode_naive(encodedData.data(), encodedData.size(),
                                        data.data());
        benchmark::DoNotOptimize(data);
        benchmark::DoNotOptimize(encodedData);
    }
    state.SetComplexityN(state.range(0));

    state.counters["CharactersDecoded/s"] =
        benchmark::Counter(static_cast<double>(state.iterations()) *
                               static_cast<double>(encodedData.size()),
                           benchmark::Counter::kIsRate);
};

BENCHMARK_REGISTER_F(base64_benchmark_fixture, base64_SVEEncodeBenchmark)
    ->RangeMultiplier(4)
    ->Range((1 << 6), (1 << 14))
    ->Complexity(benchmark::oN);
BENCHMARK_REGISTER_F(base64_benchmark_fixture, base64_SVEEncodeNaiveBenchmark)
    ->RangeMultiplier(4)
    ->Range((1 << 6), (1 << 14))
    ->Complexity(benchmark::oN);
BENCHMARK_REGISTER_F(base64_benchmark_fixture, base64_SVEDecodeBenchmark)
    ->RangeMultiplier(4)
    ->Range((1 << 6), (1 << 14))
    ->Complexity(benchmark::oN);
BENCHMARK_REGISTER_F(base64_benchmark_fixture, base64_SVEDecodeNaiveBenchmark)
    ->RangeMultiplier(4)
    ->Range((1 << 6), (1 << 14))
    ->Complexity(benchmark::oN);
#endif