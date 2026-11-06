#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
# SPDX-License-Identifier: BSD-3-Clause-Clear

"""Generate SVE intrinsic statistics for the task-based course."""

from __future__ import annotations

import argparse
import copy
import json
import re
import subprocess
import sys
from collections import Counter, defaultdict
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


SCHEMA_VERSION = 1
GENERATOR_NAME = "tools/generate_intrinsics_statistics.py"
GENERATOR_VERSION = 1
OUTPUT_FILE_NAME = "intrinsics_statistics.json"
SOURCE_SUFFIXES = {".c", ".cpp", ".h", ".rs"}
HELPER_PREFIXES = ("sve_", "sve2_", "sve2p1_", "sve2p2_")
HELPER_NAMES = {"sve", "sve2", "sve2p1", "sve2p2"}
SVCOUNT_INTRINSICS = {"svcntb", "svcnth", "svcntw", "svcntd"}
INTRINSIC_CALL_RE = re.compile(r"\b(sv[a-z0-9_]*)\s*\(")


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def output_file() -> Path:
    return Path(__file__).resolve().with_name(OUTPUT_FILE_NAME)


def git_commit(root: Path) -> str:
    try:
        return subprocess.check_output(
            ["git", "rev-parse", "HEAD"],
            cwd=root,
            stderr=subprocess.DEVNULL,
            text=True,
        ).strip()
    except (OSError, subprocess.CalledProcessError):
        return "unknown"


def strip_comments_and_literals(source: str) -> str:
    result: list[str] = []
    i = 0
    state = "code"

    while i < len(source):
        char = source[i]
        nxt = source[i + 1] if i + 1 < len(source) else ""

        if state == "code":
            if char == "/" and nxt == "/":
                result.extend("  ")
                i += 2
                state = "line_comment"
            elif char == "/" and nxt == "*":
                result.extend("  ")
                i += 2
                state = "block_comment"
            elif char == '"':
                result.append(" ")
                i += 1
                state = "string"
            elif char == "'":
                result.append(" ")
                i += 1
                state = "char"
            else:
                result.append(char)
                i += 1
        elif state == "line_comment":
            if char == "\n":
                result.append("\n")
                state = "code"
            else:
                result.append(" ")
            i += 1
        elif state == "block_comment":
            if char == "*" and nxt == "/":
                result.extend("  ")
                i += 2
                state = "code"
            else:
                result.append("\n" if char == "\n" else " ")
                i += 1
        elif state == "string":
            if char == "\\" and nxt:
                result.extend("  ")
                i += 2
            elif char == '"':
                result.append(" ")
                i += 1
                state = "code"
            else:
                result.append("\n" if char == "\n" else " ")
                i += 1
        elif state == "char":
            if char == "\\" and nxt:
                result.extend("  ")
                i += 2
            elif char == "'":
                result.append(" ")
                i += 1
                state = "code"
            else:
                result.append("\n" if char == "\n" else " ")
                i += 1

    return "".join(result)


def is_task_dir(path: Path) -> bool:
    if (path / "Cargo.toml").is_file():
        return True
    cmake_file = path / "CMakeLists.txt"
    if not path.is_dir() or not cmake_file.is_file():
        return False
    try:
        return "stbc_add_task(" in cmake_file.read_text(encoding="utf-8")
    except OSError:
        return False


def task_dirs(root: Path) -> list[Path]:
    return sorted(path for path in root.iterdir() if is_task_dir(path))


def display_name(task_dir: Path) -> str:
    readme = task_dir / "README.md"
    if readme.is_file():
        for line in readme.read_text(encoding="utf-8").splitlines():
            if line.startswith("# "):
                return line[2:].strip()
    return task_dir.name


def source_files(task_dir: Path) -> list[Path]:
    return sorted(
        path
        for path in (*task_dir.iterdir(), *(task_dir / "src").rglob("*.rs"))
        if path.is_file() and path.suffix in SOURCE_SUFFIXES
    )


def normalize_intrinsic(name: str) -> str:
    if name in SVCOUNT_INTRINSICS:
        return "svcnt"
    if "_" in name:
        return name.split("_", 1)[0]
    return name


def is_project_helper(name: str) -> bool:
    return name in HELPER_NAMES or name.startswith(HELPER_PREFIXES)


