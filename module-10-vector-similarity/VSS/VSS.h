// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#pragma once

#include <array>
#include <cstddef>
#include <initializer_list>
#include <span>
#include <stdexcept>

namespace VSS {

// The course exposes three common vector similarity metrics through one
// compile-time selector. Keeping the metric as a template argument lets the
// implementation keep a single visible loop shape while compiling away the
// branches for each metric.
struct Metric {
    enum Kind {
        EUCLIDEAN_DISTANCE,
        DOT_PRODUCT,
        COSINE_SIMILARITY,
    };

    // The metric result type is always float for this course.
    using ResultTy = float;
};

// In real life, together with some metadata, an embedding contains some sort of
// array of N elements of type Ty. The number of elements is known a priori at
// compile time, so we make it a template parameter so the compiler can optimize
// it fully --- this matters when one has to process millions of embeddings in a
// tight loop. For this course, we do not fully model an Embedding with a class
// of its own, as we do not care about the metadata, and we instead
// just reuse the std::array container.
template <size_t N, typename Ty> using Embedding = std::array<Ty, N>;

// Portable reference implementation.
template <size_t N, typename Ty, Metric::Kind Kind>
Metric::ResultTy similarity_ref(const Embedding<N, Ty> &a,
                                const Embedding<N, Ty> &b) noexcept;

#ifdef __ARM_FEATURE_SVE
// SVE implementation for the same metric selector. It is available only
// when this translation unit is built for AArch64 with SVE enabled.
template <size_t N, typename Ty, Metric::Kind Kind>
Metric::ResultTy similarity_sve(const Embedding<N, Ty> &a,
                                const Embedding<N, Ty> &b) noexcept;
#endif // __ARM_FEATURE_SVE

// Convenience helper to construct an embedding from a span of values. The span
// must have exactly N elements, or an exception is thrown. This is a
// convenience for the course; in real life, one would likely have a more
// complex embedding class with its own constructor and validation logic.
template <size_t N, typename Ty>
constexpr Embedding<N, Ty> make_embedding(std::span<const Ty> values) {
    if (values.size() != N)
        throw std::invalid_argument("Embedding input has wrong size");

    Embedding<N, Ty> embedding{};
    for (size_t i = 0; i < N; ++i)
        embedding[i] = values[i];

    return embedding;
}

// Convenience helper to construct an embedding from an initializer list of
// values. The list must have exactly N elements, or an exception is thrown.
// This is a convenience for the course; in real life, one would likely have a
// more complex embedding class with its own constructor and validation logic.
template <size_t N, typename Ty>
constexpr Embedding<N, Ty> make_embedding(std::initializer_list<Ty> values) {
    return make_embedding<N, Ty>(
        std::span<const Ty>(values.begin(), values.size()));
}

} // namespace VSS
