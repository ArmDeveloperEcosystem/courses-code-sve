// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#pragma once
#include <concepts>
#include <cstdint>

template <typename T>
concept ValidSVEType =
    std::same_as<T, std::uint8_t> || std::same_as<T, std::uint16_t> ||
    std::same_as<T, std::uint32_t>;

namespace histogram {

/**
 *
 * Scalar implementation of a histogram with a unary bin size
 *
 * @param histogram Output histogram bins to update.
 * @param records Raw input data used to generate the histogram and used as
 * histogram bin indices.
 * @param num_records Number of input records to process.
 *
 */
template <ValidSVEType T>
void calc_histogram(std::uint32_t *histogram, T *records,
                    std::uint32_t num_records);

/**
 *
 * Scalar implementation of a histogram with a variable bin size
 *
 * @param histogram Output histogram bins to update.
 * @param records Raw input data used to generate the histogram.
 * @param num_records Number of input records to process.
 * @param bin_size Number of values represented by each histogram bin.
 *
 */
template <ValidSVEType T>
void calc_histogram(std::uint32_t *histogram, T *records,
                    std::uint32_t num_records, std::uint8_t bin_size);

#ifdef __ARM_FEATURE_SVE2
/**
 *
 * Implementation of a histogram using AArch64 SVE2 instrinsics with minimal
 * optimizations
 *
 * @param histogram Output histogram bins to update.
 * @param records Raw input data used to generate the histogram and used as
 * histogram bin indices.
 * @param num_records Number of input records to process.
 *
 */
template <ValidSVEType T>
void calc_histogram_sve2(std::uint32_t *histogram, const T *records,
                         std::uint32_t num_records);

template <ValidSVEType T>
void calc_histogram_sve2(std::uint32_t *histogram, const T *records,
                         std::uint32_t num_records, std::uint8_t bin_size);

/**
 *
 * Implementation of a histogram using AArch64 SVE2 instrinsics with loop
 * unrolling and prefetching
 *
 *
 * @param histogram Output histogram bins to update.
 * @param records Raw input data used to generate the histogram and used as
 * histogram bin indices.
 * @param num_records Number of input records to process.
 *
 */
template <ValidSVEType T>
void calc_histogram_sve2_unrolled(std::uint32_t *histogram, const T *records,
                                  std::uint32_t num_records);

template <ValidSVEType T>
void calc_histogram_sve2_unrolled(std::uint32_t *histogram, const T *records,
                                  std::uint32_t num_records,
                                  std::uint8_t bin_size);
#endif

#ifdef __ARM_FEATURE_SVE
/**
 *
 * Hybrid SVE/scalar implementation of a histogram with a unary bin size.
 *
 * @param histogram Output histogram bins to update.
 * @param records Raw input data used to generate the histogram and used as
 * histogram bin indices.
 * @param num_records Number of input records to process.
 *
 */
template <ValidSVEType T>
void calc_histogram_sve(std::uint32_t *histogram, const T *records,
                        std::uint32_t num_records);

/**
 *
 * Hybrid SVE/scalar implementation of a histogram with a unary bin size.
 *
 * This variant uses SVE for vector-length-agnostic, predicated input loading
 * and widening, but performs the histogram counter updates with scalar stores.
 * On the current benchmark input this is slightly faster than the SVE
 * gather/scatter variant, because the histogram update is an indirect
 * read-modify-write and short-vector gather/scatter overhead outweighs the
 * benefit of doing those updates in vector registers.
 *
 * @param histogram Output histogram bins to update.
 * @param records Raw input data used to generate the histogram and used as
 * histogram bin indices.
 * @param num_records Number of input records to process.
 *
 */
template <ValidSVEType T>
void calc_histogram_sve_scalar_update(std::uint32_t *histogram,
                                      const T *records,
                                      std::uint32_t num_records);
#endif

} // namespace histogram