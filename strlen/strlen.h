// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#pragma once

#include <cstddef>

namespace str {
std::size_t strlen(const char *s);

#ifdef __ARM_FEATURE_SVE
std::size_t strlenSVENaive(const char *s);
std::size_t strlenSVEImproved(const char *s);
std::size_t strlenSVESimdLoops(const char *s);
#endif
} // namespace str
