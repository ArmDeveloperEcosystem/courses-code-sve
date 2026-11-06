<!--
  SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
  SPDX-License-Identifier: BSD-3-Clause-Clear
-->

# JSON Structural-Character Indexing

This task dives into JSON structural indexing.

JSON structural-character indexing scans JSON text to identify and record the positions of structural tokens such as `{`, `}`, `[`, `]`, `:`, and `,`, enabling fast parsing, navigation, and query execution over semi-structured data.

Learners will learn how to turn compare results into useful positional information by generating a match predicate with instructions such as `CMPEQ` and then converting that predicate into indices with `BRK`- and `CNT`-style operations. In the context of JSON structural-character indexing, this teaches the key step between detecting interesting characters and producing positions a parser can consume.

## Why does it matter ?

JSON is at the heart of many systems:
- Analytics systems
- APIs
- Databases
- Observability stacks
- Browsers/tooling indirectly

## Caveats

This task is a structural-character indexer, not a validating JSON parser: it tracks string state and escaped-quote/backslash parity for structural indexing, but it does not validate UTF-8, escape-sequence legality, numbers, literals, nesting, or the complete JSON grammar.

- Real JSON data is UTF-8 encoded. For the sake of conciseness, simplification and learning, we consider only 8bit characters in this task.
- A JSON file can be of arbitrary length. A real world JSON parser would process it in blocks, maybe with some parallelization involved, with some sort of state passed between the blocks processed. Again, for the sake of conciseness, simplification and learning, in this task we process a single buffer.

## Implementation notes

JSON structural character indexing is so important to the industry that when the SVE instruction evolved to SVE2 and then SVE2p2, some instructions have been added to better solve the indexing problem. The code for this task shows how to benefit from the later versions of SVE when they are available. One can see in the benchmarking section that using the new instructions in SVE2 for example allow to recover some loss of performance in some specific corner case. At the time of writing, although SVE2p2 do not have an available silicon implementation, an example implementation has been provided that should further improve the performances.

The indexer is structured as an object (`SCI`), defined in `JSON.h`, to keep some state. The `SCI` object exposes various `get_*` methods to run the indexing, either with a reference implementation of using SVE, SVE2 or SVE2p2.

### Reference implementation

In the implementation file, some generally useful routines are provided, like `is_structural` to determine if a character is structuring or not:

```C++
// Return whether a byte is one of the JSON structural characters that should be
// reported when seen outside a string literal.
INLINE constexpr bool is_structural(char c) noexcept {
    return c == '{' || c == '[' || c == '}' || c == ']' || c == ',' || c == ':';
}
```

As well as the `is_escaped_quote` routine used to find the closing `"` character of a string despite escaping characters:

```C++
// Return whether a quote is escaped by an odd-length run of immediately
// preceding backslashes.
INLINE bool is_escaped_quote(string_view json, IndiceTy i) noexcept {
    IndiceTy backslash_count = 0;
    while (backslash_count < i && json[i - backslash_count - 1] == '\\')
        ++backslash_count;
    return (backslash_count & 1) != 0;
}
```

With those 2 routines, the reference implementation of the indexer is now straightforward: each character in the input JSON will be processed by `process_byte` in `process_bytes`:

```C++
// Run the scalar JSON structural-character state machine for one byte, updating
// the output indices and the current string-literal state.
INLINE void process_byte(IndicesTy &result, IndiceTy i, string_view json,
                         bool &in_string) noexcept {
    const auto c = json[i];
    if (!in_string) {
        if (is_structural(c)) {
            result.push_back(i);
            return;
        }
        if (c == '"') {
            in_string = true;
            return;
        }
    }

    // We are inside a string, so check for escaped quotes
    if (c == '"' && !is_escaped_quote(json, i)) {
        in_string = false;
        return;
    }
}

// Run the scalar state machine over a half-open byte range.
INLINE void process_bytes(IndicesTy &result, IndiceTy begin, IndiceTy end,
                          string_view json, bool &in_string) noexcept {
    for (auto i = begin; i < end; ++i)
        process_byte(result, i, json, in_string);
}
```

The top-level `SCI::get_ref` just resets the state of the `SCI`object and then use the above scalar implementation before returning a copy of the result:

