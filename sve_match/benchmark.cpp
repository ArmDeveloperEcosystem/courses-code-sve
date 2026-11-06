// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "sve_match.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <random>
#include <span>
#include <vector>

#include "benchmark/benchmark.h"

using std::int64_t;
using std::size_t;
using std::uint16_t;
using std::uint32_t;
using std::uint64_t;
using std::uint8_t;

using std::map;
using std::vector;

namespace {

constexpr size_t HAYSTACK_SIZE = 1ULL << 26;
constexpr uint64_t RANDOM_SEED = 0x5EED;

template <sve_match::MatchElement T> constexpr auto benchmark_keys() {
    if constexpr (std::same_as<T, uint8_t>) {
        return std::to_array<uint8_t>(
            {0x13, 0x7F, 0xA5, 0xEE, 0x4C, 0x42, 0x01, 0x9B});
    } else {
        return std::to_array<uint16_t>(
            {0x1234, 0x7F7F, 0xA5A5, 0xEEEE, 0x4C4C, 0x4242});
    }
}

template <sve_match::MatchElement T>
bool is_key(T value, std::span<const T> keys) {
    return std::find(keys.begin(), keys.end(), value) != keys.end();
}

template <sve_match::MatchElement T>
vector<T> make_haystack(int64_t hit_probability_basis_points) {
    static constexpr auto keys = benchmark_keys<T>();
    const double hit_probability =
        static_cast<double>(hit_probability_basis_points) / 10'000.0;

    std::mt19937_64 generator(RANDOM_SEED);
    std::bernoulli_distribution choose_hit(hit_probability);
    std::uniform_int_distribution<size_t> choose_key(0, keys.size() - 1);
    std::uniform_int_distribution<uint32_t> choose_value(
        0, std::numeric_limits<T>::max());

    vector<T> haystack(HAYSTACK_SIZE);
    for (T &value : haystack) {
        if (choose_hit(generator)) {
            value = keys[choose_key(generator)];
            continue;
        }

        do {
            value = static_cast<T>(choose_value(generator));
        } while (is_key<T>(value, keys));
    }
    return haystack;
}

template <sve_match::MatchElement T>
const vector<T> &cached_haystack(int64_t hit_probability_basis_points) {
    // Cache one deterministic dataset per type and hit rate so scalar and SVE2
    // variants benchmark precisely the same input.
    static map<int64_t, vector<T>> cache;
    auto [position, inserted] = cache.try_emplace(hit_probability_basis_points);
    if (inserted) {
        position->second = make_haystack<T>(hit_probability_basis_points);
    }
    return position->second;
}

template <sve_match::MatchElement T>
using SearchFunction = bool (*)(std::span<const T>,
                                std::span<const T>) noexcept;

template <sve_match::MatchElement T>
void run_search_benchmark(benchmark::State &state, SearchFunction<T> search) {
    static constexpr auto keys = benchmark_keys<T>();
    const auto &haystack = cached_haystack<T>(state.range(0));
    const std::span<const T> haystack_span{haystack};
    const std::span<const T> key_span{keys};

    for (auto _ : state) {
        bool found = search(haystack_span, key_span);
        benchmark::DoNotOptimize(found);
    }
}

template <sve_match::MatchElement T>
void sve_match_scalar(benchmark::State &state) {
    run_search_benchmark<T>(state, sve_match::search_scalar<T>);
}

#ifdef __ARM_FEATURE_SVE2
template <sve_match::MatchElement T>
void sve_match_sve2(benchmark::State &state) {
    run_search_benchmark<T>(state, sve_match::search_sve2<T>);
}

template <sve_match::MatchElement T>
void sve_match_sve2_unrolled(benchmark::State &state) {
    run_search_benchmark<T>(state, sve_match::search_sve2_unrolled<T>);
}
#endif

} // namespace

BENCHMARK(sve_match_scalar<uint8_t>)
    ->ArgName("hit_probability_basis_points")
    ->Arg(0)
    ->Arg(1)
    ->Arg(10)
    ->Arg(100);
BENCHMARK(sve_match_scalar<uint16_t>)
    ->ArgName("hit_probability_basis_points")
    ->Arg(0)
    ->Arg(1)
    ->Arg(10)
    ->Arg(100);

#ifdef __ARM_FEATURE_SVE2
BENCHMARK(sve_match_sve2<uint8_t>)
    ->ArgName("hit_probability_basis_points")
    ->Arg(0)
    ->Arg(1)
    ->Arg(10)
    ->Arg(100);
BENCHMARK(sve_match_sve2<uint16_t>)
    ->ArgName("hit_probability_basis_points")
    ->Arg(0)
    ->Arg(1)
    ->Arg(10)
    ->Arg(100);
BENCHMARK(sve_match_sve2_unrolled<uint8_t>)
    ->ArgName("hit_probability_basis_points")
    ->Arg(0)
    ->Arg(1)
    ->Arg(10)
    ->Arg(100);
BENCHMARK(sve_match_sve2_unrolled<uint16_t>)
    ->ArgName("hit_probability_basis_points")
    ->Arg(0)
    ->Arg(1)
    ->Arg(10)
    ->Arg(100);
#endif
