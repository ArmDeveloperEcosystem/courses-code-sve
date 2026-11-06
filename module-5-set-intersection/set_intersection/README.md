<!--
  SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
  SPDX-License-Identifier: BSD-3-Clause-Clear
-->

# Set Intersection

This task implements sorted set intersection in scalar C++ and with SVE
intrinsics. It follows the course-plan slot for an algorithm that is not easily
auto-vectorized, but still benefits from direct SIMD-style reasoning.

The core lesson is deliberately practical: learners first inspect the compiler's
auto-vectorization behaviour, try a source-level hint such as `restrict`, explain
which data and control dependencies still block vectorization, and then compare
that with an intrinsic implementation that reshapes the work for SVE.

## What Set Intersection Does

Set intersection takes two sorted, unique input sets and writes the values that
appear in both:

```txt
A = [1, 4, 5, 8, 13, 21, 34, 55]
B = [0, 1, 2, 3, 5, 8, 13, 21, 35, 55, 89]
A ∩ B = [1, 5, 8, 13, 21, 55]
```

This task uses `uint32_t` values, which is a common shape for row IDs, document
IDs, dictionary IDs, bitmap positions, or other compact database-style keys.

## Why It Matters

Set intersection is a small algorithm that appears inside larger systems:

- Database joins and semi-joins over sorted key lists.
- Search engines intersecting posting lists.
- Column-store and analytics engines combining filtered row ID sets.
- Deduplication, indexing, and sparse feature matching pipelines.

The scalar implementation is the familiar two-pointer merge:

```cpp
while (lhs_pos < lhs_count && rhs_pos < rhs_count) {
    const auto lhs_value = lhs[lhs_pos];
    const auto rhs_value = rhs[rhs_pos];

    if (lhs_value == rhs_value) {
        output[output_count] = lhs_value;
        ++output_count;
        ++lhs_pos;
        ++rhs_pos;
        continue;
    }

    if (lhs_value < rhs_value) {
        ++lhs_pos;
    } else {
        ++rhs_pos;
    }
}
```

That loop is compact and branchy. Each iteration decides which input pointer
advances next, and the output pointer advances only on a match. Those
loop-carried decisions are exactly what make this a useful "hard to
auto-vectorize" example.

## Build

Build the set-intersection executable, tests, and benchmark binary in the
optimized Release configuration:

```bash
cmake --build build --config Release --target set_intersection
```

Use `--config Release` for benchmarking so the optimized configuration is
selected. With the multi-config generator, the Release binaries are placed under
configuration-specific directories such as `build/set_intersection/Release/`.

## Running The Main Executable

The `set_intersection` target builds a small command-line executable for trying
the available implementations on a fixed example input:

```bash
./build/set_intersection/Release/set_intersection
```

On a target without SVE, only the scalar and `restrict` versions are built. On an
AArch64 target with SVE, the executable also runs the SVE probe and SVE jammed
variants.

## Autovectorization Investigation

The scalar loop and the `restrict` loop are intentionally similar. The
`restrict` version lets learners test whether telling the compiler that the
input and output buffers do not alias is enough to unlock vectorization.

For Clang, configure with vectorization diagnostics:

```bash
CXX=clang++ cmake -S . -B build-vector-report -G "Ninja Multi-Config" \
  -DCMAKE_CXX_FLAGS_RELEASE="-O3 -Rpass=loop-vectorize -Rpass-missed=loop-vectorize -Rpass-analysis=loop-vectorize"
```

For GCC, use optimization reports:

```bash
CXX=g++ cmake -S . -B build-vector-report -G "Ninja Multi-Config" \
  -DCMAKE_CXX_FLAGS_RELEASE="-O3 -fopt-info-vec-optimized-missed"
```
When GCC is the configured compiler, the experiment also provides a dedicated
target that saves a report for `set_intersection.cpp`:

```bash
cmake --build build-vector-report --config Release --target vector-report-set_intersection
```

The report is written to `build-vector-report/set_intersection/vector-report/gcc-vectorization-report.txt`

Saving the report makes the compiler evidence reproducible and prevents the
diagnostics from being lost in terminal output. It lets learners compare the
scalar and `restrict` loops directly, record the compiler's stated blockers,
and revisit the evidence after changing compiler versions, flags, or source.
The report belongs under the build directory because it is generated evidence,
not portable source documentation.

