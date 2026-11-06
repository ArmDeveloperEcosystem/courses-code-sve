<!--
  SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
  SPDX-License-Identifier: BSD-3-Clause-Clear
-->

# Using SVE intrinsics from Rust

This example reads two columns of `f32` values and computes their accumulated squared difference using one of three implementations:

- `scalar`: ordinary scalar Rust;
- `simd`: target-independent vectors from `std::simd`;
- `sve`: AArch64 SVE intrinsics from `std::arch::aarch64`.

Only the computation is timed. Reading and parsing the input happens before the timer starts. The project has no third-party dependencies.

## Why is it important?

Rust is a language that is gaining a lot of traction, because amongst other things it allows to deliver programs with performance *AND* memory safety. As with C/C++, although the compiler can be clever and vectorize an algorithm by itself, it may help to get the best performances to directly use the SVE intrinsics.

Rust offers `std::simd`, which provides high-level, target-independent SIMD. Direct SVE intrinsics thru `std::arch::aarch64::*` provides access to architecture-specific operations when an algorithm needs them.

## Prerequisites

Rust has had support for the Arm Neon intrinsics for quite some time, and since April 2026, the SVE intrinsics have started to appear so users can experiment with them. Note that at the time of writing, they are considered an _experimental_ feature in Rust.

Both `std::simd` and the Rust SVE intrinsics are currently experimental, so a recent nightly Rust toolchain is required. Install Rust and nightly with:

```sh
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
rustup toolchain install nightly
```

To inspect the emitted code, install `objdump` on Ubuntu or Debian with:

```sh
sudo apt install binutils
```

## Implementation notes

The SVE implementation additionally requires an AArch64 machine with SVE. The program checks for SVE at run time and reports a clear error when it is absent.

The program will need a function to load the data from a file: `load_columns`.

We will also add 3 implementations of a function to compute the sum of squared difference:
- `squared_difference_scalar`, a reference implementation, expressing the computation using scalars,
- `squared_difference_simd`, an implementation using Rust's `std::simd` crate,
- `squared_difference_sve`, an implementation using SVE explicitly with intrinsics.

Those 3 implementations will be called from a wrapper that will time the execution of those functions: `compute_timed`.

Then the `main`function will manage the program arguments and call (and time) the required implementation. The command line will look like:

```txt
squared-difference <scalar|simd|sve> <file>
```

### First things first !

```rs
#![feature(portable_simd)]
#![cfg_attr(target_arch = "aarch64", feature(stdarch_aarch64_sve))]

use std::env;
use std::error::Error;
use std::fs;
use std::simd::{Simd, num::SimdFloat};
use std::time::{Duration, Instant};
```

At top of `src/main.rs`, we ensure that the `portable_simd` feature and that the target architecture is `aarch64` and that it has `SVE` available. We then bring in the namespaces for the different functions we will need:
- `std::env` to access the programs environment (the command line arguments),
- `std::error::Error` to handle errors,
- `std::fs` to access the data in a file,
- `std::simd` for the SIMD implementation of `squared_difference`,
- `stds::time` to time the execution of `squared_differences`.

### Input format

The input is plain text with exactly two whitespace-separated floating-point values per non-empty line. Tabs and spaces are both accepted. For example:

```text
1.0  1.5
2.0  1.0
3.5  4.0
```

This function to parse this format is `load_columns`:

```rs
fn load_columns(path: &str) -> Result<(Vec<f32>, Vec<f32>), Box<dyn Error>> {
    let input = fs::read_to_string(path)?;
    let mut first = Vec::new();
    let mut second = Vec::new();

    for (line_number, line) in input.lines().enumerate() {
        if line.trim().is_empty() {
            continue;
        }

        let mut fields = line.split_whitespace();
        let a = fields
            .next()
            .ok_or_else(|| format!("line {}: expected two columns", line_number + 1))?;
        let b = fields
            .next()
            .ok_or_else(|| format!("line {}: expected two columns", line_number + 1))?;
        if fields.next().is_some() {
            return Err(format!("line {}: expected exactly two columns", line_number + 1).into());
        }

        first.push(
            a.parse::<f32>()
                .map_err(|error| format!("line {}, column 1: {error}", line_number + 1))?,
        );
        second.push(
            b.parse::<f32>()
                .map_err(|error| format!("line {}, column 2: {error}", line_number + 1))?,
        );
    }

    Ok((first, second))
}
```

