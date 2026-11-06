// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#pragma once

#ifndef BLOOM_BITS
#define BLOOM_BITS (1 << 23)
#endif

#ifndef BLOOM_NUM_HASHES
#define BLOOM_NUM_HASHES 64
#endif

#ifndef BLOOM_NUM_PRECALC_KEYS
#define BLOOM_NUM_PRECALC_KEYS 50000
#endif

#ifndef BLOOM_MAX_KEY_LENGTH
#define BLOOM_MAX_KEY_LENGTH 96
#endif

// Storage dimensions are derived from the total bit count. Partitioned
// variants give each hash an equal slice; masks select positions within
// the power-of-two filter or slice.
#define BLOOM_WORD_BITS 64
#define BLOOM_WORDS (BLOOM_BITS / BLOOM_WORD_BITS)
#define BLOOM_BYTES (BLOOM_WORDS * 8)
#define BLOOM_MASK (BLOOM_BITS - 1)
#define BLOOM_SLICE_BITS (BLOOM_BITS / BLOOM_NUM_HASHES)
#define BLOOM_SLICE_MASK (BLOOM_SLICE_BITS - 1)
