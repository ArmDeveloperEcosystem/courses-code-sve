---
name: sve-and-simd-optimization
description: Apply general SIMD, NEON, SVE, and SVE2 optimization guidance for C or C++ hot paths. Use when improving or reviewing vector code for Arm workloads such as image processing, video transforms, filters, packing kernels, reductions, dot products, and numeric loops.
License: This skill content is provided under the BSD-3-Clause-Clear license for this repository.
---

# SVE And SIMD Optimization

Guidance provided by George Steed and Kieran Hejmadi, Arm .

Use this skill as general guidance for improving known hot paths while preserving the original contract. Treat SIMD as an optimization of a correct scalar implementation, not as a substitute for understanding the scalar algorithm.

## General SIMD Practices

- Use the scalar implementation as the correctness reference and performance baseline. Compare scalar, compiler-autovectorized, and hand-vectorized versions before deciding that intrinsics are justified.
- Preserve function signatures, data layout, strides, rounding, clipping, saturation, supported input domains, and observable behavior unless the user explicitly changes the contract.
- Prefer full-width vector work in the hot path. For NEON this usually means 128-bit loads, stores, and arithmetic, such as 8x8-bit or 4x16-bit lane groups where the data type fits.
- Prefer contiguous loads and stores. Avoid strided loads, gathers, scatters, and pointer chasing when the data can be arranged or traversed linearly.
- Align loads and stores to cache-friendly boundaries when practical, but do not assume alignment unless the code proves or checks it.
- Broadcast constants once and reuse them. Avoid repeated `dup`, `ld1r`, `ld4r`, or equivalent setup inside inner loops when the value can remain live.
- Reuse vector registers for intermediate results instead of reloading the same data. Keep hot constants and frequently reused values resident when register pressure allows it.
- Use post-increment addressing when it trims pointer arithmetic and shortens dependency chains.
- Manage loop counters and branches efficiently. In assembly paths, combine counter update and branch tests where the ISA makes that natural, such as `subs` followed by a conditional branch.
- Balance unrolling against register pressure, spills, instruction-cache footprint, and code size. More unrolling is useful only when it hides latency or creates enough independent work without spilling.
- For reductions, prefer vector reductions, pairwise sums, and add-across instructions over scalar fallback work inside hot loops.
- For multiply-heavy integer kernels, prefer widening multiply and multiply-add sequences such as `umull` and `umlal` when they delay narrowing and avoid overflow.
- Use saturating arithmetic and narrowing instructions when they express both the arithmetic and clamping behavior, such as `uqsub`, `uqshrn`, or paired narrowing forms that fill a full output register before storing.
- Use interleaved stores or loads when the memory format is naturally interleaved, such as RGB-like tuples, rather than scalarizing channel packing.
- When using C++ template metaprogramming or helper wrappers around intrinsics in hot paths, guarantee inlining with the `INLINE` macro, defined in `include/SVE-task-based-course.h`, which abstracts how to force the compiler to inline a specific function. With Clang and similar compilers, missed inlining can leave function call overhead that outweighs the SIMD speedup.
- Treat prefetching as benchmark-driven. Add `prfm` or equivalent prefetches only when access is predictable and measurements show a benefit.
- Inspect generated code after each meaningful change. Intrinsics do not guarantee the compiler selected the best instruction sequence.

## SVE And SVE2 Practices

- Use SVE or SVE2 when it improves the measured hot path while keeping the same function contract and semantics as the scalar implementation.
- Prefer vector-length-agnostic SVE loops. Drive iteration with `svcnt*()` and tail predicates such as `svwhilelt_*` instead of hard-coded lane counts.
- Use predicated loads, compute operations, and stores so the main vector loop handles tails cleanly without duplicating scalar cleanup.
- Choose inactive-lane semantics deliberately. Use merge, zero, or don't-care forms only when their behavior is correct for the operation.
- Preserve exact rounding, clipping, filtering, border, and packing behavior when replacing scalar or NEON code with SVE.
- Keep existing SVE style in a codebase when it is already established, especially helper wrappers, predicate conventions, and intrinsic naming patterns.
- Prefer contiguous SVE loads and stores over gather/scatter unless the data layout requires otherwise.
- Use SVE widening, narrowing, permutation, and predicate operations when they directly express the algorithm; avoid translating scalar code mechanically if it creates unnecessary lane shuffles.
- For FP16, BF16, or mixed-precision kernels, consider SVE/SVE2 widening, dot-product, or BF16 instructions only when the target supports them and the accuracy tradeoff is acceptable.
- Gate SVE2 or newer instructions with the narrowest correct feature check, and keep baseline SVE, NEON, or scalar fallbacks available when needed.
- Avoid fixed vector-length assumptions in indexing, storage, ABI assumptions, or loop trip counts.
- Build, run correctness tests, inspect generated code, and compare performance after replacing naive scalar code with SVE or SVE/NEON code.

