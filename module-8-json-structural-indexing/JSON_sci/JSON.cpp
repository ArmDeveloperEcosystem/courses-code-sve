// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "JSON.h"
#include "SVE-task-based-course.h"

#include <array>
#include <cstddef>
#include <cstdint>

#ifdef __aarch64__
#include <arm_sve.h>
#else
#warning Platform is not AArch64: no SVE-accelerated implementations will be available !
#endif

using std::size_t;
using std::uint64_t;
using std::uint8_t;

using std::array;
using std::string;
using std::string_view;
using std::vector;

using IndiceTy = JSON::SCI::IndiceTy;
using IndicesTy = JSON::SCI::IndicesTy;

namespace {

// Return whether a byte is one of the JSON structural characters that should be
// reported when seen outside a string literal.
INLINE constexpr bool is_structural(char c) noexcept {
    return c == '{' || c == '[' || c == '}' || c == ']' || c == ',' || c == ':';
}

// Return whether a quote is escaped by an odd-length run of immediately
// preceding backslashes.
INLINE bool is_escaped_quote(string_view json, IndiceTy i) noexcept {
    IndiceTy backslash_count = 0;
    while (backslash_count < i && json[i - backslash_count - 1] == '\\')
        ++backslash_count;
    return (backslash_count & 1) != 0;
}

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

#if defined(__ARM_FEATURE_SVE) || defined(__ARM_FEATURE_SVE2p2)
// Process a byte that is already known to be interesting: either a quote or a
// structural character. Other byte classes are intentionally not handled here.
INLINE void process_interesting_byte(IndicesTy &result, IndiceTy i,
                                     string_view json,
                                     bool &in_string) noexcept {
    const auto c = json[i];
    if (c == '"') {
        if (!in_string) {
            in_string = true;
            return;
        }
        if (!is_escaped_quote(json, i))
            in_string = false;
        return;
    }

    if (!in_string)
        result.push_back(i);
}
#endif

} // namespace

