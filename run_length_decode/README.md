<!--
  SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
  SPDX-License-Identifier: BSD-3-Clause-Clear
-->

# Module: Evaluate Run-Length Decoding with SVE

### When does SVE improve run-length decoding, and when is exact allocation more important than avoiding a second parse?

# Introduction

## About this module

Run-length encoding stores repeated values as a decimal count followed by the
value to repeat. This reduces the size of inputs containing long runs, but the
decoder must expand the input into its original form. The decoded size is not
known from the encoded byte count alone.

Run-length encoding is useful for data with long repeated runs, such as simple
bitmap regions, masks, and repeated-symbol streams.

In this module, you will inspect and evaluate scalar and SVE decoders for this
format. You will compare two allocation strategies:

| Implementation | Purpose |
|---|---|
| Scalar two-pass | Calculates the exact output size, then allocates once and decodes. |
| Scalar single-pass | Decodes once while growing a buffer geometrically. |
| SVE two-pass | Uses exact allocation and SVE stores to fill repeated values. |
| SVE single-pass | Uses a growable buffer and SVE stores. |

The decimal count parsing remains scalar in every implementation. SVE is used
for the part of the workload that maps naturally to vector operations: storing
many copies of one byte.

## What will you be able to do?

By the end of this module, you will be able to:

- explain the count/value run-length format and its input assumptions;
- identify why the decoded length requires parsing the decimal counts;
- distinguish two-pass and single-pass allocation strategies;
- check decoder correctness for valid, empty, and null inputs;
- explain which part of the decoder is vectorized with SVE;
- run the supplied benchmark and interpret its workload and counters;
- choose an implementation based on run sizes, output expansion, and allocation cost.

# Before you begin

## Understand the encoded workload

A run contains one or more decimal digits followed by one value character. For
example:

```text
Encoded:  3a12b1c5d
Decoded:  aaabbbbbbbbbbbbcddddd
```

The counts are `3`, `12`, `1`, and `5`, so the decoded length is 21 characters.
The parser constructs a multi-digit count incrementally:

```cpp
count = count * 10 + static_cast<size_t>(*input - '0');
```

The decoder assumes valid input. Each run must contain decimal digits followed
by a value character, and the resulting allocation must fit in `std::size_t`
and succeed. Empty strings and null `const char*` inputs return an allocated
empty C string in the tested pointer-based interfaces. Malformed encodings are
not diagnosed.

The output length cannot be inferred from the encoded length: `1a` and `9a`
both contain two input characters but produce different output lengths. The
two-pass implementations therefore parse and sum every count before
allocating. First-faulting loads could help find the end of a C string, but they
would not calculate the sum of its decimal counts.

## Prerequisites

Before starting, you need:

- a C++20 compiler and CMake;
- Ninja if using the documented multi-config generator;
- an AArch64 target with SVE, or the repository's QEMU cross-compilation setup,
  to build and run the SVE paths;
- familiarity with basic C++, dynamic allocation, and command-line builds;
- the repository checked out at its root directory.

## Prepare for the module

Build the task, its tests, and the benchmark:

```bash
cmake --build build --config Release --target run_length_decode
```

Use a Release configuration when collecting performance evidence. The SVE
functions are compiled only when both AArch64 and the SVE compiler feature are
available.

# Plan the run-length decoding workflow

You will:

1. inspect the encoding format, parser, and allocation strategies;
2. run the command-line decoder and check the supplied tests;
3. inspect the SVE fill helper and identify the scalar/vector boundary;
4. run the benchmark across several count sizes;
5. use correctness and performance evidence to answer the guiding question.

By the end of the workflow, you should have:

- correctness evidence for every implementation available on the target;
- an identified boundary between scalar and vector work;
- benchmark results for the registered run lengths;
- a comparison of allocation and fill strategies;
- a reasoned implementation choice.

## Repository and workflow assets

| Path or asset | Purpose | Used in |
|---|---|---|
| `run_length_decode/run_length_decoding.cpp` | Scalar and SVE implementations | Inspection and analysis |
| `run_length_decode/run_length_decoding.h` | Public decoder declarations and SVE guards | Inspection |
| `run_length_decode/test.cpp` | Valid, empty, and null-input correctness tests | Correctness step |
| `run_length_decode/benchmark.cpp` | Registered workloads and decoded-byte counters | Measurement step |
| `run_length_decode/main.cpp` | `scalar` and, on SVE targets, `sve` command-line entry points | Functional check |

# Complete the run-length decoding workflow

## 1. Inspect the parser and allocation strategies

Start with `run_length_decode/run_length_decoding.cpp`. Each implementation
parses a count by multiplying the previous count by ten and adding the next
digit. The two-pass path calls `_calcDecodeLength`, allocates
`decodedLength + 1` bytes, then parses the input again while filling the output.