```C++
// Reference implementation: scan every byte with the scalar state machine.
IndicesTy SCI::get_ref() noexcept {
    reset();
    process_bytes(result, 0, json.size(), json, in_string);
    return result;
}
```

### Vectorized implementations notes

The SVE, SVE2 and SVE2p2 implementations have similar high level structures:
1. Using SVE has some costs in the setup which starts to pay off only for JSON files that are big enough. For small JSON files (think of a file with less characters than a vector line), using the scalar implementation might actually be the fastest way to parse that file ! All SVE implementations use some sort of threshold to use the fastest implementations.
2. They then process the part of the JSON file that fully fits in a vector: this is the `full_active` predicate, which is always true, meaning all lanes (that is all bytes) have to be processed.
3. For the remainder of the JSON file, the `active` predicate is built to select the remaining bytes to process.

### SVE

Function `sve_scan_structural_chars` is the vectorized counter part of `is_structural` in the reference implementation. It will return a predicate where all active lanes correspond to lanes with a structural characters. It compares all lanes against the structural characters, then `OR` the results using a balanced tree instead of a chain. This in effect builds a predicate of all lanes that contain structural characters.

```C++
// Build a predicate for structural bytes using plain SVE compares.
static INLINE svbool_t sve_scan_structural_chars(svbool_t active,
                                                 svuint8_t chars) noexcept {
    // Reduce the compares' results by ORing in a balanced tree (depth 3)
    // rather than a linear chain (depth 6). This will cost a few more live
    // predicates, but gives more scheduling freedom to the compiler or the
    // processor.
    const auto open = svorr_b_z(
        active, svcmpeq_n_u8(active, chars, static_cast<uint8_t>('{')),
        svcmpeq_n_u8(active, chars, static_cast<uint8_t>('[')));
    const auto close = svorr_b_z(
        active, svcmpeq_n_u8(active, chars, static_cast<uint8_t>('}')),
        svcmpeq_n_u8(active, chars, static_cast<uint8_t>(']')));
    const auto separators = svorr_b_z(
        active, svcmpeq_n_u8(active, chars, static_cast<uint8_t>(',')),
        svcmpeq_n_u8(active, chars, static_cast<uint8_t>(':')));
    return svorr_b_z(active, svorr_b_z(active, open, close), separators);
}
```

This predicate can then be consumed by `sve_scan_interesting_lanes`, that uses the `svcntp*` / `svbrk*` intrinsics to fast forward move to the next interesting position:

```C++
// Walk the interesting-byte predicate in lane order using BRK/CNTP operations,
// invoking the scalar transition only at selected byte positions.
static INLINE void
sve_scan_interesting_lanes(IndicesTy &result, svbool_t active,
                           svbool_t interesting, IndiceTy offset,
                           string_view json, bool &in_string) noexcept {
    IndiceTy lane_offset = 0;

    while (svptest_any(active, interesting)) {
        // BRKB gives the active prefix before the next interesting byte; CNTP
        // turns that prefix into the byte offset of the next scalar transition.
        const auto before = svbrkb_b_z(active, interesting);
        lane_offset += svcntp_b8(before, before);

        process_interesting_byte(result, offset + lane_offset, json, in_string);
        ++lane_offset;

        // Consume the prefix through the byte we just handled, so the next
        // iteration searches only the remaining suffix of this vector.
        const auto consumed = svbrka_b_z(active, interesting);
        interesting = svbic_b_z(active, interesting, consumed);
        active = svbic_b_z(active, active, consumed);
    }
}
```

The above 2 functions are then assembled in `sve_process_chunk`, which process the active part of a vector:

```C++
// Process one plain-SVE vector chunk whose active lanes are supplied by the
// caller.
static INLINE void sve_process_chunk(IndicesTy &result, svbool_t active,
                                     const uint8_t *ptr, const uint8_t *data,
                                     string_view json,
                                     bool &in_string) noexcept {
    const auto chars = svld1_u8(active, ptr);
    const auto quote = svcmpeq_n_u8(active, chars, static_cast<uint8_t>('"'));

    // Structural characters inside strings do not emit indices. If this
    // string chunk has no quote, it cannot change parser state either.
    if (in_string && !svptest_any(active, quote))
        return;

    const auto structural = sve_scan_structural_chars(active, chars);
    const auto interesting = svorr_b_z(active, structural, quote);
    if (!svptest_any(active, interesting))
        return;

    const auto chunk_begin = static_cast<IndiceTy>(ptr - data);
    sve_scan_interesting_lanes(result, active, interesting, chunk_begin, json,
                               in_string);
}
```

