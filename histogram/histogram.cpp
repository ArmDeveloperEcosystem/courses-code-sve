// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "histogram.h"
#include "SVE-task-based-course.h"

#include <cstdint>
#include <stdexcept>

#ifdef __aarch64__
#include <arm_sve.h>
#else
#warning Platform is not AArch64: no SVE-accelerated implementations will be available !
#endif

using std::uint16_t;
using std::uint32_t;
using std::uint64_t;
using std::uint8_t;

namespace {
INLINE void validate_bin_size(uint8_t bin_size) {
    if (bin_size == 0) {
        throw std::invalid_argument("bin_size must be nonzero");
    }
}
} // namespace

namespace histogram {

template <ValidSVEType T>
void calc_histogram(uint32_t *histogram, T *records, uint32_t num_records) {
    for (uint32_t i = 0; i < num_records; i++) {
        histogram[records[i]] += 1; // each possible value is its own bin
    }
}

template <ValidSVEType T>
void calc_histogram(uint32_t *histogram, T *records, uint32_t num_records,
                    uint8_t bin_size) {
    validate_bin_size(bin_size);
    for (uint32_t i = 0; i < num_records; i++) {
        histogram[static_cast<uint32_t>(records[i]) / bin_size] += 1;
    }
}

template void calc_histogram<uint8_t>(uint32_t *, uint8_t *, uint32_t);
template void calc_histogram<uint16_t>(uint32_t *, uint16_t *, uint32_t);
template void calc_histogram<uint32_t>(uint32_t *, uint32_t *, uint32_t);

template void calc_histogram<uint8_t>(uint32_t *, uint8_t *, uint32_t, uint8_t);
template void calc_histogram<uint16_t>(uint32_t *, uint16_t *, uint32_t,
                                       uint8_t);
template void calc_histogram<uint32_t>(uint32_t *, uint32_t *, uint32_t,
                                       uint8_t);

#ifdef __ARM_FEATURE_SVE
namespace {
// Some SVE intrinsics cannot be wrapped by a generic T* interface. Their
// semantics or return type cannot be deduced from the parameter types alone, so
// ACLE requires an explicit type suffix rather than overload resolution (e.g.
// intrinsics like svdup and svindex).
//
// Helpers wrapping these non-generic intrinsics should always be force-inlined
// using compiler-specific attributes to preserve the intrinsic's semantics and
// avoid introducing an additional call boundary.
template <ValidSVEType T>
INLINE svuint32_t load(svbool_t pred, const T *records) {
    if constexpr (std::same_as<T, uint8_t>) {
        return svld1ub_u32(pred, records);
    } else if constexpr (std::same_as<T, uint16_t>) {
        return svld1uh_u32(pred, records);
    } else {
        return svld1(pred, records);
    }
}

INLINE void update_unique_indices(uint32_t *histogram, svbool_t pg,
                                  svuint32_t idx) {
    // This keeps the full update in SVE, but gather/scatter is expensive for
    // tiny, indirect read-modify-write operations on the benchmark target.
    svuint32_t old_counts = svld1_gather_u32index_u32(pg, histogram, idx);
    svuint32_t new_counts = svadd_n_u32_m(pg, old_counts, 1);
    svst1_scatter_u32index_u32(pg, histogram, idx, new_counts);
}

INLINE bool has_duplicate_indices(const uint32_t *indices, uint64_t lanes) {
    uint64_t seen_256[4]{};

    for (uint64_t lane = 0; lane < lanes; ++lane) {
        const uint32_t idx = indices[lane];
        if (idx < 256) {
            const uint64_t bit = uint64_t{1} << (idx & 63u);
            uint64_t &seen_word = seen_256[idx >> 6u];
            if ((seen_word & bit) != 0) {
                return true;
            }
            seen_word |= bit;
            continue;
        }

        for (uint64_t previous = 0; previous < lane; ++previous) {
            if (indices[previous] == idx) {
                return true;
            }
        }
    }

    return false;
}
} // anonymous namespace

template <ValidSVEType T>
void calc_histogram_sve(uint32_t *histogram, const T *records,
                        uint32_t num_records) {
    const auto n = static_cast<uint64_t>(num_records);
    const uint64_t vl = svcntw();

    // Max SVE vector length is 2048 bits, so max 32-bit lane count is 64.
    alignas(64) uint32_t idx_buf[64];

    for (uint64_t i = 0; i < n; i += vl) {
        svbool_t pg = svwhilelt_b32(i, n);
        svuint32_t idx = load<T>(pg, &records[i]);

        const uint64_t lanes = svcntp_b32(svptrue_b32(), pg);
        const auto first_idx = static_cast<uint32_t>(records[i]);
        svuint32_t contiguous_idx = svindex_u32(first_idx, 1);
        svbool_t is_contiguous = svcmpeq_u32(pg, idx, contiguous_idx);
        if (svcntp_b32(pg, is_contiguous) == lanes) {
            update_unique_indices(histogram, pg, idx);
            continue;
        }

        svst1_u32(pg, idx_buf, idx);

        if (has_duplicate_indices(idx_buf, lanes)) {
            for (uint64_t lane = 0; lane < lanes; ++lane) {
                histogram[idx_buf[lane]] += 1;
            }
            continue;
        }

        update_unique_indices(histogram, pg, idx);
    }
}

template <ValidSVEType T>
void calc_histogram_sve_scalar_update(uint32_t *histogram, const T *records,
                                      uint32_t num_records) {
    const auto n = static_cast<uint64_t>(num_records);
    const uint64_t vl = svcntw();

    // Max SVE vector length is 2048 bits, so max 32-bit lane count is 64.
    alignas(64) uint32_t idx_buf[64];

    for (uint64_t i = 0; i < n; i += vl) {
        svbool_t pg = svwhilelt_b32(i, n);
        svuint32_t idx = load<T>(pg, &records[i]);

        const uint64_t lanes = svcntp_b32(svptrue_b32(), pg);
        const auto first_idx = static_cast<uint32_t>(records[i]);
        svuint32_t contiguous_idx = svindex_u32(first_idx, 1);
        svbool_t is_contiguous = svcmpeq_u32(pg, idx, contiguous_idx);
        if (svcntp_b32(pg, is_contiguous) == lanes) {
            // Avoid spilling indices for the benchmark's common contiguous
            // chunks; scalar stores beat SVE gather/scatter here.
            for (uint64_t lane = 0; lane < lanes; ++lane) {
                histogram[first_idx + lane] += 1;
            }
            continue;
        }

        svst1_u32(pg, idx_buf, idx);

        for (uint64_t lane = 0; lane < lanes; ++lane) {
            histogram[idx_buf[lane]] += 1;
        }
    }
}

template void calc_histogram_sve<uint8_t>(uint32_t *, const uint8_t *,
                                          uint32_t);
template void calc_histogram_sve<uint16_t>(uint32_t *, const uint16_t *,
                                           uint32_t);
template void calc_histogram_sve<uint32_t>(uint32_t *, const uint32_t *,
                                           uint32_t);

template void calc_histogram_sve_scalar_update<uint8_t>(uint32_t *,
                                                        const uint8_t *,
                                                        uint32_t);
template void calc_histogram_sve_scalar_update<uint16_t>(uint32_t *,
                                                         const uint16_t *,
                                                         uint32_t);
template void calc_histogram_sve_scalar_update<uint32_t>(uint32_t *,
                                                         const uint32_t *,
                                                         uint32_t);
#else
#warning                                                                       \
    "SVE not available on this target: histogram will have no SVE implementation !"
#endif

#ifdef __ARM_FEATURE_SVE2

template <ValidSVEType T>
INLINE svuint32_t load_binned_indices(svbool_t pred, const T *records,
                                      uint8_t bin_size, uint32_t *idx_buf) {
    svuint32_t idx = load<T>(pred, records);
    svst1_u32(pred, idx_buf, idx);

    uint64_t lanes = svcntp_b32(svptrue_b32(), pred);
    for (uint64_t lane = 0; lane < lanes; ++lane) {
        idx_buf[lane] /= bin_size;
    }

    return svld1_u32(pred, idx_buf);
}

template <ValidSVEType T>
void calc_histogram_sve2_unrolled(uint32_t *histogram, const T *records,
                                  uint32_t num_records) {

    const auto n = static_cast<uint64_t>(num_records);
    const uint64_t vl = svcntw();
    const svbool_t pg_all = svptrue_b32();

    uint64_t i = 0;

    // Process 4 vectors per iteration.
    for (; i + (4 * vl) <= n; i += (4 * vl)) {

        __builtin_prefetch(&records[i + 16 * vl], 0, 0);
        // load (and widen) the raw data (records) into indices vector register,
        // idx_x
        svuint32_t idx0 = load<T>(pg_all, &records[i]);
        svuint32_t idx1 = load<T>(pg_all, &records[i + vl]);
        svuint32_t idx2 = load<T>(pg_all, &records[i + 2 * vl]);
        svuint32_t idx3 = load<T>(pg_all, &records[i + 3 * vl]);

        // Update each vector before gathering counts for the next one so
        // duplicate indices spanning vectors observe the preceding update.
        svuint32_t old_counts0 =
            svld1_gather_u32index_u32(pg_all, histogram, idx0);
        svuint32_t dup_counts0 = svhistcnt_u32_z(pg_all, idx0, idx0);
        svuint32_t new_counts0 = svadd_u32_m(pg_all, old_counts0, dup_counts0);
        svst1_scatter_u32index_u32(pg_all, histogram, idx0, new_counts0);

        svuint32_t old_counts1 =
            svld1_gather_u32index_u32(pg_all, histogram, idx1);
        svuint32_t dup_counts1 = svhistcnt_u32_z(pg_all, idx1, idx1);
        svuint32_t new_counts1 = svadd_u32_m(pg_all, old_counts1, dup_counts1);
        svst1_scatter_u32index_u32(pg_all, histogram, idx1, new_counts1);

        svuint32_t old_counts2 =
            svld1_gather_u32index_u32(pg_all, histogram, idx2);
        svuint32_t dup_counts2 = svhistcnt_u32_z(pg_all, idx2, idx2);
        svuint32_t new_counts2 = svadd_u32_m(pg_all, old_counts2, dup_counts2);
        svst1_scatter_u32index_u32(pg_all, histogram, idx2, new_counts2);

        svuint32_t old_counts3 =
            svld1_gather_u32index_u32(pg_all, histogram, idx3);
        svuint32_t dup_counts3 = svhistcnt_u32_z(pg_all, idx3, idx3);
        svuint32_t new_counts3 = svadd_u32_m(pg_all, old_counts3, dup_counts3);
        svst1_scatter_u32index_u32(pg_all, histogram, idx3, new_counts3);
    }

    // Process remaining full vectors one at a time with a predicate
    for (; i < n; i += vl) {
        svbool_t pred = svwhilelt_b32(i, n);
        svuint32_t idx = load<T>(pred, &records[i]);
        svuint32_t old_counts = svld1_gather_u32index_u32(pred, histogram, idx);
        svuint32_t dup_counts = svhistcnt_u32_z(pred, idx, idx);
        svuint32_t new_counts = svadd_u32_m(pred, old_counts, dup_counts);
        svst1_scatter_u32index_u32(pred, histogram, idx, new_counts);
    }
}

template <ValidSVEType T>
void calc_histogram_sve2(uint32_t *histogram, const T *RESTRICT records,
                         uint32_t num_records) {

    const auto n = static_cast<uint64_t>(num_records);
    const uint64_t vl = svcntw();

    uint64_t i = 0;

    // Process in chunks of VL, including the final partial vector.
    for (; i < n; i += vl) {

        svbool_t pg = svwhilelt_b32(i, n);
        // load (and widen) the raw data (records) into indices vector register,
        // idx_x
        svuint32_t idx = load<T>(pg, &records[i]);
        // load the histogram data at those indices into a vector register,
        // old_counts_x
        svuint32_t old_counts = svld1_gather_u32index_u32(pg, histogram, idx);

        svuint32_t dup_counts = svhistcnt_u32_z(pg, idx, idx);

        svuint32_t new_counts = svadd_u32_m(pg, old_counts, dup_counts);

        svst1_scatter_u32index_u32(pg, histogram, idx, new_counts);
    }
}

template <ValidSVEType T>
void calc_histogram_sve2(uint32_t *histogram, const T *RESTRICT records,
                         uint32_t num_records, uint8_t bin_size) {

    validate_bin_size(bin_size);

    const auto n = static_cast<uint64_t>(num_records);
    const uint64_t vl = svcntw();

    alignas(64) uint32_t idx_buf[64];

    uint64_t i = 0;

    // Process in chunks of VL, including the final partial vector.
    for (; i < n; i += vl) {

        svbool_t pg = svwhilelt_b32(i, n);
        // load (and widen) the raw data (records) into indices vector register,
        // idx_x
        svuint32_t idx =
            load_binned_indices<T>(pg, &records[i], bin_size, idx_buf);
        // load the histogram data at those indices into a vector register,
        // old_counts_x
        svuint32_t old_counts = svld1_gather_u32index_u32(pg, histogram, idx);

        svuint32_t dup_counts = svhistcnt_u32_z(pg, idx, idx);

        svuint32_t new_counts = svadd_u32_m(pg, old_counts, dup_counts);

        svst1_scatter_u32index_u32(pg, histogram, idx, new_counts);
    }
}

template <ValidSVEType T>
void calc_histogram_sve2_unrolled(uint32_t *histogram,
                                  const T *RESTRICT records,
                                  uint32_t num_records, uint8_t bin_size) {
    validate_bin_size(bin_size);

    const auto n = static_cast<uint64_t>(num_records);
    const uint64_t vl = svcntw();

    alignas(64) uint32_t idx_buf[64];

    for (uint64_t i = 0; i < n; i += vl) {
        svbool_t pg = svwhilelt_b32(i, n);
        svuint32_t idx =
            load_binned_indices<T>(pg, &records[i], bin_size, idx_buf);
        svuint32_t old_counts = svld1_gather_u32index_u32(pg, histogram, idx);
        svuint32_t dup_counts = svhistcnt_u32_z(pg, idx, idx);
        svuint32_t new_counts = svadd_u32_m(pg, old_counts, dup_counts);
        svst1_scatter_u32index_u32(pg, histogram, idx, new_counts);
    }
}

template void calc_histogram_sve2<uint8_t>(uint32_t *, const uint8_t *,
                                           uint32_t);
template void calc_histogram_sve2<uint16_t>(uint32_t *, const uint16_t *,
                                            uint32_t);
template void calc_histogram_sve2<uint32_t>(uint32_t *, const uint32_t *,
                                            uint32_t);

template void calc_histogram_sve2<uint8_t>(uint32_t *, const uint8_t *,
                                           uint32_t, uint8_t);
template void calc_histogram_sve2<uint16_t>(uint32_t *, const uint16_t *,
                                            uint32_t, uint8_t);
template void calc_histogram_sve2<uint32_t>(uint32_t *, const uint32_t *,
                                            uint32_t, uint8_t);

template void calc_histogram_sve2_unrolled<uint8_t>(uint32_t *, const uint8_t *,
                                                    uint32_t);
template void
calc_histogram_sve2_unrolled<uint16_t>(uint32_t *, const uint16_t *, uint32_t);
template void
calc_histogram_sve2_unrolled<uint32_t>(uint32_t *, const uint32_t *, uint32_t);

template void calc_histogram_sve2_unrolled<uint8_t>(uint32_t *, const uint8_t *,
                                                    uint32_t, uint8_t);
template void calc_histogram_sve2_unrolled<uint16_t>(uint32_t *,
                                                     const uint16_t *, uint32_t,
                                                     uint8_t);
template void calc_histogram_sve2_unrolled<uint32_t>(uint32_t *,
                                                     const uint32_t *, uint32_t,
                                                     uint8_t);

#else
#warning                                                                       \
    "SVE2 not available on this target: histogram will have no SVE2 implementation !"
#endif

} // namespace histogram