The scalar fill uses:

```cpp
std::memset(output, value, count);
output += count;
```

The single-pass pointer implementation starts with 16 bytes of capacity and
doubles the allocation until the current run fits, copying already-written
output when it grows. The `string` overload delegates capacity management
to `string` and uses `append(count, value)`.

Before continuing, predict which strategy will be helped most by large counts.
Large counts create large outputs from short inputs, so avoiding repeated buffer
growth and copies is increasingly important. Single-pass decoding still avoids
the second parse, which can help when output growth is small.

## 2. Run and check the decoder

Run the scalar command-line decoder from the repository root:

```bash
build/run_length_decode/Release/run_length_decode scalar 3a5b
```

Expected output:

```text
aaabbbbb
```

On an AArch64/SVE build, the SVE entry point is selected with:

```bash
build/run_length_decode/Release/run_length_decode sve 3a5b
```

Run the task tests:

```bash
cmake --build build --config Release --target test-run_length_decode
```

The tests apply the expected decoding to all available implementations. They
also check empty and null inputs for the `const char*` interfaces, and empty
input for the `string` overload. A successful run currently reports five
passing tests when SVE is enabled:

```text
[==========] 5 tests from 2 test suites ran. (0 ms total)
[  PASSED  ] 5 tests.
```

Do not interpret a passing functional test as evidence that malformed input is
handled: malformed input is outside this task's contract.

### Run the tests with QEMU

On an AArch64 development machine without SVE hardware, use QEMU user-mode
emulation to build and run the SVE paths. Install it on Ubuntu:

```bash
sudo apt update
sudo apt install qemu-user
```

Configure a separate build that uses the repository's QEMU cross-compilation
support. CMake auto-detects `qemu-aarch64` from `PATH` and runs task test targets
under emulation:

```bash
cmake -S . -B build-qemu -DSTBC_SVE_CROSS_COMPILE=ON
```

Run the task tests at each supported SVE vector length:

```bash
for vector_bits in 128 256 512 1024 2048; do
    printf '\nSVE vector length: %s bits\n' "$vector_bits"
    STBC_SVE_VL="$vector_bits" \
        cmake --build build-qemu --target test-run_length_decode
done
```

`STBC_SVE_VL` is specified in bits and supports powers of two from 128 through
2048. Each invocation should report passing `run_length_decode` tests. Running
the same tests across these vector lengths validates that the decoder remains
functionally correct when the SVE vector length changes and helps detect
incorrect fixed-vector-length assumptions.

Use QEMU for functional correctness and vector-length-agnostic validation, not
for performance conclusions.

## 3. Inspect the SVE fill and allocation strategies

When SVE is available, `_sve_fill` duplicates the value into an SVE vector and
stores it with a predicate that is active only for the remaining output:

```cpp
svuint8_t values = svdup_n_u8(static_cast<uint8_t>(value));

while (i < count) {
    svbool_t pg = svwhilelt_b8(i, count);
    svst1_u8(pg, reinterpret_cast<uint8_t *>(output + i), values);
    i += svcntb();
}
```

The predicate makes the final store safe when the count is not a multiple of
the hardware vector length. The loop advances by `svcntb()`, so it is
vector-length agnostic. Decimal parsing and allocation remain scalar because
the digits of a multi-digit count are sequentially dependent and because
allocation decisions depend on the accumulated decoded length.

Both SVE decoders use `_sve_fill`; they differ in how they allocate output. The
following abridged snippets show the key distinction:

```cpp
// Two pass: calculate the exact size, allocate once, then decode.
size_t decodedLength = _calcDecodeLength(encodedString);
char *output = new char[decodedLength + 1];
while (*input != '\0') {
    // Parse count and value from input again.
    output = _sve_fill(output, value, count);
}

// Single pass: grow the output buffer when the next run will not fit.
constexpr size_t initialCapacity = 16;
size_t capacity = initialCapacity;
size_t decodedLength = 0;
auto result = std::make_unique<char[]>(capacity);
const size_t requiredCapacity = decodedLength + count + 1;
if (requiredCapacity > capacity) {
    size_t newCapacity = capacity;
    while (newCapacity < requiredCapacity) {
        newCapacity *= 2;
    }
    auto expanded = std::make_unique<char[]>(newCapacity);
    std::memcpy(expanded.get(), result.get(), decodedLength);
    result = std::move(expanded);
    capacity = newCapacity;
}
_sve_fill(result.get() + decodedLength, value, count);
decodedLength += count;
```

Two-pass decoding pays for a second scalar parse but performs one exact
allocation and no buffer copies. Single-pass decoding parses once, but growth
can require extra allocations and copies and may leave unused capacity. In both
versions, only writing the repeated values is vectorized.

