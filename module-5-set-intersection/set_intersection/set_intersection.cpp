// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "set_intersection.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

#if defined(__aarch64__)
#include <arm_sve.h>
#else
#warning Platform is not AArch64: no SVE-accelerated implementations will be available !
#endif

using std::size_t;
using std::uint64_t;

namespace SetIntersection {
namespace {

#ifdef __ARM_FEATURE_SVE
struct OrderedInputs {
    const Value *needles;
    size_t needleCount;
    const Value *haystack;
    size_t haystackCount;
};

OrderedInputs orderByProbeCount(const Value *lhs, size_t lhs_count,
                                const Value *rhs, size_t rhs_count) noexcept {
    if (lhs_count <= rhs_count)
        return {lhs, lhs_count, rhs, rhs_count};
    return {rhs, rhs_count, lhs, lhs_count};
}
#endif

} // namespace

size_t intersect_scalar(const Value *lhs, size_t lhs_count, const Value *rhs,
                        size_t rhs_count, Value *output) noexcept {
    size_t lhs_pos = 0;
    size_t rhs_pos = 0;
    size_t output_count = 0;

    while (lhs_pos < lhs_count && rhs_pos < rhs_count) {
        const auto lhs_value = lhs[lhs_pos];
        const auto rhs_value = rhs[rhs_pos];

        if (lhs_value == rhs_value) {
            output[output_count] = lhs_value;
            ++output_count;
            ++lhs_pos;
            ++rhs_pos;
            continue;
        }

        if (lhs_value < rhs_value) {
            ++lhs_pos;
        } else {
            ++rhs_pos;
        }
    }

    return output_count;
}

size_t intersect_restrict(const Value *RESTRICT lhs, size_t lhs_count,
                          const Value *RESTRICT rhs, size_t rhs_count,
                          Value *RESTRICT output) noexcept {
    size_t lhs_pos = 0;
    size_t rhs_pos = 0;
    size_t output_count = 0;

    while (lhs_pos < lhs_count && rhs_pos < rhs_count) {
        const auto lhs_value = lhs[lhs_pos];
        const auto rhs_value = rhs[rhs_pos];

        if (lhs_value == rhs_value) {
            output[output_count] = lhs_value;
            ++output_count;
            ++lhs_pos;
            ++rhs_pos;
            continue;
        }

        if (lhs_value < rhs_value) {
            ++lhs_pos;
        } else {
            ++rhs_pos;
        }
    }

    return output_count;
}

#ifdef __ARM_FEATURE_SVE
size_t intersect_sve_probe(const Value *lhs, size_t lhs_count, const Value *rhs,
                           size_t rhs_count, Value *output) noexcept {
    const auto inputs = orderByProbeCount(lhs, lhs_count, rhs, rhs_count);
    const auto *needles = inputs.needles;
    const auto *haystack = inputs.haystack;
    const auto needle_count = inputs.needleCount;
    const auto haystack_count = inputs.haystackCount;

    if (needle_count == 0 || haystack_count == 0)
        return 0;

    const auto lanes = static_cast<size_t>(svcntw());
    size_t haystack_pos = 0;
    size_t output_count = 0;

    for (size_t needle_pos = 0; needle_pos < needle_count; ++needle_pos) {
        const auto value = needles[needle_pos];

        while (haystack_pos + lanes <= haystack_count &&
               haystack[haystack_pos + lanes - 1] < value) {
            haystack_pos += lanes;
        }

        if (haystack_pos >= haystack_count ||
            haystack[haystack_count - 1] < value) {
            break;
        }

        if (haystack[haystack_pos] > value)
            continue;

        const auto active =
            svwhilelt_b32(static_cast<uint64_t>(haystack_pos),
                          static_cast<uint64_t>(haystack_count));
        const auto haystack_values = svld1_u32(active, haystack + haystack_pos);
        const auto equal = svcmpeq_n_u32(active, haystack_values, value);

        if (svptest_any(active, equal)) {
            output[output_count] = value;
            ++output_count;

            const auto before_match = svbrkb_b_z(active, equal);
            haystack_pos +=
                static_cast<size_t>(svcntp_b32(before_match, before_match)) + 1;
        }
    }

    return output_count;
}

size_t intersect_sve_jammed(const Value *lhs, size_t lhs_count,
                            const Value *rhs, size_t rhs_count,
                            Value *output) noexcept {
    const auto inputs = orderByProbeCount(lhs, lhs_count, rhs, rhs_count);
    const auto *needles = inputs.needles;
    const auto *haystack = inputs.haystack;
    const auto needle_count = inputs.needleCount;
    const auto haystack_count = inputs.haystackCount;

    if (needle_count == 0 || haystack_count == 0)
        return 0;

    const auto lanes = static_cast<size_t>(svcntw());
    size_t needle_pos = 0;
    size_t haystack_pos = 0;
    size_t output_count = 0;

    while (needle_pos < needle_count && haystack_pos < haystack_count) {
        const auto first_needle = needles[needle_pos];
        while (haystack_pos + lanes <= haystack_count &&
               haystack[haystack_pos + lanes - 1] < first_needle) {
            haystack_pos += lanes;
        }

        if (haystack_pos >= haystack_count)
            break;

        const auto chunk_end = std::min(haystack_pos + lanes, haystack_count);
        const auto chunk_first = haystack[haystack_pos];
        const auto chunk_last = haystack[chunk_end - 1];

        if (chunk_last < first_needle)
            break;

        while (needle_pos < needle_count && needles[needle_pos] < chunk_first) {
            ++needle_pos;
        }

        if (needle_pos >= needle_count)
            break;

        if (needles[needle_pos] > chunk_last) {
            haystack_pos = chunk_end;
            continue;
        }

        const auto active =
            svwhilelt_b32(static_cast<uint64_t>(haystack_pos),
                          static_cast<uint64_t>(haystack_count));
        const auto haystack_values = svld1_u32(active, haystack + haystack_pos);

        while (needle_pos < needle_count) {
            const auto value = needles[needle_pos];
            if (value > chunk_last)
                break;

            const auto equal = svcmpeq_n_u32(active, haystack_values, value);
            if (svptest_any(active, equal)) {
                output[output_count] = value;
                ++output_count;
            }
            ++needle_pos;
        }

        haystack_pos = chunk_end;
    }

    return output_count;
}
#else
#warning SVE not available on this target: set_intersection will have no SVE implementation !
#endif // __ARM_FEATURE_SVE

} // namespace SetIntersection
