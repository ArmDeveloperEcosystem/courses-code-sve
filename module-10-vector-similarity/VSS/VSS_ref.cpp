// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "VSS.h"

#include <cmath>
#include <cstddef>
#include <cstdint>

using std::int32_t;
using std::int8_t;
using std::size_t;

namespace {

using VSS::Metric;

// The scalar metric computation traits defines the element type, accumulator
// type, and result type for each supported embedding type. The accumulator type
// is used for intermediate computations to avoid overflow or loss of precision.
// The result type is always float for this course.
template <typename Ty> struct MetricComputationTraits;

struct MetricComputationTraitsBase {
    // The final result type of the metric computation. It's the same type for
    // all metrics and embedding types.
    using ResultTy = Metric::ResultTy;
};

template <>
struct MetricComputationTraits<float> : public MetricComputationTraitsBase {
    // The element type of the embedding
    using EltTy = float;
    // The accumulator type used for intermediate computations
    using AccTy = float;
};
template <>
struct MetricComputationTraits<_Float16> : public MetricComputationTraitsBase {
    // The element type of the embedding
    using EltTy = _Float16;
    // The accumulator type used for intermediate computations
    using AccTy = float;
};
template <>
struct MetricComputationTraits<int8_t> : public MetricComputationTraitsBase {
    // The element type of the embedding
    using EltTy = int8_t;
    // The accumulator type used for intermediate computations
    using AccTy = int32_t;
};

// Scalar metric policies. The embedding traversal code below owns the loop;
// these specializations own only the per-element arithmetic and any final
// reduction needed to produce the metric value.
template <typename ElementTy> struct MetricComputationPolicy {
    using EltTy = typename MetricComputationTraits<ElementTy>::EltTy;
    using AccTy = typename MetricComputationTraits<ElementTy>::AccTy;
    using ResultTy = typename MetricComputationTraits<ElementTy>::ResultTy;

    AccTy acc{AccTy(0)};

    void compute(const EltTy &av,
                 const EltTy &bv) noexcept { /* nothing to do */ }

    [[nodiscard]] ResultTy reduce() const { return static_cast<ResultTy>(acc); }
};

template <Metric::Kind Kind, typename ElementTy> struct MetricComputation;

template <typename ElementTy>
struct MetricComputation<Metric::Kind::EUCLIDEAN_DISTANCE, ElementTy>
    : public MetricComputationPolicy<ElementTy> {
    using AccTy = typename MetricComputationPolicy<ElementTy>::AccTy;

    // Forces conversion of the arguments from EltTy to AccTy before the
    // subtraction, so that the difference is computed in the accumulator type.
    void compute(const AccTy &av, const AccTy &bv) noexcept {
        const AccTy diff = av - bv;
        this->acc += diff * diff;
    }
};

template <typename ElementTy>
struct MetricComputation<Metric::Kind::DOT_PRODUCT, ElementTy>
    : public MetricComputationPolicy<ElementTy> {
    using AccTy = typename MetricComputationPolicy<ElementTy>::AccTy;

    // Forces conversion of the arguments from EltTy to AccTy before the
    // multiplication, so that the product is computed in the accumulator type.
    void compute(const AccTy &av, const AccTy &bv) noexcept {
        const AccTy prod = av * bv;
        this->acc += prod;
    }
};

template <typename ElementTy>
struct MetricComputation<Metric::Kind::COSINE_SIMILARITY, ElementTy>
    : public MetricComputationPolicy<ElementTy> {
    using ResultTy = typename MetricComputationPolicy<ElementTy>::ResultTy;
    using AccTy = typename MetricComputationPolicy<ElementTy>::AccTy;

    AccTy a_norm_acc{AccTy(0)};
    AccTy b_norm_acc{AccTy(0)};

    // Forces conversion of the arguments from EltTy to AccTy before the
    // multiplication, so that the product is computed in the accumulator type.
    void compute(const AccTy &av, const AccTy &bv) noexcept {
        this->acc += av * bv;
        a_norm_acc += av * av;
        b_norm_acc += bv * bv;
    }

    [[nodiscard]] ResultTy reduce() const noexcept {
        const auto a_norm = static_cast<ResultTy>(a_norm_acc);
        const auto b_norm = static_cast<ResultTy>(b_norm_acc);
        return static_cast<ResultTy>(this->acc) / std::sqrt(a_norm * b_norm);
    }
};
} // namespace

namespace VSS {

template <size_t N, typename Ty, Metric::Kind Kind>
Metric::ResultTy similarity_ref(const Embedding<N, Ty> &a,
                                const Embedding<N, Ty> &b) noexcept {
    MetricComputation<Kind, Ty> metric;

    // Reference path: one scalar pass over every element. The loop shape stays
    // common across metrics; the MetricComputation policy supplies the metric
    // math.
    for (size_t i = 0; i < N; i++)
        metric.compute(a[i], b[i]);

    return metric.reduce();
}

#define VSS_INSTANTIATE_SIMILARITY_REF(DIMENSION, TYPE)                        \
    template Metric::ResultTy                                                  \
    similarity_ref<DIMENSION, TYPE, Metric::EUCLIDEAN_DISTANCE>(               \
        const Embedding<DIMENSION, TYPE> &a,                                   \
        const Embedding<DIMENSION, TYPE> &b) noexcept;                         \
                                                                               \
    template Metric::ResultTy                                                  \
    similarity_ref<DIMENSION, TYPE, Metric::DOT_PRODUCT>(                      \
        const Embedding<DIMENSION, TYPE> &a,                                   \
        const Embedding<DIMENSION, TYPE> &b) noexcept;                         \
                                                                               \
    template Metric::ResultTy                                                  \
    similarity_ref<DIMENSION, TYPE, Metric::COSINE_SIMILARITY>(                \
        const Embedding<DIMENSION, TYPE> &a,                                   \
        const Embedding<DIMENSION, TYPE> &b) noexcept;

VSS_INSTANTIATE_SIMILARITY_REF(3, float)
VSS_INSTANTIATE_SIMILARITY_REF(8, float)
VSS_INSTANTIATE_SIMILARITY_REF(67, float)
VSS_INSTANTIATE_SIMILARITY_REF(384, float)
VSS_INSTANTIATE_SIMILARITY_REF(768, float)
VSS_INSTANTIATE_SIMILARITY_REF(1024, float)
VSS_INSTANTIATE_SIMILARITY_REF(3072, float)

VSS_INSTANTIATE_SIMILARITY_REF(3, int8_t)
VSS_INSTANTIATE_SIMILARITY_REF(8, int8_t)
VSS_INSTANTIATE_SIMILARITY_REF(259, int8_t)
VSS_INSTANTIATE_SIMILARITY_REF(384, int8_t)
VSS_INSTANTIATE_SIMILARITY_REF(768, int8_t)
VSS_INSTANTIATE_SIMILARITY_REF(1024, int8_t)
VSS_INSTANTIATE_SIMILARITY_REF(3072, int8_t)

VSS_INSTANTIATE_SIMILARITY_REF(3, _Float16)
VSS_INSTANTIATE_SIMILARITY_REF(8, _Float16)
VSS_INSTANTIATE_SIMILARITY_REF(131, _Float16)
VSS_INSTANTIATE_SIMILARITY_REF(384, _Float16)
VSS_INSTANTIATE_SIMILARITY_REF(768, _Float16)
VSS_INSTANTIATE_SIMILARITY_REF(1024, _Float16)
VSS_INSTANTIATE_SIMILARITY_REF(3072, _Float16)

#undef VSS_INSTANTIATE_SIMILARITY_REF

} // namespace VSS
