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

    def test_discovers_module_tasks_and_root_template_without_build_caches(self):
        self.write("example/CMakeLists.txt", "stbc_add_task(LIB_SOURCES example.cpp)")
        self.write(
            "module-1-vectors/vector_examples/CMakeLists.txt",
            "stbc_add_task(LIB_SOURCES vector_examples.cpp)",
        )
        self.write("module-12-rust/rust/Cargo.toml", '[package]\nname = "rust"')
        self.write("module-13-go/golang/go.mod", "module example.com/golang")
        self.write("module-1-vectors/CMakeLists.txt", "add_subdirectory(vector_examples)")
        self.write("module-not-a-directory", "")
        self.write("build/_deps/copy/CMakeLists.txt", "stbc_add_task(LIB_SOURCES copy.cpp)")
        self.write(
            "module-1-vectors/vector_examples/build/Cargo.toml",
            '[package]\nname = "generated"',
        )
        self.write("external/helper/Cargo.toml", '[package]\nname = "dependency"')

        discovered = {
            path.relative_to(self.root).as_posix() for path in statistics.task_dirs(self.root)
        }

        self.assertEqual(
            discovered,
            {
                "example",
                "module-1-vectors/vector_examples",
                "module-12-rust/rust",
                "module-13-go/golang",
            },
        )

    def test_relocation_preserves_task_ids_and_intrinsic_counts(self):
        self.write("vectors/CMakeLists.txt", "stbc_add_task(LIB_SOURCES vectors.cpp)")
        self.write("vectors/README.md", "# Integer vector operations\n")
        self.write("vectors/vectors.cpp", "svcntw(); svadd_u32_x(pg, x, y);")
        self.write("rust/Cargo.toml", '[package]\nname = "rust"')
        self.write("rust/src/main.rs", "svcntd(); svld1_f32(pg, input);")
        self.write("golang/go.mod", "module example.com/golang")
        self.write(
            "golang/vector_kernels.cpp",
            'svcntb(); svadd_s32_x(pg, x, y); // svdup_u32(0)\n'
            'const char *text = "svst1_u32(pg, output, value)"; sve_helper();',
        )
        before = statistics.build_statistics(self.root)

        for number, name in enumerate(("vectors", "rust", "golang"), start=1):
            module_dir = self.root / f"module-{number}-{name}"
            module_dir.mkdir()
            (self.root / name).rename(module_dir / name)

        after = statistics.build_statistics(self.root)

        self.assertEqual(before["tasks"], after["tasks"])
        self.assertEqual(before["intrinsics"], after["intrinsics"])
        self.assertEqual(set(after["tasks"]), {"vectors", "rust", "golang"})
        self.assertEqual(
            after["tasks"]["golang"]["intrinsics"], {"svadd": 1, "svcnt": 1}
        )
        self.assertEqual(
            after["intrinsics"]["svcnt"],
            {"total_count": 3, "tasks": {"golang": 1, "rust": 1, "vectors": 1}},
        )


if __name__ == "__main__":
    unittest.main()