`sve_process_chunk` is eventually called in the top-level `get_sve`, either using the `full_active` predicate (for vectors that are fully loaded with the JSON data), or with predicate `active` to process the remaining last bytes.

### SVE2

The SVE2 implementation is very similar in spirit to the SVE implementation, but it makes use of the `svmatch*` instruction which was added to SVE2 in order to simplify the search of multiple elements (the structural characters in our case). This saves the search from all the `svcmp*` and their reduction. Note that the so-called `needles` used by `svmatch` are built by the compiler at compile time using the C++ `constexpr` feature, and that as it's built at compile time, the actual vector size is not known, thus needles is built by the compiler for the largest vector length allowed by the SVE specification:

```C++
// Build the repeated structural-character needle table used by MATCH. The table
// is intentionally full-vector-sized enough for any SVE vector length.
static constexpr array<uint8_t, 256> make_structural_chars() noexcept {
    constexpr auto chars =
        std::to_array<uint8_t>({'{', '[', '}', ']', ',', ':'});
    array<uint8_t, 256> needles{};
    for (size_t i = 0; i < 256; ++i)
        needles[i] = chars[i % chars.size()];
    return needles;
}
static const auto structural_chars = make_structural_chars();
```

### SVE2p2

The SVE2p2 implementation is similar to the SVE2 one, but it uses the newly added `svcompact` intrinsic, which allow to extract exactly the interesting byte offsets instead of walking the predicate with `svbrk*` instructions.

## Configure and build

Refer to the instructions in the top level `README.md` for the platform requirements for configuring and building this course.

Ensure the JSON_sci executable, tests and benchmarks are up to date with:

```bash
cmake --build build --config Release --target JSON_sci
```

## Using JSON_sci

You can experiment with `JSON_sci`. It will either get its input from the standard input or from a file given as its first argument:

```sh
$ printf '{"a":[1,2]}' | build/JSON_sci/JSON_sci
Input JSON:
{"a":[1,2]}

Structural characters found at indices:
0 4 5 7 9 10

$ build/JSON_sci/Release/JSON_sci file.json
Input JSON:
{"a":[1,2]}

Structural characters found at indices:
0 4 5 7 9 10
```

## Testing

```sh
$ cmake --build build --config Release --target test-JSON_sci
ninja: Entering directory `build'
[1/1] cd /home/ubuntu/SVE-task-based-course.git/build/test/Release && /home/ubuntu/SVE-task-based-course.git/build/test/Release/test --gtest_color=yes --gtest_filter="JSON_sci*"
Note: Google Test filter = JSON_sci*
[==========] Running 15 tests from 3 test suites.
[----------] Global test environment set-up.
[----------] 5 tests from JSON_sci/reference
[ RUN      ] JSON_sci/reference.matches_expected_indices/empty
[       OK ] JSON_sci/reference.matches_expected_indices/empty (0 ms)
[ RUN      ] JSON_sci/reference.matches_expected_indices/all_structural
[       OK ] JSON_sci/reference.matches_expected_indices/all_structural (0 ms)
[ RUN      ] JSON_sci/reference.matches_expected_indices/with_strings
[       OK ] JSON_sci/reference.matches_expected_indices/with_strings (0 ms)
[ RUN      ] JSON_sci/reference.matches_expected_indices/with_strings_esc
[       OK ] JSON_sci/reference.matches_expected_indices/with_strings_esc (0 ms)
[ RUN      ] JSON_sci/reference.matches_expected_indices/with_backslash_parity
[       OK ] JSON_sci/reference.matches_expected_indices/with_backslash_parity (0 ms)
[----------] 5 tests from JSON_sci/reference (0 ms total)

