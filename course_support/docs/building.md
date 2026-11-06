<!--
  SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
  SPDX-License-Identifier: BSD-3-Clause-Clear
-->

# Building, testing, and benchmarking

Use the [course](https://learn.arm.com/courses/sve/) for each exercise's setup and instructions. This guide describes the shared CMake build for the C++ tasks. Run its commands from the repository root unless stated otherwise.

The [Rust task](../../module-12-rust-squared-distance/rust/) and [Go task](../../module-13-go-vectorization/golang/) use Cargo and Go respectively, and are built and tested separately from CMake. See [Rust and Go examples](#rust-and-go-examples) below.

## Software requirements

- CMake 3.20 or later.
- Ninja.
- GCC 13 or later, or Clang 18 or later, with C++20 and SVE support. Individual exercises can require additional architecture features or newer compilers.
- Git and network access for the initial dependency downloads.

On Ubuntu 24.04, for example:

```sh
sudo apt update
sudo apt install -y cmake ninja-build g++-13 git
```

## Platform assumptions

The C++ tasks target AArch64 Linux. Some reference implementations are portable, but a complete build on other architectures is not supported by the course workflow. Use native hardware with the required SVE features for performance measurements.

By default, CMake targets the native CPU with `-mcpu=native` or `-march=native`, enables `-Wall`, and uses the normal CMake flags for `Debug` and `Release`. Configuration checks for SVE, SVE2, SVE2.1, and SVE2.2. Missing SVE produces a warning rather than a configuration error; availability of reference implementations and feature-specific paths depends on the task.

On an AArch64 Linux machine without SVE, use the [QEMU workflow](#cross-compiling-for-sve) to build and test the SVE paths.

## Third-party software

During configuration, CMake downloads, builds, and installs these dependencies into the build directory:

| Dependency | Purpose | License |
|---|---|---|
| [GoogleTest](https://github.com/google/googletest) v1.17.0 | Unit tests | [GoogleTest license](https://github.com/google/googletest/blob/v1.17.0/LICENSE) |
| [Google Benchmark](https://github.com/google/benchmark) v1.9.5 | Benchmarks | [Google Benchmark license](https://github.com/google/benchmark/blob/v1.9.5/LICENSE) |

These dependencies are not bundled with the repository and retain their own licenses. Their versions are configured in [course_support/external/CMakeLists.txt](../external/CMakeLists.txt).

## Configuring and building

Use Ninja Multi-Config to keep `Debug` and `Release` builds in one build directory:

```sh
cmake -G "Ninja Multi-Config" -S course_support -B build
cmake --build build --config Release
```

`-G` selects the generator, `-S course_support` selects the shared CMake project as the source directory, and `-B build` selects the generated build directory. The first configuration can take longer because it fetches and builds the dependencies. The `build/` directory is ignored by Git; do not edit its generated files.

If you previously configured with `-S .`, use a fresh build directory when switching to `-S course_support`: CMake records the source directory in its cache. For example, configure with `-B build/updated-layout` and use that build directory in subsequent commands. Task names and output paths relative to the build directory remain the same: for example, the source for `strlen` is in `module-03-c-string-length/strlen/`, and its Release executable is `build/strlen/Release/strlen`.

Select Debug explicitly when needed:

```sh
cmake --build build --config Debug
```

To build one task or clean one configuration:

```sh
cmake --build build --config Release --target strlen
cmake --build build --config Release --target clean
```

### Selecting a compiler

Set `CC` and `CXX` when configuring a fresh build directory. For example, after installing Clang 18:

```sh
CC=/usr/bin/clang-18 CXX=/usr/bin/clang++-18 cmake -G "Ninja Multi-Config" -S course_support -B build/clang
cmake --build build/clang --config Release
```

Use that same build directory in subsequent test and benchmark commands. If CCache is available, the project enables it by default; configure with `-DENABLE_CCACHE_BUILD=OFF` to disable it.

## Testing

Run the unit tests for all CMake tasks:

```sh
cmake --build build --config Release --target test-all
```

Run one task's tests using its original task name, without the module prefix:

```sh
cmake --build build --config Release --target test-strlen
```

The general target name is `test-<task_name>`. Replace `Release` with `Debug` to test the Debug build. The `test-all` target also writes `build/test/test-all.json`.

## Benchmarking

Use Release builds for meaningful performance results. Build the shared benchmark executable without running it:

```sh
cmake --build build --config Release --target benchmark
```

Run all benchmarks or one task's benchmarks:

```sh
cmake --build build --config Release --target benchmark-all
cmake --build build --config Release --target benchmark-strlen
```

The general target name is `benchmark-<task_name>`. The `benchmark-all` target also writes `build/benchmark/benchmark-all.json`. Interpret measurements using the task's workload and platform guidance.

## Cross-compiling for SVE

The `STBC_SVE_CROSS_COMPILE` option supports the course workflow on AArch64 Linux hosts without SVE hardware. It selects SVE/SVE2 compiler flags and runs CMake test and benchmark targets under QEMU user-mode emulation. It does not install or select a cross-compiler for an x86 host.

Install QEMU if needed:

```sh
sudo apt update
sudo apt install qemu-user
```

Enable the option and build:

```sh
cmake -G "Ninja Multi-Config" -S course_support -B build -DSTBC_SVE_CROSS_COMPILE=ON
cmake --build build --config Release
cmake --build build --config Release --target test-all
```

This uses `-march=armv8-a+sve+sve2` in place of the native CPU flag. CMake finds `qemu-aarch64` or `qemu-aarch64-static` on `PATH`; specify its location if necessary:

```sh
cmake -G "Ninja Multi-Config" -S course_support -B build \
  -DSTBC_SVE_CROSS_COMPILE=ON \
  -DSTBC_QEMU_EXECUTABLE=/path/to/qemu-aarch64
```

The `test-*` and `benchmark-*` targets use the generated QEMU wrapper automatically. To run a task executable directly, invoke the wrapper explicitly:

```sh
./build/qemu-wrapper.sh ./build/strlen/Release/strlen
```

Benchmark commands remain available under QEMU, but their timings do not represent native hardware performance. Use emulation to check correctness and native SVE hardware to measure performance.

### Changing the SVE vector length

With the QEMU option enabled, set `STBC_SVE_VL` to a supported vector length in bits:

```sh
STBC_SVE_VL=128  cmake --build build --config Release --target test-all
STBC_SVE_VL=256  cmake --build build --config Release --target test-all
STBC_SVE_VL=512  cmake --build build --config Release --target test-all
STBC_SVE_VL=1024 cmake --build build --config Release --target test-all
STBC_SVE_VL=2048 cmake --build build --config Release --target test-all
```

The wrapper accepts exactly `128`, `256`, `512`, `1024`, or `2048`. If the variable is unset, the wrapper leaves the vector length to QEMU's default. Set it explicitly when comparing correctness across vector lengths.

## Rust and Go examples

These examples use their own build tools. Follow the corresponding course module for the full exercise and interpretation of results.

### Rust

The [Rust example](../../module-12-rust-squared-distance/rust/) requires a nightly toolchain with the `portable_simd` and, on AArch64, `stdarch_aarch64_sve` features used by its source. Install a suitable nightly toolchain with `rustup toolchain install nightly`.

Starting from the repository root:

```sh
cd module-12-rust-squared-distance/rust
cargo +nightly build --release
cargo +nightly test
cargo +nightly run --release -- scalar data_large.tsv
cargo +nightly run --release -- simd data_large.tsv
```

On an AArch64 system with SVE, also run:

```sh
cargo +nightly run --release -- sve data_large.tsv
```

The program checks for SVE at runtime. It reads two columns of floating-point input and reports their accumulated squared difference and computation time. The input files `data.tsv` and `data_large.tsv` are kept beside `Cargo.toml`.

### Go

The [Go example](../../module-13-go-vectorization/golang/) requires Go 1.24 or later. Its Linux Arm64 cgo path also needs a GCC- or Clang-compatible C++ compiler with SVE intrinsics support. Runtime checks select SVE or SVE2 kernels when supported, with Go fallbacks otherwise.

Starting from the repository root:

```sh
cd module-13-go-vectorization/golang
mkdir -p build
go build -o build/golang-sve .
./build/golang-sve
go test ./...
go test -bench=. -benchmem ./...
```

To test the Go fallback with cgo disabled:

```sh
CGO_ENABLED=0 go test ./...
```

Keep subsequent Cargo or Go commands in their respective task directories. Use native hardware with the relevant features for performance measurements.

Return to [Find your module](../../README.md#find-your-module).
