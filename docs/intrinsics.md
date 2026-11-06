<!--
  SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
  SPDX-License-Identifier: BSD-3-Clause-Clear
-->

# Intrinsics by task

This overview groups the SVE intrinsics used in the course by their base mnemonic; type and predication suffixes are omitted. Open each task for the implementation and its architecture requirements. The Rust task also demonstrates `std::simd`.

| Task | Intrinsics demonstrated |
|---|---|
| [1: Integer vector operations](../module-1-integer-vector-operations/vector_examples/README.md) | `svadd`, `svcnt`, `svld1`, `svmaxv`, `svminv`, `svst1`, `svwhilelt` |
| [2: Run-length decoding](../module-2-run-length-decoding/run_length_decode/README.md) | `svcnt`, `svdup`, `svst1`, `svwhilelt` |
| [3: C-string length](../module-3-c-string-length/strlen/README.md) | `svbrkb`, `svcmpeq`, `svcnt`, `svcntp`, `svld1`, `svldff1`, `svptest`, `svptrue`, `svrdffr`, `svsetffr`, `svwhilele` |
| [4: Multi-value search with SVE2 MATCH](../module-4-multi-value-search/sve_match/README.md) | `svcnt`, `svld1`, `svmatch`, `svptest`, `svptrue` |
| [5: Sorted set intersection](../module-5-set-intersection/set_intersection/README.md) | `svbrkb`, `svcmpeq`, `svcnt`, `svcntp`, `svld1`, `svptest`, `svwhilelt` |
| [6: Find and process matching values in order](../module-6-match-and-visit/match_and_visit/README.md) | `svcmplt`, `svcnt`, `svcntp`, `svcompact`, `svlastb`, `svld1`, `svpfalse`, `svpnext`, `svptest`, `svst1`, `svwhilelt` |
| [7: Histogram updates with SVE and SVE2](../module-7-histograms/histogram/README.md) | `svadd`, `svcmpeq`, `svcnt`, `svcntp`, `svhistcnt`, `svindex`, `svld1`, `svld1ub`, `svld1uh`, `svptrue`, `svst1`, `svwhilelt` |
| [8: JSON structural-character indexing](../module-8-json-structural-indexing/JSON_sci/README.md) | `svbic`, `svbrka`, `svbrkb`, `svcmpeq`, `svcnt`, `svcntp`, `svcompact`, `svdup`, `svindex`, `svld1`, `svmatch`, `svorr`, `svptest`, `svptrue`, `svst1`, `svwhilelt` |
| [9: Bloom filter](../module-9-bloom-filter/bloom_filter/README.md) | `svadd`, `svand`, `svcmpeq`, `svcnt`, `svdup`, `sveor`, `svindex`, `svld1`, `svlsl`, `svlsr`, `svmla`, `svmul`, `svorr`, `svptest`, `svst1`, `svst1w`, `svwhilelt` |
| [10: Vector-similarity metrics](../module-10-vector-similarity/VSS/README.md) | `svadd`, `svaddv`, `svcnt`, `svcvt`, `svcvtlt`, `svdot`, `svdup`, `svld1`, `svmla`, `svmlalb`, `svmlalt`, `svptrue`, `svsel`, `svsub`, `svsublb`, `svsublt`, `svtrn2`, `svunpkhi`, `svunpklo`, `svwhilelt` |
| [11: Base64 encode and decode](../module-11-base64/base64/README.md) | `svadd`, `svand`, `svcmpeq`, `svcmpge`, `svcmpgt`, `svcmplt`, `svcnt`, `svdup`, `svindex`, `svld1`, `svlsl`, `svlsr`, `svmul`, `svorr`, `svptrue`, `svreinterpret`, `svsel`, `svst1`, `svsub`, `svtbl`, `svwhilelt` |
| [12: Squared-distance calculations in Rust](../module-12-rust-squared-distance/rust/README.md) | `svaddv`, `svcnt`, `svdup`, `svld1`, `svmla`, `svptrue`, `svsub`, `svwhilelt` + `std::simd` library |
| [13: Go SVE kernels](../module-13-go-vectorization/golang/README.md) | `svadd`, `svand`, `svcmpeq`, `svcmpge`, `svcmplt`, `svcnt`, `svdup`, `svhistcnt`, `svindex`, `svld1`, `svlsl`, `svlsr`, `svmul`, `svorr`, `svptrue`, `svreinterpret`, `svsel`, `svst1`, `svsub`, `svtbl`, `svwhilelt` |

The [statistics generator](../tools/generate_intrinsics_statistics.py) counts intrinsic calls and writes [intrinsics_statistics.json](../tools/intrinsics_statistics.json). Run it from the repository root:

```sh
python3 tools/generate_intrinsics_statistics.py
```

Use `--check` to verify that the JSON matches the current sources without rewriting it.

Return to [Find your module](../README.md#find-your-module).