def count_intrinsics(files: list[Path]) -> Counter[str]:
    counts: Counter[str] = Counter()
    for path in files:
        source = strip_comments_and_literals(path.read_text(encoding="utf-8"))
        for match in INTRINSIC_CALL_RE.finditer(source):
            intrinsic = match.group(1)
            if is_project_helper(intrinsic):
                continue
            counts[normalize_intrinsic(intrinsic)] += 1
    return counts


def build_statistics(root: Path) -> dict[str, Any]:
    tasks: dict[str, Any] = {}
    aggregate: dict[str, Counter[str]] = defaultdict(Counter)

    for task_dir in task_dirs(root):
        task_name = task_dir.name
        task_counts = count_intrinsics(source_files(task_dir))
        task_intrinsics = dict(sorted(task_counts.items()))
        tasks[task_name] = {
            "display_name": display_name(task_dir),
            "intrinsics": task_intrinsics,
        }

        for intrinsic, count in task_intrinsics.items():
            aggregate[intrinsic][task_name] = count

    intrinsics: dict[str, Any] = {}
    for intrinsic in sorted(aggregate):
        task_counts = dict(sorted(aggregate[intrinsic].items()))
        intrinsics[intrinsic] = {
            "total_count": sum(task_counts.values()),
            "tasks": task_counts,
        }

    return {
        "schema_version": SCHEMA_VERSION,
        "generated": {
            "timestamp_utc": datetime.now(timezone.utc)
            .replace(microsecond=0)
            .isoformat()
            .replace("+00:00", "Z"),
            "git_commit": git_commit(root),
            "generator": {
                "name": GENERATOR_NAME,
                "version": GENERATOR_VERSION,
            },
        },
        "normalization": {
            "description": (
                "Intrinsic names are normalized to the base mnemonic by removing "
                "element type suffixes, predicate policy suffixes, and other "
                "underscore-delimited variants."
            ),
            "examples": {
                "svadd_n_u32_m": "svadd",
                "svld1_gather_u32index_u32": "svld1",
                "svptest_any": "svptest",
                "svwhilelt_b8": "svwhilelt",
            },
            "ignored_suffix_classes": [
                "element_type",
                "predicate_policy",
                "underscore_delimited_variant",
            ],
        },
        "documentation": {
            "base_url": "https://developer.arm.com/architectures/instruction-sets/intrinsics/",
            "lookup_url_template": (
                "https://developer.arm.com/architectures/instruction-sets/intrinsics/"
                "?search={intrinsic}"
            ),
        },
        "tasks": dict(
            sorted(tasks.items(), key=lambda item: (item[1]["display_name"].lower(), item[0]))
        ),
        "intrinsics": intrinsics,
    }


def markdown_table(statistics: dict[str, Any]) -> str:
    rows = ["| Task | Intrinsics demonstrated |", "|---|---|"]
    tasks = sorted(
        statistics["tasks"].items(),
        key=lambda item: (
            len(item[1]["intrinsics"]),
            item[1]["display_name"].lower(),
            item[0],
        ),
    )
    for _, task in tasks:
        intrinsic_names = list(task["intrinsics"])
        if not intrinsic_names:
            continue
        intrinsics = ", ".join(f"`{name}`" for name in intrinsic_names)
        rows.append(f"| {task['display_name']} | {intrinsics} |")
    return "\n".join(rows)


def comparable_statistics(statistics: dict[str, Any]) -> dict[str, Any]:
    comparable = copy.deepcopy(statistics)
    generated = comparable.get("generated", {})
    generated.pop("timestamp_utc", None)
    generated.pop("git_commit", None)
    return comparable


def load_json(path: Path) -> dict[str, Any]:
    with path.open(encoding="utf-8") as file:
        return json.load(file)


def write_json(path: Path, statistics: dict[str, Any]) -> None:
    with path.open("w", encoding="utf-8") as file:
        json.dump(statistics, file, indent=2)
        file.write("\n")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate SVE intrinsic statistics for course tasks."
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="check that the generated statistics are up to date without writing them",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root = repo_root()
    output = output_file()
    statistics = build_statistics(root)

    if args.check:
        if not output.is_file():
            print(f"{output.relative_to(root)} does not exist", file=sys.stderr)
            return 1

        current = load_json(output)
        if comparable_statistics(current) != comparable_statistics(statistics):
            print(f"{output.relative_to(root)} is out of date", file=sys.stderr)
            return 1

        print(f"{output.relative_to(root)} is up to date")
        return 0

    write_json(output, statistics)
    print(markdown_table(statistics))
    return 0


if __name__ == "__main__":
    sys.exit(main())
