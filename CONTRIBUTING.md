<!--
  SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
  SPDX-License-Identifier: BSD-3-Clause-Clear
-->

# Contributing

Keep changes aligned with the [SVE course](https://learn.arm.com/courses/sve/) and the [repository conventions](AGENTS.md). For a fix, describe the affected module, the problem, and how you checked the result. Use the [build guide](docs/building.md) to configure, test, and benchmark the C++ tasks.

## Repository layout

Course tasks live inside `module-<number>-<topic>/` directories matching the course module numbers, using two digits such as `module-01-integer-vector-operations/`. The task directory name remains the CMake task name, so it must be unique across the repository. Use [course_support/example/](course_support/example/) as the template for new C++ tasks.

Shared build helpers, headers, dependency configuration, runners, the task template, and maintenance tools live under [course_support/](course_support/). Keep the complete repository together when building a task. Learner instructions belong in the course; shared repository guidance belongs in [docs/](docs/).

## Adding a C++ task

1. Create a task directory such as `my_task_name` inside the appropriate module directory. Copy the source files and `CMakeLists.txt` from [course_support/example/](course_support/example/) as a starting point.
2. Update its implementation, tests, and benchmarks, and document the exercise in the course.
3. Register the task in the top-level [CMakeLists.txt](CMakeLists.txt), before the shared test and benchmark runners.
4. Update [Find your module](README.md#find-your-module) if adding a module.
5. Update any course lesson paths, source links, and commands affected by the change.

An individual task has this layout:

```text
module-NN-topic/
└── my_task_name/
    ├── CMakeLists.txt
    ├── *.cpp
    ├── *.h
    ├── main.cpp
    ├── benchmark.cpp
    └── test.cpp
```

Register its source directory and a task-named binary directory:

```cmake
add_subdirectory(module-NN-topic/my_task_name my_task_name)
```

The second argument keeps generated files under `build/my_task_name/`, including the executable at `build/my_task_name/Release/my_task_name` with Ninja Multi-Config.

### Task CMake API

Call `stbc_add_task()` from the task's `CMakeLists.txt` without a task-name argument. The helper derives the name from the task directory. List the implementation sources and headers in `LIB_SOURCES`:

```cmake
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

`ASM_SOURCES`, `TEST_SOURCES`, `BENCHMARK_SOURCES`, and `DEFINES` are optional. Tests default to `test.cpp`, and benchmarks default to `benchmark.cpp`. On GCC and Clang toolchains, `.S` assembly files pass through the C preprocessor, allowing directives such as `#include` and `#if`; `.s` files are normally assembled directly.

The helper creates these targets for `my_task_name`:

| Target | Purpose |
|---|---|
| `libmy_task_name` | Library containing the task implementation |
| `my_task_name` | Example executable built from `main.cpp` and linked to the library |
| `test-my_task_name` | Run the task's tests through the shared test executable |
| `benchmark-my_task_name` | Run the task's benchmarks through the shared benchmark executable |

The tests and benchmarks also participate in `test-all` and `benchmark-all`.

Keep task-local headers private. When splitting tests or benchmarks into several files, use local or relative includes so task headers do not collide. Do not add task include directories globally.

### Test and benchmark names

The per-task targets select tests and benchmarks by name. Start GoogleTest suite names with the task name:

```cpp
TEST(my_task_name, HandlesEmptyInput) {
  // Check the task's behavior.
}
```

Start benchmark names with the task name followed by an underscore:

```cpp
static void my_task_name_reference(benchmark::State &state) {
  // Exercise the task's reference implementation.
}

BENCHMARK(my_task_name_reference);
```

Use ACLE feature macros to guard code requiring SVE, SVE2, or later features. Where a required feature is unavailable, provide a suitable fallback or a useful diagnostic.

## Checking changes

Format C/C++ code with the repository's [.clang-format](.clang-format), and follow the naming and other rules in [.clang-tidy](.clang-tidy). Do not edit generated build files.

Run the relevant tests and, for changes affecting performance, measure a Release build on suitable native hardware. The [build guide](docs/building.md) includes per-task targets and checks across QEMU vector lengths.

When changing intrinsic usage, regenerate the statistics from the repository root:

```sh
python3 course_support/tools/generate_intrinsics_statistics.py
python3 course_support/tools/generate_intrinsics_statistics.py --check
```

Keep the [intrinsics overview](docs/intrinsics.md) and course instructions synchronized with the implementation. Rust and Go tasks use their own [build and test commands](docs/building.md#rust-and-go-examples).

Code is provided under the [Clear BSD License](LICENSE.txt). Preserve the existing license notices and include the appropriate notices in new files.
