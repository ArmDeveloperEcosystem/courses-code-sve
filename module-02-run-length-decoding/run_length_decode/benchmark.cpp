// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "run_length_decoding.h"

#include <cstddef>
#include <string>

#include "benchmark/benchmark.h"

using std::size_t;
using std::string;

namespace {

class run_length_decode_ : public benchmark::Fixture {
  public:
    void SetUp(::benchmark::State &state) override {
        // sets the threshold for digit values within an encoded string
        // i.e. "<x>a<y>b", where x and y < THRESHOLD
        const auto threshold = static_cast<size_t>(state.range(0));
        const auto stringLength = static_cast<size_t>(state.range(1));
        const string substring = std::to_string(threshold) + 'a';

        shortRunLength.clear();
        decodedCharactersWritten = 0;
        while (shortRunLength.length() < stringLength) {
            shortRunLength.append(substring);
            decodedCharactersWritten += threshold;
        }
    }
    void TearDown(::benchmark::State &state) override {}

    string shortRunLength;
    size_t decodedCharactersWritten = 0;
};

BENCHMARK_DEFINE_F(run_length_decode_,
                   scalarCharPtrTwoPassBM)(benchmark::State &s) {
    auto input = run_length_decode_::shortRunLength.c_str();
    for (auto _ : s) {
        auto ans = runLengthDecode::decodeScalarTwoPass(input);
        benchmark::DoNotOptimize(ans);
        delete[] ans;
    }
    s.counters["DecodedCharactersWritten/s"] =
        benchmark::Counter(static_cast<double>(s.iterations()) *
                               static_cast<double>(decodedCharactersWritten),
                           benchmark::Counter::kIsRate);
}

BENCHMARK_DEFINE_F(run_length_decode_,
                   scalarCharPtrSinglePassBM)(benchmark::State &s) {
    auto input = run_length_decode_::shortRunLength.c_str();
    for (auto _ : s) {
        auto ans = runLengthDecode::decodeScalarSinglePass(input);
        benchmark::DoNotOptimize(ans);
        delete[] ans;
    }
    s.counters["DecodedCharactersWritten/s"] =
        benchmark::Counter(static_cast<double>(s.iterations()) *
                               static_cast<double>(decodedCharactersWritten),
                           benchmark::Counter::kIsRate);
}

BENCHMARK_DEFINE_F(run_length_decode_,
                   scalarStdStringSinglePassBM)(benchmark::State &s) {
    string input = run_length_decode_::shortRunLength;
    for (auto _ : s) {
        auto ans = runLengthDecode::decodeScalarSinglePass(input);
        benchmark::DoNotOptimize(ans);
    }
    s.counters["DecodedCharactersWritten/s"] =
        benchmark::Counter(static_cast<double>(s.iterations()) *
                               static_cast<double>(decodedCharactersWritten),
                           benchmark::Counter::kIsRate);
}

#ifdef __ARM_FEATURE_SVE
BENCHMARK_DEFINE_F(run_length_decode_,
                   sveCharPtrTwoPassBM)(benchmark::State &s) {
    auto input = run_length_decode_::shortRunLength.c_str();
    for (auto _ : s) {
        auto ans = runLengthDecode::decodeSVETwoPass(input);
        benchmark::DoNotOptimize(ans);
        delete[] ans;
    }
    s.counters["DecodedCharactersWritten/s"] =
        benchmark::Counter(static_cast<double>(s.iterations()) *
                               static_cast<double>(decodedCharactersWritten),
                           benchmark::Counter::kIsRate);
}

BENCHMARK_DEFINE_F(run_length_decode_,
                   sveCharPtrSinglePassBM)(benchmark::State &s) {
    auto input = run_length_decode_::shortRunLength.c_str();
    for (auto _ : s) {
        auto ans = runLengthDecode::decodeSVESinglePass(input);
        benchmark::DoNotOptimize(ans);
        delete[] ans;
    }
    s.counters["DecodedCharactersWritten/s"] =
        benchmark::Counter(static_cast<double>(s.iterations()) *
                               static_cast<double>(decodedCharactersWritten),
                           benchmark::Counter::kIsRate);
}
#endif // __ARM_FEATURE_SVE

BENCHMARK_REGISTER_F(run_length_decode_, scalarCharPtrTwoPassBM)
    ->ArgsProduct({{1, 4, 16, 64}, {512}});

BENCHMARK_REGISTER_F(run_length_decode_, scalarCharPtrSinglePassBM)
    ->ArgsProduct({{1, 4, 16, 64}, {512}});

BENCHMARK_REGISTER_F(run_length_decode_, scalarStdStringSinglePassBM)
    ->ArgsProduct({{1, 4, 16, 64}, {512}});

#ifdef __ARM_FEATURE_SVE
BENCHMARK_REGISTER_F(run_length_decode_, sveCharPtrTwoPassBM)
    ->ArgsProduct({{1, 4, 16, 64}, {512}});

BENCHMARK_REGISTER_F(run_length_decode_, sveCharPtrSinglePassBM)
    ->ArgsProduct({{1, 4, 16, 64}, {512}});
#endif // __ARM_FEATURE_SVE

} // namespace