It takes as argument the name of the file containing the 2 vectors, and it will return those 2 vectors or an error.

### Scalar sum of squared difference

The scalar implementation just zips together the 2 vectors, maps the squared differences to each pair, and then sums all:

```rs
#[inline(never)]
fn squared_difference_scalar(first: &[f32], second: &[f32]) -> f32 {
    first
        .iter()
        .zip(second)
        .map(|(&a, &b)| {
            let difference = a - b;
            difference * difference
        })
        .sum()
}
```

Note that the function is marked `inline(never)` so that it's easy to find in the assembly listing.

### SIMD sum of squared difference

```rs
#[inline(never)]
fn squared_difference_simd(first: &[f32], second: &[f32]) -> f32 {
    const LANES: usize = 16;
    let vector_length = first.len() / LANES * LANES;
    let mut sum = Simd::<f32, LANES>::splat(0.0);

    for i in (0..vector_length).step_by(LANES) {
        let difference = Simd::from_slice(&first[i..]) - Simd::from_slice(&second[i..]);
        sum += difference * difference;
    }

    sum.reduce_sum() + squared_difference_scalar(&first[vector_length..], &second[vector_length..])
}
```

The SIMD implementation first initialize the accumulator to 0. It then computes in a loop the part that can be vectorized, while the remainder is computed using the scalar implementation.

### SVE sum of squared difference

```rs
#[cfg(target_arch = "aarch64")]
#[inline(never)]
#[target_feature(enable = "sve")]
unsafe fn squared_difference_sve(first: &[f32], second: &[f32]) -> f32 {
    use std::arch::aarch64::*;

    let mut acc = svdup_n_f32(0.0);
    let mut i = 0;
    while i < first.len() {
        let predicate = svwhilelt_b32_u64(i as u64, first.len() as u64);
        // SAFETY: the predicate disables every lane at or beyond the slice boundary.
        let a = unsafe { svld1_f32(predicate, first.as_ptr().add(i)) };
        let b = unsafe { svld1_f32(predicate, second.as_ptr().add(i)) };
        let difference = svsub_f32_x(predicate, a, b);
        acc = svmla_f32_m(predicate, acc, difference, difference);
        i += svcntw() as usize;
    }

    svaddv_f32(svptrue_b32(), acc)
}
```

The SVE implementation requires an `aarch64` target machine with SVE support, which is enforced respectively with `cfg(target_arch = "aarch64")` and `target_feature(enable = "sve")`.

The SVE implementation, using intrinsics, has to be marked as `unsafe` because the SVE load intrinsics (`svld1_f32`) are loading from raw pointers, which could fail. Otherwise, the function body looks unsurprising: the accumulator (`acc`) is initialized to 0, then we process the vectors in a loop with:
 - set a predicate (`predicate`) to the element to read,
 - load with `svld1_f32` the data from both vectors,
 - compute and accumulate the difference with `svsub_f32_x` + `svmla_f32_m`. Note that the accumulation uses the `_m` suffix, that will only update the elements for which the predicate is set, and leave the others to their original value.
 - the loop index counter is eventually updated with the lane count, i.e. the number of elements that have are available in a vector register on this hardware specific implementation (`svcntw`).

 The accumulator elements are then reduced with (`svadd_f32`) to sum all them up ; all elements are reduced (its predicate is all true), because the accumulator has been zero initialized and the accumulation with `svmla_f32_m` has only updated the accumulator with valid differences.

### Timing the execution

```rs
fn compute_timed(
    implementation: &str,
    first: &[f32],
    second: &[f32],
) -> Result<(f32, Duration), String> {
    match implementation {
        "scalar" => {
            let start = Instant::now();
            let result = squared_difference_scalar(first, second);
            Ok((result, start.elapsed()))
        }
        "simd" => {
            let start = Instant::now();
            let result = squared_difference_simd(first, second);
            Ok((result, start.elapsed()))
        }
        "sve" => {
            #[cfg(target_arch = "aarch64")]
            {
                if !std::arch::is_aarch64_feature_detected!("sve") {
                    return Err("SVE is not available on this CPU".into());
                }
                // SAFETY: SVE support was checked immediately above.
                let start = Instant::now();
                let result = unsafe { squared_difference_sve(first, second) };
                return Ok((result, start.elapsed()));
            }
            #[cfg(not(target_arch = "aarch64"))]
            Err("SVE is only available on AArch64".into())
        }
        _ => Err(format!(
            "unknown implementation '{implementation}'; use scalar, simd, or sve"
        )),
    }
}
```

