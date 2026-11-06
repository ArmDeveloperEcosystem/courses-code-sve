// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "sve_match.h"
#include "SVE-task-based-course.h"

#include <array>
#include <cstddef>
#include <cstdint>

#ifdef __aarch64__
#include <arm_sve.h>
#else
#warning Platform is not AArch64: no SVE-accelerated implementations will be available !
#endif

using std::size_t;
using std::uint16_t;
using std::uint8_t;

using std::array;

namespace sve_match {
namespace {

template <MatchElement T>
[[nodiscard]] bool contains_scalar(std::span<const T> haystack,
                                   std::span<const T> keys) noexcept {
    for (const T value : haystack) {
        for (const T key : keys) {
            if (value == key) {
                return true;
            }
        }
    }
    return false;
}

#ifdef __ARM_FEATURE_SVE2
constexpr size_t MAX_SVE_VECTOR_BYTES = 256;
constexpr size_t MATCH_SEGMENT_BYTES = 16;

// Intrinsics such as svcnt* and svptrue* cannot select their element width from
// pointer arguments, so dispatch them explicitly from the C++ element type.
template <MatchElement T> INLINE size_t lane_count() noexcept {
    if constexpr (std::same_as<T, uint8_t>) {
        return svcntb();
    } else {
        return svcnth();
    }
}

template <MatchElement T> INLINE svbool_t all_lanes() noexcept {
    if constexpr (std::same_as<T, uint8_t>) {
        return svptrue_b8();
    } else {
        return svptrue_b16();
    }
}

template <MatchElement T> constexpr size_t keys_per_match_segment() noexcept {
    return MATCH_SEGMENT_BYTES / sizeof(T);
}

template <MatchElement T>
using KeyBuffer = array<T, MAX_SVE_VECTOR_BYTES / sizeof(T)>;

template <MatchElement T>
void prepare_key_buffer(KeyBuffer<T> &buffer, std::span<const T> keys,
                        size_t lanes) noexcept {
    // MATCH compares within independent 128-bit segments. Repeating the full
    // key set ensures that every segment contains every key.
    for (size_t lane = 0; lane < lanes; ++lane) {
        buffer[lane] = keys[lane % keys.size()];
    }
}
#endif

} // namespace

template <MatchElement T>
bool search_scalar(std::span<const T> haystack,
                   std::span<const T> keys) noexcept {
    return contains_scalar(haystack, keys);
}

template bool search_scalar<uint8_t>(std::span<const uint8_t>,
                                     std::span<const uint8_t>) noexcept;
template bool search_scalar<uint16_t>(std::span<const uint16_t>,
                                      std::span<const uint16_t>) noexcept;

#ifdef __ARM_FEATURE_SVE2
template <MatchElement T>
bool search_sve2(std::span<const T> haystack,
                 std::span<const T> keys) noexcept {
    if (haystack.empty() || keys.empty()) {
        return false;
    }
    if (keys.size() > keys_per_match_segment<T>()) {
        return contains_scalar(haystack, keys);
    }

    const size_t lanes = lane_count<T>();
    const svbool_t active = all_lanes<T>();
    alignas(64) KeyBuffer<T> key_buffer{};
    prepare_key_buffer(key_buffer, keys, lanes);
    const auto key_vector = svld1(active, key_buffer.data());

    size_t offset = 0;
    for (; haystack.size() - offset >= lanes; offset += lanes) {
        const auto values = svld1(active, haystack.data() + offset);
        const auto matches = svmatch(active, values, key_vector);
        if (svptest_any(active, matches)) {
            return true;
        }
    }

    return contains_scalar(haystack.subspan(offset), keys);
}

template <MatchElement T>
bool search_sve2_unrolled(std::span<const T> haystack,
                          std::span<const T> keys) noexcept {
    if (haystack.empty() || keys.empty()) {
        return false;
    }
    if (keys.size() > keys_per_match_segment<T>()) {
        return contains_scalar(haystack, keys);
    }

    const size_t lanes = lane_count<T>();
    const svbool_t active = all_lanes<T>();
    alignas(64) KeyBuffer<T> key_buffer{};
    prepare_key_buffer(key_buffer, keys, lanes);
    const auto key_vector = svld1(active, key_buffer.data());

    constexpr size_t UNROLL_FACTOR = 4;
    constexpr size_t PREFETCH_VECTORS = 16;
    const size_t unrolled_lanes = UNROLL_FACTOR * lanes;
    const size_t prefetch_distance = PREFETCH_VECTORS * lanes;

    size_t offset = 0;
    for (; haystack.size() - offset >= unrolled_lanes;
         offset += unrolled_lanes) {
        if (haystack.size() - offset > prefetch_distance) {
            __builtin_prefetch(haystack.data() + offset + prefetch_distance, 0,
                               0);
        }

        const auto values0 = svld1(active, haystack.data() + offset);
        const auto values1 = svld1(active, haystack.data() + offset + lanes);
        const auto values2 =
            svld1(active, haystack.data() + offset + 2 * lanes);
        const auto values3 =
            svld1(active, haystack.data() + offset + 3 * lanes);

        const auto matches0 = svmatch(active, values0, key_vector);
        const auto matches1 = svmatch(active, values1, key_vector);
        const auto matches2 = svmatch(active, values2, key_vector);
        const auto matches3 = svmatch(active, values3, key_vector);

        if (svptest_any(active, matches0) || svptest_any(active, matches1) ||
            svptest_any(active, matches2) || svptest_any(active, matches3)) {
            return true;
        }
    }

    for (; haystack.size() - offset >= lanes; offset += lanes) {
        const auto values = svld1(active, haystack.data() + offset);
        const auto matches = svmatch(active, values, key_vector);
        if (svptest_any(active, matches)) {
            return true;
        }
    }

    return contains_scalar(haystack.subspan(offset), keys);
}

template bool search_sve2<uint8_t>(std::span<const uint8_t>,
                                   std::span<const uint8_t>) noexcept;
template bool search_sve2<uint16_t>(std::span<const uint16_t>,
                                    std::span<const uint16_t>) noexcept;
template bool search_sve2_unrolled<uint8_t>(std::span<const uint8_t>,
                                            std::span<const uint8_t>) noexcept;
template bool
    search_sve2_unrolled<uint16_t>(std::span<const uint16_t>,
                                   std::span<const uint16_t>) noexcept;

#else
#warning SVE2 not available on this target: sve_match will have no SVE2 implementation !
#endif

} // namespace sve_match
