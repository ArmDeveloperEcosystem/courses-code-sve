// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#pragma once

#define INLINE __attribute__((always_inline)) inline

#if defined(__clang__) || defined(__GNUC__)
#define RESTRICT __restrict__
#else
#define RESTRICT
#endif