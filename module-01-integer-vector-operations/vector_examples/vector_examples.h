// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

#ifdef __aarch64__
#include <arm_sve.h>
#else
#warning Platform is not AArch64: no SVE-accelerated implementations will be available !
#endif

// Adds two vectors element-by-element and stores the result.
template <typename T>
void vector_add_scalar(std::span<const T> lhs, std::span<const T> rhs,
                       std::span<T> result) {
    // All three spans must describe the same number of elements.
    assert(lhs.size() == rhs.size());
    assert(lhs.size() == result.size());

    // Walk the vectors one element at a time: result[i] = lhs[i] + rhs[i].
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        result[i] = lhs[i] + rhs[i];
    }
}

// Finds the smallest and largest value in a span.
template <typename T>
std::pair<T, T> min_max_scalar(std::span<const T> values) {
    // The first element seeds the running minimum and maximum.
    assert(!values.empty());

    T minimum = values[0];
    T maximum = values[0];

    // Start at index 1 because index 0 is already our initial answer.
    for (std::size_t i = 1; i < values.size(); ++i) {
        if (values[i] < minimum) {
            minimum = values[i];
        }

        if (values[i] > maximum) {
            maximum = values[i];
        }
    }

    return {minimum, maximum};
}

#ifdef __ARM_FEATURE_SVE
template <typename ScalarTy> struct SVE;
template <> struct SVE<std::int8_t> {
    using ScalarType = std::int8_t;
    using VectorType = svint8_t;
    static std::uint64_t CNT() { return svcntb(); }
    static svbool_t WHILELT(std::size_t i, std::size_t size) {
        return svwhilelt_b8(i, size);
    }
};
template <> struct SVE<std::uint8_t> {
    using ScalarType = std::uint8_t;
    using VectorType = svuint8_t;
    static std::uint64_t CNT() { return svcntb(); }
    static svbool_t WHILELT(std::size_t i, std::size_t size) {
        return svwhilelt_b8(i, size);
    }
};
template <> struct SVE<std::int16_t> {
    using ScalarType = std::int16_t;
    using VectorType = svint16_t;
    static std::uint64_t CNT() { return svcnth(); }
    static svbool_t WHILELT(std::size_t i, std::size_t size) {
        return svwhilelt_b16(i, size);
    }
};
template <> struct SVE<std::uint16_t> {
    using ScalarType = std::uint16_t;
    using VectorType = svuint16_t;
    static std::uint64_t CNT() { return svcnth(); }
    static svbool_t WHILELT(std::size_t i, std::size_t size) {
        return svwhilelt_b16(i, size);
    }
};
template <> struct SVE<std::int32_t> {
    using ScalarType = std::int32_t;
    using VectorType = svint32_t;
    static std::uint64_t CNT() { return svcntw(); }
    static svbool_t WHILELT(std::size_t i, std::size_t size) {
        return svwhilelt_b32(i, size);
    }
};
template <> struct SVE<std::uint32_t> {
    using ScalarType = std::uint32_t;
    using VectorType = svuint32_t;
    static std::uint64_t CNT() { return svcntw(); }
    static svbool_t WHILELT(std::size_t i, std::size_t size) {
        return svwhilelt_b32(i, size);
    }
};

// Generic declaration that will compile for the types explicitly support above
// with SVEOps.
template <typename T>
void vector_add_sve(std::span<const T> lhs, std::span<const T> rhs,
                    std::span<T> result) {
    assert(lhs.size() == rhs.size());
    assert(lhs.size() == result.size());

    using VectorType = SVE<T>::VectorType;

    std::size_t i = 0;

    while (i < lhs.size()) {
        svbool_t pg = SVE<T>::WHILELT(i, lhs.size());

        VectorType lhs_vec = svld1(pg, lhs.data() + i);
        VectorType rhs_vec = svld1(pg, rhs.data() + i);

        VectorType result_vec = svadd_x(pg, lhs_vec, rhs_vec);

        svst1(pg, result.data() + i, result_vec);

        i += SVE<T>::CNT();
    }
}

// Generic declaration.
// The supported types are implemented explicitly below.
template <typename T> std::pair<T, T> min_max_sve(std::span<const T> values) {
    assert(!values.empty());

    using ScalarType = SVE<T>::ScalarType;
    using VectorType = SVE<T>::VectorType;

    ScalarType minimum = values[0];
    ScalarType maximum = minimum;

    std::size_t i = 0;

    while (i < values.size()) {
        svbool_t pg = SVE<T>::WHILELT(i, values.size());

        VectorType values_vec = svld1(pg, values.data() + i);

        ScalarType chunk_min = svminv(pg, values_vec);
        ScalarType chunk_max = svmaxv(pg, values_vec);

        if (chunk_min < minimum)
            minimum = chunk_min;

        if (chunk_max > maximum)
            maximum = chunk_max;

        i += SVE<T>::CNT();
    }

    return {minimum, maximum};
}
#else
#warning SVE support not detected: vector_examples will have no SVE implementation !
#endif
