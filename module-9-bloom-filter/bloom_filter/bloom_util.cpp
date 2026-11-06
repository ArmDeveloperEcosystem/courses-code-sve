// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "bloom_util.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

using std::size_t;
using std::string;
using std::uint64_t;
using std::vector;

namespace {
uint64_t lcg_next(uint64_t &s) {
    s = (s * 6364136223846793005ULL) + 1442695040888963407ULL;
    return s;
}

// seed is passed by value and can be updated without affecting the caller.
string make_key(uint64_t seed, size_t cap) {
    size_t len = 1 + (size_t)(lcg_next(seed) % cap);
    string key;
    key.reserve(len);

    for (size_t i = 0; i < len; ++i)
        key += (char)('a' + (lcg_next(seed) % 26u));

    return key;
}
} // namespace

namespace Bloom {
vector<string> build_keys(size_t nkeys, size_t max_key_length) {
    vector<string> keys;
    keys.reserve(nkeys);
    for (size_t i = 0; i < nkeys; ++i)
        keys.push_back(make_key(0x12345678ULL + i, max_key_length));
    return keys;
}
} // namespace Bloom
