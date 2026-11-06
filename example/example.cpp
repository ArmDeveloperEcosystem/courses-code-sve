// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "example.h"

extern "C" int example_add_one_asm(int value);

namespace example {

int add_one(int value) { return example_add_one_asm(value); }

bool foo(bool bar) { return !bar; }

} // namespace example