`compute_timed` is dispatching the call to the requested implementation of `squared_difference` and times its execution.

### Assembling everything in `main`

```rs
fn main() -> Result<(), Box<dyn Error>> {
    let mut arguments = env::args();
    let program = arguments
        .next()
        .unwrap_or_else(|| "squared-difference".into());
    let implementation = arguments
        .next()
        .ok_or_else(|| format!("usage: {program} <scalar|simd|sve> <file>"))?;
    let path = arguments
        .next()
        .ok_or_else(|| format!("usage: {program} <scalar|simd|sve> <file>"))?;
    if arguments.next().is_some() {
        return Err(format!("usage: {program} <scalar|simd|sve> <file>").into());
    }

    let (first, second) = load_columns(&path)?;
    let (result, elapsed) = compute_timed(&implementation, &first, &second)?;

    println!("accumulated squared difference: {result}");
    println!("computation time: {elapsed:?}");
    Ok(())
}
```

The `main` function assembles everything. It :
- parses the command line arguments (`implementation` and `path`),
- loads the 2 vectors from the file given as argument,
- time the selected computation,
- reports the result and the time it took to compute it.

### Adding tests

Testing is good practice, so let's add some tests, comparing the SVE (resp. SIMD) implementations to the scalar implementation. The scalar implementation serves a reference implementation. Note that the scalar/SIMD test will run on all architectures, whereas the scalar/SVE test requires an `aarch64` target with SVE.

```rs
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn scalar_and_simd_agree() {
        let first: Vec<_> = (0..37).map(|x| x as f32 * 0.5).collect();
        let second: Vec<_> = (0..37).map(|x| x as f32 * 0.25).collect();
        let scalar = squared_difference_scalar(&first, &second);
        let simd = squared_difference_simd(&first, &second);
        assert!((scalar - simd).abs() < 0.001 * scalar);
    }

    #[cfg(target_arch = "aarch64")]
    #[test]
    fn scalar_and_sve_agree() {
        if !std::arch::is_aarch64_feature_detected!("sve") {
            eprintln!("skipping SVE comparison: SVE is not available on this CPU");
            return;
        }

        let first: Vec<_> = (0..37).map(|x| x as f32 * 0.5).collect();
        let second: Vec<_> = (0..37).map(|x| x as f32 * 0.25).collect();
        let scalar = squared_difference_scalar(&first, &second);
        // SAFETY: SVE support was checked immediately above.
        let sve = unsafe { squared_difference_sve(&first, &second) };
        assert!((scalar - sve).abs() < 0.001 * scalar);
    }
}
```

## Build

From this directory, make an optimized build:

```sh
cargo +nightly build --release
```

## Test

Run the scalar/SIMD/SVE consistency test with:

```sh
cargo +nightly test
```

## Run

Select the implementation with the first argument (`scalar`, `simd` or `sve`) and pass the data file as the second argument. The repository contains 2 sample data files, `data.tsv` with only 3 elements, whereas `data_large.tsv` contains 3072 elements. The latter is more fit for benchmarking the vectorization effectiveness. Note though that benchmarking with a low element count has also its merits in specific cases, especially if the workload at hand often has a low element count. In those cases, the scalar version might very well be the fastest. It's not uncommon in libraries to dispatch the computation to different implementations depending on the problem's size.

```sh
cargo +nightly run --release -- scalar data_large.tsv
    Finished `release` profile [optimized] target(s) in 0.00s
     Running `target/release/squared-difference scalar data_large.tsv`
accumulated squared difference: 30041732
computation time: 2.464µs

cargo +nightly run --release -- simd data_large.tsv
    Finished `release` profile [optimized] target(s) in 0.00s
     Running `target/release/squared-difference simd data_large.tsv`
accumulated squared difference: 30041720
computation time: 863ns

cargo +nightly run --release -- sve data_large.tsv
    Finished `release` profile [optimized] target(s) in 0.00s
     Running `target/release/squared-difference simd data_large.tsv`
accumulated squared difference: 30041720
computation time: 817ns
```