## 4. Run the benchmark

Run only the run-length benchmarks:

```bash
cmake --build build --config Release --target benchmark-run_length_decode
```

The fixture targets an encoded length of 512 characters and repeats a run such
as `4a` until it reaches that target. The registered count values produce this
workload:

```output
run_length_decode_/scalarCharPtrTwoPassBM/1/512             1115 ns         1115 ns       627781 DecodedCharactersWritten/s=229.681M/s
run_length_decode_/scalarCharPtrTwoPassBM/4/512             1027 ns         1027 ns       681956 DecodedCharactersWritten/s=997.164M/s
run_length_decode_/scalarCharPtrTwoPassBM/16/512             895 ns          895 ns       781975 DecodedCharactersWritten/s=3.05672G/s
run_length_decode_/scalarCharPtrTwoPassBM/64/512             890 ns          890 ns       786056 DecodedCharactersWritten/s=12.2933G/s
run_length_decode_/scalarCharPtrSinglePassBM/1/512           833 ns          833 ns       839259 DecodedCharactersWritten/s=307.213M/s
run_length_decode_/scalarCharPtrSinglePassBM/4/512           828 ns          828 ns       842876 DecodedCharactersWritten/s=1.237G/s
run_length_decode_/scalarCharPtrSinglePassBM/16/512          752 ns          752 ns       931913 DecodedCharactersWritten/s=3.63892G/s
run_length_decode_/scalarCharPtrSinglePassBM/64/512         1114 ns         1114 ns       629238 DecodedCharactersWritten/s=9.82548G/s
run_length_decode_/scalarStdStringSinglePassBM/1/512         648 ns          648 ns      1079801 DecodedCharactersWritten/s=395.137M/s
run_length_decode_/scalarStdStringSinglePassBM/4/512         823 ns          823 ns       851007 DecodedCharactersWritten/s=1.24441G/s
run_length_decode_/scalarStdStringSinglePassBM/16/512        774 ns          774 ns       904468 DecodedCharactersWritten/s=3.53533G/s
run_length_decode_/scalarStdStringSinglePassBM/64/512       1044 ns         1044 ns       670674 DecodedCharactersWritten/s=10.4841G/s
run_length_decode_/sveCharPtrTwoPassBM/1/512                 631 ns          631 ns      1109295 DecodedCharactersWritten/s=405.669M/s
run_length_decode_/sveCharPtrTwoPassBM/4/512                 631 ns          631 ns      1109247 DecodedCharactersWritten/s=1.62261G/s
run_length_decode_/sveCharPtrTwoPassBM/16/512                644 ns          644 ns      1087444 DecodedCharactersWritten/s=4.24976G/s
run_length_decode_/sveCharPtrTwoPassBM/64/512                823 ns          823 ns       850884 DecodedCharactersWritten/s=13.3045G/s
run_length_decode_/sveCharPtrSinglePassBM/1/512              488 ns          488 ns      1433640 DecodedCharactersWritten/s=524.345M/s
run_length_decode_/sveCharPtrSinglePassBM/4/512              562 ns          562 ns      1240769 DecodedCharactersWritten/s=1.82338G/s
run_length_decode_/sveCharPtrSinglePassBM/16/512             576 ns          576 ns      1217827 DecodedCharactersWritten/s=4.74811G/s
run_length_decode_/sveCharPtrSinglePassBM/64/512            1105 ns         1105 ns       634709 DecodedCharactersWritten/s=9.90089G/s
```

Each benchmark reports `DecodedCharactersWritten/s`. Record the results for
the scalar and SVE two-pass and single-pass implementations when SVE is
available. Compare matching implementations on the same count; keep the
processor, compiler, build configuration, and benchmark command fixed.

Ensure the processor, SVE vector length, compiler, build configuration are the same when comparing results.

For interpretation, look for these patterns:

- SVE can improve repeated-byte filling, especially when runs are large enough
  to amortize vector setup and use many lanes.
- Single-pass decoding avoids parsing the encoded input twice.
- Two-pass decoding performs one exact allocation and avoids copying existing
  output during growth.
- At large counts, allocation and copying can outweigh the cost of the second
  parse, making the two-pass path preferable.

# Review the evidence from the workflow

Use the following questions to record your conclusion:

| Question | Your conclusion |
|---|---|
| Did every tested implementation produce the expected output? | |
| Which part of the decoder is vectorized? | |
| How did decoded size change as the count increased? | |
| Which allocation cost matters for the larger workloads? | |
| Which implementation best fits the workload being considered? | |
| What remains uncertain about the result? | |

The benchmark does not prove that one implementation is universally best. It
measures a fixed encoded length, a repeated single-character workload, and the
configuration in which it was run. Applications should measure representative
input distributions on their deployment target.

