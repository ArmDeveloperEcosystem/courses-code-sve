// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "SVE-task-based-course.h"

#include "bloom.h"

#include <array>
#include <cstdint>

#ifdef __aarch64__
#include <arm_sve.h>
#else
#warning Platform is not AArch64: SVE-accelerated implementations will not be available !
#endif

using std::array;
using std::size_t;
using std::uint32_t;
using std::uint64_t;

using Bloom::Key_t;

namespace {
// Both widths follow the same pipeline: hash the key bytes once, generate
// distinct seeds from that base, then mix each seed before selecting bits.
// Unsigned arithmetic deliberately wraps at the chosen hash width.
template <typename T> struct BloomBasicOps;
template <> struct BloomBasicOps<uint64_t> {
    static constexpr uint64_t golden = 0x9e3779b97f4a7c15ULL;
    static constexpr uint64_t c1 = 0xbf58476d1ce4e5b9ULL;
    static constexpr uint64_t c2 = 0x94d049bb133111ebULL;

    // FNV-1a is a serial recurrence: each byte depends on the previous state.
    static INLINE uint64_t fnv1a(Key_t key) {
        uint64_t h = 14695981039346656037ULL;
        for (const auto &c : key) {
            h ^= static_cast<uint64_t>(c);
            h *= 1099511628211ULL;
        }
        return h;
    }

    static INLINE uint64_t mix(uint64_t x) {
        x ^= x >> 30;
        x *= c1;
        x ^= x >> 27;
        x *= c2;
        x ^= x >> 31;
        return x;
    }

#ifdef __ARM_FEATURE_SVE
    // Apply the scalar mixing sequence independently to each active lane.
    // Inactive lanes are unspecified and must be ignored by callers.
    static INLINE svuint64_t mix(svbool_t pg, svuint64_t x) {
        x = sveor_u64_x(pg, x, svlsr_n_u64_x(pg, x, 30));
        x = svmul_n_u64_x(pg, x, c1);
        x = sveor_u64_x(pg, x, svlsr_n_u64_x(pg, x, 27));
        x = svmul_n_u64_x(pg, x, c2);
        x = sveor_u64_x(pg, x, svlsr_n_u64_x(pg, x, 31));
        return x;
    }
#endif

    static INLINE uint32_t hash(uint64_t base, uint64_t i) {
        return mix(base + golden * (i + 1u));
    }
};

template <> struct BloomBasicOps<uint32_t> {
    static constexpr uint32_t golden = 0x9e3779b9u;
    static constexpr uint32_t c1 = 0x7feb352du;
    static constexpr uint32_t c2 = 0x846ca68bu;

    static INLINE uint32_t fnv1a(Key_t key) {
        uint32_t h = 2166136261u;
        for (const auto &c : key) {
            h ^= static_cast<uint32_t>(c);
            h *= 16777619u;
        }
        return h;
    }

    static INLINE uint32_t mix(uint32_t x) {
        x ^= x >> 16;
        x *= c1;
        x ^= x >> 15;
        x *= c2;
        x ^= x >> 16;
        return x;
    }

#ifdef __ARM_FEATURE_SVE
    // Apply the scalar mixing sequence independently to each active lane.
    // Inactive lanes are unspecified and must be ignored by callers.
    static INLINE svuint32_t mix(svbool_t pg, svuint32_t x) {
        x = sveor_u32_x(pg, x, svlsr_n_u32_x(pg, x, 16));
        x = svmul_n_u32_x(pg, x, c1);
        x = sveor_u32_x(pg, x, svlsr_n_u32_x(pg, x, 15));
        x = svmul_n_u32_x(pg, x, c2);
        x = sveor_u32_x(pg, x, svlsr_n_u32_x(pg, x, 16));
        return x;
    }
#endif

    static INLINE uint32_t hash(uint32_t base, uint32_t i) {
        return mix(base + golden * (i + 1u));
    }
};

} // namespace

