#!/usr/bin/env python3
#
# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
# SPDX-License-Identifier: BSD-3-Clause-Clear

"""Update the profiling summary table in VSS/README.md.

The script reads the Google Benchmark output embedded in the README profiling
section, extracts the reference and SVE Metric/s counters, computes speedups,
and replaces the Markdown table below "The results can be summarized...".
"""

from __future__ import annotations

import re
import sys
from pathlib import Path


BENCHMARK_RE = re.compile(
    r"^VSS_(?P<metric>.+)_(?P<implementation>ref|sve)"
    r"/(?P<size>\d+)_(?P<element_type>\w+)"
    r".*Metric/s=(?P<throughput>[0-9.]+)(?P<suffix>[kMGT]?)/s$"
)

TABLE_HEADER = (
    "| Metric              | Embedding size | Embedding type | "
    "Base throughput | SVE throughput | SVE speedup |"
)
TABLE_SEPARATOR = (
    "|:-------------------:|:--------------:|:--------------:|"
    ":---------------:|:--------------:|:-----------:|"
)

METRIC_NAMES = {
    "euclidean_distance": "Euclidean distance",
    "dot_product": "Dot product",
    "cosine_similarity": "Cosine similarity",
}
METRIC_ORDER = ["euclidean_distance", "dot_product", "cosine_similarity"]
ELEMENT_TYPE_ORDER = ["fp32", "fp16", "int8"]
THROUGHPUT_MULTIPLIERS = {
    "": 1.0,
    "k": 1_000.0,
    "M": 1_000_000.0,
    "G": 1_000_000_000.0,
    "T": 1_000_000_000_000.0,
}


def read_benchmarks(text: str) -> dict[tuple[str, int, str], dict[str, tuple[str, float]]]:
    benchmarks: dict[tuple[str, int, str], dict[str, tuple[str, float]]] = {}

    for line in text.splitlines():
        match = BENCHMARK_RE.match(line.strip())
        if match is None:
            continue

        metric = match.group("metric")
        implementation = match.group("implementation")
        size = int(match.group("size"))
        element_type = match.group("element_type")
        throughput = match.group("throughput")
        suffix = match.group("suffix")
        throughput_text = f"{throughput}{suffix}/s"
        throughput_value = float(throughput) * THROUGHPUT_MULTIPLIERS[suffix]

        key = (metric, size, element_type)
        benchmarks.setdefault(key, {})[implementation] = (
            throughput_text,
            throughput_value,
        )

    return benchmarks


def make_table(benchmarks: dict[tuple[str, int, str], dict[str, tuple[str, float]]]) -> list[str]:
    rows = [TABLE_HEADER, TABLE_SEPARATOR]

    keys = [
        key
        for key, implementations in benchmarks.items()
        if "ref" in implementations and "sve" in implementations
    ]
    keys.sort(
        key=lambda key: (
            METRIC_ORDER.index(key[0]),
            key[1],
            ELEMENT_TYPE_ORDER.index(key[2]),
        )
    )

    for metric, size, element_type in keys:
        ref_text, ref_value = benchmarks[(metric, size, element_type)]["ref"]
        sve_text, sve_value = benchmarks[(metric, size, element_type)]["sve"]
        speedup = sve_value / ref_value
        rows.append(
            f"| {METRIC_NAMES[metric]:<19} | {size:^14} | {element_type:^14} | "
            f"{ref_text:^15} | {sve_text:^14} | {speedup:>10.1f}x |"
        )

    if len(rows) == 2:
        raise RuntimeError("No matching ref/SVE benchmark pairs found")

    return rows


def replace_summary_table(text: str, table: list[str]) -> str:
    lines = text.splitlines()

    try:
        start = lines.index(TABLE_HEADER)
    except ValueError as exc:
        raise RuntimeError("Could not find the profiling summary table") from exc

    end = start
    while end < len(lines) and lines[end].startswith("|"):
        end += 1

    return "\n".join(lines[:start] + table + lines[end:]) + "\n"


def main() -> int:
    readme = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).with_name("README.md")
    text = readme.read_text(encoding="utf-8")
    benchmarks = read_benchmarks(text)
    updated = replace_summary_table(text, make_table(benchmarks))
    readme.write_text(updated, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
