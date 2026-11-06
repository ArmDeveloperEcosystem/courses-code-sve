// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "SVE-task-based-course.h"
#include "VSS.h"

#include <cmath>
#include <cstddef>
#include <cstdint>

#ifdef __aarch64__
#include <arm_sve.h>
#else
#warning Platform is not AArch64: no SVE-accelerated implementations will be available !
#endif

#ifdef __ARM_FEATURE_SVE
using std::int32_t;
using std::int8_t;
using std::size_t;
using std::uint64_t;

namespace {

using VSS::Metric;

// The SVE metric computation traits define how each scalar embedding element
// maps to an SVE load vector and an SVE accumulator vector. The scalar
// accumulator type is used when describing the computation result; the final
// public result type is always float for this course.
template <typename Ty> struct SVEMetricComputationTraits;

template <typename ElementTy, typename ScalarAccTy, typename VectorTy,
          typename AccVectorTy>
struct SVEMetricComputationTraitsBase {
    using EltTy = ElementTy;
    using AccTy = ScalarAccTy;
    using VecTy = VectorTy;
    using AccVecTy = AccVectorTy;
    using PredTy = svbool_t;
    using ResultTy = Metric::ResultTy;
};

template <>
struct SVEMetricComputationTraits<float>
    : public SVEMetricComputationTraitsBase<float, float, svfloat32_t,
                                            svfloat32_t> {};

template <>
struct SVEMetricComputationTraits<_Float16>
    : public SVEMetricComputationTraitsBase<_Float16, float, svfloat16_t,
                                            svfloat32_t> {};

template <>
struct SVEMetricComputationTraits<int8_t>
    : public SVEMetricComputationTraitsBase<int8_t, int32_t, svint8_t,
                                            svint32_t> {};

// A tiny wrapper around the SVE intrinsics used by the task. It keeps the
// metric code below focused on the algorithm rather than intrinsic spelling.
template <typename ElementTy> struct SVEOpsBase {
    using EltTy = typename SVEMetricComputationTraits<ElementTy>::EltTy;
    using VecTy = typename SVEMetricComputationTraits<ElementTy>::VecTy;
    using AccTy = typename SVEMetricComputationTraits<ElementTy>::AccTy;
    using AccVecTy = typename SVEMetricComputationTraits<ElementTy>::AccVecTy;
    using PredTy = typename SVEMetricComputationTraits<ElementTy>::PredTy;
    using ResultTy = typename SVEMetricComputationTraits<ElementTy>::ResultTy;
};

template <typename ElementTy> struct SVEOps;

template <> struct SVEOps<float> : public SVEOpsBase<float> {

    static INLINE uint64_t vector_length() noexcept { return svcntw(); }
    static INLINE PredTy all_acc_true() noexcept { return svptrue_b32(); }
    static INLINE VecTy zero_vec() noexcept {
        return svdup_n_f32(static_cast<EltTy>(0));
    }
    static INLINE AccVecTy zero_acc() noexcept {
        return svdup_n_f32(static_cast<AccTy>(0));
    }
    static INLINE VecTy load(const PredTy &pred, const EltTy *ptr) noexcept {
        return svld1(pred, ptr);
    }
    static INLINE PredTy while_lt(uint64_t i, uint64_t n) noexcept {
        return svwhilelt_b32(i, n);
    }

    static INLINE void accumulate_sqdiff(const PredTy &pred, AccVecTy &acc0,
                                         AccVecTy &, AccVecTy &, AccVecTy &,
                                         const VecTy &a,
                                         const VecTy &b) noexcept {
        const VecTy diff = svsub_x(pred, a, b);
        acc0 = svmla_m(pred, acc0, diff, diff);
    }

    static INLINE void accumulate_product(const PredTy &pred, AccVecTy &acc0,
                                          AccVecTy &, AccVecTy &, AccVecTy &,
                                          const VecTy &a,
                                          const VecTy &b) noexcept {
        acc0 = svmla_m(pred, acc0, a, b);
    }

    static INLINE ResultTy reduce_acc(const AccVecTy &acc0, const AccVecTy &,
                                      const AccVecTy &,
                                      const AccVecTy &) noexcept {
        return svaddv(all_acc_true(), acc0);
    }
};

template <> struct SVEOps<_Float16> : public SVEOpsBase<_Float16> {

    static INLINE uint64_t vector_length() noexcept { return svcnth(); }
    static INLINE PredTy all_acc_true() noexcept { return svptrue_b32(); }
    static INLINE VecTy zero_vec() noexcept {
        return svdup_n_f16(static_cast<__fp16>(0));
    }
    static INLINE AccVecTy zero_acc() noexcept {
        return svdup_n_f32(static_cast<AccTy>(0));
    }
    static INLINE VecTy load(const PredTy &pred, const EltTy *ptr) noexcept {
        return svsel(pred, svld1(pred, reinterpret_cast<const __fp16 *>(ptr)),
                     zero_vec());
    }
    static INLINE PredTy while_lt(uint64_t i, uint64_t n) noexcept {
        return svwhilelt_b16(i, n);
    }

    static INLINE void accumulate_sqdiff(const PredTy &, AccVecTy &acc0,
                                         AccVecTy &acc1, AccVecTy &, AccVecTy &,
                                         const VecTy &a,
                                         const VecTy &b) noexcept {
        const auto all_true = all_acc_true();
        const auto a_lo = svcvt_f32_f16_x(all_true, a);
        const auto b_lo = svcvt_f32_f16_x(all_true, b);
#ifdef __ARM_FEATURE_SVE2
        const auto a_hi = svcvtlt_f32_f16_x(all_true, a);
        const auto b_hi = svcvtlt_f32_f16_x(all_true, b);
#else
        // FCVT widens the even FP16 lanes. Duplicate each odd lane into an
        // even lane to convert the upper half of every FP16 pair.
        const auto a_hi = svcvt_f32_f16_x(all_true, svtrn2_f16(a, a));
        const auto b_hi = svcvt_f32_f16_x(all_true, svtrn2_f16(b, b));
#endif
        const auto diff_lo = svsub_x(all_true, a_lo, b_lo);
        const auto diff_hi = svsub_x(all_true, a_hi, b_hi);
        acc0 = svmla_x(all_true, acc0, diff_lo, diff_lo);
        acc1 = svmla_x(all_true, acc1, diff_hi, diff_hi);
    }

    static INLINE void accumulate_product(const PredTy &, AccVecTy &acc0,
                                          AccVecTy &acc1, AccVecTy &,
                                          AccVecTy &, const VecTy &a,
                                          const VecTy &b) noexcept {
#ifdef __ARM_FEATURE_SVE2
        acc0 = svmlalb_f32(acc0, a, b);
        acc1 = svmlalt_f32(acc1, a, b);
#else
        const auto all_true = all_acc_true();
        const auto a_lo = svcvt_f32_f16_x(all_true, a);
        const auto b_lo = svcvt_f32_f16_x(all_true, b);
        const auto a_hi = svcvt_f32_f16_x(all_true, svtrn2_f16(a, a));
        const auto b_hi = svcvt_f32_f16_x(all_true, svtrn2_f16(b, b));
        acc0 = svmla_x(all_true, acc0, a_lo, b_lo);
        acc1 = svmla_x(all_true, acc1, a_hi, b_hi);
#endif
    }

    static INLINE ResultTy reduce_acc(const AccVecTy &acc0,
                                      const AccVecTy &acc1, const AccVecTy &,
                                      const AccVecTy &) noexcept {
        const auto all_true = all_acc_true();
        const auto acc01 = svadd_x(all_true, acc0, acc1);
        return svaddv(all_true, acc01);
    }
};

template <> struct SVEOps<int8_t> : public SVEOpsBase<int8_t> {

    static INLINE uint64_t vector_length() noexcept { return svcntb(); }
    static INLINE PredTy all_acc_true() noexcept { return svptrue_b32(); }
    static INLINE VecTy zero_vec() noexcept { return svdup_n_s8(0); }
    static INLINE AccVecTy zero_acc() noexcept { return svdup_n_s32(0); }
    static INLINE VecTy load(const PredTy &pred, const EltTy *ptr) noexcept {
        return svsel(pred, svld1(pred, ptr), zero_vec());
    }
    static INLINE PredTy while_lt(uint64_t i, uint64_t n) noexcept {
        return svwhilelt_b8(i, n);
    }

    static INLINE void accumulate_sqdiff(const PredTy &, AccVecTy &acc0,
                                         AccVecTy &acc1, AccVecTy &acc2,
                                         AccVecTy &acc3, const VecTy &a,
                                         const VecTy &b) noexcept {
#ifdef __ARM_FEATURE_SVE2
        // Subtract 2 int8_t vectors into 2 int16_t vectors to avoid overflow.
        const auto diff_lo = svsublb_s16(a, b);
        const auto diff_hi = svsublt_s16(a, b);

        // Accumulate the squared differences into 4 int32_t accumulators.
        acc0 = svmlalb_s32(acc0, diff_lo, diff_lo);
        acc1 = svmlalt_s32(acc1, diff_lo, diff_lo);
        acc2 = svmlalb_s32(acc2, diff_hi, diff_hi);
        acc3 = svmlalt_s32(acc3, diff_hi, diff_hi);
#else
        // Unpack all int8_t elements before subtraction to avoid overflow.
        const auto all_true_16 = svptrue_b16();
        const auto all_true_32 = all_acc_true();
        const auto diff_lo =
            svsub_x(all_true_16, svunpklo_s16(a), svunpklo_s16(b));
        const auto diff_hi =
            svsub_x(all_true_16, svunpkhi_s16(a), svunpkhi_s16(b));

        // Widen each half again before squaring into the four accumulators.
        const auto diff00 = svunpklo_s32(diff_lo);
        const auto diff01 = svunpkhi_s32(diff_lo);
        const auto diff10 = svunpklo_s32(diff_hi);
        const auto diff11 = svunpkhi_s32(diff_hi);
        acc0 = svmla_x(all_true_32, acc0, diff00, diff00);
        acc1 = svmla_x(all_true_32, acc1, diff01, diff01);
        acc2 = svmla_x(all_true_32, acc2, diff10, diff10);
        acc3 = svmla_x(all_true_32, acc3, diff11, diff11);
#endif
    }

    static INLINE void accumulate_product(const PredTy &, AccVecTy &acc0,
                                          AccVecTy &, AccVecTy &, AccVecTy &,
                                          const VecTy &a,
                                          const VecTy &b) noexcept {

        // All int8 dot products are accumulated in acc0. This requires
        // the other accumulators to stay zero for the reduction.
        // SVDOT is the right primitive here, but this single-vector helper only
        // has one input vector pair, so it naturally feeds one accumulator. The
        // compiler's auto-vectorized reference wins by unrolling the loop and
        // issuing multiple independent SVDOT accumulator chains.
        acc0 = svdot_s32(acc0, a, b);
    }

    static INLINE ResultTy reduce_acc(const AccVecTy &acc0,
                                      const AccVecTy &acc1,
                                      const AccVecTy &acc2,
                                      const AccVecTy &acc3) noexcept {
        const auto all_true = all_acc_true();
        const auto acc01 = svadd_x(all_true, acc0, acc1);
        const auto acc23 = svadd_x(all_true, acc2, acc3);
        const auto acc0123 = svadd_x(all_true, acc01, acc23);
        return svaddv(all_true, acc0123);
    }
};

// ACLE scalable vector types cannot be stored directly as data members. This
// helper stores references to the local vector accumulators owned by
// similarity_sve, so metric policies can talk about one logical accumulator
// group instead of four separate vector references.
template <typename Ops> struct SVEAccumulatorGroup {
    using PredTy = typename Ops::PredTy;
    using AccVecTy = typename Ops::AccVecTy;
    using VecTy = typename Ops::VecTy;
    using ResultTy = typename Ops::ResultTy;

    AccVecTy &acc0;
    AccVecTy &acc1;
    AccVecTy &acc2;
    AccVecTy &acc3;

    static INLINE SVEAccumulatorGroup bind(AccVecTy &acc0, AccVecTy &acc1,
                                           AccVecTy &acc2,
                                           AccVecTy &acc3) noexcept {
        return {acc0, acc1, acc2, acc3};
    }

    INLINE void accumulate_sqdiff(const PredTy &pred, const VecTy &a,
                                  const VecTy &b) const noexcept {
        Ops::accumulate_sqdiff(pred, acc0, acc1, acc2, acc3, a, b);
    }

    INLINE void accumulate_product(const PredTy &pred, const VecTy &a,
                                   const VecTy &b) const noexcept {
        Ops::accumulate_product(pred, acc0, acc1, acc2, acc3, a, b);
    }

    [[nodiscard]] INLINE ResultTy reduce() const noexcept {
        return Ops::reduce_acc(acc0, acc1, acc2, acc3);
    }
};

// SVE metric policies. They remain stateless: similarity_sve owns the vector
// accumulators as local variables and passes grouped references to them.
template <typename Ops> struct SVEMetricComputationPolicy {

    using ResultTy = typename Ops::ResultTy;

    static INLINE ResultTy finish(const ResultTy &acc, const ResultTy &,
                                  const ResultTy &) noexcept {
        return acc;
    }
};

template <Metric::Kind Kind, typename Ops> struct SVEMetricComputation;

template <typename Ops>
struct SVEMetricComputation<Metric::Kind::EUCLIDEAN_DISTANCE, Ops>
    : public SVEMetricComputationPolicy<Ops> {
    using PredTy = typename Ops::PredTy;
    using AccumulatorGroup = SVEAccumulatorGroup<Ops>;
    using VecTy = typename Ops::VecTy;

    static INLINE void compute(const PredTy &pred, const AccumulatorGroup &acc,
                               const AccumulatorGroup &,
                               const AccumulatorGroup &, const VecTy &av,
                               const VecTy &bv) noexcept {
        acc.accumulate_sqdiff(pred, av, bv);
    }
};

template <typename Ops>
struct SVEMetricComputation<Metric::Kind::DOT_PRODUCT, Ops>
    : public SVEMetricComputationPolicy<Ops> {
    using PredTy = typename Ops::PredTy;
    using AccumulatorGroup = SVEAccumulatorGroup<Ops>;
    using VecTy = typename Ops::VecTy;

    static INLINE void compute(const PredTy &pred, const AccumulatorGroup &acc,
                               const AccumulatorGroup &,
                               const AccumulatorGroup &, const VecTy &av,
                               const VecTy &bv) noexcept {
        acc.accumulate_product(pred, av, bv);
    }
};

template <typename Ops>
struct SVEMetricComputation<Metric::Kind::COSINE_SIMILARITY, Ops>
    : public SVEMetricComputationPolicy<Ops> {
    using PredTy = typename Ops::PredTy;
    using AccumulatorGroup = SVEAccumulatorGroup<Ops>;
    using VecTy = typename Ops::VecTy;
    using ResultTy = typename Ops::ResultTy;

    static INLINE void compute(const PredTy &pred, const AccumulatorGroup &acc,
                               const AccumulatorGroup &a_norm_acc,
                               const AccumulatorGroup &b_norm_acc,
                               const VecTy &av, const VecTy &bv) noexcept {
        acc.accumulate_product(pred, av, bv);
        a_norm_acc.accumulate_product(pred, av, av);
        b_norm_acc.accumulate_product(pred, bv, bv);
    }

    static INLINE ResultTy finish(const ResultTy &acc,
                                  const ResultTy &a_norm_acc,
                                  const ResultTy &b_norm_acc) noexcept {
        return acc / static_cast<ResultTy>(std::sqrt(a_norm_acc * b_norm_acc));
    }
};
} // namespace