[----------] 5 tests from JSON_sci/sve
[ RUN      ] JSON_sci/sve.matches_expected_indices/empty
[       OK ] JSON_sci/sve.matches_expected_indices/empty (0 ms)
[ RUN      ] JSON_sci/sve.matches_expected_indices/all_structural
[       OK ] JSON_sci/sve.matches_expected_indices/all_structural (0 ms)
[ RUN      ] JSON_sci/sve.matches_expected_indices/with_strings
[       OK ] JSON_sci/sve.matches_expected_indices/with_strings (0 ms)
[ RUN      ] JSON_sci/sve.matches_expected_indices/with_strings_esc
[       OK ] JSON_sci/sve.matches_expected_indices/with_strings_esc (0 ms)
[ RUN      ] JSON_sci/sve.matches_expected_indices/with_backslash_parity
[       OK ] JSON_sci/sve.matches_expected_indices/with_backslash_parity (0 ms)
[----------] 5 tests from JSON_sci/sve (0 ms total)

[----------] 5 tests from JSON_sci/sve2
[ RUN      ] JSON_sci/sve2.matches_expected_indices/empty
[       OK ] JSON_sci/sve2.matches_expected_indices/empty (0 ms)
[ RUN      ] JSON_sci/sve2.matches_expected_indices/all_structural
[       OK ] JSON_sci/sve2.matches_expected_indices/all_structural (0 ms)
[ RUN      ] JSON_sci/sve2.matches_expected_indices/with_strings
[       OK ] JSON_sci/sve2.matches_expected_indices/with_strings (0 ms)
[ RUN      ] JSON_sci/sve2.matches_expected_indices/with_strings_esc
[       OK ] JSON_sci/sve2.matches_expected_indices/with_strings_esc (0 ms)
[ RUN      ] JSON_sci/sve2.matches_expected_indices/with_backslash_parity
[       OK ] JSON_sci/sve2.matches_expected_indices/with_backslash_parity (0 ms)
[----------] 5 tests from JSON_sci/sve2 (0 ms total)

[----------] Global test environment tear-down
[==========] 15 tests from 3 test suites ran. (0 ms total)
[  PASSED  ] 15 tests.
```

## Benchmarking

The time it takes to parse the structure of a JSON file highly depends on the structure of the JSON file. Some files will be small, others (very) large. Some will have very long key names, some others very short key names and so on. Our benchmarking thus needs to exercise all the different corner cases, or at the very least those that matter to us. In this task, a couple cases have been selected:
- `empty`: an empty JSON file,
- `tiny`: an extremely small JSON file,
- `small`: a slightly bigger JSON file,
- `wide_fields`: a JSON file with very wide fields (long strings to parse),
- `compact_kb`: a JSON files with compact fields and no whitespace at all,
- `whitespace_strings_arrays_kb`: a JSON file with long white spaces,
- `json_sample`: a real world JSON sample.

Of course, in real life, a much larger set of cases would be used to exercise all corners.

```sh
$ cmake --build build --config Release --target benchmark-JSON_sci
...
Running /home/ubuntu/SVE-task-based-course.git/build/benchmark/Release/benchmark
Run on (4 X 2000 MHz CPU s)
CPU Caches:
  L1 Data 64 KiB (x4)
  L1 Instruction 64 KiB (x4)
  L2 Unified 2048 KiB (x4)
  L3 Unified 36864 KiB (x1)