The output contains the accumulated squared difference and the computation time. Floating-point results can differ slightly between implementations because vector reductions add values in a different order.

## Inspecting the generated code

The three computation functions use `#[inline(never)]`, so each remains a real function in optimized builds and can be compared in the disassembly. Build in
release mode and use `objdump -Cd` to demangle the Rust symbols:

```sh
objdump -Cd target/release/squared-difference
```

and search for:

- `squared_difference::squared_difference_sve`:
```txt
...
000000000001eb30 <squared_difference::squared_difference_sve>:
   1eb30:   b40001e1    cbz x1, 1eb6c <squared_difference::squared_difference_sve+0x3c>
   1eb34:   6f00e400    movi    v0.2d, #0x0
   1eb38:   aa1f03e8    mov x8, xzr
   1eb3c:   04a0e3e9    cntw    x9
   1eb40:   25a11d00    whilelo p0.s, x8, x1
   1eb44:   a5484001    ld1w    {z1.s}, p0/z, [x0, x8, lsl #2]
   1eb48:   a5484042    ld1w    {z2.s}, p0/z, [x2, x8, lsl #2]
   1eb4c:   8b090108    add x8, x8, x9
   1eb50:   eb01011f    cmp x8, x1
   1eb54:   65818041    fsub    z1.s, p0/m, z1.s, z2.s
   1eb58:   65a10020    fmla    z0.s, p0/m, z1.s, z1.s
   1eb5c:   54ffff23    b.cc    1eb40 <squared_difference::squared_difference_sve+0x10>  // b.lo, b.ul, b.last
   1eb60:   2598e3e0    ptrue   p0.s
   1eb64:   65802000    faddv   s0, p0, z0.s
   1eb68:   d65f03c0    ret
   1eb6c:   6f00e400    movi    v0.2d, #0x0
   1eb70:   2598e3e0    ptrue   p0.s
   1eb74:   65802000    faddv   s0, p0, z0.s
   1eb78:   d65f03c0    ret
...
```
The SVE implementations matches very closely the source code, thanks to the intrinsics: we can observe the loads (`ld1w`), the difference (`fsub`) and the multiply-accumulate (`fmla`) in the loop. The accumulator is then reduced with `faddv` to return the result.

