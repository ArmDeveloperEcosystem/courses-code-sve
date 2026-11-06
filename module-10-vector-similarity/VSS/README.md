<!--
  SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
  SPDX-License-Identifier: BSD-3-Clause-Clear
-->

# Vector Similarity Search

This task dives into Vector SImilarity Search (VSS).

Vector similarity search - as used in systems like [FAISS](https://faiss.ai/), finds the nearest vectors to a query embedding in high-dimensional space by efficiently computing approximate or exact distance metrics across large vector datasets.

Learners are introduced to dot-product accumulation, reduction, and comparison patterns through a distance calculation between vectors. Using a vector-similarity style workload, they learn the core instruction patterns behind nearest-neighbor search, including operations such as `svdot` and `svadd` that repeat across many high-dimensional comparisons.

## Why does it matter ?

It’s a foundational technology behind modern AI systems such as semantic search, Retrieval-Augmented Generation (RAG), recommendation engines, image search, and embedding databases.

It's computationally intensive and optimizations will bring sensible and visible improvements.

It is now part of the major databases and libraries:

### Libraries

- FAISS (Meta)
- Annoy (Spotify)
- HNSWlib
- ScaNN (Google)

### Vector Databases

- Qdrant
- Milvus
- Pinecone
- Weaviate
- Chroma

### Traditional Databases with Vector Support

- PostgreSQL + pgvector
- Elasticsearch
- OpenSearch
- MongoDB Atlas Vector Search

## The core idea behind Vector Similarity Search

Instead of representing a document as words, it is represented as a vector (an array of numbers):

```txt
"How to optimize Arm CPU performance"
→ [0.12, -0.34, 0.91, ..., 0.45]

"Improving performance on AArch64 processors"
→ [0.15, -0.31, 0.89, ..., 0.47]
```

An embedding model creates these vectors such that semantically similar content ends up close together in a high-dimensional space.

A query is embedded the same way:

```txt
"speed up arm64 code"
→ [0.13, -0.29, 0.87, ..., 0.43]
```

The search system then finds the stored vectors nearest to the query vector.

### Why not keyword search ?

Traditional search:

```txt
Query: "arm64 optimization"

Document:
"Improving AArch64 performance"
```

A keyword search might miss this because “arm64” and “AArch64” are different strings.

Vector search captures meaning:

```txt
arm64 ≈ AArch64
optimization ≈ performance tuning
```

and returns the document.

In real world systems, an hybrid approach combining vector search + keyword search is often done as the exact terms used in the keywords often matter. For example, keywords are still valuable for: part numbers, error codes, API names, Product IDs, ...

Example:

```txt
Neoverse V3 SVE2 latency
```

## Similarity metrics

Once vectors are generated, similarity is computed using a distance metric.

### Euclidean Distance

Physical distance:

```txt
distance = √Σ(Ai - Bi)²
```

Smaller is better.

Less common for modern text retrieval.

### Dot Product

Often used when embeddings are normalized during training.

```txt
score = A · B
```

Popular in recommendation systems.

### Cosine Similarity

Most common.

Measures angle between vectors:

```txt
similarity = (A · B) / (|A||B|)
```

Range:
- `1.0`: identical direction
- `0.0`: unrelated
- `-1.0`: opposite

Example:

```txt
Query: "arm cpu optimization"

Doc A: "AArch64 performance tuning"
Cosine = 0.92

Doc B: "French cooking recipes"
Cosine = 0.08
```

It can be written elegantly as three dot products:

```C++
const auto dot = dot_product(a, b);
const auto a_norm = dot_product(a, a);
const auto b_norm = dot_product(b, b);
const auto similarity = dot / std::sqrt(a_norm * b_norm);
```

But at the time of writing, neither GCC nor clang do loop fusion, that is to say merge the 3 loops into a single one and load the a and b vectors only once --- instead of 3 times. We have thus chosen to fuse the loop directly in the source code.

## Going further, finding the nearest vector

So far, once the metric has been computed, the system will have to find what corresponds to the nearest neighbors. This part of the problem is not adressed here, but a high level description of how it is done is included in the reader's interest.

### Exact k-Nearest Neighbors (kNN)

The simplest (naive !) approach looks like:

```txt
For every vector:
    compute similarity
Sort
Return top k
```

However, it has a high computational complexity per query, which is fine for thousands of vectors ... but not so fine for millions or billions of them.

### Approximate Nearest Neighbor (ANN)

Production systems use ANN indexes instead.

With the Approximate Nearest Neighbor (ANN) algorithm, the cores idea is not to compare against every and each vector, but rather to search only promising _regions_ which are clusters of vectors. This makes the operation 10-1000x faster, and makes vector databases practical.

## What real systems do use ?

### Embeddings dimensions

In practice, embedding dimensions have converged into a surprisingly narrow range. Most production systems today use 384 to 3072 dimensions, with 768–1536 dimensions being the sweet spot.

Common Dimensions in Real Systems:

| Dimensions | Typical Models                | Usage                   |
|:----------:|:------------------------------|:------------------------|
| 128        | Older recommendation systems  | Legacy ANN/search       |
| 256        | Lightweight retrieval         | Mobile/edge deployments |
| 384        | MiniLM, BGE-small             | Fast semantic search    |
| 512        | Some proprietary models       | Balanced retrieval      |
| 768        | BERT, E5-base, BGE-base       | Very common             |
| 1024       | Modern retrieval models       | Increasingly common     |
| 1536       | OpenAI text-embedding-3-small | Extremely common in RAG |
| 3072       | OpenAI text-embedding-3-large | High-quality retrieval  |
| 4096+      | Rare                          | Specialized research    |

### Data type used for similarity computations

Roughly:

| Scale            | Typical Storage |
|:-----------------|:---------------:|
| < 1M vectors     | FP32            |
| 1M–100M vectors  | FP16 or INT8    |
| 100M–10B vectors | PQ / INT8       |
| Internet-scale   | PQ + reranking  |

PQ: Product Quantization

## Performance considerations

For a RAG system, key parameters are:
- Embedding dimensions:
    - 384: smaller, faster
    - 768: balanced
    - 1024: common higher-quality retrieval
    - 1536: better quality, more memory
    - 3072: high-quality retrieval, more computation
- Embedding element type:
    - `float`: simple baseline for the course
    - `_Float16`: common compact floating-point format
    - `int8_t`: common quantized format with fast SVE dot-product support
- Chunk size, typically 200 - 1000 tokens:
    - too small: loses context
    - too large: hurts retrieval precision
- Top-k, typically 5 - 20:
    - too small: misses information
    - too large: pollutes context window

## Implementation Notes

This task will rely on C++ programming patterns, mainly with templates, in order to be able to write code once yet be able to use the different types (`float`, `_Float16` and `int8_t`) with the different metrics (euclidean distance, dot product, cosine similarity, ...). This makes use of the traits and policies C++ programming pattern. This definitely adds some layers of abstraction, but is worth when the different building blocks need to be composed into one specific solution to solve a problem without reinventing the wheel every time. This is a core idea used in many C++ libraries, including the stand one.

The task uses 2 main data structures and top level functions to compute similarity (in `VSS.h`):
- `Metric`, which provides the metric kinf to compute as well as its type (always a `float` in this course):
```C++
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
```

- `Embedding`, to represent embeddings.
```C++
// In real life, together with some metadata, an embedding contains some sort of
// array of N elements of type Ty. The number of elements is known a priori at
// compile time, so we make it a template parameter so the compiler can optimize
// it fully --- this matters when one has to process millions of embeddings in a
// tight loop. For this course, we do not fully model an Embedding with a class
// of its own, as we do not care about the metadata, and we instead
// just reuse the std::array container.
template <size_t N, typename Ty> using Embedding = std::array<Ty, N>;
```

- `similarity_ref` and `similarity_sve` (if SVE is available) to perform the similarity computation on 2 embeddings.
```C++
// Portable reference implementation.
template <size_t N, typename Ty, Metric::Kind Kind>
Metric::ResultTy similarity_ref(const Embedding<N, Ty> &a,
                                const Embedding<N, Ty> &b) noexcept;

#ifdef __aarch64__
#ifdef __ARM_FEATURE_SVE
// SVE implementation for the same metric selector. It is available only
// when this translation unit is built for AArch64 with SVE enabled.
template <size_t N, typename Ty, Metric::Kind Kind>
Metric::ResultTy similarity_sve(const Embedding<N, Ty> &a,
                                const Embedding<N, Ty> &b) noexcept;
#endif // __ARM_FEATURE_SVE
#endif // __aarch64__
```
Note that none of `similarity_ref` and `similarity_sve` have their implementation declared in `VSS.h`. This is made on purpose. The specific implementations, which are in finite numbers (types x metric x embedding size) are all instantiated in `VSS_ref.cpp` and `VSS_sve.cpp`. This pattern is useful when one does not want to provide full genericity and restrict the usage of the library to known and validated cases for example. It can also spare compilation time because users of the library to have to compile the templates on their end.

The reference and SVE implementations of `similarity_*` are very similar at a high level. Whichever metric is used, the computation looks like, in pseudo algorithmic language:

```txt
similarity:
    Initialize accumulator(s)
    For each element in the embedding:
        compute metric part to the accumulators
    Reduce the accumulators
    Return metric
```

### Reference implementation

This is expressed almost as-is in C++, using the traits and policies programming pattern:
```C++
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
```

Note how the concerns have been separated in the above code. The loop is common for all metrics. `MetricComputation<Kind, Ty>` implements the actual metric computation by defining the meaning of `compute` and `reduce` for each metric and type. For example, the cosine similarity has a reduction step, because the some further processing need to be done with the accumulator before the metric is returned, whereas for dot product or the euclidean distance, the accumulator can be directly returned.

This is how the metric computation policies are written:
```C++
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
```

All `MetricComputation` policies share a same `MetricComputationPolicy` base class, that defines the required interface (`compute`, `reduce`, with sensible default implementation) as well as the types to be used for the metric computation:
- the embedding elements' type.
- the accumulator type, which does not have to be the same as the the embeddings elements' type or the metric computation return type. This is particularly important to ensure numerical accuracy and avoid overflows.
- the metric computation return type.
This definition of types are the _traits_, and they are defined near the top of `VSS_ref.cpp`:

```C++
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
```

Note how the `MetricComputation` specializations for dot product (`struct MetricComputation<Metric::Kind::DOT_PRODUCT, ElementTy>`) and euclidean distance (`struct MetricComputation<Metric::Kind::EUCLIDEAN_DISTANCE, ElementTy>`) only define the core of the computation with their `compute` method and don't even define the `reduce` method (as the base class default method provides all the required functionality).

If you scroll to the end of `VSS_ref.cpp`, you'll see some macro magic with `VSS_INSTANTIATE_SIMILARITY_REF` in order to instantiate (i.e. generate) `similarity_ref` for all the variants that are supported. Although the macro system can be abused, and is not needed, it is still valuable and legitimate to use in some specific cases (like this one) to reduce boiler plate and make sure no case is missed:

```C++
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
```

While the C++ code looks a bit verbose, with some level of abstractions, the compiler does a very good job at flattening the abstraction layers and providing an optimized assembly implementation. For the software developers, once the the abstraction layers step is well understood, this allows to reduce the complexity of the code base to the minimum for adding new metrics. This design allows to add new metrics very easily (as long as their computation can fit in `similarity_`): it's just adding a new `enum` field in `struct Metric::Kind` and specializing the `MetricComputation` template for that new metric, et voilà !

### SVE implementation

The SVE implementation uses exactly the same overall approach, with traits and policies, to show how to perform the extensibility of the codebase _while using intrinsics_.

The intrinsics provided by ACLE allow some form of genericity when the compiler can guess the exact intrinsic from operands' types. While a very useful (and used) feature, the compiler may not always be able to infer alone the proper types, so this SVE implementation uses an `SVEOps` class policy that provides somehow higher level implementations of the operations that each metric has to perform, e.g. for metric operations over the `float` type:

```C++
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
```

Note how all types for the intermediate computations are defined in `SVEOpsBase` --- this allows the compiler to pick the proper intrinsic for `svsub_x` and `svmla_m` in `accumulate_sqdiff(...)` and ensure consistent typing accross the different operations for example. In the above code, `SVEOps<float>` provides implementations for:
- `vector_length` to get the vector length --- the compiler can not guess on its own that it has to use `svcntw` (instead of `svcntb` for example).
- `all_acc_true``
- `zero_vec` and `zero_acc`, to zero a vector of input or a vector of accumulators
- `load`, a predicated load operations from memory
- `while_lt`, to compute the operation predicate
- `accumulate_sqdiff`, to accumulate the squared difference of 2 vectors
- `accumulate_product`, to accumulate the dot-wise product of 2 vectors
- `reduce_acc`, to reduce a vector of accumulators to a single value, typically to return the metric computation.

This additional layer of abstraction (or indirection) allows to use different code sequences in the higher level operations like `accumulate_sqdiff(...)` or `accumulate_product(...)` depending on the argument type. For example, the above implementation is straightforward when working with `float` elements ... but becomes slightly more complex to support `_Float16`:

```C++
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
```

You can note along the way that SVE2 makes the conversion from f16 to f32 much easier for the even lanes with a single instruction, rather than 2 with SVE in method `accumulate_sqdiff`, and that for `accumulate_product`, SVE2 enables a much shorter implementation of the product accumulation (just 2 instructions with SVE2 !)

Another interesting aspect is that depending on the type selected for a metric, the size of the accumulator implies that several vector registers may have to be used. For example, the intermediate computations when doing the dot product of `int8` data need to use a larger data type: the processing of vectors of input elements requires using several registers to hold the corresponding accumulators (because their types are larger then the input types). An additional complexity is that the accumulators can not be part of _state_ because as SVE introduces sizeless types, they can not be part of a struct (or class) data members --- they have to be passed around as arguments. In order to simplify the writing, the code base uses an `SVEAccumulatorGroup` to expose a _logical_ accumulator group rather than several individual accumulators. The compiler will however do a perfect job at removing the unused arguments from the functions:

```C++
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
```

With this `SVEOps` and `SVEAccumulatorGroup` in place, it becomes easy to express the different metrics' computation policies for euclidean distance, dot product and cosine similarity:

```C++
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
```

Finally, `` can be implemented in high level terms with:

```C++
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
```

Note:
- at this level we can see the algorithm structure that we saw back in the reference / scalar case:
```txt
similarity:
    Initialize accumulator(s)
    For each element in the embedding:
        compute metric part to the accumulators
    Reduce the accumulators
    Return metric
```
- the compiler will remove the unused accumulators, depending on the data type and the metric used when instantiating the template.

Last, but not least, `VSS_sve.cpp` ends with the explicit instantiation of `similarity` for all metrics, data types and embeddings size that we use:

```C++
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
```

Genericity with template meta programming allows to express the algorithm at a high level while letting the compiler handle composition of the different aspects (data types, metrics, embeddings size, ...) as well as the simplification and optimization of each specific instantiation. 3 types x 3 metrics x 6 dimensions : 54 specific instances have been generated for us, instead of having to write each one of them individually. This uses the the compiler for what it is: a code generation tool.

## Configure and build

See the [build guide](../../docs/building.md) for platform requirements and configuration commands. Run the following commands from the repository root.

Build the VSS executable with:

```bash
cmake --build build --config Release --target VSS
```

## Using VSS

You can experiment with `VSS`. It takes as inputs 2 vectors and prints the different metrics:

```sh
$ ./build/VSS/Release/VSS "1 2 3 4 5 6 7 8" "8 7 6 5 4 3 2 1"
Vector1[8]: [1, 2, 3, 4, 5, 6, 7, 8]
Vector2[8]: [8, 7, 6, 5, 4, 3, 2, 1]
Euclidean distance: 168
Dot product: 120
Cosine similarity: 0.588235
```

## Testing

```sh
$ cmake --build build --config Release --target test-VSS
ninja: Entering directory `build/'
[1/1] cd /home/ubuntu/SVE-task-based-course.git/build/test/Release && /home/ubuntu/SVE-task-based-course.git/build/test/Release/test --gtest_color=yes --gtest_filter="VSS*"
Note: Google Test filter = VSS*
[==========] Running 34 tests from 3 test suites.
[----------] Global test environment set-up.
[----------] 7 tests from VSS
[ RUN      ] VSS.constructs_from_contiguous_sequences
[       OK ] VSS.constructs_from_contiguous_sequences (0 ms)
[ RUN      ] VSS.rejects_wrong_sized_sequences
[       OK ] VSS.rejects_wrong_sized_sequences (0 ms)
[ RUN      ] VSS.exposes_elements
[       OK ] VSS.exposes_elements (0 ms)
...
[ RUN      ] VSS.supports_fp16_metrics_sve
[       OK ] VSS.supports_fp16_metrics_sve (0 ms)
[----------] 7 tests from VSS (0 ms total)

[----------] 15 tests from VSS/metric_ref
[ RUN      ] VSS/metric_ref.matches_expected_value/euclidean_distance_8_fp32
[       OK ] VSS/metric_ref.matches_expected_value/euclidean_distance_8_fp32 (0 ms)
[ RUN      ] VSS/metric_ref.matches_expected_value/euclidean_distance_384_fp32
[       OK ] VSS/metric_ref.matches_expected_value/euclidean_distance_384_fp32 (0 ms)
[ RUN      ] VSS/metric_ref.matches_expected_value/euclidean_distance_768_fp32
[       OK ] VSS/metric_ref.matches_expected_value/euclidean_distance_768_fp32 (0 ms)
[ RUN      ] VSS/metric_ref.matches_expected_value/euclidean_distance_1024_fp32
[       OK ] VSS/metric_ref.matches_expected_value/euclidean_distance_1024_fp32 (0 ms)
[ RUN      ] VSS/metric_ref.matches_expected_value/euclidean_distance_3072_fp32
[       OK ] VSS/metric_ref.matches_expected_value/euclidean_distance_3072_fp32 (0 ms)
[ RUN      ] VSS/metric_ref.matches_expected_value/dot_product_8_fp32
[       OK ] VSS/metric_ref.matches_expected_value/dot_product_8_fp32 (0 ms)
[ RUN      ] VSS/metric_ref.matches_expected_value/dot_product_384_fp32
[       OK ] VSS/metric_ref.matches_expected_value/dot_product_384_fp32 (0 ms)
[ RUN      ] VSS/metric_ref.matches_expected_value/dot_product_768_fp32
[       OK ] VSS/metric_ref.matches_expected_value/dot_product_768_fp32 (0 ms)
[ RUN      ] VSS/metric_ref.matches_expected_value/dot_product_1024_fp32
[       OK ] VSS/metric_ref.matches_expected_value/dot_product_1024_fp32 (0 ms)
[ RUN      ] VSS/metric_ref.matches_expected_value/dot_product_3072_fp32
[       OK ] VSS/metric_ref.matches_expected_value/dot_product_3072_fp32 (0 ms)
[ RUN      ] VSS/metric_ref.matches_expected_value/cosine_similarity_8_fp32
[       OK ] VSS/metric_ref.matches_expected_value/cosine_similarity_8_fp32 (0 ms)
[ RUN      ] VSS/metric_ref.matches_expected_value/cosine_similarity_384_fp32
[       OK ] VSS/metric_ref.matches_expected_value/cosine_similarity_384_fp32 (0 ms)
[ RUN      ] VSS/metric_ref.matches_expected_value/cosine_similarity_768_fp32
[       OK ] VSS/metric_ref.matches_expected_value/cosine_similarity_768_fp32 (0 ms)
[ RUN      ] VSS/metric_ref.matches_expected_value/cosine_similarity_1024_fp32
[       OK ] VSS/metric_ref.matches_expected_value/cosine_similarity_1024_fp32 (0 ms)
[ RUN      ] VSS/metric_ref.matches_expected_value/cosine_similarity_3072_fp32
[       OK ] VSS/metric_ref.matches_expected_value/cosine_similarity_3072_fp32 (0 ms)
[----------] 15 tests from VSS/metric_ref (0 ms total)

[----------] 12 tests from VSS/metric_sve
[ RUN      ] VSS/metric_sve.matches_reference_value/euclidean_distance_384_fp32
[       OK ] VSS/metric_sve.matches_reference_value/euclidean_distance_384_fp32 (0 ms)
[ RUN      ] VSS/metric_sve.matches_reference_value/euclidean_distance_768_fp32
[       OK ] VSS/metric_sve.matches_reference_value/euclidean_distance_768_fp32 (0 ms)
[ RUN      ] VSS/metric_sve.matches_reference_value/euclidean_distance_1024_fp32
[       OK ] VSS/metric_sve.matches_reference_value/euclidean_distance_1024_fp32 (0 ms)
[ RUN      ] VSS/metric_sve.matches_reference_value/euclidean_distance_3072_fp32
[       OK ] VSS/metric_sve.matches_reference_value/euclidean_distance_3072_fp32 (0 ms)
[ RUN      ] VSS/metric_sve.matches_reference_value/dot_product_384_fp32
[       OK ] VSS/metric_sve.matches_reference_value/dot_product_384_fp32 (0 ms)
[ RUN      ] VSS/metric_sve.matches_reference_value/dot_product_768_fp32
[       OK ] VSS/metric_sve.matches_reference_value/dot_product_768_fp32 (0 ms)
[ RUN      ] VSS/metric_sve.matches_reference_value/dot_product_1024_fp32
[       OK ] VSS/metric_sve.matches_reference_value/dot_product_1024_fp32 (0 ms)
[ RUN      ] VSS/metric_sve.matches_reference_value/dot_product_3072_fp32
[       OK ] VSS/metric_sve.matches_reference_value/dot_product_3072_fp32 (0 ms)
[ RUN      ] VSS/metric_sve.matches_reference_value/cosine_similarity_384_fp32
[       OK ] VSS/metric_sve.matches_reference_value/cosine_similarity_384_fp32 (0 ms)
[ RUN      ] VSS/metric_sve.matches_reference_value/cosine_similarity_768_fp32
[       OK ] VSS/metric_sve.matches_reference_value/cosine_similarity_768_fp32 (0 ms)
[ RUN      ] VSS/metric_sve.matches_reference_value/cosine_similarity_1024_fp32
[       OK ] VSS/metric_sve.matches_reference_value/cosine_similarity_1024_fp32 (0 ms)
[ RUN      ] VSS/metric_sve.matches_reference_value/cosine_similarity_3072_fp32
[       OK ] VSS/metric_sve.matches_reference_value/cosine_similarity_3072_fp32 (0 ms)
[----------] 12 tests from VSS/metric_sve (0 ms total)

[----------] Global test environment tear-down
[==========] 34 tests from 3 test suites ran. (0 ms total)
[  PASSED  ] 34 tests.
```

## Profiling

The benchmark covers embedding sizes 384, 768, 1024 and 3072 for each metric
and for `float`, `int8_t` and `_Float16` embeddings. It includes both the
portable reference implementation and the SVE implementation when SVE is
available.

```sh
$ cmake --build build --config Release --target benchmark-VSS
...
Running /home/ubuntu/SVE-task-based-course.git/build/benchmark/Release/benchmark
Run on (4 X 2000 MHz CPU s)
CPU Caches:
  L1 Data 64 KiB (x4)
  L1 Instruction 64 KiB (x4)
  L2 Unified 2048 KiB (x4)
  L3 Unified 36864 KiB (x1)
Load Average: 0.56, 0.21, 0.08
-----------------------------------------------------------------------------------------------
Benchmark                                     Time             CPU   Iterations UserCounters...
-----------------------------------------------------------------------------------------------
VSS_euclidean_distance_ref/384_fp32         223 ns          223 ns      3135319 Metric/s=4.47911M/s
VSS_dot_product_ref/384_fp32                224 ns          224 ns      3124578 Metric/s=4.46332M/s
VSS_cosine_similarity_ref/384_fp32          624 ns          624 ns      1123528 Metric/s=1.60221M/s
VSS_euclidean_distance_ref/384_int8        19.6 ns         19.6 ns     35852517 Metric/s=51.1028M/s
VSS_dot_product_ref/384_int8               6.16 ns         6.16 ns    113463759 Metric/s=162.276M/s
VSS_cosine_similarity_ref/384_int8         14.1 ns         14.1 ns     49211404 Metric/s=70.8248M/s
VSS_euclidean_distance_ref/384_fp16         232 ns          232 ns      3010112 Metric/s=4.30111M/s
VSS_dot_product_ref/384_fp16                229 ns          229 ns      3053860 Metric/s=4.3621M/s
VSS_cosine_similarity_ref/384_fp16          619 ns          619 ns      1130931 Metric/s=1.61549M/s
VSS_euclidean_distance_ref/768_fp32         464 ns          464 ns      1508795 Metric/s=2.15528M/s
VSS_dot_product_ref/768_fp32                465 ns          465 ns      1506223 Metric/s=2.15158M/s
VSS_cosine_similarity_ref/768_fp32         1250 ns         1250 ns       560017 Metric/s=799.71k/s
VSS_euclidean_distance_ref/768_int8        36.8 ns         36.8 ns     19000430 Metric/s=27.1583M/s
VSS_dot_product_ref/768_int8               12.1 ns         12.1 ns     57632130 Metric/s=82.3311M/s
VSS_cosine_similarity_ref/768_int8         27.2 ns         27.2 ns     25633105 Metric/s=36.7247M/s
VSS_euclidean_distance_ref/768_fp16         473 ns          473 ns      1478840 Metric/s=2.11285M/s
VSS_dot_product_ref/768_fp16                470 ns          470 ns      1489641 Metric/s=2.12785M/s
VSS_cosine_similarity_ref/768_fp16         1238 ns         1238 ns       565448 Metric/s=807.771k/s
VSS_euclidean_distance_ref/1024_fp32        624 ns          624 ns      1121128 Metric/s=1.60162M/s
VSS_dot_product_ref/1024_fp32               625 ns          625 ns      1119782 Metric/s=1.59962M/s
VSS_cosine_similarity_ref/1024_fp32        1667 ns         1667 ns       419979 Metric/s=599.978k/s
VSS_euclidean_distance_ref/1024_int8       62.7 ns         62.7 ns     11166267 Metric/s=15.9527M/s
VSS_dot_product_ref/1024_int8              16.3 ns         16.3 ns     42949521 Metric/s=61.3261M/s
VSS_cosine_similarity_ref/1024_int8        32.6 ns         32.6 ns     21421826 Metric/s=30.6762M/s
VSS_euclidean_distance_ref/1024_fp16        634 ns          634 ns      1103903 Metric/s=1.57686M/s
VSS_dot_product_ref/1024_fp16               631 ns          631 ns      1110076 Metric/s=1.58575M/s
VSS_cosine_similarity_ref/1024_fp16        1651 ns         1651 ns       424004 Metric/s=605.704k/s
VSS_euclidean_distance_ref/3072_fp32       1908 ns         1908 ns       366830 Metric/s=524.011k/s
VSS_dot_product_ref/3072_fp32              1909 ns         1909 ns       366656 Metric/s=523.757k/s
VSS_cosine_similarity_ref/3072_fp32        5002 ns         5002 ns       139997 Metric/s=199.905k/s
VSS_euclidean_distance_ref/3072_int8        185 ns          185 ns      3787212 Metric/s=5.41043M/s
VSS_dot_product_ref/3072_int8              49.3 ns         49.3 ns     14386463 Metric/s=20.2648M/s
VSS_cosine_similarity_ref/3072_int8        79.1 ns         79.1 ns      8861255 Metric/s=12.6453M/s
VSS_euclidean_distance_ref/3072_fp16       1918 ns         1918 ns       364964 Metric/s=521.48k/s
VSS_dot_product_ref/3072_fp16              1914 ns         1914 ns       365651 Metric/s=522.5k/s
VSS_cosine_similarity_ref/3072_fp16        4951 ns         4951 ns       141386 Metric/s=201.979k/s
VSS_euclidean_distance_sve/384_fp32        57.4 ns         57.4 ns     12183656 Metric/s=17.4095M/s
VSS_dot_product_sve/384_fp32               55.3 ns         55.3 ns     12575638 Metric/s=18.083M/s
VSS_cosine_similarity_sve/384_fp32         68.8 ns         68.8 ns     10172107 Metric/s=14.5425M/s
VSS_euclidean_distance_sve/384_int8        23.7 ns         23.7 ns     29570279 Metric/s=42.2034M/s
VSS_dot_product_sve/384_int8               11.8 ns         11.8 ns     59209872 Metric/s=84.5875M/s
VSS_cosine_similarity_sve/384_int8         17.8 ns         17.8 ns     39211691 Metric/s=56.184M/s
VSS_euclidean_distance_sve/384_fp16         125 ns          125 ns      5587558 Metric/s=7.98239M/s
VSS_dot_product_sve/384_fp16               28.3 ns         28.3 ns     24744432 Metric/s=35.3584M/s
VSS_cosine_similarity_sve/384_fp16         49.3 ns         49.3 ns     14241825 Metric/s=20.2921M/s
VSS_euclidean_distance_sve/768_fp32         126 ns          126 ns      5545929 Metric/s=7.92344M/s
VSS_dot_product_sve/768_fp32                123 ns          123 ns      5696252 Metric/s=8.14034M/s
VSS_cosine_similarity_sve/768_fp32          138 ns          138 ns      5090008 Metric/s=7.27146M/s
VSS_euclidean_distance_sve/768_int8        46.9 ns         46.9 ns     14916953 Metric/s=21.3048M/s
VSS_dot_product_sve/768_int8               22.6 ns         22.6 ns     31015435 Metric/s=44.3104M/s
VSS_cosine_similarity_sve/768_int8         33.8 ns         33.8 ns     20566121 Metric/s=29.6113M/s
VSS_euclidean_distance_sve/768_fp16         250 ns          250 ns      2799528 Metric/s=3.99931M/s
VSS_dot_product_sve/768_fp16               58.1 ns         58.1 ns     12050333 Metric/s=17.2139M/s
VSS_cosine_similarity_sve/768_fp16         85.5 ns         85.5 ns      8198321 Metric/s=11.6935M/s
VSS_euclidean_distance_sve/1024_fp32        172 ns          172 ns      4068052 Metric/s=5.81171M/s
VSS_dot_product_sve/1024_fp32               169 ns          169 ns      4127778 Metric/s=5.90129M/s
VSS_cosine_similarity_sve/1024_fp32         183 ns          183 ns      3817526 Metric/s=5.45342M/s
VSS_euclidean_distance_sve/1024_int8       62.9 ns         62.9 ns     11125004 Metric/s=15.887M/s
VSS_dot_product_sve/1024_int8              29.7 ns         29.7 ns     23542082 Metric/s=33.6296M/s
VSS_cosine_similarity_sve/1024_int8        43.7 ns         43.7 ns     16177215 Metric/s=22.8789M/s
VSS_euclidean_distance_sve/1024_fp16        333 ns          333 ns      2099525 Metric/s=2.99917M/s
VSS_dot_product_sve/1024_fp16              81.0 ns         81.0 ns      8638165 Metric/s=12.3466M/s
VSS_cosine_similarity_sve/1024_fp16         108 ns          108 ns      6470320 Metric/s=9.2407M/s
VSS_euclidean_distance_sve/3072_fp32        568 ns          568 ns      1232253 Metric/s=1.76007M/s
VSS_dot_product_sve/3072_fp32               536 ns          536 ns      1305733 Metric/s=1.86515M/s
VSS_cosine_similarity_sve/3072_fp32         551 ns          551 ns      1271380 Metric/s=1.81647M/s
VSS_euclidean_distance_sve/3072_int8        199 ns          199 ns      3524483 Metric/s=5.02201M/s
VSS_dot_product_sve/3072_int8              87.5 ns         87.5 ns      8002790 Metric/s=11.4329M/s
VSS_cosine_similarity_sve/3072_int8         116 ns          116 ns      6047807 Metric/s=8.6289M/s
VSS_euclidean_distance_sve/3072_fp16       1001 ns         1001 ns       699042 Metric/s=998.656k/s
VSS_dot_product_sve/3072_fp16               265 ns          265 ns      2644901 Metric/s=3.77794M/s
VSS_cosine_similarity_sve/3072_fp16         347 ns          347 ns      2022197 Metric/s=2.88071M/s
```

The results can be summarized in the following table:

| Metric              | Embedding size | Embedding type | Base throughput | SVE throughput | SVE speedup |
|:-------------------:|:--------------:|:--------------:|:---------------:|:--------------:|:-----------:|
| Euclidean distance  |      384       |      fp32      |   4.47911M/s    |   17.4095M/s   |        3.9x |
| Euclidean distance  |      384       |      fp16      |   4.30111M/s    |   7.98239M/s   |        1.9x |
| Euclidean distance  |      384       |      int8      |   51.1028M/s    |   42.2034M/s   |        0.8x |
| Euclidean distance  |      768       |      fp32      |   2.15528M/s    |   7.92344M/s   |        3.7x |
| Euclidean distance  |      768       |      fp16      |   2.11285M/s    |   3.99931M/s   |        1.9x |
| Euclidean distance  |      768       |      int8      |   27.1583M/s    |   21.3048M/s   |        0.8x |
| Euclidean distance  |      1024      |      fp32      |   1.60162M/s    |   5.81171M/s   |        3.6x |
| Euclidean distance  |      1024      |      fp16      |   1.57686M/s    |   2.99917M/s   |        1.9x |
| Euclidean distance  |      1024      |      int8      |   15.9527M/s    |   15.887M/s    |        1.0x |
| Euclidean distance  |      3072      |      fp32      |   524.011k/s    |   1.76007M/s   |        3.4x |
| Euclidean distance  |      3072      |      fp16      |    521.48k/s    |   998.656k/s   |        1.9x |
| Euclidean distance  |      3072      |      int8      |   5.41043M/s    |   5.02201M/s   |        0.9x |
| Dot product         |      384       |      fp32      |   4.46332M/s    |   18.083M/s    |        4.1x |
| Dot product         |      384       |      fp16      |    4.3621M/s    |   35.3584M/s   |        8.1x |
| Dot product         |      384       |      int8      |   162.276M/s    |   84.5875M/s   |        0.5x |
| Dot product         |      768       |      fp32      |   2.15158M/s    |   8.14034M/s   |        3.8x |
| Dot product         |      768       |      fp16      |   2.12785M/s    |   17.2139M/s   |        8.1x |
| Dot product         |      768       |      int8      |   82.3311M/s    |   44.3104M/s   |        0.5x |
| Dot product         |      1024      |      fp32      |   1.59962M/s    |   5.90129M/s   |        3.7x |
| Dot product         |      1024      |      fp16      |   1.58575M/s    |   12.3466M/s   |        7.8x |
| Dot product         |      1024      |      int8      |   61.3261M/s    |   33.6296M/s   |        0.5x |
| Dot product         |      3072      |      fp32      |   523.757k/s    |   1.86515M/s   |        3.6x |
| Dot product         |      3072      |      fp16      |    522.5k/s     |   3.77794M/s   |        7.2x |
| Dot product         |      3072      |      int8      |   20.2648M/s    |   11.4329M/s   |        0.6x |
| Cosine similarity   |      384       |      fp32      |   1.60221M/s    |   14.5425M/s   |        9.1x |
| Cosine similarity   |      384       |      fp16      |   1.61549M/s    |   20.2921M/s   |       12.6x |
| Cosine similarity   |      384       |      int8      |   70.8248M/s    |   56.184M/s    |        0.8x |
| Cosine similarity   |      768       |      fp32      |    799.71k/s    |   7.27146M/s   |        9.1x |
| Cosine similarity   |      768       |      fp16      |   807.771k/s    |   11.6935M/s   |       14.5x |
| Cosine similarity   |      768       |      int8      |   36.7247M/s    |   29.6113M/s   |        0.8x |
| Cosine similarity   |      1024      |      fp32      |   599.978k/s    |   5.45342M/s   |        9.1x |
| Cosine similarity   |      1024      |      fp16      |   605.704k/s    |   9.2407M/s    |       15.3x |
| Cosine similarity   |      1024      |      int8      |   30.6762M/s    |   22.8789M/s   |        0.7x |
| Cosine similarity   |      3072      |      fp32      |   199.905k/s    |   1.81647M/s   |        9.1x |
| Cosine similarity   |      3072      |      fp16      |   201.979k/s    |   2.88071M/s   |       14.3x |
| Cosine similarity   |      3072      |      int8      |   12.6453M/s    |   8.6289M/s    |        0.7x |

A side by side comparison of `similarity_ref<1024, int8_t, EUCLIDEAN_DISTANCE>` to `similarity_sve<1024, int8_t, EUCLIDEAN_DISTANCE>`, compiled with clang version 22.1.8 shows that the reference implementation was vectorized with Neon with a 2x unrolling factor, thus explaining the performance difference --- although the SVE implementation is perfectly tight and compares reasonably well without being unrolled.

<table>
<tr>
<th>Reference implementation (1024, `int8_t`, euclidean distance)</th>
<th>SVE implementation (1024, `int8_t`, euclidean distance)</th>
</tr>

<tr>
<td valign="top">
<pre><code>float VSS::similarity_ref&lt1024ul, signed char, ...:
        add     x8, x1, #0x10
        add     x9, x0, #0x10
        mov     w10, #0x400
        movi    v0.2d, #0x0
        movi    v1.2d, #0x0
        movi    v3.2d, #0x0
        movi    v2.2d, #0x0
        movi    v4.2d, #0x0
        movi    v5.2d, #0x0
        movi    v6.2d, #0x0
        movi    v7.2d, #0x0
+0x2c:  ldp     q16, q17, [x9, #-16]
        ldp     q18, q19, [x8, #-16]
        subs    x10, x10, #0x20
        add     x8, x8, #0x20
        add     x9, x9, #0x20
        ssubl   v20.8h, v16.8b, v18.8b
        ssubl2  v16.8h, v16.16b, v18.16b
        ssubl   v18.8h, v17.8b, v19.8b
        ssubl2  v17.8h, v17.16b, v19.16b
        smlal2  v4.4s, v16.8h, v16.8h
        smlal   v2.4s, v16.4h, v16.4h
        smlal2  v3.4s, v20.8h, v20.8h
        smlal   v1.4s, v20.4h, v20.4h
        smlal2  v7.4s, v17.8h, v17.8h
        smlal   v6.4s, v17.4h, v17.4h
        smlal2  v0.4s, v18.8h, v18.8h
        smlal   v5.4s, v18.4h, v18.4h
        b.ne    &lt;similarity_ref&lt;1024ul, signed char,+0x2c&gt;
        add     v0.4s, v0.4s, v3.4s
        add     v3.4s, v7.4s, v4.4s
        add     v1.4s, v5.4s, v1.4s
        add     v2.4s, v6.4s, v2.4s
        add     v1.4s, v1.4s, v2.4s
        add     v0.4s, v0.4s, v3.4s
        add     v0.4s, v1.4s, v0.4s
        addv    s0, v0.4s
        ucvtf   s0, s0
        ret
        nop
</code></pre>

</td>
<td valign="top">
<pre><code>float VSS::similarity_sve&lt;1024ul, signed char, ...:
        mov      w9, #0x400
        mov      x8, xzr
        movi     v0.2d, #0x0
        movi     v2.2d, #0x0
        movi     v1.2d, #0x0
        movi     v3.2d, #0x0
        nop
        nop
+0x20:  whilelo  p0.b, x8, x9
        ld1b     {z4.b}, p0/z, [x0, x8]
        ld1b     {z5.b}, p0/z, [x1, x8]
        incb     x8
        ssublb   z6.h, z4.b, z5.b
        ssublt   z4.h, z4.b, z5.b
        smlalb   z0.s, z6.h, z6.h
        smlalt   z2.s, z6.h, z6.h
        smlalb   z1.s, z4.h, z4.h
        smlalt   z3.s, z4.h, z4.h
        cmp      x8, #0x400
        b.cc     &lt;similarity_sve&lt;1024ul, signed char,+0x20&gt;
        add      z0.s, z0.s, z2.s
        add      z1.s, z1.s, z3.s
        ptrue    p0.s
        add      z0.s, z0.s, z1.s
        saddv    d0, p0, z0.s
        fmov     x8, d0
        scvtf    s0, x8
        ret
        nop
        nop
        nop
        nop
</code></pre>
</td>
</tr>
</table>

One can also notice that for a given metric, using a smaller datatype provides a boost on the number of metrics computed per seconds as with smaller elements, more are processed in the same chunk. This is true in our case... apart for the euclidean distance. The reason is that the SVE implementation `SVEOps<_Float16>::accumulate_sqdiff` is bit accurate with the reference implementation: the elements are first widened to `svfloat32_t`, then substracted, before being squared and accumulated. If bit accuracy was not a concern, then a much faster SVE2 implementation is possible, where the substraction is performed in the element type (`svfloat16_t`) and `svmlalb_f32`/ `svmlalt_f32` will widen their inputs to `svfloat32_t` before multiplying them and accumulating the result:

<table>
<tr>
<th>Bit accurate SVE implementation </th>
<th>Relaxed SVE implementation</th>
</tr>

<tr>
<td valign="top">
<pre><code>void SVEOps&lt;_Float16&gt;::accumulate_sqdiff(...) {
        const auto all_true = all_acc_true();
        const auto a_lo = svcvt_f32_f16_x(all_true, a);
        const auto b_lo = svcvt_f32_f16_x(all_true, b);
        const auto a_hi = svcvtlt_f32_f16_x(all_true, a);
        const auto b_hi = svcvtlt_f32_f16_x(all_true, b);
        const auto diff_lo = svsub_x(all_true, a_lo, b_lo);
        const auto diff_hi = svsub_x(all_true, a_hi, b_hi);
        acc0 = svmla_x(all_true, acc0, diff_lo, diff_lo);
        acc1 = svmla_x(all_true, acc1, diff_hi, diff_hi);
    }
</code></pre>

</td>
<td valign="top">
<pre><code>void SVEOps&lt;_Float16&gt;::accumulate_sqdiff(...) {
        const auto diff = svsub_x(svptrue_b16(), a, b);
        acc0 = svmlalb_f32(acc0, diff, diff);
        acc1 = svmlalt_f32(acc1, diff, diff);
    }
</code></pre>
</td>
</tr>
</table>

## Misc notes

An important aspect with the VSS codebase as it stands is that it puts together 3 orthogonal concerns:
- how the embeddings data are traversed: the actual traversal, which is handled in the top level `similarity_ref` and `similarity_sve` functions,
- the actual metric computation, for different kinds of metrics: euclidean distance, dot product and cosine similarity,
- support for different data types: `float`, `fp16` and `int8`.

The code structure allows to compose those concerns by using standard C++ patterns: traits, policies.

It's worth comparing the assembly outputs for `metric_sve` and `metric_ref`. Depending on the compiler (`gcc` or `clang`), and their version, the compilers have generated widely different codes. In all cases, the reference implementations have been vectorized, but the above benchmarking shows that using intrinsics still produces better performances.
