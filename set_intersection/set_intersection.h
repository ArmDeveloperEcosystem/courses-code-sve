// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#pragma once

#include "SVE-task-based-course.h"

#include <cstddef>
#include <cstdint>

namespace SetIntersection {

using Value = std::uint32_t;

/// Intersect two sorted, unique sets and write the sorted intersection to
/// output. The output buffer must have room for min(lhs_count, rhs_count)
/// values.
std::size_t intersect_scalar(const Value *lhs, std::size_t lhs_count,
                             const Value *rhs, std::size_t rhs_count,
                             Value *output) noexcept;

/// Same algorithm as intersect_scalar(), but with a no-alias contract on all
/// arrays so learners can test whether aliasing information is enough to help
/// the compiler auto-vectorize the loop.
std::size_t intersect_restrict(const Value *RESTRICT lhs, std::size_t lhs_count,
                               const Value *RESTRICT rhs, std::size_t rhs_count,
                               Value *RESTRICT output) noexcept;

#ifdef __ARM_FEATURE_SVE
/// SVE implementation that probes the smaller set into vector chunks from the
/// larger set.
std::size_t intersect_sve_probe(const Value *lhs, std::size_t lhs_count,
                                const Value *rhs, std::size_t rhs_count,
                                Value *output) noexcept;

/// SVE implementation that jams all probes belonging to one loaded vector range
/// before advancing to the next vector chunk.
std::size_t intersect_sve_jammed(const Value *lhs, std::size_t lhs_count,
                                 const Value *rhs, std::size_t rhs_count,
                                 Value *output) noexcept;
#endif

} // namespace SetIntersection