- `squared_difference::squared_difference_simd`:
```txt
...
000000000001eb7c <squared_difference::squared_difference_simd>:
   1eb7c:   fc1e0fe8    str d8, [sp, #-32]!
   1eb80:   a9017bfd    stp x29, x30, [sp, #16]
   1eb84:   910043fd    add x29, sp, #0x10
   1eb88:   d344fc2a    lsr x10, x1, #4
   1eb8c:   b4000a0a    cbz x10, 1eccc <squared_difference::squared_difference_simd+0x150>
   1eb90:   6f00e402    movi    v2.2d, #0x0
   1eb94:   6f00e403    movi    v3.2d, #0x0
   1eb98:   d37ced4a    lsl x10, x10, #4
   1eb9c:   6f00e401    movi    v1.2d, #0x0
   1eba0:   6f00e400    movi    v0.2d, #0x0
   1eba4:   aa1f03e9    mov x9, xzr
   1eba8:   aa1f03e8    mov x8, xzr
   1ebac:   aa0003eb    mov x11, x0
   1ebb0:   aa0203ec    mov x12, x2
   1ebb4:   eb01011f    cmp x8, x1
   1ebb8:   54000a68    b.hi    1ed04 <squared_difference::squared_difference_simd+0x188>  // b.pmore
   1ebbc:   8b09002d    add x13, x1, x9
   1ebc0:   f1003dbf    cmp x13, #0xf
   1ebc4:   54000aa9    b.ls    1ed18 <squared_difference::squared_difference_simd+0x19c>  // b.plast
   1ebc8:   eb03011f    cmp x8, x3
   1ebcc:   54000b28    b.hi    1ed30 <squared_difference::squared_difference_simd+0x1b4>  // b.pmore
   1ebd0:   8b09006d    add x13, x3, x9
   1ebd4:   f1003dbf    cmp x13, #0xf
   1ebd8:   54000ba9    b.ls    1ed4c <squared_difference::squared_difference_simd+0x1d0>  // b.plast
   1ebdc:   ad411165    ldp q5, q4, [x11, #32]
   1ebe0:   91004108    add x8, x8, #0x10
   1ebe4:   ad401d86    ldp q6, q7, [x12]
   1ebe8:   eb08015f    cmp x10, x8
   1ebec:   acc24171    ldp q17, q16, [x11], #64
   1ebf0:   ad414993    ldp q19, q18, [x12, #32]
   1ebf4:   d1004129    sub x9, x9, #0x10
   1ebf8:   9101018c    add x12, x12, #0x40
   1ebfc:   4ea7d607    fsub    v7.4s, v16.4s, v7.4s
   1ec00:   4ea6d626    fsub    v6.4s, v17.4s, v6.4s
   1ec04:   4eb3d4a5    fsub    v5.4s, v5.4s, v19.4s
   1ec08:   4eb2d484    fsub    v4.4s, v4.4s, v18.4s
   1ec0c:   6e27dce7    fmul    v7.4s, v7.4s, v7.4s
   1ec10:   6e26dcc6    fmul    v6.4s, v6.4s, v6.4s
   1ec14:   6e24dc84    fmul    v4.4s, v4.4s, v4.4s
   1ec18:   6e25dca5    fmul    v5.4s, v5.4s, v5.4s
   1ec1c:   4e27d463    fadd    v3.4s, v3.4s, v7.4s
   1ec20:   4e26d442    fadd    v2.4s, v2.4s, v6.4s
   1ec24:   4e25d421    fadd    v1.4s, v1.4s, v5.4s
   1ec28:   4e24d400    fadd    v0.4s, v0.4s, v4.4s
   1ec2c:   54fffc41    b.ne    1ebb4 <squared_difference::squared_difference_simd+0x38>  // b.any
   1ec30:   927ce028    and x8, x1, #0x1ffffffffffffff0
   1ec34:   eb080069    subs    x9, x3, x8
   1ec38:   54000583    b.cc    1ece8 <squared_difference::squared_difference_simd+0x16c>  // b.lo, b.ul, b.last
   1ec3c:   5e140444    mov s4, v2.s[2]
   1ec40:   7e30d845    faddp   s5, v2.2s
   1ec44:   d37ef508    lsl x8, x8, #2
   1ec48:   5e1c0442    mov s2, v2.s[3]
   1ec4c:   92400c21    and x1, x1, #0xf
   1ec50:   aa0903e3    mov x3, x9
   1ec54:   8b080000    add x0, x0, x8
   1ec58:   8b080042    add x2, x2, x8
   1ec5c:   1e2428a4    fadd    s4, s5, s4
   1ec60:   1e222882    fadd    s2, s4, s2
   1ec64:   5e0c0464    mov s4, v3.s[1]
   1ec68:   1e232842    fadd    s2, s2, s3
   1ec6c:   1e242842    fadd    s2, s2, s4
   1ec70:   5e140464    mov s4, v3.s[2]
   1ec74:   5e1c0463    mov s3, v3.s[3]
   1ec78:   1e242842    fadd    s2, s2, s4
   1ec7c:   1e232842    fadd    s2, s2, s3
   1ec80:   5e0c0423    mov s3, v1.s[1]
   1ec84:   1e212842    fadd    s2, s2, s1
   1ec88:   1e232842    fadd    s2, s2, s3
   1ec8c:   5e140423    mov s3, v1.s[2]
   1ec90:   5e1c0421    mov s1, v1.s[3]
   1ec94:   1e232842    fadd    s2, s2, s3
   1ec98:   1e212841    fadd    s1, s2, s1
   1ec9c:   5e0c0402    mov s2, v0.s[1]
   1eca0:   1e202821    fadd    s1, s1, s0
   1eca4:   1e222821    fadd    s1, s1, s2
   1eca8:   5e140402    mov s2, v0.s[2]
   1ecac:   5e1c0400    mov s0, v0.s[3]
   1ecb0:   1e222821    fadd    s1, s1, s2
   1ecb4:   1e202828    fadd    s8, s1, s0
   1ecb8:   9400002b    bl  1ed64 <squared_difference::squared_difference_scalar>
   1ecbc:   1e202900    fadd    s0, s8, s0
   1ecc0:   a9417bfd    ldp x29, x30, [sp, #16]
   1ecc4:   fc4207e8    ldr d8, [sp], #32
   1ecc8:   d65f03c0    ret
   1eccc:   6f00e402    movi    v2.2d, #0x0
   1ecd0:   6f00e403    movi    v3.2d, #0x0
   1ecd4:   6f00e401    movi    v1.2d, #0x0
   1ecd8:   6f00e400    movi    v0.2d, #0x0
   1ecdc:   927ce028    and x8, x1, #0x1ffffffffffffff0
   1ece0:   eb080069    subs    x9, x3, x8
   1ece4:   54fffac2    b.cs    1ec3c <squared_difference::squared_difference_simd+0xc0>  // b.hs, b.nlast
   1ece8:   90000289    adrp    x9, 6e000 <anon.594011ee5b8ef3db52b32c08a333e37c.208.llvm.8589523194035409723+0x4c0>
   1ecec:   91336129    add x9, x9, #0xcd8
   1ecf0:   aa0803e0    mov x0, x8
   1ecf4:   aa0303e1    mov x1, x3
   1ecf8:   aa0303e2    mov x2, x3
   1ecfc:   aa0903e3    mov x3, x9
   1ed00:   97ff985d    bl  4e74 <core::slice::index::slice_index_fail>
   1ed04:   90000283    adrp    x3, 6e000 <anon.594011ee5b8ef3db52b32c08a333e37c.208.llvm.8589523194035409723+0x4c0>
   1ed08:   9134e063    add x3, x3, #0xd38
   1ed0c:   aa0803e0    mov x0, x8
   1ed10:   aa0103e2    mov x2, x1
   1ed14:   97ff9858    bl  4e74 <core::slice::index::slice_index_fail>
   1ed18:   f0000140    adrp    x0, 49000 <panic_unwind::imp::CANARY+0x35>
   1ed1c:   91090800    add x0, x0, #0x242
   1ed20:   90000282    adrp    x2, 6e000 <anon.594011ee5b8ef3db52b32c08a333e37c.208.llvm.8589523194035409723+0x4c0>
   1ed24:   9133c042    add x2, x2, #0xcf0
   1ed28:   52800d21    mov w1, #0x69                   // #105
   1ed2c:   97ff9848    bl  4e4c <core::panicking::panic_fmt>
   1ed30:   90000289    adrp    x9, 6e000 <anon.594011ee5b8ef3db52b32c08a333e37c.208.llvm.8589523194035409723+0x4c0>
   1ed34:   91348129    add x9, x9, #0xd20
   1ed38:   aa0803e0    mov x0, x8
   1ed3c:   aa0303e1    mov x1, x3
   1ed40:   aa0303e2    mov x2, x3
   1ed44:   aa0903e3    mov x3, x9
   1ed48:   97ff984b    bl  4e74 <core::slice::index::slice_index_fail>
   1ed4c:   f0000140    adrp    x0, 49000 <panic_unwind::imp::CANARY+0x35>
   1ed50:   91090800    add x0, x0, #0x242
   1ed54:   90000282    adrp    x2, 6e000 <anon.594011ee5b8ef3db52b32c08a333e37c.208.llvm.8589523194035409723+0x4c0>
   1ed58:   91342042    add x2, x2, #0xd08
   1ed5c:   52800d21    mov w1, #0x69                   // #105
   1ed60:   97ff983b    bl  4e4c <core::panicking::panic_fmt>

...
```
The SIMD implementation is much bigger, as the compiler has applied an unrolling factor of x4. Note:
- the initializer of the 4 accumulators (`v0.2d`, `v1.2d`, `v2.2d` and `v3.2d`) with `movi    v?.2d, #0x0`,
- the 4 load-pair (`ldp`),
- the 4 `fsub` to compute the difference,
- the 4 `fmul` + `fadd` to perform the multiply-accumulate.

