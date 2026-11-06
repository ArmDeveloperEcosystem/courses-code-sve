// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "histogram.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using std::uint32_t;
using std::uint8_t;

using std::array;
using std::cerr;
using std::cout;
using std::string;
using std::vector;

constexpr uint32_t kHistogramBins = 256;
constexpr double kTargetSeconds = 2.0;
constexpr double kLowerTargetSeconds = 1.8;
constexpr uint32_t kDefaultRecords = 10'000'000;
constexpr uint32_t kMaxPasses = 256;

void print_usage(const char *prog) {
    cerr << "Usage: " << prog << " <impl> [num_records] [seed]\n"
         << "  <impl>        : scalar | sve | sve2 | sve2-unrolled\n"
         << "  [num_records] : number of samples (default " << kDefaultRecords
         << ")\n"
         << "  [seed]        : RNG seed (default 42)\n\n"
         << "Data model: clipped Gaussian over unary bins [0,255], centered at "
            "24.\n"
         << "Examples:\n"
         << "  " << prog << " scalar\n"
         << "  " << prog << " sve2-unrolled 12000000 7\n";
}

void print_histogram_raw(const array<uint32_t, kHistogramBins> &hist) {
    for (uint32_t bin = 0; bin < kHistogramBins; ++bin) {
        cout << bin << ' ' << hist[bin] << '\n';
    }
}

vector<uint8_t> make_gaussian_data(uint32_t n, uint32_t seed) {
    std::mt19937 rng(seed);
    std::normal_distribution<double> gauss(24.0, 8.0);
    vector<uint8_t> data(n);
    for (auto &v : data) {
        const double sample = std::clamp(gauss(rng), 0.0, 255.0);
        v = static_cast<uint8_t>(sample);
    }
    return data;
}

bool run_impl(const string &impl, array<uint32_t, kHistogramBins> &hist,
              vector<uint8_t> &data) {
    if (impl == "scalar") {
        histogram::calc_histogram<uint8_t>(hist.data(), data.data(),
                                           data.size());
        return true;
    }
    if (impl == "sve") {
#ifdef __ARM_FEATURE_SVE
        histogram::calc_histogram_sve(hist.data(), data.data(), data.size());
        return true;
#else
        cerr << "Requested 'sve' but SVE is not enabled on this target.\n";
        return false;
#endif
    }
    if (impl == "sve2") {
#ifdef __ARM_FEATURE_SVE2
        histogram::calc_histogram_sve2<uint8_t>(hist.data(), data.data(),
                                                data.size());
        return true;
#else
        cerr << "Requested 'sve2' but SVE2 is not enabled on this target.\n";
        return false;
#endif
    }
    if (impl == "sve2-unrolled") {
#ifdef __ARM_FEATURE_SVE2
        histogram::calc_histogram_sve2_unrolled<uint8_t>(
            hist.data(), data.data(), data.size());
        return true;
#else
        cerr << "Requested 'sve2-unrolled' but SVE2 is not enabled on this "
                "target.\n";
        return false;
#endif
    }
    cerr << "Unknown implementation: " << impl << '\n';
    return false;
}

int main(int argc, char **argv) {
    if (argc < 2 || argc > 4) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    const string impl = argv[1];
    uint32_t num_records = kDefaultRecords;
    uint32_t seed = 42;

    try {
        if (argc >= 3) {
            num_records = static_cast<uint32_t>(std::stoul(argv[2]));
        }
        if (argc == 4) {
            seed = static_cast<uint32_t>(std::stoul(argv[3]));
        }
    } catch (...) {
        cerr << "Invalid numeric argument.\n";
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (num_records == 0) {
        cerr << "num_records must be > 0.\n";
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    vector<uint8_t> data = make_gaussian_data(num_records, seed);
    array<uint32_t, kHistogramBins> histogram_bins{};

    const auto start = std::chrono::steady_clock::now();
    uint32_t passes = 0;
    while (passes < kMaxPasses) {
        histogram_bins.fill(0);
        if (!run_impl(impl, histogram_bins, data)) {
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
        ++passes;
        const auto now = std::chrono::steady_clock::now();
        const double elapsed =
            std::chrono::duration<double>(now - start).count();
        if (elapsed >= kLowerTargetSeconds) {
            break;
        }
    }

    const auto end = std::chrono::steady_clock::now();
    const double elapsed_total =
        std::chrono::duration<double>(end - start).count();
    uint32_t nonzero_bins = 0;
    for (uint32_t v : histogram_bins) {
        nonzero_bins += (v != 0u);
    }

    cout << std::fixed << std::setprecision(3) << "impl=" << impl
         << " samples=" << num_records << " seed=" << seed
         << " passes=" << passes << " elapsed_s=" << elapsed_total
         << " target_s~" << kTargetSeconds << " nonzero_bins=" << nonzero_bins
         << '\n';
    print_histogram_raw(histogram_bins);

    return EXIT_SUCCESS;
}