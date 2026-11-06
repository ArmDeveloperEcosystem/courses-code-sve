<!--
SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
SPDX-License-Identifier: BSD-3-Clause-Clear
-->

# Evaluate explicit SVE implementations for simple integer vector operations

## Module information

| Item | Details |
|---|---|
| Estimated time | 45–60 minutes, excluding long builds or benchmark runs |
| Target audience | Developers learning how to use Arm Scalable Vector Extension (SVE) intrinsics in C++ |
| Prerequisite knowledge | Basic C++, loops, `std::vector`, templates, and compiling C++ applications |
| Previous module or dependencies | No previous module required |
| Tested platform or environment | AArch64 Linux system with SVE support |
| Required architecture features | Arm SVE |
| Tested software and toolchain | CMake, Ninja, GCC with C++20 support, Google Test, and Google Benchmark |
| Important configuration | Use the Ninja Multi-Config generator and the Release configuration when comparing performance |

# Introduction

## About this module

In this module, you will evaluate whether explicit SVE is beneficial for two simple integer-vector workloads:

- vector addition;
- finding the minimum and maximum values in a vector.

You will work with the following implementation paths:

| Path, stage, or implementation | Purpose in the module |
|---|---|
| Scalar C++ implementations | Establish a correct reference and performance starting point |
| Explicit SVE implementations | Apply vector-length-agnostic SVE techniques |
| Google Test typed tests | Verify correctness across supported integer types |
| Google Benchmark measurements | Compare optimised scalar and SVE performance across several input sizes |

Simple loops are common candidates for vectorisation. However, an optimising compiler may already generate efficient code from straightforward scalar C++, so writing an explicit SIMD implementation does not automatically improve performance.

The aim of this module is therefore not to assume that SVE is faster. You will use correctness checks and benchmark evidence to decide whether explicit SVE is useful for these workloads.

## Guiding engineering question

> When does an explicit SVE implementation improve performance over optimised scalar C++?

## What will you be able to do?

By the end of this module, you will be able to:

- explain how SVE predicates support vector-length-agnostic loops;
- identify the SVE intrinsics used for vector addition;
- use horizontal SVE reductions for minimum and maximum operations;
- verify SVE results against scalar reference implementations;
- benchmark optimised scalar and SVE implementations across a meaningful range of input sizes;
- interpret performance evidence without assuming that explicit vectorisation is always faster;
- use the results to make an evidence-based implementation decision.

# Before you begin

## Understand the workloads

This module uses two simple workloads.

### Vector addition

Vector addition adds corresponding elements from two input vectors:

```txt
lhs    = [1, 2, 3, 4]
rhs    = [5, 6, 7, 8]

result = [6, 8, 10, 12]
```

### Min/max

The min/max operation finds the smallest and largest values in a vector:

```txt
values = [7, 2, 9, 4, 5]

minimum = 2
maximum = 9
```

Both workloads have scalar and SVE implementations.

The examples support:

- `int8_t`
- `uint8_t`
- `int16_t`
- `uint16_t`
- `int32_t`
- `uint32_t`

## Prerequisites

Before starting, you need:

- an AArch64 Linux environment;
- SVE support on the target CPU;
- CMake;
- Ninja;
- a C++20-capable compiler;
- the repository dependencies required for Google Test and Google Benchmark;
- basic familiarity with C++ loops and containers.

## Prepare for the module

Run the following commands on the SVE-capable target system from the repository root.

Verify that SVE is available:

```sh
lscpu | grep -o '\bsve2\?\b' | sort -u
```

You should expect the output to include:

```txt
sve
```

Before continuing, confirm that:

- the repository haq been configured successfully;
- SVE is detected on the target platform;
- you are working from the repository root.

# Plan the scalar-to-SVE evaluation workflow

You will:

1. inspect scalar vector addition;
2. examine how the same operation is expressed with SVE;
3. inspect scalar min/max;
4. examine the SVE min/max implementation;
5. build and run the example;
6. verify correctness with Google Test;
7. benchmark optimised scalar and SVE implementations;
8. compare the evidence and answer the guiding engineering question.

By the end of the workflow, you should have:

- a correct scalar baseline;
- a correct SVE implementation;
- passing correctness tests;
- Release benchmark measurements across several input sizes;
- an evidence-based conclusion about the value of explicit SVE for these workloads.

