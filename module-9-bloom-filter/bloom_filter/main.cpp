// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on

#include "bloom.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string_view>
#include <vector>

#ifdef __ARM_FEATURE_SVE
#include <arm_sve.h>
#endif

using std::cerr;
using std::cout;
using std::size_t;
using std::string_view;
using std::unique_ptr;
using std::vector;

template <typename FilterType> std::unique_ptr<Bloom::Filter> make_filter() {
    return std::make_unique<FilterType>();
}

struct Variant {
    string_view name;
    unique_ptr<Bloom::Filter> (*create)();
};

const vector<Variant> variants{
    {"64-bit scalar", make_filter<Bloom::BloomFilterScalar64>},
    {"32-bit scalar", make_filter<Bloom::BloomFilterScalar32>},
    {"64-bit scalar partitioned",
     make_filter<Bloom::BloomFilterScalar64Partitioned>},
    {"32-bit scalar partitioned",
     make_filter<Bloom::BloomFilterScalar32Partitioned>},
#ifdef __ARM_FEATURE_SVE
    {"64-bit SVE", make_filter<Bloom::BloomFilterSVE64>},
    {"32-bit SVE", make_filter<Bloom::BloomFilterSVE32>},
    {"64-bit SVE partitioned", make_filter<Bloom::BloomFilterSVE64Partitioned>},
    {"32-bit SVE partitioned", make_filter<Bloom::BloomFilterSVE32Partitioned>},
#endif
};

int main(int argc, char *argv[]) {

    if (argc == 1) {
        cout << "Bloom bits: " << BLOOM_BITS << '\n'
             << "Bloom hashes k: " << BLOOM_NUM_HASHES << " fixed hashes\n"
             << "Partitioned slice bits per hash: " << BLOOM_SLICE_BITS << '\n'
             << "Number of pre-calculated keys: " << BLOOM_NUM_PRECALC_KEYS
             << '\n'
             << "Maximum length of each key: " << BLOOM_MAX_KEY_LENGTH << '\n';
#ifdef __ARM_FEATURE_SVE
        cout << "Scalable Vector Length: " << 64 * svcntd() << " bits\n"
             << "Number of 64-bit SVE lanes: " << svcntd() << '\n'
             << "Number of 32-bit SVE lanes: " << svcntw() << '\n';
#else
        cout << "No SVE support: __ARM_FEATURE_SVE is not defined\n";
#endif
        cout << "\nAvailable Bloom filter variants:\n";
        for (size_t i = 0; i < variants.size(); ++i)
            cout << "  " << i << ": " << variants[i].name << '\n';

        cout << "\nUsage: " << argv[0] << " <variant> [key ...]\n";
        return EXIT_SUCCESS;
    }

    size_t variant = 0;
    size_t parsed_characters = 0;
    try {
        variant = std::stoul(argv[1], &parsed_characters);
    } catch (const std::exception &e) {
        cerr << "Invalid Bloom filter variant: " << argv[1]
             << ". Run without arguments to list available variants.\n";
        return EXIT_FAILURE;
    }

    if (parsed_characters != string_view(argv[1]).size() ||
        variant >= variants.size()) {
        cerr << "Invalid Bloom filter variant: " << argv[1]
             << ". Run without arguments to list available variants.\n";
        return EXIT_FAILURE;
    }

    // Create the selected Bloom filter variant.
    auto filter = variants[variant].create();
    cout << "Bloom filter variant " << variant << ": " << variants[variant].name
         << '\n';

    if (argc > 2) {
        cout << "Inserting " << (argc - 2) << " keys:\n";
        for (int i = 2; i < argc; ++i) {
            const Bloom::Key_t key = argv[i];
            cout << " - \"" << key << "\": ";
            if (filter->maybe_contains(key)) {
                cout << "possibly present, insertion skipped\n";
            } else {
                filter->add(key);
                cout << "absent, inserted\n";
            }
        }
    } else {
        cout << "No keys provided for insertion.\n";
    }

    return EXIT_SUCCESS;
}
