// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "match_and_visit.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

#include "benchmark/benchmark.h"

using std::int64_t;
using std::size_t;
using std::uint32_t;
using std::uint64_t;

using std::vector;

namespace {

constexpr size_t DATA_SIZE = 1 << 14;
constexpr uint32_t THRESHOLD = DATA_SIZE;

using ParseFunction = int (*)(const vector<uint32_t> &, uint32_t,
                              const matchAndVisit::MatchCallback &);

vector<uint32_t> makeInput(size_t callbackProbabilityBasisPoints) {
    const auto callbackCount =
        DATA_SIZE * callbackProbabilityBasisPoints / 10'000;
    vector<uint32_t> input(DATA_SIZE, THRESHOLD);
    for (size_t i = 0; i < callbackCount; ++i) {
        input[i] = static_cast<uint32_t>(i);
    }

    std::mt19937 generator(0x5EED);
    std::shuffle(input.begin(), input.end(), generator);
    return input;
}

void runParseBenchmark(benchmark::State &state, ParseFunction parse) {
    const auto callbackProbabilityBasisPoints =
        static_cast<size_t>(state.range(0));
    const auto input = makeInput(callbackProbabilityBasisPoints);
    const auto callbackCount =
        DATA_SIZE * callbackProbabilityBasisPoints / 10'000;
    uint64_t callbackChecksum = 0;
    const matchAndVisit::MatchCallback callback =
        [&callbackChecksum](uint32_t value) {
            callbackChecksum = callbackChecksum * 131 + value + 1;
        };

    for (auto _ : state) {
        auto status = parse(input, THRESHOLD, callback);
        benchmark::DoNotOptimize(status);
        benchmark::DoNotOptimize(callbackChecksum);
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(DATA_SIZE));
    state.counters["callbacks"] = static_cast<double>(callbackCount);
}

void match_and_visit_scalar(benchmark::State &state) {
    runParseBenchmark(state, matchAndVisit::matchAndVisitScalar);
}

#ifdef __ARM_FEATURE_SVE
void match_and_visit_sve_optimized(benchmark::State &state) {
    runParseBenchmark(state, matchAndVisit::matchAndVisitSVEOptimized);
}

void match_and_visit_sve_basic(benchmark::State &state) {
    runParseBenchmark(state, matchAndVisit::matchAndVisitSVEBasic);
}

[[maybe_unused]] void match_and_visit_sve_compact(benchmark::State &state) {
    runParseBenchmark(state, matchAndVisit::matchAndVisitSVECompact);
}
#endif

} // namespace

BENCHMARK(match_and_visit_scalar)
    ->ArgName("callback_probability_basis_points")
    ->Arg(1)
    ->Arg(10)
    ->Arg(100)
    ->Arg(1'000)
    ->Arg(10'000);

#ifdef __ARM_FEATURE_SVE
BENCHMARK(match_and_visit_sve_optimized)
    ->ArgName("callback_probability_basis_points")
    ->Arg(1)
    ->Arg(10)
    ->Arg(100)
    ->Arg(1'000)
    ->Arg(10'000);
BENCHMARK(match_and_visit_sve_basic)
    ->ArgName("callback_probability_basis_points")
    ->Arg(1)
    ->Arg(10)
    ->Arg(100)
    ->Arg(1'000)
    ->Arg(10'000);
// The compact implementation is retained as further-reading material.
/*
BENCHMARK(match_and_visit_sve_compact)
    ->ArgName("callback_probability_basis_points")
    ->Arg(1)
    ->Arg(10)
    ->Arg(100)
    ->Arg(1'000)
    ->Arg(10'000);
*/
#endif