namespace VSS {

template <size_t N, typename Ty, Metric::Kind Kind>
Metric::ResultTy similarity_sve(const Embedding<N, Ty> &a,
                                const Embedding<N, Ty> &b) noexcept {
    using Ops = SVEOps<Ty>;
    using MetricComputation = SVEMetricComputation<Kind, Ops>;
    using AccVecTy = typename Ops::AccVecTy;

    const uint64_t VL = Ops::vector_length();

    // Some metric computations need more than one accumulator because the data
    // type has been widened in order to avoid overflow or loss of precision.
    // The compiler will remove any unused accumulators for the given data type
    // and metric.

    // The main accumulators for the metric computation.
    AccVecTy acc0 = Ops::zero_acc();
    AccVecTy acc1 = Ops::zero_acc();
    AccVecTy acc2 = Ops::zero_acc();
    AccVecTy acc3 = Ops::zero_acc();
    const auto acc = SVEAccumulatorGroup<Ops>::bind(acc0, acc1, acc2, acc3);

    // a norm --- only used for COSINE_SIMILARITY.
    AccVecTy a_norm_acc0 = Ops::zero_acc();
    AccVecTy a_norm_acc1 = Ops::zero_acc();
    AccVecTy a_norm_acc2 = Ops::zero_acc();
    AccVecTy a_norm_acc3 = Ops::zero_acc();
    const auto a_norm_acc = SVEAccumulatorGroup<Ops>::bind(
        a_norm_acc0, a_norm_acc1, a_norm_acc2, a_norm_acc3);

    // b norm --- only used for COSINE_SIMILARITY.
    AccVecTy b_norm_acc0 = Ops::zero_acc();
    AccVecTy b_norm_acc1 = Ops::zero_acc();
    AccVecTy b_norm_acc2 = Ops::zero_acc();
    AccVecTy b_norm_acc3 = Ops::zero_acc();
    const auto b_norm_acc = SVEAccumulatorGroup<Ops>::bind(
        b_norm_acc0, b_norm_acc1, b_norm_acc2, b_norm_acc3);

    // Main SVE loop: process as many full vector-length chunks as possible.
    // SVEMetricComputation keeps the arithmetic metric-specific while this loop
    // stays focused on loading and advancing through the embedding.
    for (uint64_t i = 0; i < N; i += VL) {
        const auto active = Ops::while_lt(i, N);
        const auto av = Ops::load(active, a.data() + i);
        const auto bv = Ops::load(active, b.data() + i);

        MetricComputation::compute(active, acc, a_norm_acc, b_norm_acc, av, bv);
    }

    // Now reduce and finalize the metric computation.
    return MetricComputation::finish(acc.reduce(), a_norm_acc.reduce(),
                                     b_norm_acc.reduce());
}

#define VSS_INSTANTIATE_SIMILARITY_SVE(DIMENSION, TYPE)                        \
    template Metric::ResultTy                                                  \
    similarity_sve<DIMENSION, TYPE, Metric::EUCLIDEAN_DISTANCE>(               \
        const Embedding<DIMENSION, TYPE> &a,                                   \
        const Embedding<DIMENSION, TYPE> &b) noexcept;                         \
                                                                               \
    template Metric::ResultTy                                                  \
    similarity_sve<DIMENSION, TYPE, Metric::DOT_PRODUCT>(                      \
        const Embedding<DIMENSION, TYPE> &a,                                   \
        const Embedding<DIMENSION, TYPE> &b) noexcept;                         \
                                                                               \
    template Metric::ResultTy                                                  \
    similarity_sve<DIMENSION, TYPE, Metric::COSINE_SIMILARITY>(                \
        const Embedding<DIMENSION, TYPE> &a,                                   \
        const Embedding<DIMENSION, TYPE> &b) noexcept;

VSS_INSTANTIATE_SIMILARITY_SVE(3, float)
VSS_INSTANTIATE_SIMILARITY_SVE(67, float)
VSS_INSTANTIATE_SIMILARITY_SVE(384, float)
VSS_INSTANTIATE_SIMILARITY_SVE(768, float)
VSS_INSTANTIATE_SIMILARITY_SVE(1024, float)
VSS_INSTANTIATE_SIMILARITY_SVE(3072, float)

VSS_INSTANTIATE_SIMILARITY_SVE(3, int8_t)
VSS_INSTANTIATE_SIMILARITY_SVE(259, int8_t)
VSS_INSTANTIATE_SIMILARITY_SVE(384, int8_t)
VSS_INSTANTIATE_SIMILARITY_SVE(768, int8_t)
VSS_INSTANTIATE_SIMILARITY_SVE(1024, int8_t)
VSS_INSTANTIATE_SIMILARITY_SVE(3072, int8_t)

VSS_INSTANTIATE_SIMILARITY_SVE(3, _Float16)
VSS_INSTANTIATE_SIMILARITY_SVE(131, _Float16)
VSS_INSTANTIATE_SIMILARITY_SVE(384, _Float16)
VSS_INSTANTIATE_SIMILARITY_SVE(768, _Float16)
VSS_INSTANTIATE_SIMILARITY_SVE(1024, _Float16)
VSS_INSTANTIATE_SIMILARITY_SVE(3072, _Float16)

#undef VSS_INSTANTIATE_SIMILARITY_SVE
} // namespace VSS
#else
#warning SVE support not available on this target: VSS::similarity_sve will not be available !
#endif // __ARM_FEATURE_SVE
