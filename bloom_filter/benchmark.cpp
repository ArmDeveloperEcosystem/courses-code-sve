// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on

#include "bloom.h"
#include "bloom_util.h"

#include <cstddef>
#include <string>
#include <vector>

#include "benchmark/benchmark.h"

using std::size_t;
using std::string;
using std::vector;

namespace {
const vector<string> keys =
    Bloom::build_keys(BLOOM_NUM_PRECALC_KEYS, BLOOM_MAX_KEY_LENGTH);

// This benchmark measures the performance if key insertion into the Bloom
// filter. It repeatedly adds keys from the pre-built key pool to the filter and
// tracks the throughput.
template <typename FilterType>
void bloom_filter_insert_key(benchmark::State &state) {
    if (keys.empty()) {
        state.SkipWithError("The key pool must not be empty");
        return;
    }

    FilterType filter;

    size_t index = 0;
    for (auto _ : state) {
        filter.add(keys[index]);
        benchmark::DoNotOptimize(filter);

        if (++index == keys.size())
            index = 0;
    }

    // Provides keys-per-second throughput.
    state.SetItemsProcessed(state.iterations());
}

// This benchmark measures the performance of key lookups for existing keys in
// the Bloom filter. It repeatedly checks for the presence of keys from the
// pre-built key pool and tracks the throughput.
template <typename FilterType>
void bloom_filter_lookup_existing_key(benchmark::State &state) {
    if (keys.empty()) {
        state.SkipWithError("The key pool must not be empty");
        return;
    }

    FilterType filter(keys);

    bool found;
    size_t index = 0;
    for (auto _ : state) {
        found = filter.maybe_contains(keys[index]);
        benchmark::DoNotOptimize(found);

        if (++index == keys.size())
            index = 0;
    }

    // Provides keys-per-second throughput.
    state.SetItemsProcessed(state.iterations());
}

// This benchmark measures the performance of key lookups for non-existing keys
// in the Bloom filter. It repeatedly checks for the presence of keys from the
// pre-built key pool and tracks the throughput. Note that the keys being
// checked are not added to the filter, simulating lookups for non-existing
// keys.
template <typename FilterType>
void bloom_filter_lookup_non_existing_key(benchmark::State &state) {
    if (keys.empty()) {
        state.SkipWithError("The key pool must not be empty");
        return;
    }

    FilterType filter;

    bool found;
    size_t index = 0;
    for (auto _ : state) {
        found = filter.maybe_contains(keys[index]);
        benchmark::DoNotOptimize(found);

        if (++index == keys.size())
            index = 0;
    }

    // Provides keys-per-second throughput.
    state.SetItemsProcessed(state.iterations());
}
} // namespace

using Bloom::BloomFilterScalar32;
using Bloom::BloomFilterScalar32Partitioned;
using Bloom::BloomFilterScalar64;
using Bloom::BloomFilterScalar64Partitioned;

BENCHMARK(bloom_filter_insert_key<BloomFilterScalar64>);
BENCHMARK(bloom_filter_insert_key<BloomFilterScalar32>);
BENCHMARK(bloom_filter_insert_key<BloomFilterScalar64Partitioned>);
BENCHMARK(bloom_filter_insert_key<BloomFilterScalar32Partitioned>);

#ifdef __ARM_FEATURE_SVE
using Bloom::BloomFilterSVE32;
using Bloom::BloomFilterSVE32Partitioned;
using Bloom::BloomFilterSVE64;
using Bloom::BloomFilterSVE64Partitioned;

BENCHMARK(bloom_filter_insert_key<BloomFilterSVE64>);
BENCHMARK(bloom_filter_insert_key<BloomFilterSVE32>);
BENCHMARK(bloom_filter_insert_key<BloomFilterSVE64Partitioned>);
BENCHMARK(bloom_filter_insert_key<BloomFilterSVE32Partitioned>);
#endif

BENCHMARK(bloom_filter_lookup_existing_key<BloomFilterScalar64>);
BENCHMARK(bloom_filter_lookup_existing_key<BloomFilterScalar32>);
BENCHMARK(bloom_filter_lookup_existing_key<BloomFilterScalar64Partitioned>);
BENCHMARK(bloom_filter_lookup_existing_key<BloomFilterScalar32Partitioned>);

#ifdef __ARM_FEATURE_SVE
BENCHMARK(bloom_filter_lookup_existing_key<BloomFilterSVE64>);
BENCHMARK(bloom_filter_lookup_existing_key<BloomFilterSVE32>);
BENCHMARK(bloom_filter_lookup_existing_key<BloomFilterSVE64Partitioned>);
BENCHMARK(bloom_filter_lookup_existing_key<BloomFilterSVE32Partitioned>);
#endif

BENCHMARK(bloom_filter_lookup_non_existing_key<BloomFilterScalar64>);
BENCHMARK(bloom_filter_lookup_non_existing_key<BloomFilterScalar32>);
BENCHMARK(bloom_filter_lookup_non_existing_key<BloomFilterScalar64Partitioned>);
BENCHMARK(bloom_filter_lookup_non_existing_key<BloomFilterScalar32Partitioned>);

#ifdef __ARM_FEATURE_SVE
BENCHMARK(bloom_filter_lookup_non_existing_key<BloomFilterSVE64>);
BENCHMARK(bloom_filter_lookup_non_existing_key<BloomFilterSVE32>);
BENCHMARK(bloom_filter_lookup_non_existing_key<BloomFilterSVE64Partitioned>);
BENCHMARK(bloom_filter_lookup_non_existing_key<BloomFilterSVE32Partitioned>);
#endif