namespace Bloom {

// Unpartitioned variants map every hash into the complete filter bit space.
BitPositions_t hashScalar64(Key_t key) {
    BitPositions_t bits{};
    const uint64_t base = BloomBasicOps<uint64_t>::fnv1a(key);
    for (uint64_t i = 0; i < BLOOM_NUM_HASHES; ++i)
        bits[i] = BloomBasicOps<uint64_t>::hash(base, i) & BLOOM_MASK;
    return bits;
}

BitPositions_t hashScalar32(Key_t key) {
    BitPositions_t bits{};
    const auto base = BloomBasicOps<uint32_t>::fnv1a(key);
    for (uint32_t i = 0; i < BLOOM_NUM_HASHES; ++i)
        bits[i] = BloomBasicOps<uint32_t>::hash(base, i) & BLOOM_MASK;
    return bits;
}

// Partitioned variants map hash i into slice i, then add the slice offset
// to obtain a position in the complete filter.
BitPositions_t hashScalar64Partitioned(Key_t key) {
    BitPositions_t bits{};
    const auto base = BloomBasicOps<uint64_t>::fnv1a(key);
    for (uint32_t i = 0; i < BLOOM_NUM_HASHES; ++i) {
        const auto in_slice = BloomBasicOps<uint64_t>::hash(base, i);
        bits[i] = (i * BLOOM_SLICE_BITS) + (in_slice & BLOOM_SLICE_MASK);
    }
    return bits;
}

BitPositions_t hashScalar32Partitioned(Key_t key) {
    BitPositions_t bits{};
    const auto base = BloomBasicOps<uint32_t>::fnv1a(key);
    for (uint32_t i = 0; i < BLOOM_NUM_HASHES; ++i) {
        const auto in_slice = BloomBasicOps<uint32_t>::hash(base, i);
        bits[i] = (i * BLOOM_SLICE_BITS) + (in_slice & BLOOM_SLICE_MASK);
    }
    return bits;
}

#ifdef __ARM_FEATURE_SVE
// Vectorize the independent seeds and mixing steps for one key. FNV-1a stays
// scalar. Store the resulting positions for FilterWithHash to consume.
// Predication handles the final batch when it has fewer hashes than lanes.
BitPositions_t hashSVE32(Key_t key) {
    BitPositions_t bits{};

    const svuint32_t vec_golden = svdup_u32(BloomBasicOps<uint32_t>::golden);
    const svuint32_t vec_mask = svdup_u32(BLOOM_MASK);
    const svuint32_t vec_fnvbase =
        svdup_u32(BloomBasicOps<uint32_t>::fnv1a(key));

    for (uint32_t done = 0; done < BLOOM_NUM_HASHES;
         done += (uint32_t)svcntw()) {
        svbool_t pg = svwhilelt_b32(done, (uint32_t)BLOOM_NUM_HASHES);
        svuint32_t vec_lane = svindex_u32(done + 1, 1u);
        svuint32_t vec_x = svmla_u32_x(pg, vec_fnvbase, vec_lane, vec_golden);

        vec_x = BloomBasicOps<uint32_t>::mix(pg, vec_x);

        svuint32_t vec_pos_in_filter = svand_u32_x(pg, vec_x, vec_mask);

        svst1_u32(pg, &bits[done], vec_pos_in_filter);
    }

    return bits;
}

// The same pipeline uses 64-bit lanes, with narrowing stores into the
// 32-bit position array after masking to the filter bit range.
BitPositions_t hashSVE64(Key_t key) {
    BitPositions_t bits{};

    const svuint64_t vec_golden = svdup_u64(BloomBasicOps<uint64_t>::golden);
    const svuint64_t vec_mask = svdup_u64(BLOOM_MASK);
    const svuint64_t vec_fnvbase =
        svdup_u64(BloomBasicOps<uint64_t>::fnv1a(key));

    for (uint32_t done = 0; done < BLOOM_NUM_HASHES;
         done += (uint32_t)svcntd()) {
        svbool_t pg = svwhilelt_b64((uint64_t)done, (uint64_t)BLOOM_NUM_HASHES);
        svuint64_t vec_lane = svindex_u64((uint64_t)done + 1, 1u);
        svuint64_t vec_x = svmla_u64_x(pg, vec_fnvbase, vec_lane, vec_golden);

        vec_x = BloomBasicOps<uint64_t>::mix(pg, vec_x);

        svuint64_t vec_pos_in_filter = svand_u64_x(pg, vec_x, vec_mask);

        svst1w_u64(pg, &bits[done], vec_pos_in_filter);
    }

    return bits;
}

// Fused insertion: calculate positions, gather words, set bits, and scatter
// the updated words. Distinct word-aligned slices prevent collisions between
// active lanes within a single update; this is not a concurrent insertion API.
void BloomFilterSVE32Partitioned::add(Key_t key) noexcept {
    const uint32_t base = BloomBasicOps<uint32_t>::fnv1a(key);
    svuint32_t vec_golden = svdup_u32(BloomBasicOps<uint32_t>::golden);
    // Folding the first golden increment into base lets slice indices start at
    // 0.
    const svuint32_t vec_fnvbase =
        svdup_u32(base + BloomBasicOps<uint32_t>::golden);
    const svuint32_t vec_mask = svdup_u32(BLOOM_SLICE_MASK);
    const svuint32_t vec_ones = svdup_u32(1);

    // Access 32-bit halves of the stored 64-bit words to match the SVE lanes.
    // This bit numbering agrees with scalar access on little-endian targets.
    auto *words32 = reinterpret_cast<uint32_t *>(words.data());

    for (uint32_t done = 0; done < BLOOM_NUM_HASHES;
         done += (uint32_t)svcntw()) {
        svbool_t pg = svwhilelt_b32(done, (uint32_t)BLOOM_NUM_HASHES);
        svuint32_t vec_i = svindex_u32(done, 1u);
        svuint32_t vec_x = svmla_u32_x(pg, vec_fnvbase, vec_i, vec_golden);

        vec_x = BloomBasicOps<uint32_t>::mix(pg, vec_x);

        // Map the mixed hash into its own slice, then add the slice base
        // to obtain a bit position in the complete filter.
        svuint32_t vec_pos_in_slice = svand_u32_x(pg, vec_x, vec_mask);
        svuint32_t vec_slice_base = svmul_n_u32_x(pg, vec_i, BLOOM_SLICE_BITS);
        svuint32_t vec_pos_in_filter =
            svadd_u32_x(pg, vec_slice_base, vec_pos_in_slice);

        // Divide the bit position by 32 to select the storage word.
        svuint32_t vec_word_index = svlsr_n_u32_x(pg, vec_pos_in_filter, 5);

        // Keep the lower 5 bits for the offset within that word, then
        // shift a single 1 to build the mask used to set or test the bit.
        svuint32_t vec_bit_in_word = svand_n_u32_x(pg, vec_pos_in_filter, 31u);
        svuint32_t vec_bit_mask = svlsl_u32_x(pg, vec_ones, vec_bit_in_word);

        svuint32_t vec_current =
            svld1_gather_u32index_u32(pg, words32, vec_word_index);

        svuint32_t vec_updated = svorr_u32_x(pg, vec_current, vec_bit_mask);
        svst1_scatter_u32index_u32(pg, words32, vec_word_index, vec_updated);
    }
}

// Recompute the insertion positions and test the gathered bits. A missing
// bit in any active lane proves absence, so later batches can be skipped.
bool BloomFilterSVE32Partitioned::maybe_contains(Key_t key) const noexcept {
    const uint32_t base = BloomBasicOps<uint32_t>::fnv1a(key);
    const svuint32_t vec_golden = svdup_u32(BloomBasicOps<uint32_t>::golden);
    const svuint32_t vec_fnvbase =
        svdup_u32(base + BloomBasicOps<uint32_t>::golden);
    const svuint32_t vec_mask = svdup_u32(BLOOM_SLICE_MASK);
    const svuint32_t vec_ones = svdup_u32(1);

    // Access 32-bit halves of the stored 64-bit words to match the SVE lanes.
    // This bit numbering agrees with scalar access on little-endian targets.
    auto *words32 = reinterpret_cast<const uint32_t *>(words.data());

    for (uint32_t done = 0; done < BLOOM_NUM_HASHES;
         done += (uint32_t)svcntw()) {
        svbool_t pg = svwhilelt_b32(done, (uint32_t)BLOOM_NUM_HASHES);
        svuint32_t vec_i = svindex_u32(done, 1u);
        svuint32_t vec_x = svmla_u32_x(pg, vec_fnvbase, vec_i, vec_golden);

        vec_x = BloomBasicOps<uint32_t>::mix(pg, vec_x);

        // Map the mixed hash into its own slice, then add the slice base
        // to obtain a bit position in the complete filter.
        svuint32_t vec_pos_in_slice = svand_u32_x(pg, vec_x, vec_mask);
        svuint32_t vec_slice_base = svmul_n_u32_x(pg, vec_i, BLOOM_SLICE_BITS);
        svuint32_t vec_pos_in_filter =
            svadd_u32_x(pg, vec_slice_base, vec_pos_in_slice);

        // Divide the bit position by 32 to select the storage word.
        svuint32_t vec_word_index = svlsr_n_u32_x(pg, vec_pos_in_filter, 5);

        // Keep the lower 5 bits for the offset within that word, then
        // shift a single 1 to build the mask used to set or test the bit.
        svuint32_t vec_bit_in_word = svand_n_u32_x(pg, vec_pos_in_filter, 31u);
        svuint32_t vec_bit_mask = svlsl_u32_x(pg, vec_ones, vec_bit_in_word);

        svuint32_t vec_current =
            svld1_gather_u32index_u32(pg, words32, vec_word_index);

        svuint32_t vec_hit = svand_u32_x(pg, vec_current, vec_bit_mask);
        svbool_t vec_fail = svcmpeq_n_u32(pg, vec_hit, 0);
        if (svptest_any(pg, vec_fail))
            return false;
    }

    return true;
}

// The 64-bit variant accesses the native storage words directly. Each
// lane still targets a distinct slice, making the scatter update safe.
void BloomFilterSVE64Partitioned::add(Key_t key) noexcept {
    const uint64_t base = BloomBasicOps<uint64_t>::fnv1a(key);
    const svuint64_t vec_golden = svdup_u64(BloomBasicOps<uint64_t>::golden);
    const svuint64_t vec_fnvbase =
        svdup_u64(base + BloomBasicOps<uint64_t>::golden);
    const svuint64_t vec_mask = svdup_u64(BLOOM_SLICE_MASK);
    const svuint64_t vec_ones = svdup_u64(1);

    for (uint32_t done = 0; done < BLOOM_NUM_HASHES;
         done += (uint32_t)svcntd()) {
        svbool_t pg = svwhilelt_b64((uint64_t)done, (uint64_t)BLOOM_NUM_HASHES);
        svuint64_t vec_i = svindex_u64((uint64_t)done, 1u);
        svuint64_t vec_x = svmla_u64_x(pg, vec_fnvbase, vec_i, vec_golden);

        vec_x = BloomBasicOps<uint64_t>::mix(pg, vec_x);

        // Map the mixed hash into its own slice, then add the slice base
        // to obtain a bit position in the complete filter.
        svuint64_t vec_pos_in_slice = svand_u64_x(pg, vec_x, vec_mask);
        svuint64_t vec_slice_base = svmul_n_u64_x(pg, vec_i, BLOOM_SLICE_BITS);
        svuint64_t vec_pos_in_filter =
            svadd_u64_x(pg, vec_slice_base, vec_pos_in_slice);

        // Divide the bit position by 64 to select the storage word.
        svuint64_t vec_word_index = svlsr_n_u64_x(pg, vec_pos_in_filter, 6);

        // Keep the lower 6 bits for the offset within that word, then
        // shift a single 1 to build the mask used to set or test the bit.
        svuint64_t vec_bit_in_word = svand_n_u64_x(pg, vec_pos_in_filter, 63u);
        svuint64_t vec_bit_mask = svlsl_u64_x(pg, vec_ones, vec_bit_in_word);

        svuint64_t vec_current =
            svld1_gather_u64index_u64(pg, words.data(), vec_word_index);

        svuint64_t vec_updated = svorr_u64_x(pg, vec_current, vec_bit_mask);
        svst1_scatter_u64index_u64(pg, words.data(), vec_word_index,
                                   vec_updated);
    }
}

// Lookup follows the same position calculation as insertion and returns
// early when any gathered word lacks its requested bit.
bool BloomFilterSVE64Partitioned::maybe_contains(Key_t key) const noexcept {
    const uint64_t base = BloomBasicOps<uint64_t>::fnv1a(key);
    const svuint64_t vec_golden = svdup_u64(BloomBasicOps<uint64_t>::golden);
    const svuint64_t vec_fnvbase =
        svdup_u64(base + BloomBasicOps<uint64_t>::golden);
    const svuint64_t vec_mask = svdup_u64(BLOOM_SLICE_MASK);
    const svuint64_t vec_ones = svdup_u64(1);

    for (uint32_t done = 0; done < BLOOM_NUM_HASHES;
         done += (uint32_t)svcntd()) {
        svbool_t pg = svwhilelt_b64((uint64_t)done, (uint64_t)BLOOM_NUM_HASHES);
        svuint64_t vec_i = svindex_u64((uint64_t)done, 1u);
        svuint64_t vec_x = svmla_u64_x(pg, vec_fnvbase, vec_i, vec_golden);

        vec_x = BloomBasicOps<uint64_t>::mix(pg, vec_x);

        // Map the mixed hash into its own slice, then add the slice base
        // to obtain a bit position in the complete filter.
        svuint64_t vec_pos_in_slice = svand_u64_x(pg, vec_x, vec_mask);
        svuint64_t vec_slice_base = svmul_n_u64_x(pg, vec_i, BLOOM_SLICE_BITS);
        svuint64_t vec_pos_in_filter =
            svadd_u64_x(pg, vec_slice_base, vec_pos_in_slice);

        // Divide the bit position by 64 to select the storage word.
        svuint64_t vec_word_index = svlsr_n_u64_x(pg, vec_pos_in_filter, 6);

        // Keep the lower 6 bits for the offset within that word, then
        // shift a single 1 to build the mask used to set or test the bit.
        svuint64_t vec_bit_in_word = svand_n_u64_x(pg, vec_pos_in_filter, 63u);
        svuint64_t vec_bit_mask = svlsl_u64_x(pg, vec_ones, vec_bit_in_word);

        svuint64_t vec_current =
            svld1_gather_u64index_u64(pg, words.data(), vec_word_index);

        svuint64_t vec_hit = svand_u64_x(pg, vec_current, vec_bit_mask);
        svbool_t vec_fail = svcmpeq_n_u64(pg, vec_hit, 0);
        if (svptest_any(pg, vec_fail))
            return false;
    }

    return true;
}
#else
#warning SVE not available on this target: bloom_filter will have no SVE implementation !
#endif

} // namespace Bloom