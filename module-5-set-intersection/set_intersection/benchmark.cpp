// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "set_intersection.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "benchmark/benchmark.h"

using std::int64_t;
using std::size_t;

using SetIntersection::Value;
using std::vector;

namespace {

struct Dataset {
    vector<Value> lhs;
    vector<Value> rhs;
};

Dataset makeSparseProbeDataset(size_t needle_count, size_t haystack_count,
                               size_t match_every) {
    Dataset dataset;
    dataset.lhs.reserve(needle_count);
    dataset.rhs.reserve(haystack_count);

    for (size_t i = 0; i < haystack_count; ++i)
        dataset.rhs.push_back(static_cast<Value>(i * 4));

    for (size_t i = 0; i < needle_count; ++i) {
        const auto haystack_index =
            ((i + 1) * (haystack_count - 2)) / (needle_count + 1);
        auto value = dataset.rhs[haystack_index];
        if (match_every == 0 || i % match_every != 0)
            ++value;
        dataset.lhs.push_back(value);
    }

    return dataset;
}

Dataset makeDenseEqualDataset(size_t value_count) {
    Dataset dataset;
    dataset.lhs.reserve(value_count);
    dataset.rhs.reserve(value_count);

    for (size_t i = 0; i < value_count; ++i) {
        const auto value = static_cast<Value>(i * 2);
        dataset.lhs.push_back(value);
        dataset.rhs.push_back(value);
    }

    return dataset;
}

Dataset makeInterleavedMissDataset(size_t value_count) {
    Dataset dataset;
    dataset.lhs.reserve(value_count);
    dataset.rhs.reserve(value_count);

    for (size_t i = 0; i < value_count; ++i) {
        dataset.lhs.push_back(static_cast<Value>(i * 4 + 1));
        dataset.rhs.push_back(static_cast<Value>(i * 4 + 3));
    }

    return dataset;
}

Dataset makeControlledIntersectionDataset(size_t shorter_count,
                                          size_t longer_count,
                                          size_t intersection_percent) {
    Dataset dataset;
    dataset.lhs.reserve(shorter_count);
    dataset.rhs.reserve(longer_count);

    for (size_t i = 0; i < longer_count; ++i)
        dataset.rhs.push_back(static_cast<Value>(i * 4));

    const auto match_count = shorter_count * intersection_percent / 100;
    for (size_t i = 0; i < shorter_count; ++i) {
        const auto longer_index =
            ((i + 1) * longer_count) / (shorter_count + 1);
        auto value = dataset.rhs[longer_index];

        const auto matches_before = i * match_count / shorter_count;
        const auto matches_after = (i + 1) * match_count / shorter_count;
        if (matches_after == matches_before)
            ++value;

        dataset.lhs.push_back(value);
    }

    return dataset;
}

using IntersectFn = size_t (*)(const Value *, size_t, const Value *, size_t,
                               Value *) noexcept;

void runBenchmark(benchmark::State &state, const Dataset &dataset,
                  IntersectFn function) {
    vector<Value> output(std::min(dataset.lhs.size(), dataset.rhs.size()));
    size_t output_count = 0;

    for (auto _ : state) {
        output_count =
            function(dataset.lhs.data(), dataset.lhs.size(), dataset.rhs.data(),
                     dataset.rhs.size(), output.data());
        benchmark::DoNotOptimize(output_count);
        benchmark::DoNotOptimize(output.data());
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(
        static_cast<int64_t>(state.iterations()) *
        static_cast<int64_t>(dataset.lhs.size() + dataset.rhs.size()));
    state.SetBytesProcessed(
        static_cast<int64_t>(state.iterations()) *
        static_cast<int64_t>(dataset.lhs.size() + dataset.rhs.size()) *
        static_cast<int64_t>(sizeof(Value)));
    state.counters["matches"] = static_cast<double>(output_count);
}

void set_intersection_scalar_sparse_probe(benchmark::State &state) {
    const auto dataset =
        makeSparseProbeDataset(static_cast<size_t>(state.range(0)),
                               static_cast<size_t>(state.range(1)),
                               static_cast<size_t>(state.range(2)));
    runBenchmark(state, dataset, SetIntersection::intersect_scalar);
}

void set_intersection_restrict_sparse_probe(benchmark::State &state) {
    const auto dataset =
        makeSparseProbeDataset(static_cast<size_t>(state.range(0)),
                               static_cast<size_t>(state.range(1)),
                               static_cast<size_t>(state.range(2)));
    runBenchmark(state, dataset, SetIntersection::intersect_restrict);
}

void set_intersection_scalar_dense_equal(benchmark::State &state) {
    const auto dataset =
        makeDenseEqualDataset(static_cast<size_t>(state.range(0)));
    runBenchmark(state, dataset, SetIntersection::intersect_scalar);
}

void set_intersection_restrict_dense_equal(benchmark::State &state) {
    const auto dataset =
        makeDenseEqualDataset(static_cast<size_t>(state.range(0)));
    runBenchmark(state, dataset, SetIntersection::intersect_restrict);
}

void set_intersection_scalar_interleaved_miss(benchmark::State &state) {
    const auto dataset =
        makeInterleavedMissDataset(static_cast<size_t>(state.range(0)));
    runBenchmark(state, dataset, SetIntersection::intersect_scalar);
}

void set_intersection_restrict_interleaved_miss(benchmark::State &state) {
    const auto dataset =
        makeInterleavedMissDataset(static_cast<size_t>(state.range(0)));
    runBenchmark(state, dataset, SetIntersection::intersect_restrict);
}

Dataset makeControlledIntersectionDataset(const benchmark::State &state) {
    const auto shorter_count = static_cast<size_t>(state.range(0));
    const auto size_ratio = static_cast<size_t>(state.range(1));
    const auto intersection_percent = static_cast<size_t>(state.range(2));
    return makeControlledIntersectionDataset(
        shorter_count, shorter_count * size_ratio, intersection_percent);
}

void set_intersection_scalar_workload_matrix(benchmark::State &state) {
    const auto dataset = makeControlledIntersectionDataset(state);
    runBenchmark(state, dataset, SetIntersection::intersect_scalar);
}

void set_intersection_restrict_workload_matrix(benchmark::State &state) {
    const auto dataset = makeControlledIntersectionDataset(state);
    runBenchmark(state, dataset, SetIntersection::intersect_restrict);
}

#ifdef __ARM_FEATURE_SVE
void set_intersection_sve_probe_sparse_probe(benchmark::State &state) {
    const auto dataset =
        makeSparseProbeDataset(static_cast<size_t>(state.range(0)),
                               static_cast<size_t>(state.range(1)),
                               static_cast<size_t>(state.range(2)));
    runBenchmark(state, dataset, SetIntersection::intersect_sve_probe);
}

void set_intersection_sve_jammed_sparse_probe(benchmark::State &state) {
    const auto dataset =
        makeSparseProbeDataset(static_cast<size_t>(state.range(0)),
                               static_cast<size_t>(state.range(1)),
                               static_cast<size_t>(state.range(2)));
    runBenchmark(state, dataset, SetIntersection::intersect_sve_jammed);
}

void set_intersection_sve_probe_dense_equal(benchmark::State &state) {
    const auto dataset =
        makeDenseEqualDataset(static_cast<size_t>(state.range(0)));
    runBenchmark(state, dataset, SetIntersection::intersect_sve_probe);
}

void set_intersection_sve_jammed_dense_equal(benchmark::State &state) {
    const auto dataset =
        makeDenseEqualDataset(static_cast<size_t>(state.range(0)));
    runBenchmark(state, dataset, SetIntersection::intersect_sve_jammed);
}

void set_intersection_sve_probe_interleaved_miss(benchmark::State &state) {
    const auto dataset =
        makeInterleavedMissDataset(static_cast<size_t>(state.range(0)));
    runBenchmark(state, dataset, SetIntersection::intersect_sve_probe);
}

void set_intersection_sve_jammed_interleaved_miss(benchmark::State &state) {
    const auto dataset =
        makeInterleavedMissDataset(static_cast<size_t>(state.range(0)));
    runBenchmark(state, dataset, SetIntersection::intersect_sve_jammed);
}

void set_intersection_sve_probe_workload_matrix(benchmark::State &state) {
    const auto dataset = makeControlledIntersectionDataset(state);
    runBenchmark(state, dataset, SetIntersection::intersect_sve_probe);
}

void set_intersection_sve_jammed_workload_matrix(benchmark::State &state) {
    const auto dataset = makeControlledIntersectionDataset(state);
    runBenchmark(state, dataset, SetIntersection::intersect_sve_jammed);
}
#endif

} // namespace

BENCHMARK(set_intersection_scalar_sparse_probe)
    ->ArgNames({"needles", "haystack", "match_every"})
    ->Args({4096, 262144, 4})
    ->Args({16384, 1048576, 8});
BENCHMARK(set_intersection_restrict_sparse_probe)
    ->ArgNames({"needles", "haystack", "match_every"})
    ->Args({4096, 262144, 4})
    ->Args({16384, 1048576, 8});

BENCHMARK(set_intersection_scalar_dense_equal)->ArgName("values")->Arg(65536);
BENCHMARK(set_intersection_restrict_dense_equal)->ArgName("values")->Arg(65536);

BENCHMARK(set_intersection_scalar_interleaved_miss)
    ->ArgName("values")
    ->Arg(65536);
BENCHMARK(set_intersection_restrict_interleaved_miss)
    ->ArgName("values")
    ->Arg(65536);

BENCHMARK(set_intersection_scalar_workload_matrix)
    ->ArgNames({"shorter", "ratio", "intersection_pct"})
    ->ArgsProduct({{128, 1024}, {1, 8, 64}, {1, 5, 50, 95}});
BENCHMARK(set_intersection_restrict_workload_matrix)
    ->ArgNames({"shorter", "ratio", "intersection_pct"})
    ->ArgsProduct({{128, 1024}, {1, 8, 64}, {1, 5, 50, 95}});

#ifdef __ARM_FEATURE_SVE
BENCHMARK(set_intersection_sve_probe_sparse_probe)
    ->ArgNames({"needles", "haystack", "match_every"})
    ->Args({4096, 262144, 4})
    ->Args({16384, 1048576, 8});
BENCHMARK(set_intersection_sve_jammed_sparse_probe)
    ->ArgNames({"needles", "haystack", "match_every"})
    ->Args({4096, 262144, 4})
    ->Args({16384, 1048576, 8});

BENCHMARK(set_intersection_sve_probe_dense_equal)
    ->ArgName("values")
    ->Arg(65536);
BENCHMARK(set_intersection_sve_jammed_dense_equal)
    ->ArgName("values")
    ->Arg(65536);

BENCHMARK(set_intersection_sve_probe_interleaved_miss)
    ->ArgName("values")
    ->Arg(65536);
BENCHMARK(set_intersection_sve_jammed_interleaved_miss)
    ->ArgName("values")
    ->Arg(65536);

BENCHMARK(set_intersection_sve_probe_workload_matrix)
    ->ArgNames({"shorter", "ratio", "intersection_pct"})
    ->ArgsProduct({{128, 1024}, {1, 8, 64}, {1, 5, 50, 95}});
BENCHMARK(set_intersection_sve_jammed_workload_matrix)
    ->ArgNames({"shorter", "ratio", "intersection_pct"})
    ->ArgsProduct({{128, 1024}, {1, 8, 64}, {1, 5, 50, 95}});
#endif
