// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "example.h"

#include <cstdlib>
#include <iostream>

using std::cout;

int main(int argc, char *argv[]) {
    cout << "Hello, World! This is the example task\n";
    cout << "add_one(41) = " << example::add_one(41) << '\n';
    cout << "foo(true) = " << example::foo(true) << '\n';
    cout << "foo(false) = " << example::foo(false) << '\n';
    return EXIT_SUCCESS;
}