This explains why the SIMD version performances almost reach the SVE performances --- at the expense of a much bigger function though.

- `squared_difference::squared_difference_scalar`:
```
000000000001ed64 <squared_difference::squared_difference_scalar>:
   1ed64:   0f046400    movi    v0.2s, #0x80, lsl #24
   1ed68:   eb03003f    cmp x1, x3
   1ed6c:   9a833028    csel    x8, x1, x3, cc  // cc = lo, ul, last
   1ed70:   b40005c8    cbz x8, 1ee28 <squared_difference::squared_difference_scalar+0xc4>
   1ed74:   f100211f    cmp x8, #0x8
   1ed78:   54000062    b.cs    1ed84 <squared_difference::squared_difference_scalar+0x20>  // b.hs, b.nlast
   1ed7c:   aa1f03e9    mov x9, xzr
   1ed80:   1400001f    b   1edfc <squared_difference::squared_difference_scalar+0x98>
   1ed84:   927de509    and x9, x8, #0x1ffffffffffffff8
   1ed88:   9100404a    add x10, x2, #0x10
   1ed8c:   9100400b    add x11, x0, #0x10
   1ed90:   927de50c    and x12, x8, #0x1ffffffffffffff8
   1ed94:   ad7f9141    ldp q1, q4, [x10, #-16]
   1ed98:   f100218c    subs    x12, x12, #0x8
   1ed9c:   ad7f8d62    ldp q2, q3, [x11, #-16]
   1eda0:   9100814a    add x10, x10, #0x20
   1eda4:   9100816b    add x11, x11, #0x20
   1eda8:   4ea1d441    fsub    v1.4s, v2.4s, v1.4s
   1edac:   6e21dc21    fmul    v1.4s, v1.4s, v1.4s
   1edb0:   5e0c0422    mov s2, v1.s[1]
   1edb4:   1e212800    fadd    s0, s0, s1
   1edb8:   5e140425    mov s5, v1.s[2]
   1edbc:   5e1c0421    mov s1, v1.s[3]
   1edc0:   1e222800    fadd    s0, s0, s2
   1edc4:   4ea4d462    fsub    v2.4s, v3.4s, v4.4s
   1edc8:   1e252800    fadd    s0, s0, s5
   1edcc:   6e22dc42    fmul    v2.4s, v2.4s, v2.4s
   1edd0:   1e212800    fadd    s0, s0, s1
   1edd4:   5e0c0441    mov s1, v2.s[1]
   1edd8:   5e140443    mov s3, v2.s[2]
   1eddc:   1e222800    fadd    s0, s0, s2
   1ede0:   1e212800    fadd    s0, s0, s1
   1ede4:   5e1c0441    mov s1, v2.s[3]
   1ede8:   1e232800    fadd    s0, s0, s3
   1edec:   1e212800    fadd    s0, s0, s1
   1edf0:   54fffd21    b.ne    1ed94 <squared_difference::squared_difference_scalar+0x30>  // b.any
   1edf4:   eb09011f    cmp x8, x9
   1edf8:   54000180    b.eq    1ee28 <squared_difference::squared_difference_scalar+0xc4>  // b.none
   1edfc:   d37ef52b    lsl x11, x9, #2
   1ee00:   cb090108    sub x8, x8, x9
   1ee04:   8b0b004a    add x10, x2, x11
   1ee08:   8b0b000b    add x11, x0, x11
   1ee0c:   bc404561    ldr s1, [x11], #4
   1ee10:   f1000508    subs    x8, x8, #0x1
   1ee14:   bc404542    ldr s2, [x10], #4
   1ee18:   1e223821    fsub    s1, s1, s2
   1ee1c:   1e210821    fmul    s1, s1, s1
   1ee20:   1e212800    fadd    s0, s0, s1
   1ee24:   54ffff41    b.ne    1ee0c <squared_difference::squared_difference_scalar+0xa8>  // b.any
   1ee28:   d65f03c0    ret
...
```
The scalar implementation is ... no longer scalar, it has been automatically vectorized by the compiler, but without unrolling, which explains why we observe a performance improvement below 4x when compared to SIMD (the unrolling adds some overhead, so the exact 4x improvement is not reached).