<!--
  SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
  SPDX-License-Identifier: BSD-3-Clause-Clear
-->

# Enable Arm SVE vectorization in Go

| Item | Details |
|---|---|
| Estimated time | 45–60 minutes |
| Target audience | Go developers exploring Arm SVE from Go |
| Required platform | Any Go 1.24 platform; SVE is selected automatically on supported Linux Arm64 systems |
| Required software | Go 1.24 and a GCC- or Clang-compatible C++ compiler for the Linux Arm64 SVE path |

# Introduction

## About this task

Go is widely used for [network services, cloud infrastructure, command-line
tools, and development operations](https://go.dev/solutions/use-cases). Its
garbage collection, standard library, fast compilation, and lightweight
[goroutines and channels](https://go.dev/tour/concurrency/1) suit concurrent
software. It was not initially designed for explicitly vectorized workloads,
and the Go compiler performs little or no general-purpose loop
autovectorization.

The [`cgo` command](https://pkg.go.dev/cmd/cgo) lets Go call C ABI functions.
This task uses it to run vector-length-agnostic SVE kernels written in C++ with
Arm C Language Extensions (ACLE) intrinsics. The focus is enabling, selecting,
and measuring those kernels from Go, not writing the intrinsics themselves.
The kernels are reused from earlier course tasks.

This task also avoids Go assembly. Go assembly would add architecture-specific
code and maintenance while moving attention away from the Go-to-SVE interface
being studied. Instead, you will compare:

| Workload | Implementations |
|---|---|
| Vector-vector addition | Scalar Go and cgo SVE |
| [Base64 encoding](../../module-11-base64/base64/README.md) | Go's [`encoding/base64`](https://pkg.go.dev/encoding/base64) and cgo SVE |

The learner-facing `VectorAdd` and `Base64Encode` functions hide CPU selection.
They check the operating system's reported SVE support at runtime and call
either scalar Go or a private cgo wrapper. Learners therefore build and run the
same program whether or not their system supports SVE.

By completing this workflow, you will be able to:

- explain why an ordinary Go loop does not normally become SVE code;
- trace a runtime-dispatched Go call into a C++ SVE kernel;
- explain the relevant cgo pointer and lifetime rules; and
- measure when useful SVE work offsets the Go/C call cost.

# Before you begin

## Prerequisites

[Install Go](https://go.dev/doc/install), then confirm that `go version` reports
Go 1.24 or newer. You should also be familiar with Go slices, tests, and
benchmarks.

All systems can build the Go fallback when cgo is disabled. On Linux Arm64 with
cgo enabled, building the SVE path requires a GCC- or Clang-compatible C++
compiler that supports SVE intrinsics.

An SVE-capable CPU is not required. Runtime checks prevent unsupported systems
from entering SVE or SVE2 functions. Performance measurements of those
functions require Linux Arm64 with the corresponding feature exposed to the
process.

# Plan the Go-to-SVE workflow

You will:

1. build and run the automatically dispatched program;
2. trace the public Go operations to their Go or SVE implementations;
3. review the pointer and call-boundary requirements;
4. test the SVE and fallback paths; and
5. benchmark the documented implementations.

## Repository and workflow assets

All Go task files are under `module-13-go-vectorization/golang/`:

```text
module-13-go-vectorization/golang/
├── README.md
├── go.mod
├── go.sum
├── main.go                       # runnable course example
├── operations.go                 # public operations and runtime dispatch
├── kernels_linux_arm64.go         # CPU detection and cgo wrapper
├── kernels_fallback.go            # definitions for other build configurations
├── vector_kernels.h               # C ABI declarations
├── vector_kernels.cpp             # C++ SVE kernels
├── operations_test.go
└── benchmark_test.go
```

The module requires [`golang.org/x/sys/cpu`](https://pkg.go.dev/golang.org/x/sys/cpu)
for operating-system CPU capability reports. From the repository root, enter the
task directory before running the remaining commands:

```sh
cd module-13-go-vectorization/golang
```

# Complete the Go-to-SVE workflow

## 1. Build and run the automatically dispatched program

The program is designed to run portably on systems with and without Arm SVE:

```sh
mkdir -p build
go build -o build/golang-sve .
./build/golang-sve
```

The build writes `build/golang-sve`. On the tested Arm Neoverse V2 system the
program printed:

```text
System CPU features

SVE available:  true
SVE2 available: true

Running Vector Addition of  [1 2 3 4]  +  [10 20 30 40]
Scalar Go: [11 22 33 44]
Cgo SVE:    [11 22 33 44]

Running Base64 Encoding of "Hello, SVE from Go!"
Base64 (Cgo SVE): SGVsbG8sIFNWRSBmcm9tIEdvIQ==
```

On a system without SVE, the capability line reports `false`, the program labels
the automatically dispatched result `Go fallback`, and the results remain the
same. `main.go` is an executable course example rather than a production-ready
library.

### Testing

Build the Go test binary once, then run it with each supported SVE vector
length:

```sh
go test -c -o /tmp/golang-sve.test .
for vector_bits in 128 256 512 1024 2048; do
    vector_bytes=$((vector_bits / 8))
    echo "SVE vector length: ${vector_bits} bits"
    qemu-aarch64 -cpu "max,sve-default-vector-length=${vector_bytes}" \
        /tmp/golang-sve.test -test.v
done
```

These tests exercise the C++ kernels through the Go cgo wrappers. If you change
or need to diagnose the kernels themselves, you may also need to test the C++
implementations separately.

Runtime dispatch keeps both the Go fallback and SVE implementation in the
executable, and cgo adds its wrapper and support code. This increases file size
compared with a fallback-only build. On the tested system, the cgo executable
was 2,494,672 bytes, compared with 2,459,508 bytes with `CGO_ENABLED=0`: about
35 KB, or 1.4%. The exact difference depends on the toolchain and linking
configuration.

### Inspect the encapsulated operations

`operations.go` owns the runtime selection. Its vector-addition flow is:

```go
func VectorAdd(dst, x, y []int32) {
    checkVectorLengths(len(dst), len(x), len(y))
    if len(dst) == 0 {
        return
    }
    // Tip: If VectorAdd is called frequently, avoid repeated runtime checks by performing
	  // feature detection at a higher level and dispatching directly to the SVE variant.
	  // This may also enable additional compiler optimizations.
    if runtimeHasSVE {
        vectorAddSVE(dst, x, y)
        return
    }
    vectorAddScalar(dst, x, y)
}
```

`Base64Encode` follows the same pattern after checking its destination length.
Callers use operation names and do not select scalar or SVE implementations.
The unexported helpers exist for dispatch, tests, and benchmark comparisons.

The feature variables snapshot `detectSVE()` and `detectSVE2()` once. On Linux
Arm64 with cgo, those functions return `cpu.ARM64.HasSVE` and
`cpu.ARM64.HasSVE2`. These values reflect capabilities exposed to the process by
the operating system, not merely what the hardware might implement.

### Understand the two build configurations

[`//go:build` constraints](https://pkg.go.dev/cmd/go#hdr-Build_constraints)
select `kernels_linux_arm64.go` for `linux && arm64 && cgo`. That file contains
feature detection and the private functions that call C. Runtime dispatch still
happens in `operations.go` before a C function is entered.

Every other configuration selects `kernels_fallback.go`. This file is required
because the program must still define `detectSVE`, `detectSVE2`, and the private
kernel functions when the Linux Arm64 cgo file is excluded. Its detectors
return `false`, and its kernel functions defensively call scalar Go.

The resulting selection is:

| Build and runtime state | Public-operation behavior |
|---|---|
| Linux Arm64, cgo enabled, required SVE feature reported | Call the corresponding private cgo wrapper |
| Linux Arm64, cgo enabled, required SVE feature absent | Call scalar Go directly |
| cgo disabled | Compile `kernels_fallback.go` and call scalar Go |
| Non-Linux or non-Arm64 | Compile `kernels_fallback.go` and call scalar Go |

This separation answers two different questions. Build constraints decide
which source files can compile for a platform; runtime detection decides which
instructions this process may execute. Successfully compiling an SVE function
does not establish that it is safe to call on the eventual system.

The C++ file is compiled for baseline Armv8-A. Its SVE functions have GCC- and
Clang-compatible [`target`
attributes](https://gcc.gnu.org/onlinedocs/gcc/AArch64-Function-Attributes.html)
for `+sve` or `+sve2`. This confines scalable-vector instructions to functions
protected by the matching runtime check and lets the build system compile on an
Arm64 system that does not itself expose SVE.

## 2. Trace the reused implementations

### Vector addition

`VectorAdd` verifies equal slice lengths. An empty slice is valid, and `dst` may
be exactly the same slice as either input. When SVE is available,
`vectorAddSVE` passes three backing-array addresses and the element count to
`sve_vector_addition_i32`. The C++ loop uses a predicate for the final partial
vector and advances by `svcntw()`, so it contains no fixed lane count.

### Base64 encoding

`Base64Encode` uses the standard padded alphabet. The destination must contain
at least `((len(src) + 2) / 3) * 4` bytes and must not overlap the source. Empty
input returns without crossing the cgo boundary.

When SVE is available, `base64EncodeSVE` calls `base64_sve_encode`. Otherwise,
the public function calls `base64.StdEncoding.Encode`. The benchmark therefore
compares the stable Go standard-library implementation with the reused SVE
implementation.

Both workloads keep validation and fallback selection in Go; the C++ functions
receive already-validated buffers and perform only the requested computation.

## 3. Pass Go memory through cgo safely

C/C++ functions operate on raw pointers and sizes rather than Go slices. Pass the address of the slice data and its length separately.

[`unsafe.Pointer`](https://pkg.go.dev/unsafe) allows the Go pointer to be converted to the corresponding C pointer type without copying the data. The C/C++ type must have compatible size, alignment, and layout.

```go
(*C.int32_t)(unsafe.Pointer(&values[0]))
```

The usual [⁠cgo pointer rules](https://pkg.go.dev/cmd/cgo#hdr-Passing_pointers) still apply. In particular, C/C++ must not retain the Go pointer after the call returns. Empty slices must also be handled before taking `&values[0]`.

Longer-lived integrations need different ownership mechanisms. Use
[`cgo.Handle`](https://pkg.go.dev/runtime/cgo#Handle) when C must retain an
opaque token for a later Go callback. Use
[`runtime.Pinner`](https://pkg.go.dev/runtime#Pinner) only when C must retain a
pointer to Go memory beyond one call. Neither is needed here.

These synchronous calls are permitted in this example since:

- the slices contain pointer-free int32 elements;
- cgo keeps the referenced Go memory valid for the duration of the call; and
- the C/C++ functions do not retain the pointers after returning.

For the complete rules and their exceptions, read the Go documentation on
[passing pointers and optimizing C
calls](https://pkg.go.dev/cmd/cgo#hdr-Passing_pointers).

## 4. Verify correctness and fallbacks

Run the tests, the no-cgo build configuration, the stricter cgo pointer checker,
and static analysis:

```sh
go test ./...
CGO_ENABLED=0 go test ./...
GOEXPERIMENT=cgocheck2 go test ./...
go vet ./...
```

The explicit `GOEXPERIMENT` value enables the stricter cgo pointer checker for
the third command. On the tested system the three test commands printed:

```text
ok  github.com/arm-education/SVE-task-based-course/golang  0.003s
ok  github.com/arm-education/SVE-task-based-course/golang  0.002s
ok  github.com/arm-education/SVE-task-based-course/golang  0.003s
```

`go vet` prints nothing when it succeeds. The tests cover empty and tail inputs,
wrapping `int32` arithmetic, exact in-place addition, invalid lengths, Base64
padding and bounds, and forced runtime fallbacks.

## 5. Benchmark the documented implementations

Run all vector-addition and Base64 benchmarks without running unit tests first:

```sh
go test . -run '^$' \
  -bench '^(BenchmarkVectorAdd|BenchmarkBase64Encode)$' \
  -benchmem -cpu=1
```

The following results were captured on an Arm Neoverse V2 system using Go
1.27.0 and GCC 15.2.0. Timings vary by system:

```text
goos: linux
goarch: arm64
pkg: github.com/arm-education/SVE-task-based-course/golang
BenchmarkVectorAdd/size_16/ScalarGo          100000000  10.38 ns/op  18495.84 MB/s  0 B/op  0 allocs/op
BenchmarkVectorAdd/size_16/CgoSVE             24219944  49.55 ns/op   3874.55 MB/s  0 B/op  0 allocs/op
BenchmarkVectorAdd/size_256/ScalarGo           8061321 149.0 ns/op   20622.94 MB/s  0 B/op  0 allocs/op
BenchmarkVectorAdd/size_256/CgoSVE            15417013  77.93 ns/op  39419.84 MB/s  0 B/op  0 allocs/op
BenchmarkVectorAdd/size_4096/ScalarGo           511107   2345 ns/op  20963.80 MB/s  0 B/op  0 allocs/op
BenchmarkVectorAdd/size_4096/CgoSVE            2002746  599.2 ns/op  82023.44 MB/s  0 B/op  0 allocs/op
BenchmarkBase64Encode/size_16/StdlibGo        89158900  13.45 ns/op   1189.42 MB/s  0 B/op  0 allocs/op
BenchmarkBase64Encode/size_16/CgoSVE          21507432  55.46 ns/op    288.49 MB/s  0 B/op  0 allocs/op
BenchmarkBase64Encode/size_256/StdlibGo        7352022 163.5 ns/op    1565.52 MB/s  0 B/op  0 allocs/op
BenchmarkBase64Encode/size_256/CgoSVE         11391945 105.4 ns/op    2428.40 MB/s  0 B/op  0 allocs/op
BenchmarkBase64Encode/size_4096/StdlibGo        467167   2564 ns/op   1597.28 MB/s  0 B/op  0 allocs/op
BenchmarkBase64Encode/size_4096/CgoSVE         1368302  876.9 ns/op   4670.98 MB/s  0 B/op  0 allocs/op
PASS
ok  github.com/arm-education/SVE-task-based-course/golang  17.839s
```

`CgoSVE` cases skip when SVE is unavailable rather than reporting Go fallback
timings under an SVE label. Addition reports two input reads plus one output
write; Base64 reports input bytes. Buffers are allocated before timing.

> FFI stands for **Foreign Function Interface**: the mechanism for code in one
> language to call code using another language's interface. Go FFI calls into C
> ABI code have high per-function-call overhead because substantial work is
> required to move into and out of the Go ABI. This can be more costly than the
> equivalent call from an environment already using the C ABI. Keep as much of
> a kernel as practical on the C side and amortize one transition over enough
> useful work. Although it examines a Go/assembly boundary rather than cgo,
> [coregex issue #120](https://github.com/coregx/coregex/issues/120) illustrates
> the same failure mode when work is split into too many small calls.

The measurements show the effect. SVE is about 1.9 times faster than scalar Go
at 256 `int32` elements and 3.9 times faster at 4096 elements. At 16 elements,
however, the cgo call cost dominates and SVE is slower than scalar Go. The
measured switch point at which SVE becomes faster than scalar Go is therefore
somewhere between 16 and 256 elements; add intermediate sizes to locate it more
precisely on a target system.

For Base64 input of 4096 bytes, the SVE implementation takes 876.9 ns compared
with 2564 ns for `encoding/base64`: approximately 2.9 times faster. Treat these as workload- and system-specific results rather than a
general guarantee.

# Review the evidence

## Compare the available approaches

| Approach | Advantages | Costs and limits | Prefer it when |
|---|---|---|---|
| Scalar Go | Idiomatic, memory-safe, portable, and simple to cross-compile | Go does not generally autovectorize loops | Simplicity dominates or inputs are small |
| Runtime-dispatched cgo SVE | Scalable vectors, safe fallback, reuses C++ kernels | Call overhead, `unsafe.Pointer`, CPU detection, and C++ toolchain | Work can be batched and measurements justify the complexity |

The cgo interface is supported, but target instructions and the C ABI still
require deployment care. Cross-compilation normally needs a suitable C++
cross-compiler. Runtime checks solve instruction availability; they do not
remove toolchain or ABI requirements.

Use the evidence to answer:

- At which sizes does useful SVE work amortize the cgo transition?
- Does SVE Base64 outperform the Go standard library on this system?
- Do the measured gains justify the pointer, toolchain, and deployment costs?

# Check your understanding

**Why can the same executable run on Linux Arm64 systems with and without SVE?**

The public operations check `cpu.ARM64.HasSVE` or `HasSVE2` before calling a
specialized function. A false result selects scalar Go.

**Why is `kernels_fallback.go` needed if runtime checks already exist?**

Build constraints exclude the Linux Arm64 cgo file on other configurations.
The fallback file supplies the definitions needed for those configurations to
compile, while its detectors ensure that the public operations choose Go.

**Why can cgo SVE lose on a small slice?**

The fixed Go/C transition cost can exceed the useful work performed by a small
kernel. Batching more work into one call amortizes that cost.

# Apply the conclusion

Keep operation selection behind a small Go API. Detect SVE at runtime, enter C
only after the matching feature check, keep the cgo wrapper narrow, and pass
only pointer-free buffers for synchronous calls. Benchmark realistic sizes to
find the crossover point rather than assuming wider vectors always win.

cgo provides access to scalable SVE today. Choose it only when its measured
benefit outweighs the additional pointer, ABI, toolchain, and deployment work.

## Further reading

- [Install Go](https://go.dev/doc/install)
- [cgo command, pointer rules, and call optimization](https://pkg.go.dev/cmd/cgo)
- [`unsafe.Pointer`](https://pkg.go.dev/unsafe#Pointer)
- [`golang.org/x/sys/cpu`](https://pkg.go.dev/golang.org/x/sys/cpu)
- [Linux Arm64 hardware capabilities](https://docs.kernel.org/arch/arm64/elf_hwcaps.html)
- [Arm C Language Extensions](https://arm-software.github.io/acle/)
- [Go compiler autovectorization discussion](https://github.com/golang/go/issues/80857)
- [Go proposal for Arm64 SVE intrinsics](https://github.com/golang/go/issues/79781)
- [Go/assembly boundary overhead example](https://github.com/coregx/coregex/issues/120)
