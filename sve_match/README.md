<!--
  SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
  SPDX-License-Identifier: BSD-3-Clause-Clear
-->

# Vectorize a multi-value search with SVE2 MATCH

### When does SVE2 MATCH outperform a scalar search as the hit rate changes?

## Module information

| Item | Details |
|---|---|
| Estimated time | 45–60 minutes |
| Target audience | C++ developers learning SVE2 intrinsics |
| Prerequisite knowledge | C++20 templates, spans, and basic SIMD concepts |
| Previous module or dependencies | None |
| Tested platform | Arm Neoverse V2, 128-bit SVE vector length |
| Required architecture features | AArch64 and SVE2 for vector implementations |
| Tested toolchain | GCC 15.2, CMake 3.20 or later, GoogleTest, Google Benchmark 1.9.5 |
| Important configuration | Release build with `-mcpu=native` for benchmarking |

# Introduction

Searching large arrays for specific values is a core operation in
performance-sensitive software, from filtering database records to detecting
tokens in text and images. On Arm Neoverse servers, the SVE2 MATCH instruction
can test multiple input elements against multiple keys with one instruction.

In this module, you will compare three implementations of a search that returns
`true` when any haystack element equals any key:

| Implementation | Purpose |
|---|---|
| `search_scalar` | Portable reference implementation |
| `search_sve2` | Vector-length-agnostic SVE2 MATCH implementation |
| `search_sve2_unrolled` | Four-vector unrolled SVE2 implementation with prefetching |

All three implementations use the same C++20 API and support `std::uint8_t` and
`std::uint16_t`. You will verify correctness and measure how early exits at
different hit probabilities affect the result.

## What will you be able to do?

By the end of this module, you will be able to:

- express contiguous, read-only input with `std::span`;
- constrain templates to the element widths supported by an algorithm;
- use `svmatch` for a multi-key search;
- account for MATCH's 128-bit segment behavior;
- handle arbitrary SVE vector lengths and scalar tails safely;
- compare scalar and SVE2 implementations with Google Benchmark; and
- explain how hit probability changes the useful amount of vector work.

# Before you begin

## What is SVE2 MATCH?

SVE2 extends the Arm Scalable Vector Extension with instructions for additional
general-purpose workloads. Its vector-length-agnostic model lets the same binary
operate on SVE vector lengths from 128 to 2048 bits.

`svmatch` compares each active element in a haystack vector against the active
elements of a key vector. It returns a predicate identifying haystack elements
that occur in the key vector. A subsequent `svptest_any` answers the existence
question used by this task.

MATCH works independently within 128-bit segments. Each segment can therefore
hold at most:

- 16 `std::uint8_t` keys; or
- 8 `std::uint16_t` keys.

