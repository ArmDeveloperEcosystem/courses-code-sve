<!--
  SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
  SPDX-License-Identifier: BSD-3-Clause-Clear
-->

# AGENTS.md

## Repository Conventions

- This is a CMake-based SVE task course repository.
- The course uses multi-configuration ninja.
- Most tasks are C++ based, part from a couple examples in Rust and Go.
- Configure with ` cmake -G "Ninja Multi-Config" -S . -B build`.
- Build in `Release` mode with `cmake --build build --config Release`, in `Debug` mode with `cmake --build build --config Debug`.
- Run all tests with `cmake --build build --config Release --target test-all` - or `cmake --build build --config Debug --target test-all`.
- Build benchmarks with `cmake --build build --config Release --target benchmark`. For meaningful results, the benchmarks have to be run in `Release` mode.

## Task Structure

- Course tasks live in `module-<number>-<topic>/<task_name>/`, matching the course module numbers.
- C++ tasks are registered from the top-level `CMakeLists.txt` with `add_subdirectory(module-<number>-<topic>/<task_name> <task_name>)` to preserve build output paths. Rust and Go tasks use their own build tools.
- The root-level `example/` directory is the template for new C++ tasks.
- A task name is derived from its directory name.
- Task `CMakeLists.txt` files should call `stbc_add_task(` without passing the task name.
- Keep task-local headers private; do not expose task include directories globally.
- If tests or benchmarks are split into multiple files, keep includes local/relative so task headers do not collide across tasks.

## Editing Guidance

- Prefer following the `example/` task layout when adding new tasks.
- Do not edit generated files under `build/`.
- Keep README examples synchronized with the actual CMake helper API.
- C/C++ code is formatted with `clang-format` according to the rules from the `.clang-format` file.
- Naming and other conventions for C/C++ code come from the `.clang-tidy` rules file, which can be used by `clang-tidy`.
