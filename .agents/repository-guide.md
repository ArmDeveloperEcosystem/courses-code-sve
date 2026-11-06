<!--
  SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
  SPDX-License-Identifier: BSD-3-Clause-Clear
-->

# Repository guide

Paths and commands in this guide are relative to the repository root.

## Repository Conventions

- This is a CMake-based SVE task course repository.
- The course uses multi-configuration ninja.
- Most tasks are C++ based, part from a couple examples in Rust and Go.
- Configure with ` cmake -G "Ninja Multi-Config" -S course_support -B build`.
- Build in `Release` mode with `cmake --build build --config Release`, in `Debug` mode with `cmake --build build --config Debug`.
- Run all tests with `cmake --build build --config Release --target test-all` - or `cmake --build build --config Debug --target test-all`.
- Build benchmarks with `cmake --build build --config Release --target benchmark`. For meaningful results, the benchmarks have to be run in `Release` mode.

## Task Structure

- Course tasks live in `module-<number>-<topic>/<task_name>/`, matching the course module numbers with two digits (for example, `module-01-integer-vector-operations/`).
- C++ tasks are registered from `course_support/CMakeLists.txt` with `add_subdirectory(../module-<number>-<topic>/<task_name> <task_name>)` to preserve build output paths. Rust and Go tasks use their own build tools.
- Shared build infrastructure and maintenance tools live under `course_support/`. The `course_support/example/` directory is the template for new C++ tasks.
- A task name is derived from its directory name.
- Task `CMakeLists.txt` files should call `stbc_add_task(` without passing the task name.
- Keep task-local headers private; do not expose task include directories globally.
- If tests or benchmarks are split into multiple files, keep includes local/relative so task headers do not collide across tasks.

## Editing Guidance

- Prefer following the `course_support/example/` task layout when adding new tasks.
- Do not edit generated files under `build/`.
- Keep shared documentation in `course_support/docs/` synchronized with the CMake helper API. Learner instructions belong in the course rather than nested module READMEs.
- C/C++ code is formatted with `clang-format` according to the rules from `course_support/config/.clang-format`; pass `--style=file:course_support/config/.clang-format` explicitly.
- Naming and other conventions for C/C++ code come from `course_support/config/.clang-tidy`; pass `--config-file=course_support/config/.clang-tidy` explicitly.
