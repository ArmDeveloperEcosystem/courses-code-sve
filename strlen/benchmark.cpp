// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "strlen.h"

#include <cstddef>
#include <cstring>
#include <string>

#include "benchmark/benchmark.h"

using std::size_t;

namespace {

using StrlenFunction = size_t (*)(const char *);

void run(benchmark::State &state, StrlenFunction function) {
    const std::string input(static_cast<size_t>(state.range(0)), 'x');

    for (auto _ : state)
        benchmark::DoNotOptimize(function(input.c_str()));

    state.SetBytesProcessed(state.iterations() * state.range(0));
    state.SetComplexityN(state.range(0));
}

void strlen_scalar(benchmark::State &state) { run(state, str::strlen); }
void strlen_std(benchmark::State &state) { run(state, std::strlen); }

#ifdef __ARM_FEATURE_SVE
void strlen_sve_naive(benchmark::State &state) {
    run(state, str::strlenSVENaive);
}
void strlen_sve_improved(benchmark::State &state) {
    run(state, str::strlenSVEImproved);
}
void strlen_sve_simd_loops(benchmark::State &state) {
    run(state, str::strlenSVESimdLoops);
}

#endif

} // namespace

BENCHMARK(strlen_scalar)
    ->RangeMultiplier(4)
    ->Range(1, 1 << 12)
    ->Complexity(benchmark::oN);
BENCHMARK(strlen_std)
    ->RangeMultiplier(4)
    ->Range(1, 1 << 12)
    ->Complexity(benchmark::oN);

#ifdef __ARM_FEATURE_SVE
BENCHMARK(strlen_sve_naive)
    ->RangeMultiplier(4)
    ->Range(1, 1 << 12)
    ->Complexity(benchmark::oN);
BENCHMARK(strlen_sve_improved)
    ->RangeMultiplier(4)
    ->Range(1, 1 << 12)
    ->Complexity(benchmark::oN);
BENCHMARK(strlen_sve_simd_loops)
    ->RangeMultiplier(4)
    ->Range(1, 1 << 12)
    ->Complexity(benchmark::oN);
#endif
