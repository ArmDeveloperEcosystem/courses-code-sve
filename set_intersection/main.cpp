// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "set_intersection.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <iostream>

using SetIntersection::Value;
using std::array;
using std::cout;
using std::size_t;

template <size_t N>
void printValues(const array<Value, N> &values, size_t count) {
    for (size_t i = 0; i < count; ++i)
        cout << values[i] << ' ';
    cout << '\n';
}

int main() {
    constexpr array<Value, 8> lhs{1, 4, 5, 8, 13, 21, 34, 55};
    constexpr array<Value, 11> rhs{0, 1, 2, 3, 5, 8, 13, 21, 35, 55, 89};
    array<Value, std::min(lhs.size(), rhs.size())> output{};

    auto output_count = SetIntersection::intersect_scalar(
        lhs.data(), lhs.size(), rhs.data(), rhs.size(), output.data());
    cout << "Scalar intersection count: " << output_count << '\n';
    printValues(output, output_count);

    output_count = SetIntersection::intersect_restrict(
        lhs.data(), lhs.size(), rhs.data(), rhs.size(), output.data());
    cout << "Restrict intersection count: " << output_count << '\n';
    printValues(output, output_count);

#ifdef __ARM_FEATURE_SVE
    output_count = SetIntersection::intersect_sve_probe(
        lhs.data(), lhs.size(), rhs.data(), rhs.size(), output.data());
    cout << "SVE probe intersection count: " << output_count << '\n';
    printValues(output, output_count);

    output_count = SetIntersection::intersect_sve_jammed(
        lhs.data(), lhs.size(), rhs.data(), rhs.size(), output.data());
    cout << "SVE jammed intersection count: " << output_count << '\n';
    printValues(output, output_count);
#else
    cout
        << "SVE implementations are only built for AArch64 targets with SVE.\n";
#endif

    return EXIT_SUCCESS;
}
