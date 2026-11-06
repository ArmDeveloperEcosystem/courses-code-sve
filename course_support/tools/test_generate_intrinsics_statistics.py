# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
# SPDX-License-Identifier: BSD-3-Clause-Clear

"""Regression checks for statistics discovery after reorganizing course tasks."""

import tempfile
import unittest
from pathlib import Path

import generate_intrinsics_statistics as statistics


class TaskDiscoveryTests(unittest.TestCase):
    def setUp(self):
        self.workspace = tempfile.TemporaryDirectory()
        self.addCleanup(self.workspace.cleanup)
        self.root = Path(self.workspace.name)

    def write(self, relative_path, text):
        path = self.root / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    def test_discovers_module_tasks_and_support_template_without_build_caches(self):
        self.write(
            "course_support/example/CMakeLists.txt",
            "stbc_add_task(LIB_SOURCES example.cpp)",
        )
        self.write(
            "module-01-vectors/vector_examples/CMakeLists.txt",
            "stbc_add_task(LIB_SOURCES vector_examples.cpp)",
        )
        self.write("module-12-rust/rust/Cargo.toml", '[package]\nname = "rust"')
        self.write("module-13-go/golang/go.mod", "module example.com/golang")
        self.write("module-01-vectors/CMakeLists.txt", "add_subdirectory(vector_examples)")
        self.write("module-not-a-directory", "")
        self.write("build/_deps/copy/CMakeLists.txt", "stbc_add_task(LIB_SOURCES copy.cpp)")
        self.write(
            "module-01-vectors/vector_examples/build/Cargo.toml",
            '[package]\nname = "generated"',
        )
        self.write(
            "course_support/external/CMakeLists.txt",
            "stbc_add_task(LIB_SOURCES dependency.cpp)",
        )
        self.write(
            "course_support/example/build/Cargo.toml", '[package]\nname = "generated"'
        )
        self.write("course_support/tools/go.mod", "module example.com/tooling")
        self.write("unregistered/CMakeLists.txt", "stbc_add_task(LIB_SOURCES other.cpp)")

        discovered = {
            path.relative_to(self.root).as_posix() for path in statistics.task_dirs(self.root)
        }

        self.assertEqual(
            discovered,
            {
                "course_support/example",
                "module-01-vectors/vector_examples",
                "module-12-rust/rust",
                "module-13-go/golang",
            },
        )

    def test_readme_removal_and_module_rename_preserve_statistics(self):
        self.write(
            "module-1-vectors/vectors/CMakeLists.txt",
            "stbc_add_task(LIB_SOURCES vectors.cpp)",
        )
        self.write("module-1-vectors/vectors/README.md", "# Integer vector operations\n")
        self.write(
            "module-1-vectors/vectors/vectors.cpp", "svcntw(); svadd_u32_x(pg, x, y);"
        )
        self.write("module-2-rust/rust/Cargo.toml", '[package]\nname = "rust"')
        self.write("module-2-rust/rust/README.md", "# Using SVE from Rust\n")
        self.write("module-2-rust/rust/src/main.rs", "svcntd(); svld1_f32(pg, input);")
        self.write("module-3-golang/golang/go.mod", "module example.com/golang")
        self.write("module-3-golang/golang/README.md", "# Using SVE from Go\n")
        self.write(
            "module-3-golang/golang/vector_kernels.cpp",
            'svcntb(); svadd_s32_x(pg, x, y); // svdup_u32(0)\n'
            'const char *text = "svst1_u32(pg, output, value)"; sve_helper();',
        )
        before = statistics.build_statistics(self.root)

        for number, name in enumerate(("vectors", "rust", "golang"), start=1):
            module_dir = self.root / f"module-{number}-{name}"
            (module_dir / name / "README.md").unlink()
            module_dir.rename(self.root / f"module-{number:02d}-{name}")

        after = statistics.build_statistics(self.root)

        self.assertEqual(before["tasks"], after["tasks"])
        self.assertEqual(before["intrinsics"], after["intrinsics"])
        self.assertEqual(set(after["tasks"]), {"vectors", "rust", "golang"})
        self.assertEqual(
            {name: task["display_name"] for name, task in after["tasks"].items()},
            {"vectors": "vectors", "rust": "rust", "golang": "golang"},
        )
        self.assertEqual(
            after["tasks"]["golang"]["intrinsics"], {"svadd": 1, "svcnt": 1}
        )
        self.assertEqual(
            after["intrinsics"]["svcnt"],
            {"total_count": 3, "tasks": {"golang": 1, "rust": 1, "vectors": 1}},
        )


if __name__ == "__main__":
    unittest.main()