Use your evidence to answer the guiding question:

> When does SVE improve run-length decoding, and when is exact allocation more
> important than avoiding a second parse?

## Generalise the method

The reusable method is to separate a workload into stages before vectorizing:

1. identify which stage has sequential dependencies;
2. identify which stage performs regular independent work;
3. vectorize the regular stage with predication for the tail;
4. measure allocation and data-movement costs as well as arithmetic throughput.

Here, count parsing and capacity decisions remain sequential, while filling a
run is independent byte-wise work. The same reasoning applies to other
expanders and parsers that transform compact metadata into repeated output.

# Check your understanding

**Question 1**

Why does `_calcDecodeLength` combine adjacent decimal digits before adding a
run length?

A. Each digit represents a separate run.

B. A count such as `13` must be interpreted as one value before its run length
is added.

C. SVE requires counts to be converted to binary in a separate pass.

D. The output buffer must always be a multiple of the encoded length.

<details>
<summary>Answer</summary>

**B. A count such as `13` must be interpreted as one value before its run
length is added.**

Treating `1` and `3` as separate counts would calculate the wrong decoded size
and would lose the boundary between a multi-digit count and its value.

</details>

<details>
<summary>Incorrect-answer feedback</summary>

**A.** Adjacent digits belong to the same count until the value character is
reached.

**C.** Decimal parsing is scalar in both the scalar and SVE implementations.

**D.** The decoded size is determined by the sum of the counts, not by a fixed
multiple of the encoded length.

</details>

**Question 2**

Which work does the SVE implementation accelerate?

A. Finding every value character in the encoded input.

B. Parsing multi-digit decimal counts and choosing allocation sizes.

C. Storing repeated copies of each run value into the output.

D. Detecting and reporting malformed encodings.

<details>
<summary>Answer</summary>

**C. Storing repeated copies of each run value into the output.**

The SVE helper uses predicated stores to fill a run. Count parsing and
allocation decisions remain scalar, and malformed-input validation is outside
the decoder contract.

</details>

<details>
<summary>Incorrect-answer feedback</summary>

**A.** The implementation walks the encoded input with scalar code.

**B.** Multi-digit parsing has a sequential dependency, and capacity decisions
depend on the accumulated decoded length.

**D.** The supplied implementations assume valid input.

</details>

## Choose a decoder for an expanding workload

An application normally receives short encoded inputs containing large counts.
Profiling shows that the single-pass pointer decoder repeatedly grows and
copies its output buffer. The application runs on an SVE target, and an extra
parse of the short encoded input is acceptable.

Which implementation should be measured first as the likely best fit?

A. Scalar pointer, single-pass.

B. Scalar `string`, single-pass.

C. SVE pointer, two-pass.

D. SVE pointer, single-pass.

<details>
<summary>Answer</summary>

**C. SVE pointer, two-pass.**

The two-pass implementation calculates the exact output size and avoids growth
copies, while the SVE fill targets the repeated-byte work. The choice still
requires measurement with representative inputs.

</details>

<details>
<summary>Incorrect-answer feedback</summary>

**A.** This retains the observed growth-and-copy cost and does not use SVE.

**B.** `string` manages growth, but it does not guarantee exact allocation
and does not use the task's SVE fill.

**D.** This uses SVE but retains the pointer implementation's geometric growth
strategy.

</details>

# Apply the run-length decoding conclusion

In this module, you:

- identified the count/value format and its valid-input assumptions;
- compared exact-allocation and growable-buffer implementations;
- checked scalar and SVE decoder correctness;
- located the boundary between scalar parsing and vector filling;
- measured decoded-character throughput across several run sizes;
- considered parsing, allocation, copying, and vector-store costs.

SVE is most likely to help when repeated-byte filling is a meaningful part of
the runtime and runs are large enough to use multiple vector lanes. Exact
allocation becomes more important when output expansion causes repeated buffer
growth and copying; in that case, the extra parse of a short encoded input may
cost less than relocating a large decoded output.

Choose the path using the workload evidence:

- use the two-pass path when exact output sizing and avoiding growth copies are
  important;
- use the single-pass path when avoiding a second parse is more valuable and
  output growth is modest;
- use the SVE variants when the target provides SVE and repeated-byte stores are
  a meaningful part of the workload.

The measured result answers the guiding question only for the tested target and
input shape. Validate the selected implementation with production-like run
length distributions, output sizes, and allocation behaviour before adopting
it in a larger system.

## Further reading

- [Arm C Language Extensions specification](https://arm-software.github.io/acle/)
- [QEMU Arm CPU feature documentation](https://www.qemu.org/docs/master/system/arm/cpu-features.html)