The target uses `-fopt-info-vec-optimized-missed`: `optimized` records loops GCC
auto-vectorized, while `missed` records unsuccessful candidates and the reason.
These flags report compiler decisions; they do not enable vectorization.

The expected conclusion is that aliasing information helps the compiler reason
about memory, but it does not remove the loop-carried pointer updates, conditional
output store, and data-dependent control flow that shape this algorithm.

## Testing

Run only the set-intersection tests with:

```bash
cmake --build build --config Release --target test-set_intersection
```

The tests cover:

- Empty inputs on either side.
- No-overlap inputs.
- Full-overlap inputs.
- Cases where either side is the smaller set.
- Sparse matches across uneven value ranges.
- SVE probe and SVE jammed agreement with the scalar expected output when SVE is
  available at compile time.

## Benchmarking

Run only the set-intersection benchmarks with:

```bash
cmake --build build --config Release --target benchmark-set_intersection
```

The benchmark data sets are chosen to show different outcomes:

- `sparse_probe`: a smaller selected set intersected with a much larger sorted
  candidate set. This models a database-style semi-join or posting-list probe
  and is the case where the SVE versions should have the clearest opportunity.
- `dense_equal`: both inputs contain the same values. This keeps the scalar
  two-pointer loop very efficient and shows that intrinsics are not a magic
  speedup button.
- `interleaved_miss`: both inputs are similarly sized and interleaved but do not
  match. This stresses branch behaviour and repeated comparisons without output
  stores.
- `workload_matrix`: systematically combines shorter-set sizes of 128 and 1,024
  elements, size ratios of 1:1, 1:8, and 1:64, and intersection densities of
  1%, 5%, 50%, and 95%. This separates the effects of input size, imbalance, and
  match density instead of drawing a conclusion from one distribution.

## Implementation

### Scalar Baseline

`intersect_scalar` is the canonical sorted merge. It is the reference
implementation used for correctness and the baseline used for performance
comparison.

### Restrict Variant

`intersect_restrict` uses the same control flow as the scalar baseline, but gives
the compiler a no-alias contract for the input and output arrays. It is included
so learners can test a realistic compiler hint and then explain why the loop is
still difficult to vectorize.

### SVE Probe Variant

`intersect_sve_probe` treats the smaller input as a probe set and the larger
input as a sorted haystack. For each probe value, it skips haystack chunks whose
last value is too small, then compares the probe value against one SVE vector of
haystack values:

```cpp
const auto active =
    svwhilelt_b32(static_cast<uint64_t>(haystack_pos),
                  static_cast<uint64_t>(haystack_count));
const auto haystack_values = svld1_u32(active, haystack + haystack_pos);
const auto equal = svcmpeq_n_u32(active, haystack_values, value);
```

`svptest_any` detects whether any lane matched. When a match is found,
`svbrkb_b_z` and `svcntp_b32` find how many lanes came before the matching lane,
so the next probe can resume after that value.

### SVE Jammed Variant

`intersect_sve_jammed` improves the direct probe approach by loading a haystack
vector once, then processing every probe value that falls inside that vector's
value range before advancing. This is the loop-jamming part of the task: rather
than repeatedly loading the same vector chunk for nearby probe values, the
implementation reuses the loaded vector while it is still useful.

## Expected Performance Pattern

The SVE implementations are expected to be most interesting when one sorted set
is much smaller than the other and the larger set can be skipped or probed in
vector chunks. Dense equal-sized intersections can remain competitive with the
scalar baseline because the scalar two-pointer loop does very little work per
match. That contrast is intentional: the task is about learning when intrinsics
are worth considering, not claiming they always win.

## Measured Results

The following results are from a Release build using GCC with
`-O3 -mcpu=native` on a 2 GHz AArch64 system with SVE. The workload matrix was
run three times. Each cell reports the range of median speedups for shorter-set
sizes 128 and 1,024:

```txt
speedup = scalar_time / implementation_time
```

A value above `1.00x` is faster than scalar. A value below `1.00x` is slower.
All implementations materialize the matching values in the output array; this
is not a count-only benchmark.

