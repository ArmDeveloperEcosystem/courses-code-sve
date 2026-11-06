# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
# SPDX-License-Identifier: BSD-3-Clause-Clear

set(BLOOM_BITS
    "(1 << 23)"
    CACHE STRING "Size of bloom filter in bits")

set(BLOOM_NUM_HASHES
    "64"
    CACHE STRING "Number of hash functions to use in bloom filter")

set(BLOOM_NUM_PRECALC_KEYS
    "50000"
    CACHE STRING "Number of pre-calculated keys to generate for testing or benchmarking")

set(BLOOM_MAX_KEY_LENGTH
    "96"
    CACHE STRING "Maximum length in characters of pseudo-random strings generated to test the bloom filter")

stbc_add_task(
  LIB_SOURCES
    bloom.h
    bloom_config.h
    bloom.cpp
    bloom_util.cpp
  DEFINES
    BLOOM_BITS=${BLOOM_BITS}
    BLOOM_NUM_HASHES=${BLOOM_NUM_HASHES}
    BLOOM_NUM_PRECALC_KEYS=${BLOOM_NUM_PRECALC_KEYS}
    BLOOM_MAX_KEY_LENGTH=${BLOOM_MAX_KEY_LENGTH}
)
