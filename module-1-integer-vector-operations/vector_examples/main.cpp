// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "vector_examples.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

using std::int32_t;

using std::cout;
using std::vector;

int main() {
    vector<int32_t> lhs{1, 2, 3, 4};
    vector<int32_t> rhs{5, 6, 7, 8};
    vector<int32_t> result(lhs.size());

    // Scalar vector addition.
    vector_add_scalar<int32_t>(lhs, rhs, result);

    cout << "Scalar vector add: ";
    for (int32_t value : result) {
        cout << value << " ";
    }
    cout << '\n';

    // Scalar min/max.
    auto [minimum, maximum] = min_max_scalar<int32_t>(result);

    cout << "Scalar min: " << minimum << '\n';
    cout << "Scalar max: " << maximum << '\n';

#ifdef __ARM_FEATURE_SVE
    // SVE vector addition.
    vector_add_sve<int32_t>(lhs, rhs, result);

    cout << "SVE vector add: ";
    for (int32_t value : result) {
        cout << value << " ";
    }
    cout << '\n';

    // SVE min/max.
    auto [sve_minimum, sve_maximum] = min_max_sve<int32_t>(result);

    cout << "SVE min: " << sve_minimum << '\n';
    cout << "SVE max: " << sve_maximum << '\n';
#endif

    return EXIT_SUCCESS;
}