namespace JSON {

// Reference implementation: scan every byte with the scalar state machine.
IndicesTy SCI::get_ref() noexcept {
    reset();
    process_bytes(result, 0, json.size(), json, in_string);
    return result;
}

#ifdef __ARM_FEATURE_SVE
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

// Walk the interesting-byte predicate in lane order using BRK/CNTP operations,
// invoking the scalar transition only at selected byte positions.
static INLINE void sve_scan_interesting_lanes(IndicesTy &result,
                                              svbool_t active,
                                              svbool_t interesting,
                                              IndiceTy offset, string_view json,
                                              bool &in_string) noexcept {
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

// Plain SVE implementation: find quotes first, skip quote-free string chunks,
// then process only selected quote or structural byte positions.
IndicesTy SCI::get_sve() noexcept {
    reset();
    const auto *data = reinterpret_cast<const uint8_t *>(json.data());
    const size_t VL = svcntb();
    const size_t scalar_threshold = VL;

    // Plain SVE setup is still too expensive for a single vector on the current
    // benchmark inputs; beyond that, the quote-first vector scan pays off.
    if (json.size() <= scalar_threshold) {
        process_bytes(result, 0, json.size(), json, in_string);
        return result;
    }

    const auto *const end = data + json.size();
    const auto *ptr = data;
    const auto *const full_end = end - (json.size() % VL);

    // Full vectors do not need per-iteration tail predicates.
    const auto full_active = svptrue_b8();
    while (ptr < full_end) {
        sve_process_chunk(result, full_active, ptr, data, json, in_string);
        ptr += VL;
    }

    if (ptr < end) {
        const auto active = svwhilelt_b8(reinterpret_cast<uint64_t>(ptr),
                                         reinterpret_cast<uint64_t>(end));
        sve_process_chunk(result, active, ptr, data, json, in_string);
    }

    return result;
}
#else
#warning SVE support not detected: JSON_sci::get_sve() will not be available !
#endif // __ARM_FEATURE_SVE

#ifdef __ARM_FEATURE_SVE2
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

// Emit structural byte positions directly when quotes cannot affect state.
static INLINE void sve2_emit_structural_lanes(IndicesTy &result,
                                              svbool_t active,
                                              svbool_t structural,
                                              IndiceTy offset) noexcept {
    IndiceTy lane_offset = 0;

    while (svptest_any(active, structural)) {
        const auto before = svbrkb_b_z(active, structural);
        lane_offset += svcntp_b8(before, before);

        result.push_back(offset + lane_offset);
        ++lane_offset;

        const auto consumed = svbrka_b_z(active, structural);
        structural = svbic_b_z(active, structural, consumed);
        active = svbic_b_z(active, active, consumed);
    }
}

// Process one SVE2 vector chunk whose active lanes are supplied by the caller.
static INLINE void sve2_process_chunk(IndicesTy &result, svbool_t active,
                                      const uint8_t *ptr, const uint8_t *data,
                                      string_view json, bool &in_string,
                                      svuint8_t structural_needles,
                                      svuint8_t quote_needles) noexcept {
    const auto chars = svld1_u8(active, ptr);
    const auto quote = svmatch_u8(active, chars, quote_needles);

    // Structural characters inside strings do not emit indices. If this
    // string chunk has no quote, it cannot change parser state either.
    if (in_string && !svptest_any(active, quote))
        return;

    const auto structural = svmatch_u8(active, chars, structural_needles);
    const auto interesting = svorr_b_z(active, structural, quote);
    if (!svptest_any(active, interesting))
        return;

    // Outside strings, a quote-free chunk cannot change string state, so
    // structural bytes can be emitted directly without the scalar transition.
    if (!in_string && !svptest_any(active, quote)) {
        const auto chunk_begin = static_cast<IndiceTy>(ptr - data);
        sve2_emit_structural_lanes(result, active, structural, chunk_begin);
        return;
    }

    const auto chunk_begin = static_cast<IndiceTy>(ptr - data);
    sve_scan_interesting_lanes(result, active, interesting, chunk_begin, json,
                               in_string);
}

// SVE2 implementation: use MATCH to find quotes and structural bytes, skip
// quote-free string chunks, and process selected bytes with the BRK walker.
IndicesTy SCI::get_sve2() noexcept {
    reset();
    if (json.empty())
        return result;

    const size_t VL = svcntb();
    const size_t scalar_threshold = 4 * VL;

    // MATCH setup and event extraction need a few vectors to amortize; below
    // this empirically chosen cutoff, scalar scanning is cheaper and simpler.
    if (json.size() <= scalar_threshold) {
        process_bytes(result, 0, json.size(), json, in_string);
        return result;
    }

    const auto structural_needles =
        svld1_u8(svptrue_b8(), structural_chars.data());
    const auto quote_needles = svdup_n_u8(static_cast<uint8_t>('"'));
    const auto *data = reinterpret_cast<const uint8_t *>(json.data());
    const auto *const end = data + json.size();

    // Byte 0 has no previous byte for escape checking. Handle it once, then all
    // vector-selected scalar transitions can safely inspect json[i - 1].
    process_byte(result, 0, json, in_string);

    auto *ptr = data + 1;
    const auto *const full_end = end - VL;

    // Full vectors do not need per-iteration tail predicates or active counts.
    const auto full_active = svptrue_b8();
    while (ptr <= full_end) {
        sve2_process_chunk(result, full_active, ptr, data, json, in_string,
                           structural_needles, quote_needles);
        ptr += VL;
    }

    if (ptr < end) {
        const auto active = svwhilelt_b8(reinterpret_cast<uint64_t>(ptr),
                                         reinterpret_cast<uint64_t>(end));
        sve2_process_chunk(result, active, ptr, data, json, in_string,
                           structural_needles, quote_needles);
    }

    return result;
}
#else
#warning SVE2 support not detected: JSON_sci::get_sve2() will not be available !
#endif // __ARM_FEATURE_SVE2

#ifdef __ARM_FEATURE_SVE2p2
// Extract selected byte positions with SVE2p2 byte compact, then process those
// positions in scalar lane order.
static INLINE void
sve2p2_scan_interesting_lanes(IndicesTy &result, svbool_t active,
                              svbool_t interesting, IndiceTy offset,
                              string_view json, bool &in_string) noexcept {
    uint8_t candidate_lanes[256];
    const auto candidate_count = svcntp_b8(active, interesting);

    // SVE2p2 adds byte-lane compact, so extract exactly the interesting byte
    // offsets instead of walking the predicate with BRK instructions.
    const auto packed_lanes = svcompact_u8(interesting, svindex_u8(0, 1));
    const auto packed_active = svwhilelt_b8(uint64_t{0}, candidate_count);
    svst1_u8(packed_active, candidate_lanes, packed_lanes);

    for (uint64_t i = 0; i < candidate_count; ++i)
        process_interesting_byte(result, offset + candidate_lanes[i], json,
                                 in_string);
}

// Emit selected structural lanes directly with SVE2p2 byte compact.
static INLINE void sve2p2_emit_structural_lanes(IndicesTy &result,
                                                svbool_t active,
                                                svbool_t structural,
                                                IndiceTy offset) noexcept {
    uint8_t candidate_lanes[256];
    const auto candidate_count = svcntp_b8(active, structural);
    const auto packed_lanes = svcompact_u8(structural, svindex_u8(0, 1));
    const auto packed_active = svwhilelt_b8(uint64_t{0}, candidate_count);
    svst1_u8(packed_active, candidate_lanes, packed_lanes);

    for (uint64_t i = 0; i < candidate_count; ++i)
        result.push_back(offset + candidate_lanes[i]);
}

// Process one SVE2p2 vector chunk, using byte compact to extract selected
// lanes.
static INLINE void sve2p2_process_chunk(IndicesTy &result, svbool_t active,
                                        const uint8_t *ptr, const uint8_t *data,
                                        string_view json, bool &in_string,
                                        svuint8_t structural_needles,
                                        svuint8_t quote_needles) noexcept {
    const auto chars = svld1_u8(active, ptr);
    const auto quote = svmatch_u8(active, chars, quote_needles);

    // Same string fast path as get_sve2. Until a quote is encountered,
    // bytes inside a string cannot emit structural indices.
    if (in_string && !svptest_any(active, quote))
        return;

    const auto structural = svmatch_u8(active, chars, structural_needles);
    const auto interesting = svorr_b_z(active, structural, quote);
    if (!svptest_any(active, interesting))
        return;

    // Outside strings, quote-free chunks can emit structural positions
    // directly.
    if (!in_string && !svptest_any(active, quote)) {
        const auto chunk_begin = static_cast<IndiceTy>(ptr - data);
        sve2p2_emit_structural_lanes(result, active, structural, chunk_begin);
        return;
    }

    const auto chunk_begin = static_cast<IndiceTy>(ptr - data);
    sve2p2_scan_interesting_lanes(result, active, interesting, chunk_begin,
                                  json, in_string);
}

// SVE2p2 implementation: follow the SVE2 MATCH-based scanner but use byte
// compact for selected-lane extraction when the architecture provides it.
IndicesTy SCI::get_sve2p2() noexcept {
    reset();
    if (json.empty())
        return result;

    const size_t VL = svcntb();
    const size_t scalar_threshold = 4 * VL;

    // Keep the cutoff aligned with get_sve2(): SVE2p2 changes selected-lane
    // extraction, but not the setup cost of the MATCH-based scanner.
    if (json.size() <= scalar_threshold) {
        process_bytes(result, 0, json.size(), json, in_string);
        return result;
    }

    const auto structural_needles =
        svld1_u8(svptrue_b8(), structural_chars.data());
    const auto quote_needles = svdup_n_u8(static_cast<uint8_t>('"'));
    const auto *data = reinterpret_cast<const uint8_t *>(json.data());
    const auto *const end = data + json.size();

    // See get_sve2(): starting at byte 1 keeps escaped-quote checks local and
    // avoids special casing json[i - 1] in the vector-selected scalar handler.
    process_byte(result, 0, json, in_string);

    auto *ptr = data + 1;
    const auto *const full_end = end - VL;

    // Full vectors do not need per-iteration tail predicates or active counts.
    const auto full_active = svptrue_b8();
    while (ptr <= full_end) {
        sve2p2_process_chunk(result, full_active, ptr, data, json, in_string,
                             structural_needles, quote_needles);
        ptr += VL;
    }

    if (ptr < end) {
        const auto active = svwhilelt_b8(reinterpret_cast<uint64_t>(ptr),
                                         reinterpret_cast<uint64_t>(end));
        sve2p2_process_chunk(result, active, ptr, data, json, in_string,
                             structural_needles, quote_needles);
    }

    return result;
}
#else
#warning SVE2.2 support not detected: JSON_sci::get_sve2p2() will not be available !
#endif // __ARM_FEATURE_SVE2p2

} // namespace JSON