| Size ratio | Intersection | `restrict` | SVE probe | SVE jammed |
|---:|---:|---:|---:|---:|
| 1:1 | 1% | 0.99-1.01x | 0.48-0.49x | 0.73-0.76x |
| 1:1 | 5% | 1.02-1.03x | 0.49-0.51x | 0.75x |
| 1:1 | 50% | 0.88-0.99x | 0.22-0.23x | 0.61-0.63x |
| 1:1 | 95% | 0.99-1.00x | 0.10x | 0.51-0.52x |
| 1:8 | 1% | 1.00x | 1.71-1.81x | 1.34-1.36x |
| 1:8 | 5% | 1.00-1.01x | 1.76-1.91x | 1.34-1.35x |
| 1:8 | 50% | 1.00-1.01x | 1.01-1.11x | 1.22-1.27x |
| 1:8 | 95% | 1.00x | 0.67-0.71x | 1.20-1.26x |
| 1:64 | 1% | 1.00-1.01x | 4.29-4.61x | 3.76-3.84x |
| 1:64 | 5% | 1.00x | 4.04-4.34x | 3.62-3.88x |
| 1:64 | 50% | 1.00x | 3.19-3.20x | 3.59-3.60x |
| 1:64 | 95% | 1.00x | 2.75-2.91x | 3.72-3.95x |

Absolute timings depend on the processor, SVE vector length, compiler, and
compiler version. The similar trends at 128 and 1,024 elements indicate that
size ratio and match density, rather than one selected input size, drive the
observed algorithm choice on this target.

## Learner Conclusions

### Why The Scalar Loop Resists Autovectorization

The scalar merge loop decides which input position advances after comparing the
current values. The next iteration therefore depends on the result of the
previous iteration. The output position also advances only on a match. These
loop-carried dependencies, conditional stores, and data-dependent branches do
not form the regular independent iterations expected by a conventional
auto-vectorizer.

`restrict` removes a possible input/output aliasing concern, but it does not
remove those dependencies. Its measured speedup ranges from `0.88x` to `1.03x`
without a consistent positive trend. The lesson is to use compiler diagnostics
to identify all vectorization blockers: resolving aliasing alone is not enough
when control flow and induction variables remain dependent.

### When Intrinsics Are Worth Considering

Intrinsics let the programmer express an algorithmic transformation that the
compiler cannot infer from the scalar merge: take one value from the shorter
set, compare it with a vector of candidates, and skip the longer set in
vector-sized chunks.

- At 1:1, both SVE implementations are slower than scalar. The scalar merge does
  little work per consumed value, while probing adds vector setup, predicate,
  and comparison overhead.
- At 1:8, the answer depends on match density. Direct probing wins for sparse
  intersections, while jamming wins as more probe values reuse a loaded chunk.
- At 1:64, both SVE variants consistently beat scalar. Direct probing reaches
  `4.61x` for sparse inputs, while the jammed version reaches `3.95x` for dense
  intersections.

Intrinsics are therefore justified for a profiled asymmetric-set workload, not
as a universal replacement for scalar code.

### What Loop Jamming Teaches

The direct probe can load or examine the same haystack chunk for nearby probe
values. The jammed implementation reverses that relationship: it loads a chunk
once and compares every relevant probe value before advancing.

That reuse has a cost. At low density, there may be too little useful work per
chunk, and direct probing is faster. As density rises, reuse outweighs the extra
range checks and loop control. At a 1:64 ratio and 95% intersection, jamming
improves the scalar-relative speedup from `2.75-2.91x` to `3.72-3.95x`.

### Balancing Further Unrolling

Loop jamming is measured in this experiment, but there is not yet a separately
unrolled SVE kernel. The current results must not be used to claim that more
unrolling will help. Further unrolling is useful only if it hides instruction
latency or creates enough independent comparisons to keep the machine busy.

Each additional unrolled operation also needs vector values, predicates, and
temporaries. Excessive unrolling can cause register spills, add loads and
stores, enlarge the instruction-cache footprint, and increase code size. A
follow-up experiment should compare small factors such as 1x, 2x, and 4x, inspect
the generated assembly for spills, record code size, and retain an unrolled
variant only when it produces a repeatable workload-relevant speedup.

## Scope And Follow-Up Work

These conclusions apply to the scalar-to-vector SVE probe algorithms implemented
here. They do not establish that every SIMD set-intersection strategy is slow
for balanced inputs. A complete algorithm-selection study should also compare a
scalar galloping baseline for extremely uneven sets and an SVE2 vector-to-vector
implementation for balanced or dense sets. Those additions would separate the
benefit of changing the search algorithm from the benefit of SIMD and show when
runtime dispatch by size ratio or ISA is worthwhile.
