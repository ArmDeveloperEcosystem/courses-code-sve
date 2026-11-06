// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on

#ifndef SVE_VECTOR_KERNELS_H
#define SVE_VECTOR_KERNELS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void sve_vector_addition_i32(int32_t *dst, const int32_t *x, const int32_t *y,
                             size_t count);
size_t base64_sve_encode(const uint8_t *input, size_t input_len, char *output);
void sve2_histogram_u32(uint32_t *histogram, const uint32_t *records,
                        size_t count);
#ifdef __cplusplus
}
#endif

#endif