## Repository and workflow assets

| Path or asset | Purpose | Used in |
|---|---|---|
| `vector_examples.hpp` | Contains the scalar and SVE implementations | Implementation inspection |
| `vector_examples.cpp` | Provides the task library translation unit | Build |
| `main.cpp` | Runs a simple scalar and SVE example | Functional check |
| `test.cpp` | Contains Google Test typed tests | Correctness verification |
| `benchmark.cpp` | Contains scalar and SVE Google Benchmarks | Performance measurement |
| `CMakeLists.txt` | Registers the task with the repository build system | Build, test, and benchmark workflow |

# Complete the scalar-to-SVE evaluation workflow

## 1. Inspect scalar vector addition

The scalar vector-add implementation processes one element at a time:

```cpp
for (size_t i = 0; i < lhs.size(); ++i) {
    result[i] = lhs[i] + rhs[i];
}
```

This provides a simple reference implementation for both correctness and performance.

When the code is compiled in Release mode, the compiler is free to optimise this loop. Therefore, scalar C++ source code should not be assumed to result in inefficient scalar machine code.

### Predict the effect of vectorising the loop

Before examining the SVE version, consider:

| Prediction question | Your prediction | Reason |
|---|---|---|
| Will SVE process more than one element per loop iteration? | | |
| How should the final partial vector be handled? | | |
| Will explicit SVE necessarily outperform the optimised scalar implementation? | | |

### Workflow-step takeaway

The scalar loop establishes a clear and correct starting point. A fair performance comparison requires this starting point to be compiled with optimisation enabled.

## 2. Inspect the SVE vector-add implementation

For 32-bit signed values, the generic implementation uses `SVE<T>` to select
the correct predicate and vector length for each element type. Its core
operations are:

```cpp
svbool_t pg = SVE<T>::WHILELT(i, lhs.size());

VectorType lhs_vec = svld1(pg, lhs.data() + i);
VectorType rhs_vec = svld1(pg, rhs.data() + i);

VectorType result_vec = svadd_x(pg, lhs_vec, rhs_vec);

svst1(pg, result.data() + i, result_vec);

i += SVE<T>::CNT();
```

### Create a predicate

```cpp
svbool_t pg = SVE<T>::WHILELT(i, lhs.size());
```

For `T = int32_t`, `SVE<T>::WHILELT` calls `svwhilelt_b32` to create a
predicate identifying the active 32-bit lanes.

On the final iteration, only lanes corresponding to valid input elements remain active.

### Load the inputs

```cpp
VectorType lhs_vec = svld1(pg, lhs.data() + i);
VectorType rhs_vec = svld1(pg, rhs.data() + i);
```

`svld1` loads the active values selected by the predicate.

### Add the active lanes

```cpp
VectorType result_vec = svadd_x(pg, lhs_vec, rhs_vec);
```

`svadd_x` performs the addition across the active lanes.

### Store the result

```cpp
svst1(pg, result.data() + i, result_vec);
```

`svst1` writes the active result lanes back to memory.

### Advance by the current vector length

```cpp
i += SVE<T>::CNT();
```

For `T = int32_t`, `SVE<T>::CNT()` calls `svcntw()`, which returns the number
of 32-bit elements that fit in an SVE vector on the current system.

The equivalent operations for the supported widths are:

| Element width | Predicate | Element count |
|:---:|:---:|:---:|
| 8-bit | `svwhilelt_b8` | `svcntb` |
| 16-bit | `svwhilelt_b16` | `svcnth` |
| 32-bit | `svwhilelt_b32` | `svcntw` |

### Check the important behaviour

Confirm that:

- the loop does not assume a fixed vector length;
- the predicate controls the active lanes;
- the final partial vector is handled without a separate scalar cleanup loop.

### Workflow-step takeaway

The vector-add implementation demonstrates predication, vector-length-agnostic iteration, predicated loads and stores, and vector arithmetic.

## 3. Inspect scalar min/max

The scalar min/max implementation begins with the first element:

```cpp
T minimum = values[0];
T maximum = values[0];
```

It then checks the remaining values:

```cpp
for (size_t i = 1; i < values.size(); ++i) {
    if (values[i] < minimum) {
        minimum = values[i];
    }

    if (values[i] > maximum) {
        maximum = values[i];
    }
}
```

As with vector addition, the Release compiler can optimise this straightforward scalar loop.

### Predict the SVE approach

Before examining the SVE implementation, consider:

| Prediction question | Your prediction | Reason |
|---|---|---|
| Can several input elements be examined in one SVE iteration? | | |
| How can the active vector lanes be reduced to a single minimum value? | | |
| Will an explicit SVE reduction necessarily be faster than the optimised scalar loop? | | |

### Workflow-step takeaway

The scalar implementation establishes the expected min/max behaviour and provides the reference against which the SVE implementation can be checked and measured.

## 4. Inspect the SVE min/max implementation

For 32-bit signed values, the core operations are:

```cpp
svbool_t pg = SVE<T>::WHILELT(i, values.size());

VectorType values_vec = svld1(pg, values.data() + i);

ScalarType chunk_min = svminv(pg, values_vec);
ScalarType chunk_max = svmaxv(pg, values_vec);
```

### Select and load the active elements

```cpp
svbool_t pg = SVE<T>::WHILELT(i, values.size());
VectorType values_vec = svld1(pg, values.data() + i);
```

The predicate identifies valid lanes and allows the final partial vector to be handled safely.

### Reduce the active vector

```cpp
ScalarType chunk_min = svminv(pg, values_vec);
ScalarType chunk_max = svmaxv(pg, values_vec);
```

`svminv` returns the minimum across the active lanes.

`svmaxv` returns the maximum across the active lanes.

The chunk results are then compared with the running scalar minimum and maximum.

### Check the important behaviour

Confirm that:

- inactive lanes are excluded from the reduction;
- the implementation maintains the overall scalar minimum and maximum;
- the loop remains vector-length agnostic.

### Workflow-step takeaway

The min/max implementation demonstrates how SVE horizontal reductions can be combined with a scalar running result.

## 5. Build and run the example

Run the following commands on the SVE-capable target system from the repository root.

Build the task using the Release configuration:

```sh
cmake --build build --config Release --target vector_examples
```

Run the Release executable:

```sh
./build/vector_examples/Release/vector_examples
```

You should expect output similar to:

```txt
Scalar vector add: 6 8 10 12
Scalar min: 6
Scalar max: 12
SVE vector add: 6 8 10 12
SVE min: 6
SVE max: 12
```

### Check the result

Confirm that:

- scalar vector addition produces `6 8 10 12`;
- SVE vector addition produces the same result;
- scalar min/max produces `6` and `12`;
- SVE min/max produces the same values.

### Workflow-step takeaway

The example demonstrates that the scalar and SVE implementations agree for the sample input. The next step is to verify correctness more systematically across all supported integer types.

## 6. Verify correctness with Google Test

The task uses Google Test typed tests to run the same checks across the supported integer types.

The tests verify:

- scalar vector addition against an expected result;
- SVE vector addition against the scalar reference;
- scalar min/max against expected values;
- SVE min/max against the scalar reference.

Run the task tests from the repository root using the Release configuration:

```sh
cmake --build build --config Release --target test-vector_examples
```

On an SVE-enabled system, the vector example contributes 24 test executions.

You should expect the vector example tests to report:

```txt
[==========] 24 tests from 24 test suites ran.
[  PASSED  ] 24 tests.
```

The repository uses a shared test executable across its tasks, so building this target also requires the other task libraries to be supported by the platform.

### Check correctness before continuing

Do not use performance measurements to judge the SVE implementation until the correctness tests pass.

### Workflow-step takeaway

The scalar and SVE implementations now have correctness evidence across all supported integer types. Performance can be measured without confusing an incorrect result with a fast result.

## 7. Measure optimised scalar and SVE performance

A meaningful comparison requires both implementations to be compiled with optimisation enabled.

Debug builds are not suitable for this comparison because the straightforward scalar loops may receive little compiler optimisation. Comparing explicit SVE against an unoptimised scalar implementation can therefore make the apparent benefit of SVE much larger than it is in an optimised application.

The benchmark compares:

- scalar vector addition;
- SVE vector addition;
- scalar min/max;
- SVE min/max.

The input size comes from Google Benchmark:

```cpp
const size_t size = state.range(0);
```

The benchmark registrations use:

```cpp
->RangeMultiplier(2)
->Range(1 << 8, 1 << 14)
```

This produces input sizes of:

```txt
256
512
1024
2048
4096
8192
16384
```

`benchmark::DoNotOptimize` prevents the benchmarked result from being removed by compiler optimisation.

### Run the Release benchmark

Run the following command on the SVE-capable target system from the repository root:

```sh
cmake --build build --config Release --target benchmark-vector_examples
```

The Release configuration ensures that the scalar and explicit SVE implementations are compared with compiler optimisation enabled.

The repository uses a shared benchmark executable across its tasks, so building this target also requires the other task libraries to be supported by the platform.

Benchmark timings depend on the processor, compiler, system load, vector length, and build configuration.

Treat the reference measurements below as illustrative rather than exact values that must be reproduced.

### Comparison conditions

The reference comparison used:

| Condition | Value |
|---|---|
| Platform | SVE-capable AArch64 Linux system |
| Build system | Ninja Multi-Config |
| Build configuration | Release |
| Compiler optimisation | `-O3`, `-DNDEBUG` |
| CPU configuration | `-mcpu=native` |
| Benchmark data type | `int32_t` |
| Input sizes | 256 to 16384 elements |
| Scalar/SVE input data | Equivalent inputs for each comparison |
| Repetitions | Determined automatically by Google Benchmark |
| Google Benchmark library | Release build |

### Vector addition results

| Elements | Scalar | SVE | Interpretation |
|---:|---:|---:|---|
| 256 | 25.9 ns | 26.9 ns | Similar; scalar slightly faster |
| 512 | 50.6 ns | 54.9 ns | Similar; scalar slightly faster |
| 1024 | 106 ns | 118 ns | Similar; scalar slightly faster |
| 2048 | 214 ns | 215 ns | Approximately equal |
| 4096 | 422 ns | 424 ns | Approximately equal |
| 8192 | 825 ns | 841 ns | Approximately equal |
| 16384 | 2150 ns | 2086 ns | Similar; SVE slightly faster |

### Min/max results

| Elements | Scalar | SVE | Interpretation |
|---:|---:|---:|---|
| 256 | 19.8 ns | 62.6 ns | Scalar faster |
| 512 | 42.4 ns | 126 ns | Scalar faster |
| 1024 | 83.5 ns | 253 ns | Scalar faster |
| 2048 | 151 ns | 504 ns | Scalar faster |
| 4096 | 296 ns | 998 ns | Scalar faster |
| 8192 | 594 ns | 2055 ns | Scalar faster |
| 16384 | 1192 ns | 4000 ns | Scalar faster |

### Analyse the benchmark result

For this tested configuration:

- vector addition performs similarly in optimised scalar and SVE form;
- the scalar implementation is slightly faster for several smaller vector-add inputs;
- SVE becomes slightly faster at the largest tested vector-add input;
- scalar min/max is faster across the complete measured range.

For simple loops such as the examples in this module, an optimising compiler can often auto-vectorise straightforward scalar C++, generating vector instructions without requiring explicit SVE intrinsics. Simple mathematical operations are particularly suitable for this type of compiler optimisation.

This behaviour was confirmed for the scalar vector-add implementation using GCC's vectorisation diagnostics with `-O3 -mcpu=native`. GCC reported that the loop was vectorised, including the use of variable-length vectors.

Inspection of the generated AArch64 assembly also confirmed that the scalar C++ source produced vector instructions. This demonstrates an important distinction: scalar C++ source does not necessarily result in scalar machine code. In this case, compiler auto-vectorisation helps explain why the optimised scalar vector-add implementation performs similarly to the explicit SVE implementation.

#### Inspect compiler auto-vectorisation

A simplified version of the scalar vector-add loop can be inspected in Compiler Explorer:

[Inspect scalar vector addition in Compiler Explorer](https://godbolt.org/z/5YcGrx84M)

The Compiler Explorer example uses ARM64 GCC with:

```text
-O3 -march=armv8-a+sve
```

The source contains an ordinary C++ loop without explicit SVE intrinsics:

```cpp
for (size_t i = 0; i < lhs.size(); ++i) {
    result[i] = lhs[i] + rhs[i];
}
```

With optimisation and SVE enabled, GCC generates SVE instructions including `whilelo`, `ld1w`, `st1w`, and `add` operations using SVE `z` registers.

This provides direct evidence that the compiler can automatically transform a simple scalar C++ loop into vectorised SVE machine code. It also helps explain why the optimised scalar vector-add implementation performs similarly to the explicit SVE implementation in this module.

Explicit SVE can become more useful when a workload is harder for the compiler to vectorise automatically, for example when it involves complex predicates, irregular memory access, gather/scatter operations, or specialised vector processing. In those situations, SVE gives the programmer greater control over how the workload is expressed.

These measurements do not prove that scalar min/max is always faster, or that explicit SVE cannot improve these workloads under different conditions.

The result applies to the tested implementation, compiler, target system, and configuration.

### Workflow-step takeaway

Explicit SVE successfully expresses both workloads in a vector-length-agnostic way, but the Release benchmark demonstrates why explicit vectorisation should be compared with an optimised scalar baseline rather than an unoptimised Debug build.

For simple compiler-friendly loops, the compiler may already generate vector instructions automatically. Explicit SVE is therefore most useful when it provides capabilities or control that the compiler cannot obtain as effectively through automatic vectorisation.

# Review the evidence from the workflow

Review the evidence you collected during the workflow.

| Question | Your conclusion |
|---|---|
| What does the correctness evidence show? | |
| What does the vector-add performance evidence show? | |
| What does the min/max performance evidence show? | |
| Why is the Release comparison more meaningful than a Debug comparison? | |
| Which evidence is most important when deciding whether to use explicit SVE? | |
| Which assumptions or limitations affect the conclusion? | |
| What remains uncertain? | |
| What should be measured or inspected next before generalising the result? | |

Use your conclusions to answer the guiding engineering question:

> When does an explicit SVE implementation improve performance over optimised scalar C++?

For the tested configuration, explicit SVE does not provide a consistent performance improvement across both workloads.

Vector addition performs similarly in optimised scalar and SVE form, with SVE becoming slightly faster at the largest tested size.

For min/max, the optimised scalar implementation remains faster across the measured range.

The engineering conclusion is that explicit SVE provides control over predication, vector processing, and vector-length-agnostic execution, but performance must be demonstrated with measurements against an optimised baseline on the intended target system.

# Check your understanding

## Question 1

What is the purpose of `svwhilelt` in these examples?

A. Select the hardware SVE vector length.

B. Create a predicate identifying valid lanes for the current iteration.

C. Convert scalar values into SVE vectors.

D. Reduce an SVE vector to one scalar value.

<details>
<summary>Answer</summary>

**B. Create a predicate identifying valid lanes for the current iteration.**

`svwhilelt` marks the lanes that correspond to valid elements between the current index and the end of the input.

</details>

<details>
<summary>Incorrect-answer feedback</summary>

**A.** `svwhilelt` does not choose the hardware vector length. The hardware determines the available SVE vector length.

**B.** Correct. The predicate identifies which lanes contain valid elements for the current iteration.

**C.** `svwhilelt` does not load or convert data. Predicated load operations such as `svld1` load the values.

**D.** `svwhilelt` does not perform a reduction. Operations such as `svminv` and `svmaxv` reduce active lanes.

</details>

## Question 2

Why does the vector-add loop use `svcntb`, `svcnth`, or `svcntw` instead of advancing by a fixed number of elements?

A. To make the loop vector-length agnostic.

B. To select between signed and unsigned arithmetic.

C. To disable inactive predicate lanes.

D. To force the compiler to use a particular SVE vector length.

<details>
<summary>Answer</summary>

**A. To make the loop vector-length agnostic.**

The count intrinsics report how many elements of the corresponding width fit in the current SVE vector length.

</details>

<details>
<summary>Incorrect-answer feedback</summary>

**A.** Correct. The loop advances according to the vector length available on the target system.

**B.** Signedness is determined by the data and intrinsic types, not by the count intrinsic.

**C.** Predicate values are controlled by operations such as `svwhilelt`, not by `svcntb`, `svcnth`, or `svcntw`.

**D.** The count intrinsics query the available vector capacity. They do not force a particular hardware vector length.

</details>

## Question 3

What do `svminv` and `svmaxv` do?

A. Compare two vectors element by element.

B. Store minimum and maximum vectors to memory.

C. Reduce the active lanes to a minimum or maximum scalar value.

D. Determine how many values fit in the SVE vector.

<details>
<summary>Answer</summary>

**C. Reduce the active lanes to a minimum or maximum scalar value.**

These are horizontal reduction operations over the active predicate lanes.

</details>

<details>
<summary>Incorrect-answer feedback</summary>

**A.** These intrinsics do not perform pairwise comparison between two vectors. They reduce one vector.

**B.** They return scalar reduction results rather than storing vectors to memory.

**C.** Correct. `svminv` and `svmaxv` produce scalar minimum and maximum values from the active lanes.

**D.** Vector capacity is queried using count intrinsics such as `svcntw`.

</details>

## Question 4

Why should the performance comparison use a Release build?

A. Release mode makes only the SVE implementation faster.

B. Release mode allows the compiler to optimise the scalar and SVE implementations before they are compared.

C. Google Benchmark cannot run in Debug mode.

D. SVE instructions are unavailable in Debug mode.

<details>
<summary>Answer</summary>

**B. Release mode allows the compiler to optimise the scalar and SVE implementations before they are compared.**

A fair performance comparison should not compare explicit SVE with an unnecessarily unoptimised scalar baseline.

</details>

<details>
<summary>Incorrect-answer feedback</summary>

**A.** Release optimisation applies to the compiled program rather than selectively making only the SVE path faster.

**B.** Correct. Both implementations should be evaluated under an appropriate optimised configuration.

**C.** Google Benchmark can run with a Debug build, but the resulting timings may not represent meaningful application performance.

**D.** SVE availability is determined by the target architecture and compiler configuration, not by whether the build is Debug or Release.

</details>

## Applied decision scenario

You replace a simple scalar loop with an explicit SVE implementation. The SVE implementation is correct but benchmarks slightly slower than the optimised scalar implementation on the intended target system.

Which next step is best?

A. Assume the benchmark is incorrect because SVE should always be faster.

B. Remove the scalar implementation because explicit vector code is preferable.

C. Inspect the generated code and profile the workload before deciding which implementation to use.

D. Hard-code a larger SVE vector length in the source code.

<details>
<summary>Answer</summary>

**C. Inspect the generated code and profile the workload before deciding which implementation to use.**

The benchmark evidence shows that correctness does not guarantee a performance improvement. Further evidence is needed before making the implementation decision.

</details>

<details>
<summary>Incorrect-answer feedback</summary>

**A.** Explicit SVE does not guarantee higher performance. A slower measured result should be investigated rather than dismissed.

**B.** The scalar implementation may remain the better engineering choice if it is correct, simpler, and faster for the target workload.

**C.** Correct. Generated code and profiling can help explain the measured behaviour and support an evidence-based decision.

**D.** Vector-length-agnostic SVE code should not hard-code a particular vector length. The available vector length is a property of the target system.

</details>

# Apply the SVE evaluation conclusion

In this module, you:

- established scalar vector addition and min/max implementations as reference points;
- inspected how SVE predicates and count intrinsics support vector-length-agnostic loops;
- examined SVE arithmetic and horizontal reduction operations;
- checked scalar and SVE correctness across the supported integer types;
- compared explicit SVE with an optimised scalar baseline;
- benchmarked both implementation paths across a range of input sizes;
- inspected evidence of compiler auto-vectorisation;
- interpreted the performance differences rather than assuming that explicit SVE would be faster;
- reached an evidence-based conclusion about these workloads.

The reusable engineering method is:

1. establish a correct reference implementation;
2. identify operations that can be expressed with SVE;
3. use vector-length-agnostic patterns;
4. verify the SVE result against the reference;
5. benchmark an optimised scalar baseline and the SVE implementation on the intended target;
6. inspect generated code or profile the workload when the measurements need explanation;
7. choose the implementation based on correctness, performance, complexity, and target requirements.

The results in this module apply to the tested configuration. Different compilers, CPUs, SVE vector lengths, workloads, and input distributions may produce different results.

## Further reading

Use the Arm C Language Extensions documentation and Arm SVE intrinsic reference material for details of the SVE intrinsics used in this module.