Load Average: 0.02, 0.08, 0.04
---------------------------------------------------------------------------------------------------------
Benchmark                                               Time             CPU   Iterations UserCounters...
---------------------------------------------------------------------------------------------------------
JSON_sci_get/empty                                   5.71 ns         5.71 ns    122549839 bytes_per_second=0/s
JSON_sci_get/tiny                                    29.7 ns         29.7 ns     23811442 bytes_per_second=416.928Mi/s
JSON_sci_get/small                                   44.6 ns         44.6 ns     15742662 bytes_per_second=513.7Mi/s
JSON_sci_get/wide_fields                              153 ns          153 ns      4564121 bytes_per_second=1.27508Gi/s
JSON_sci_get/compact_kb                              5360 ns         5359 ns       130602 bytes_per_second=972.108Mi/s
JSON_sci_get/whitespace_strings_arrays_kb            5849 ns         5848 ns       118492 bytes_per_second=1.07107Gi/s
JSON_sci_get/json_sample                            17190 ns        17191 ns        39681 bytes_per_second=1018.26Mi/s
JSON_sci_get_sve/empty                               5.01 ns         5.01 ns    139857432 bytes_per_second=0/s
JSON_sci_get_sve/tiny                                30.0 ns         30.0 ns     22815708 bytes_per_second=413.681Mi/s
JSON_sci_get_sve/small                               43.6 ns         43.6 ns     16058394 bytes_per_second=525.162Mi/s
JSON_sci_get_sve/wide_fields                         82.4 ns         82.4 ns      8498113 bytes_per_second=2.3733Gi/s
JSON_sci_get_sve/compact_kb                          5716 ns         5716 ns       122098 bytes_per_second=911.39Mi/s
JSON_sci_get_sve/whitespace_strings_arrays_kb        2384 ns         2384 ns       293642 bytes_per_second=2.62757Gi/s
JSON_sci_get_sve/json_sample                         5594 ns         5594 ns       125107 bytes_per_second=3.05587Gi/s
JSON_sci_get_sve2/empty                              4.50 ns         4.50 ns    155398600 bytes_per_second=0/s
JSON_sci_get_sve2/tiny                               27.7 ns         27.7 ns     25293411 bytes_per_second=447.976Mi/s
JSON_sci_get_sve2/small                              40.9 ns         40.9 ns     17137132 bytes_per_second=560.229Mi/s
JSON_sci_get_sve2/wide_fields                        69.4 ns         69.4 ns     10211316 bytes_per_second=2.81994Gi/s
JSON_sci_get_sve2/compact_kb                         4835 ns         4835 ns       144804 bytes_per_second=1.05238Gi/s
JSON_sci_get_sve2/whitespace_strings_arrays_kb       1682 ns         1682 ns       416319 bytes_per_second=3.72502Gi/s
JSON_sci_get_sve2/json_sample                        3669 ns         3669 ns       190752 bytes_per_second=4.65959Gi/s
```

The results can be summarized in the following table:

| Benchmark                    | Reference throughput | SVE throughput | SVE speedup | SVE2 throughput | SVE2 speedup |
|:----------------------------:|:--------------------:|:--------------:|:-----------:|:---------------:|:------------:|
| tiny	                       |         416.93       |      413.68    |     0.99x   |       447.98    |     1.07x    |
| small	                       |         513.70       |      525.16    |     1.02x   |       560.23    |     1.09x    |
| wide_fields	                 |        1275.08       |     2373.30    |     1.86x   |      2819.94    |     2.21x    |
| compact_kb	                 |         972.11       |      911.39    |     0.94x   |      1052.38    |     1.08x    |
| whitespace_strings_arrays_kb |        1071.07       |     2627.57    |     2.45x   |      3725.02    |     3.48x    |
| json_sample	                 |        1018.26       |     3055.87    |     3.00x   |      4659.59    |     4.58x    |
| Geomean                      |           -          |        -       |     1.53x   |         -       |     1.88x    |

## Further optimizations

Further optimizations --- not SVE related --- can be applied to the structural character indexer:
- the hot path has a `vector.push_back` which introduces unnecessary branching to check and extend the vector capacity if needed. The number of structural characters is not known a-priori, but it can not exceed the json buffer size. One could pre-allocate a temporary buffer, then copy/move the data to the result vector at the end of the indexing. A quick experiment with `get_sve2()` show sensible improvement on `whitespace_strings_arrays_kb`(~4.24618Gi/s) and `json_sample` (~5.3816Gi/s) for example.
- right now each chunk tests:
  - are we in a string?
  - did we find quotes?
  - did we find structural chars?
  - should we take structural-only fast path?
  - should we enter the interesting-byte walker?
  Those branches are locally sensible, but together they produce a branchy hot loop. A cleaner state-machine layout would have an outside-string loop and an inside-string loop. Outside-string chunks can use the cheap structural path until a quote appears; inside-string chunks can mostly scan for quotes. That should reduce repeated per-vector decision logic. A quick experiment with `get_sve2()`, on top of the previous optimization shows clear performance improvement on `whitespace_strings_arrays_kb`(~4.38167Gi/s) and `json_sample` (~5.59887Gi/s) for example.
