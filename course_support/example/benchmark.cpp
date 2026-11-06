// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "example.h"

#include "benchmark/benchmark.h"

using example::foo;

namespace {

void example_foo_true(benchmark::State &state) {
    bool b;
    for (auto _ : state) {
        b = foo(true);
        benchmark::DoNotOptimize(b);
    }
}

void example_foo_false(benchmark::State &state) {
    bool b;
    for (auto _ : state) {
        b = foo(false);
        benchmark::DoNotOptimize(b);
    }
}

} // namespace

BENCHMARK(example_foo_true);
BENCHMARK(example_foo_false);