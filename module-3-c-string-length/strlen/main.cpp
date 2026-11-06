// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "strlen.h"

#include <cstdlib>
#include <iostream>
#include <string>

using std::cout;
using std::string;

int main(int argc, char *argv[]) {

    string s = "hello world";
    auto ans = str::strlen(s.c_str());
    cout << ans << '\n';

#ifdef __ARM_FEATURE_SVE
    auto ans2 = str::strlenSVENaive(s.c_str());
    cout << ans2 << '\n';
#endif

    if (s.length() == ans)
        cout << "CORRECT LENGTH" << '\n';

    return EXIT_SUCCESS;
}
