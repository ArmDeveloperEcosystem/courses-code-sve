// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#pragma once

#include <cstddef>
#include <vector>
#include <string>

namespace Bloom {
// Generate repeatable pseudo-random keys for tests and benchmarks. This is
// workload generation, separate from the Bloom filter hash algorithm.
std::vector<std::string> build_keys(std::size_t nkeys,
                                    std::size_t max_key_length);
} // namespace Bloom
