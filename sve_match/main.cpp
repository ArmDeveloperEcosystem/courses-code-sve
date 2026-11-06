// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "sve_match.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>

using std::size_t;
using std::uint16_t;
using std::uint8_t;

using std::array;
using std::cout;

template <sve_match::MatchElement T, size_t HaystackSize, size_t KeyCount>
void demonstrate(const array<T, HaystackSize> &haystack,
                 const array<T, KeyCount> &keys, const char *type_name) {
    const std::span<const T> haystack_span{haystack};
    const std::span<const T> key_span{keys};

    cout << type_name << " scalar: " << std::boolalpha
         << sve_match::search_scalar<T>(haystack_span, key_span) << '\n';

#ifdef __ARM_FEATURE_SVE2
    cout << type_name
         << " SVE2:   " << sve_match::search_sve2<T>(haystack_span, key_span)
         << '\n';
    cout << type_name << " SVE2 unrolled: "
         << sve_match::search_sve2_unrolled<T>(haystack_span, key_span) << '\n';
#else
    cout << type_name
         << " SVE2 implementations are unavailable on this target.\n";
#endif
}

int main() {
    constexpr array<uint8_t, 8> bytes{3, 8, 13, 21, 34, 55, 89, 144};
    constexpr array<uint8_t, 3> byte_keys{1, 34, 200};
    demonstrate(bytes, byte_keys, "uint8_t");

    constexpr array<uint16_t, 8> halfwords{100, 200, 300, 400,
                                           500, 600, 700, 800};
    constexpr array<uint16_t, 3> halfword_keys{42, 600, 900};
    demonstrate(halfwords, halfword_keys, "uint16_t");

    return EXIT_SUCCESS;
}