For a wider SVE vector, the implementation repeats the complete key set in each
128-bit segment. Larger key sets use the scalar implementation to preserve
correctness. See Arm's [multi-token SVMATCH
overview](https://developer.arm.com/community/arm-community-blogs/b/architectures-and-processors-blog/posts/multi-token-search-strings-svmatch-instruction)
and the [Arm C Language Extensions
specification](https://arm-software.github.io/acle/main/acle.html).

## Prerequisites

You need:

- the software listed in the repository's top-level README;
- an AArch64 SVE2 machine, or the repository's QEMU cross-compilation setup;
- approximately 1 GiB of available memory for the complete benchmark matrix;
- a Release build for meaningful measurements; and
- a terminal opened at the repository root.

# Workflow

## 1. Inspect the C++20 interface

The public functions accept spans rather than a pointer and a separate length:

```cpp
template <typename T>
concept MatchElement =
    std::same_as<T, std::uint8_t> || std::same_as<T, std::uint16_t>;

template <MatchElement T>
[[nodiscard]] bool search_scalar(std::span<const T> haystack,
                                 std::span<const T> keys) noexcept;
```

A span is a non-owning view. Callers can use arrays, vectors, or other contiguous
storage without transferring ownership. The concept rejects unsupported element
types at compile time.

The SVE2 declarations are available only when the compiler enables the required
architecture feature:

```cpp
#ifdef __ARM_FEATURE_SVE2
// search_sve2 and search_sve2_unrolled declarations
#endif
```

`<arm_sve.h>` is included on AArch64; the SVE2 implementations, tests,
benchmarks, and demo calls use the SVE2 feature guard. A non-SVE2 build
therefore retains the portable scalar API.

## 2. Establish the scalar reference

The scalar algorithm checks each haystack value against every key and exits at
the first match:

```cpp
for (const T value : haystack) {
    for (const T key : keys) {
        if (value == key) {
            return true;
        }
    }
}
return false;
```

This implementation defines the expected result for tests and gives the
benchmark a direct baseline.

## 3. Prepare a key vector for MATCH

Some ACLE intrinsics cannot infer an element width from their arguments.
`lane_count<T>()` and `all_lanes<T>()` use `if constexpr` to select `svcntb` or
`svcnth`, and `svptrue_b8` or `svptrue_b16`. This is the same technique used by
the histogram task to keep one algorithm template while selecting typed
intrinsics at compile time.

The key buffer is large enough for the architectural maximum SVE vector length.
The keys repeat across its active portion:

```cpp
for (size_t lane = 0; lane < lanes; ++lane) {
    buffer[lane] = keys[lane % keys.size()];
}
```

Because the supported key count never exceeds one 128-bit segment, every segment
contains every key even when the vector is wider than 128 bits.

## 4. Search one vector at a time

The basic SVE2 loop loads a full vector, performs MATCH, and tests the returned
predicate:

```cpp
const auto values = svld1(active, haystack.data() + offset);
const auto matches = svmatch(active, values, key_vector);
if (svptest_any(active, matches)) {
    return true;
}
```

Only full vectors use MATCH. The remaining elements use the scalar helper. This
keeps the complete key vector active and gives a straightforward, bounds-safe
tail path.

## 5. Unroll independent work

`search_sve2_unrolled` processes four vectors per iteration. The loads and MATCH
operations are independent, increasing instruction-level parallelism. It also
prefetches 16 vectors ahead only when that address remains inside the span.

After the unrolled loop, the function processes remaining full vectors one at a
time and finishes with the same scalar tail. Both SVE2 functions fall back to
the scalar search when the key count exceeds the per-segment limit.

## 6. Build and run

Configure and build the task from the repository root:

```bash
cmake --build build --config Release --target sve_match
./build/sve_match/Release/sve_match
```

Expected results are `true` for all available implementations and both element
types. The exact executable path can vary with a multi-configuration generator.

## 7. Test correctness

Build and run the task-specific tests:

```bash
cmake --build build --config Release --target test-sve_match
```

The typed GoogleTests exercise both element widths and check:

- empty haystacks and key sets;
- matches and misses;
- a match in a full vector;
- a match in the scalar tail; and
- scalar fallback for an oversized key set.

On a QEMU cross-compilation build, repeat the tests with the supported
`STBC_SVE_VL` values to check vector-length-agnostic behavior.

## 8. Benchmark across hit probabilities

The benchmark uses 64 Mi elements (`2^26`) for each dataset. It generates data
once with `std::mt19937_64` seed `0x5EED`, caches it, and gives every
implementation the same input for a type and hit probability. Data generation
is outside the timed loop.

The benchmark argument is measured in basis points:

| Basis points | Hit probability |
|---:|---:|
| 0 | 0% |
| 1 | 0.01% |
| 10 | 0.1% |
| 100 | 1% |

Build and run the benchmark in Release mode:

```bash
cmake --build build --config Release --target benchmark-sve_match
```

### Measured environment

The following output was measured on 7 September 2026 with:

- Arm Neoverse V2, four 2.0 GHz CPUs;
- 128-bit SVE vector length;
- GCC 15.2.0 and `-mcpu=native`;
- CMake Release configuration; and
- Google Benchmark 1.9.5.

This is the exact console output from that run:

```text
2026-09-07T15:13:38+00:00
Running build/benchmark/Release/benchmark
Run on (4 X 2000 MHz CPU s)
CPU Caches:
  L1 Data 64 KiB (x4)
  L1 Instruction 64 KiB (x4)
  L2 Unified 2048 KiB (x4)
  L3 Unified 36864 KiB (x1)
Load Average: 3.01, 1.20, 0.44
------------------------------------------------------------------------------------------------------------------
Benchmark                                                                        Time             CPU   Iterations
------------------------------------------------------------------------------------------------------------------
sve_match_scalar<std::uint8_t>/hit_probability_basis_points:0            345767695 ns    345603208 ns            2
sve_match_scalar<std::uint8_t>/hit_probability_basis_points:1                34399 ns        34400 ns        20352
sve_match_scalar<std::uint8_t>/hit_probability_basis_points:10                8256 ns         8257 ns        84775
sve_match_scalar<std::uint8_t>/hit_probability_basis_points:100                206 ns          206 ns      3391341
sve_match_scalar<std::uint16_t>/hit_probability_basis_points:0           240590096 ns    240594259 ns            3
sve_match_scalar<std::uint16_t>/hit_probability_basis_points:1               18699 ns        18700 ns        37401
sve_match_scalar<std::uint16_t>/hit_probability_basis_points:10               3301 ns         3301 ns       212112
sve_match_scalar<std::uint16_t>/hit_probability_basis_points:100               170 ns          170 ns      4108949
sve_match_sve2<std::uint8_t>/hit_probability_basis_points:0                2303467 ns      2303445 ns          305
sve_match_sve2<std::uint8_t>/hit_probability_basis_points:1                    236 ns          236 ns      2976413
sve_match_sve2<std::uint8_t>/hit_probability_basis_points:10                  82.2 ns         82.2 ns      8509950
sve_match_sve2<std::uint8_t>/hit_probability_basis_points:100                 34.4 ns         34.4 ns     20352446
sve_match_sve2<std::uint16_t>/hit_probability_basis_points:0               4317832 ns      4317697 ns          161
sve_match_sve2<std::uint16_t>/hit_probability_basis_points:1                   279 ns          279 ns      2501859
sve_match_sve2<std::uint16_t>/hit_probability_basis_points:10                 64.9 ns         64.9 ns     10792489
sve_match_sve2<std::uint16_t>/hit_probability_basis_points:100                18.1 ns         18.1 ns     38657180
sve_match_sve2_unrolled<std::uint8_t>/hit_probability_basis_points:0       2025216 ns      2025014 ns          347
sve_match_sve2_unrolled<std::uint8_t>/hit_probability_basis_points:1           214 ns          214 ns      3267045
sve_match_sve2_unrolled<std::uint8_t>/hit_probability_basis_points:10         78.4 ns         78.4 ns      8932885
sve_match_sve2_unrolled<std::uint8_t>/hit_probability_basis_points:100        35.1 ns         35.1 ns     19914036
sve_match_sve2_unrolled<std::uint16_t>/hit_probability_basis_points:0      4067357 ns      4067384 ns          173
sve_match_sve2_unrolled<std::uint16_t>/hit_probability_basis_points:1          253 ns          253 ns      2761831
sve_match_sve2_unrolled<std::uint16_t>/hit_probability_basis_points:10        60.9 ns         60.9 ns     11490317
sve_match_sve2_unrolled<std::uint16_t>/hit_probability_basis_points:100       18.3 ns         18.3 ns     38250026
```

### Speedup over scalar

Speedup uses the unrounded CPU times recorded in the JSON output:

```text
speedup = scalar CPU time / SVE2 CPU time
```

| Type | Hit probability | Basic SVE2 | Unrolled SVE2 |
|---|---:|---:|---:|
| `uint8_t` | 0% | 150.04x | 170.67x |
| `uint8_t` | 0.01% | 145.99x | 160.57x |
| `uint8_t` | 0.1% | 100.40x | 105.34x |
| `uint8_t` | 1% | 5.98x | 5.86x |
| `uint16_t` | 0% | 55.72x | 59.15x |
| `uint16_t` | 0.01% | 67.01x | 73.79x |
| `uint16_t` | 0.1% | 50.86x | 54.18x |
| `uint16_t` | 1% | 9.42x | 9.31x |

These numbers apply only to this measured system and workload. Processor,
compiler, vector length, memory hierarchy, key count, data distribution, and
system load can all change the result.

## Interpret the evidence

At 0% hits, every implementation scans all 64 Mi elements. The scalar function
also compares each element against every key, while MATCH checks a vector of
haystack values against the key set in one vector operation. This is where the
largest measured speedups appear.

As hit probability rises, the first match occurs earlier. All implementations
do less useful work, and fixed setup and call costs become a larger proportion
of the measurement. At 1%, the search often exits after only a small prefix, so
the advantage is smaller and four-way unrolling no longer improves the measured
`uint8_t` or `uint16_t` result.

The unrolled implementation helps most when scans are long enough to amortize
its extra loads and predicate tests. It is not automatically the best choice for
short searches or high hit rates.

## Limitations and transfer

- The API reports existence only, not the matching index or key.
- MATCH supports only one 128-bit segment's worth of distinct keys in this task.
- Scalar tail handling favors clarity over a fully predicated vector tail.
- The benchmark holds the generated datasets in memory so variants use
  identical inputs.
- A workload that needs every matching position requires a different result
  extraction design.

The same pattern is useful for delimiter detection, byte classification,
protocol parsing, and searches for small sets of sentinel values.

# Check your understanding

1. Why must the key set repeat in every 128-bit segment of a wide SVE vector?
2. Why does the implementation use scalar fallback for 17 byte keys?
3. Why are the zero-hit speedups larger than the 1% hit speedups?
4. When can unrolling make an early-exit search slower rather than faster?
5. What would need to change if the caller required the first matching index?

## Applied scenario

Suppose a parser searches a byte stream for 12 delimiter values and sees one
delimiter per 20,000 bytes on average. Predict which implementation should win,
then add a benchmark point representing that probability. Verify correctness,
measure the result, and justify whether the extra complexity of unrolling is
worth retaining for that workload.

# Summary

You implemented one C++20 interface over scalar and SVE2 search algorithms,
prepared segment-correct key vectors for MATCH, handled arbitrary vector lengths
and tails, and compared the implementations across controlled hit rates. The
measurement shows that SVE2 MATCH is especially effective for long scans with
rare or absent matches, while early exits reduce the benefit of unrolling.

## Further reading

- [Arm C Language Extensions](https://arm-software.github.io/acle/main/acle.html)
- [Arm: Accelerate multi-token search with SVE2 SVMATCH](https://developer.arm.com/community/arm-community-blogs/b/architectures-and-processors-blog/posts/multi-token-search-strings-svmatch-instruction)
- [Google Benchmark user guide](https://github.com/google/benchmark/blob/main/docs/user_guide.md)
- The repository's `histogram` task for another constrained-template SVE design
