<!--
  SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
  SPDX-License-Identifier: BSD-3-Clause-Clear
-->

# Course code for 'Optimize code with Arm Scalable Vector Extension (SVE)'

This repository provides example programs, reference implementations, tests, and benchmarks for [Optimize code with Arm Scalable Vector Extension (SVE)](https://learn.arm.com/courses/sve-code-optimization/). Follow the course for prerequisites, environment setup, and exercise instructions, then use the module links to find the accompanying code.

## Find your module

| Module | Code |
|---|---|
| 1 | [Integer vector operations](module-01-integer-vector-operations/) |
| 2 | [Run-length decoding](module-02-run-length-decoding/) |
| 3 | [C-string length](module-03-c-string-length/) |
| 4 | [Multi-value search with SVE2 MATCH](module-04-multi-value-search/) |
| 5 | [Sorted set intersection](module-05-set-intersection/) |
| 6 | [Find and process matching values in order](module-06-match-and-visit/) |
| 7 | [Histogram updates with SVE and SVE2](module-07-histograms/) |
| 8 | [JSON structural-character indexing](module-08-json-structural-indexing/) |
| 9 | [Bloom filter](module-09-bloom-filter/) |
| 10 | [Vector-similarity metrics](module-10-vector-similarity/) |
| 11 | [Base64 encode and decode](module-11-base64/) |
| 12 | [Squared-distance calculations in Rust](module-12-rust-squared-distance/) |
| 13 | [Arm SVE vectorization in Go](module-13-go-vectorization/) |

Module 0 introduces the course and shared setup; it has no separate code directory.

## Using this repository

- Follow the lesson's environment and command instructions, then open the implementation in the corresponding module directory.
- Keep the complete repository together. C++ modules use the shared build infrastructure in [course_support/](course_support/). Configure and build from the repository root using the [build, test, and benchmark guide](course_support/docs/building.md).
- Build the Rust and Go examples separately using the commands in the [build guide](course_support/docs/building.md#rust-and-go-examples). Their toolchain and runtime requirements differ from the C++ tasks.
- Use the [intrinsics overview](course_support/docs/intrinsics.md) to find examples of particular SVE operations. For performance measurements, use a Release build on native hardware with the required features; the build guide also describes QEMU for correctness checks.

Optional [SVE and SIMD optimization guidance for AI agents](.agents/skills/sve-and-simd-optimization/SKILL.md) supports working with the code.

For fixes and new tasks, see [Contributing](CONTRIBUTING.md). Code is provided under the [Clear BSD License](LICENSE.md).