## Workload Patterns

For image, video, and pixel kernels:

- Preserve bit-exact rounding, clipping, packing, and channel order.
- Use packed or interleaved loads and stores where the memory format naturally stores tuples, such as RGB-like data.
- Use widening multiply-add instructions to delay narrowing and reduce overflow risk.
- Use saturating arithmetic and combined narrowing operations when they express both math and clamping.
- Keep constants, offsets, and filter coefficients hoisted and reused.

For transform and filter kernels:

- Preserve scaling, rounding, boundary, and clipping semantics exactly.
- Avoid scalar fallback work inside fixed-size hot transforms unless it is outside the measured hot path.
- Reuse intermediate vectors instead of reloading the same data.
- Schedule independent operations together when it helps hide instruction latency.

For dot products and numeric reductions:

- Use multiple accumulators to reduce dependency-chain bottlenecks.
- Avoid mixing short- and long-latency instruction sequences in a way that creates pipeline bubbles.
- Consider widening, FMLAL, BF16, or dot-product instructions only when the target supports them and the accuracy change is acceptable.
- Check denormal handling and flush-to-zero behavior before using alternate conversion or arithmetic paths.

For packing and layout kernels:

- Keep the existing layout and stride contract unless layout redesign is explicitly in scope.
- Favor full-width vector loads and stores for the vectorized part.
- Create specialized fast paths only for common shapes that are stable, measured, and still fall back correctly.

## Heuristic Optimization Loop

### Prerequisites

- **Target:** Ask for the target CPU/ISA, native versus portable deployment, and desired gain. For a portable binary, plan scalar or NEON fallback and runtime dispatch. If SVE is available, query its active vector length.
- **Stimulus:** Find representative inputs or an existing microbenchmark; choose realistic sizes and a performance metric. If C++ lacks repeatable stimulus, add a task-local Google Benchmark case.
- **Baseline:** Test correctness and record repeated Release timings for the current code and scalar reference, where distinct. Record the target, build flags, inputs, kernel time, and application time.
- **Tools:** Ensure `perf` or Arm Performix can profile the workload. Use the Arm MCP server for intrinsic references if available.

### Repeat until no worthwhile gain remains

1. Profile the optimized build with debug symbols. Use enough samples and relevant counters to find the hottest loop and its compute, memory, or control-flow bottleneck.
2. Inspect vectorization diagnostics and assembly from the same build. For native AArch64 with GCC, add these flags to the project compile command; for example:

   ```bash
   g++ -O3 -mcpu=native \
       -fopt-info-vec-optimized \
       -fopt-info-vec-missed \
       -fopt-info-vec-note \
       -c kernel.cpp -o kernel.o
   ```

   Keep the project's other flags; replace `-mcpu=native` with its target ISA for portable builds.
3. Test one change at a time. First make minimal changes that enable autovectorization; if insufficient, try targeted NEON/SVE/SVE2 intrinsics. Prefer intrinsics to inline assembly. Use assembly only when intrinsics cannot express the operation and measurements justify it; comment why.
4. Compare against the scalar reference on short inputs, tails, and supported vector lengths. Inspect instructions and repeat kernel and application timings on the same stimulus.
5. If headroom remains, test one refinement at a time, such as power-of-two unrolling, proven C `restrict` or C++ `__restrict__`, force inlining of small hot helpers, a more targeted intrinsic, or fewer gathers/scatters. Use templates for multiple data types only when they stay readable; reject complexity for small gains. Return to step 4 after each trial.
6. Keep a change only when its repeatable gain justifies its complexity and portability cost; otherwise revert it. If kept, update the baseline. Stop when no promising candidate remains; otherwise return to step 1 and reprofile.

## Review Checklist

- Does the optimized code match scalar results for zero length, short inputs, tails, unaligned inputs, and representative large inputs?
- Did measurements compare scalar, compiler-autovectorized, and intrinsic implementations?
- Did vectorization reports or assembly confirm the intended instructions were emitted?
- Are aliasing, alignment, and feature assumptions justified?
- Is the optimization limited to the hot path without generated files, benchmark shortcuts, or unrelated edits?
- Is the intrinsic version clearly faster or more reliable than cleaned-up autovectorized scalar code?

### Pitfalls to avoid

- Do not apply multiple unrelated edits in one measurement step.
- Do not change workload inputs between baseline and re-profile runs.
- Do not present unmeasured gains as improvements.
- Avoid unreadable code for marginal gains.
- Do not force intrinsics where compiler auto-vectorization with proper flags is sufficient.
- Do not ignore statistical noise; repeat runs when deltas are small or unstable.
