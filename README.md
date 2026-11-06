<!--
  SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
  SPDX-License-Identifier: BSD-3-Clause-Clear
-->

# SVE Course

[![License: BSD-3-Clause-Clear](https://img.shields.io/badge/License-BSD--3--Clause--Clear-blue.svg)](LICENSE.txt)
[![Last build status on Ubuntu 24.04](https://github.com/arm-education/SVE-task-based-course/actions/workflows/ubuntu-24.04.yml/badge.svg)](https://github.com/arm-education/SVE-task-based-course/actions/workflows/ubuntu-24.04.yml)
[![Last build status on Ubuntu 26.04](https://github.com/arm-education/SVE-task-based-course/actions/workflows/ubuntu-26.04.yml/badge.svg)](https://github.com/arm-education/SVE-task-based-course/actions/workflows/ubuntu-26.04.yml)

Development repository for a task-based course on SVE.

## Table of Contents

- [Overview](#overview)
- [Tasks](#tasks)
- [Building](#building)
- [Testing](#testing)
- [Benchmarking](#benchmarking)
- [Adding a task to the repository](#adding-a-task-to-the-repository)

## Overview

This repository contains example code, benchmarks, tests, reference implementations and supporting material used to develop hands-on SVE learning content.

The course focuses on helping developers understand:

- Compiler auto-vectorization
- Vector-length agnostic programming
- SVE intrinsics
- Performance analysis and benchmarking
- Real-world vectorization patterns

## Tasks

The tasks available in this repository are:
- [Evaluate explicit SVE implementations for simple integer vector operations](vector_examples/README.md)
- [Run-Length Decoding](run_length_decode/README.md)
- [Calculate C-string length with SVE](strlen/README.md)
- [Vectorize a multi-value search with SVE2 MATCH](sve_match/README.md)
- [Set intersection](set_intersection/README.md)
- [Match and visit programming pattern](match_and_visit/README.md)
- [Histogram](histogram/README.md)
- [JSON structural-character indexing](JSON_sci/README.md)
- [Bloom filter](bloom_filter/README.md)
- [Vector similarity search](VSS/README.md)
- [Base64 Encode and Decode](base64/README.md)
- [Enable Arm SVE vectorization in Go](golang/README.md)
- [Using SVE intrinsics from Rust](rust/README.md)

| Task | Intrinsics demonstrated |
|---|---|
| Evaluate explicit SVE implementations for simple integer vector operations | `svadd`, `svcnt`, `svld1`, `svmaxv`, `svminv`, `svst1`, `svwhilelt` |
| Run-Length Decoding | `svcnt`, `svdup`, `svst1`, `svwhilelt` |
| Calculate C-string length with SVE | `svbrkb`, `svcmpeq`, `svcnt`, `svcntp`, `svld1`, `svldff1`, `svptest`, `svptrue`, `svrdffr`, `svsetffr`, `svwhilele` |
| Vectorize a multi-value search with SVE2 MATCH | `svcnt`, `svld1`, `svmatch`, `svptest`, `svptrue` |
| Set intersection | `svbrkb`, `svcmpeq`, `svcnt`, `svcntp`, `svld1`, `svptest`, `svwhilelt` |
| Match and visit programming pattern | `svcmplt`, `svcnt`, `svcntp`, `svcompact`, `svlastb`, `svld1`, `svpfalse`, `svpnext`, `svptest`, `svst1`, `svwhilelt` |
| Histogram | `svadd`, `svcmpeq`, `svcnt`, `svcntp`, `svhistcnt`, `svindex`, `svld1`, `svld1ub`, `svld1uh`, `svptrue`, `svst1`, `svwhilelt` |
| JSON structural-character indexing | `svbic`, `svbrka`, `svbrkb`, `svcmpeq`, `svcnt`, `svcntp`, `svcompact`, `svdup`, `svindex`, `svld1`, `svmatch`, `svorr`, `svptest`, `svptrue`, `svst1`, `svwhilelt` |
| Bloom filter | `svadd`, `svand`, `svcmpeq`, `svcnt`, `svdup`, `sveor`, `svindex`, `svld1`, `svlsl`, `svlsr`, `svmla`, `svmul`, `svorr`, `svptest`, `svst1`, `svst1w`, `svwhilelt` |
| Vector similarity search | `svadd`, `svaddv`, `svcnt`, `svcvt`, `svcvtlt`, `svdot`, `svdup`, `svld1`, `svmla`, `svmlalb`, `svmlalt`, `svptrue`, `svsel`, `svsub`, `svsublb`, `svsublt`, `svtrn2`, `svunpkhi`, `svunpklo`, `svwhilelt` |
| Base64 Encode and Decode | `svadd`, `svand`, `svcmpeq`, `svcmpge`, `svcmpgt`, `svcmplt`, `svcnt`, `svdup`, `svindex`, `svld1`, `svlsl`, `svlsr`, `svmul`, `svorr`, `svptrue`, `svreinterpret`, `svsel`, `svst1`, `svsub`, `svtbl`, `svwhilelt` |
| Enable Arm SVE vectorization in Go (Base64 Encode and Decode) | `svadd`, `svand`, `svcmpeq`, `svcmpge`, `svcmpgt`, `svcmplt`, `svcnt`, `svdup`, `svindex`, `svld1`, `svlsl`, `svlsr`, `svmul`, `svorr`, `svptrue`, `svreinterpret`, `svsel`, `svst1`, `svsub`, `svtbl`, `svwhilelt` |
| Using SVE intrinsics from Rust | `svaddv`, `svcnt`, `svdup`, `svld1`, `svmla`, `svptrue`, `svsub`, `svwhilelt` + `std::simd` library|

## License

This repository is distributed under the [Clear BSD License](LICENSE.txt).

## Software requirements

This code base has the following requirements:
- CMake version 3.20 or later. On Ubuntu/Debian, `cmake` can be installed with `sudo apt update && sudo apt install -y cmake`
- The `ninja` build tool. On Ubuntu/Debian, `ninja` can be installed with `sudo apt update && sudo apt install -y ninja-build`
- A C++ compiler with C++20 and SVE support (e.g. `clang-18` / `gcc-13` or later are fine). Ubuntu 24.04 or later works.

## Third party software

This repository uses third party software that is not included in this source code repository. The configure and build system, `CMake`, will automatically download, configure and build this third party software. As each of those third party software comes with its own specific license, you have to check their licenses comply with your own specific requirements.

The third party software used by this software is:
- [GoogleTest](https://github.com/google/googletest),
  licensed under https://github.com/google/googletest/blob/main/LICENSE,
  is the unit testing framework used by this repository.
- [Google Benchmark](https://github.com/google/benchmark),
  licensed under https://github.com/google/benchmark/blob/main/LICENSE,
  is the benchmarking framework used by this repository.

## Platform assumptions

This codebase explicitly targets `aarch64` platforms. The hardware architecture / feature independent, like testing and / or reference implementations could work on other platforms, but this is not tested.

By default, `cmake` will target your native CPU, using `-mcpu=native` or `-march=native` when building the task library, executable, test and benchmark. It will enable all warnings with `-Wall` for compilation. Other compilation flags are those set by default by `cmake` for the `Debug` or `Release` builds.

If your development platform does not have SVE hardware, you can still build and run the SVE code using the cross-compilation option described in the [Cross-compiling for SVE](#cross-compiling-for-sve) section below.

## Configuring and building

The recommended way to build the repository is to use `Ninja` with `CMake` multi-configuration feature, which allows to have both `Debug` and `Release` builds without having to re-configure.

The configuration of the builds is done with:

```bash
cmake -G "Ninja Multi-Config" -S . -B build
```

The meaning of the command line options for configuring the build is:
- `-G` selects a generator for the build tool. `-G "Ninja Multi-Config"` selects the multi-configuration `ninja` build tool, but many other generators are available depending on the platofrm. Run `cmake --help` to get the list of supported generators on your platform (e.g. `Xcode`, ...).
- `-S .` tells `cmake` to where the repository can be found, the current directory in this case (`.`)
- `-B build` tells `cmake` which directory to use for building the project. Here we are using `build/`. This directory only contains files that can be rebuild at any time and can this be safely removed. It is ignored by `git`.

The configuration step might take some time as it will download, build and install locally `googletest` and `Benchmark`:

```txt
...
[16/16] Completed 'benchmark'
-- Install configuration: ""
-- Performing Test CMAKE_HAVE_LIBC_PTHREAD
-- Performing Test CMAKE_HAVE_LIBC_PTHREAD - Success
-- Found Threads: TRUE
-- Performing Test STBC_HAS__mcpu_native
-- Performing Test STBC_HAS__mcpu_native - Success
-- Performing Test STBC_PLATFORM_HAS_SVE
-- Performing Test STBC_PLATFORM_HAS_SVE - Success
-- Performing Test STBC_PLATFORM_HAS_SVE2
-- Performing Test STBC_PLATFORM_HAS_SVE2 - Failed
-- Performing Test STBC_PLATFORM_HAS_SVE2P1
-- Performing Test STBC_PLATFORM_HAS_SVE2P1 - Failed
-- Performing Test STBC_PLATFORM_HAS_SVE2P2
-- Performing Test STBC_PLATFORM_HAS_SVE2P2 - Failed
-- Using native CPU compile flag: -mcpu=native
-- Configuring done (29.3s)
-- Generating done (0.0s)
-- Build files have been written to: .../build
```

Note that `cmake` checks for SVE and will issue a warning if it is not available. It will not refuse to configure the build though and only the reference implementations, tests and benchmarks will be built.

Once configured, building and running all tests and benchmarks is done with:

```bash
cmake --build build --config Release
cmake --build build --config Release --target test-all
cmake --build build --config Release --target benchmark-all
```

Note the `--config Release` which allows to select the  `Release` configuration to exercise. Use `Debug` for debug builds.

In order to clean the `Release` build:

```bash
cmake --build build --config Release --target clean
````

### Tweaking notes

We will certainly discover over our developments that some specific versions of the compilers have to be installed. You can experiment with different compilers with cmake in the following way:

```sh
CC=/usr/bin/clang-22 CXX=/usr/bin/clang++-22 cmake -G "Ninja Multi-Config" -S . -B build
```

### Cross-compiling for SVE

If your development machine is `aarch64` but does **not** have SVE hardware, you can still build and run everything using QEMU user-mode emulation. If `qemu-user` is not already installed on your system, run:

```sh
sudo apt update
sudo apt install qemu-user
```

Enable the `STBC_SVE_CROSS_COMPILE` option:

```sh
cmake -G "Ninja Multi-Config" -S . -B build -DSTBC_SVE_CROSS_COMPILE=ON
```

This changes the build in two ways:
1. Uses `-march=armv8-a+sve+sve2` instead of `-mcpu=native`, so the compiler generates SVE/SVE2 instructions regardless of the host CPU.
2. All `test-*` and `benchmark-*` targets are run under `qemu-aarch64`, which emulates SVE at runtime.


CMake will auto-detect `qemu-aarch64` from your `PATH`. If it's installed in a non-standard location, specify it explicitly:

```sh
cmake -G "Ninja Multi-Config" -S . -B build -DSTBC_SVE_CROSS_COMPILE=ON -DSTBC_QEMU_EXECUTABLE=/path/to/qemu-aarch64
```

Once configured, building and running tests/benchmarks works exactly the same as on native SVE hardware:

```sh
cmake --build build
cmake --build build --config Release --target test-all
cmake --build build --config Release --target benchmark-all
```

Benchmarking results under QEMU will not reflect real hardware performance. Use QEMU for correctness testing, not performance analysis.

#### Changing the SVE vector length

When running under QEMU, you can control the SVE vector length at runtime using the `STBC_SVE_VL` environment variable. The value is in bits and must be a power of two between 128 and 2048:

```sh
STBC_SVE_VL=128  cmake --build build --config Release -t test-all
STBC_SVE_VL=256  cmake --build build --config Release -t test-all
STBC_SVE_VL=512  cmake --build build --config Release -t test-all
STBC_SVE_VL=1024 cmake --build build --config Release -t test-all
STBC_SVE_VL=2048 cmake --build build --config Release -t test-all
```

If `STBC_SVE_VL` is not set, QEMU uses its default vector length (512 bits). This is useful for testing that your code is truly vector-length agnostic.


## Testing

Run the unit tests for all tasks with:

```sh
cmake --build build --config Release -t test-all
```

The unit tests for a specific task can be run with:

```sh
cmake --build build --config Release -t test-${task_name}
```

## Benchmarking

Run the benchmarking for all tasks with:

```sh
cmake --build build --config Release -t benchmark-all
```

A specific task can be benchmarked with:

```sh
cmake --build build --config Release -t benchmark-${task_name}
```

## Adding a task to the repository

Tasks are directories at the root of this repository.

An individual task, say `task_name`, has the following layout:

```tree
task_name
├── README.md
├── CMakeLists.txt
├── *.cpp
├── *.h
├── main.cpp
├── benchmark.cpp
└── test.cpp
```

The headers (`*.h`) and source files (`*.cpp`) will be built into a `lib${task_name}` library, they contain the functions / classes that are the topic of this specific task based course. `main.cpp` will be linked with `lib${task_name}` so users have a `task_name` executable they can play with. The library will be used for linking the tests and benchmarks. The `README.md` file contains all info you want to communicate to the reader, especially regarding the code --- this might end up in the courses text.

Each task is *registered* to `cmake` with one line (`add_subdirectory(task_name)`) towards the end of the top-level `CMakeLists.txt`:

```txt
...
# ===================================================================
# Add task based examples here.
# -------------------------------------------------------------------
add_subdirectory(example)
add_subdirectory(task_name)
...
```

To get started, the easiest is to:
- copy and rename the `example/` directory to your own task name, say `my_task_name`,
- add a `add_subdirectory(my_task_name)` line to the top-level `CMakeLists.txt`,
- edit `my_task_name/CMakeLists.txt` to add your task's source files,
- add your task to the [Tasks](#tasks) section above.

The `stbc_add_task` function has a required `LIB_SOURCES` argument, where you should enumerate your task's specific source and header files that will end up in your task library. It accepts an optional `ASM_SOURCES` argument for assembly sources that will be built into the task library. It also accept optional `TEST_SOURCES` and `BENCHMARK_SOURCES` arguments to specify the source files that contains your task's tests (resp. benchmarks) in case the default `test.cpp` (resp. `benchmark.cpp`) does not suit your needs or you prefer / need to split them to many files. Task-local compile definitions can be passed with the optional `DEFINES` argument. Note that for most Unix/Linux toolchains (`gcc`, `clang`, ...), `.s` files are fed directly to the assembler, whereas `.S` files are first passed thru the C preprocessor, then assembled, which allows to use `#include`, `#define`, `#if`, `#ifdef`, etc...

```txt
stbc_add_task(
  LIB_SOURCES
    foo.cpp
    foo.h
  ASM_SOURCES
    foo_kernel.S
  TEST_SOURCES
    tests.cpp
    other_tests.cpp
  BENCHMARK_SOURCES
    benchmark1.cpp
    benchmark2.cpp
  DEFINES
    TASK_DEFINE=1
)
```

This will add the following targets to the build system:
- `libmy_task_name`: the library containing the task's functions / classes,
- `my_task_name`: an executable that can be invoked by the user to explore / play with the task,
- `test-my_task_name`: a target that allows to run this task's specific unit tests,
- `benchmark-my_task_name`: a target that allows to run this task's specific benchmarks.

Your tests (resp. benchmarks) will be run as part of the `test-all`(resp. `benchmark-all`) targets.

For your tests, make sure the first argument to the `TEST` macro corresponds to your task name, e.g. :

```txt
TEST(task_name, ...
```

As this is what allows selecting only your tests to run.

There is a similar requirements for the benchmarks, ensure they start with your own task name:

```txt
void task_name_XYZ(benchmark::State &state) {
  ...
}

} // namespace

BENCHMARK(task_name_XYZ);
```

Make you best effort to use the ACLE features macro to test the presence of a specific feature and fail with a useful error message if that SVE feature is required --- or provide a fallback with a warning. The diversity of hardware this codebase will be used on means we will be exposed to such cases.
