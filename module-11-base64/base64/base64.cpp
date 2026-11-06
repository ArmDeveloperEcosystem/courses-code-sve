// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "base64.h"
#include "SVE-task-based-course.h"

#include <array>
#include <cstddef>
#include <cstdint>

#ifdef __aarch64__
#include <arm_sve.h>
#else
#warning Platform is not AArch64: no SVE-accelerated implementations will be available !
#endif

using std::int8_t;
using std::size_t;
using std::uint32_t;
using std::uint64_t;
using std::uint8_t;

using std::array;

namespace {
// 64, printable base64 (6-bit) characters. Input = ASCII char, output = base64
// symbol. encode table is a char (i.e. 1 byte) since we do not have a native
// 6-bit data type
// Use an array to avoid std::string pointer reloads in the scalar loop.
constexpr array<char, 65> encode_table = {
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"};

//     "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
// mapping from index to 64 values in 8-bit (128 value) ASCII table, 0xFF (255)
// is a sentinel value. input = ASCII characters, output = binary value
// corresponding to base64 decoded value
constexpr array<uint8_t, 256> decode_table = {
    // clang-format off
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,  62,0xFF,0xFF,0xFF,  63,
    52,  53,  54,  55,  56,  57,  58,  59,   60,  61,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,   0,   1,   2,   3,   4,   5,   6,    7,   8,   9,  10,  11,  12,  13,  14,
    15,  16,  17,  18,  19,  20,  21,  22,   23,  24,  25,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,  26,  27,  28,  29,  30,  31,  32,   33,  34,  35,  36,  37,  38,  39,  40,
    41,  42,  43,  44,  45,  46,  47,  48,   49,  50,  51,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    // clang-format on
};
} // namespace

namespace base64 {

size_t base64_scalar_encode(const uint8_t *input, size_t len, char *output) {
    size_t i = 0, j = 0;

    // process in chunks of 3
    for (; i + 2 < len; i += 3) {

        // NOTE: Bits 25->32 are UNUSED
        // pack 3x 8-bit values into a 32-bit variable
        uint32_t val = ((uint32_t)input[i] << 16) |
                       ((uint32_t)input[i + 1] << 8) | (uint32_t)input[i + 2];

        // input = regular int index, output = 6-bit base64 character from enode
        output[j++] = encode_table[(val >> 18) & 0x3F]; // 0x3F == 0`b00111111
        output[j++] = encode_table[(val >> 12) & 0x3F];
        output[j++] = encode_table[(val >> 6) & 0x3F];
        output[j++] = encode_table[val & 0x3F];
    }

    // Handle remaining 1 or 2 bytes with padding
    if (i < len) {
        uint32_t val = (uint32_t)input[i] << 16;
        if (i + 1 < len)
            val |= (uint32_t)input[i + 1] << 8;

        output[j++] = encode_table[(val >> 18) & 0x3F];
        output[j++] = encode_table[(val >> 12) & 0x3F];
        // '=' is a special base64 symbol that should not be intepreted as data
        output[j++] = (i + 1 < len) ? encode_table[(val >> 6) & 0x3F] : '=';
        output[j++] = '=';
    }

    return j;
}

size_t base64_scalar_decode(const char *input, size_t len, uint8_t *output) {
    // Strip trailing padding
    while (len > 0 && input[len - 1] == '=')
        len--;

    size_t i = 0, j = 0;

    // Process full 4-character groups
    // input is 8-bit

    for (; i + 3 < len; i += 4) {
        // char on some platforms is signed so casting to uint8_t
        uint32_t val = ((uint32_t)decode_table[(uint8_t)input[i]] << 18) |
                       ((uint32_t)decode_table[(uint8_t)input[i + 1]] << 12) |
                       ((uint32_t)decode_table[(uint8_t)input[i + 2]] << 6) |
                       ((uint32_t)decode_table[(uint8_t)input[i + 3]]);

        // output chunk is 3x 8-bit ASCII characters
        output[j++] = (val >> 16) & 0xFF;
        output[j++] = (val >> 8) & 0xFF;
        output[j++] = val & 0xFF;
    }

    // Handle remaining 2 or 3 characters
    if (i + 1 < len) {
        uint32_t val = ((uint32_t)decode_table[(uint8_t)input[i]] << 18) |
                       ((uint32_t)decode_table[(uint8_t)input[i + 1]] << 12);
        output[j++] = (val >> 16) & 0xFF;

        if (i + 2 < len) {
            val |= (uint32_t)decode_table[(uint8_t)input[i + 2]] << 6;
            output[j++] = (val >> 8) & 0xFF;
        }
    }
    return j;
}

#ifdef __ARM_FEATURE_SVE
// Shift Base64 indices into their ASCII ranges after reducing them to a
// 4-bit LUT index. Since ASCII is mostly contiguous, this can be a
// small LUT of shifts instead of a full 64-byte encode table.
// Examples: 0 -> 'A' uses +65, 26 -> 'a' uses +71,
// 52 -> '0' uses -4, 62 -> '+' uses -19, and 63 -> '/' uses -16.
// Entries 14 and 15 are unused padding so the SVE table load is 16 bytes.
constexpr int8_t kEncodeShiftLut[16] = {71, -4, -4, -4,  -4,  -4, -4, -4,
                                        -4, -4, -4, -19, -16, 65, 0,  0};

// ASCII high nibbles select the common offset for each Base64 range.
// Values 191 and 185 are byte-wrapped forms of -65 and -71.
constexpr array<uint8_t, 16> kDecodeHighNibbleOffsets = {
    0, 0, 0, 4, 191, 191, 185, 185, 0, 0, 0, 0, 0, 0, 0, 0};

// High nibble 2 covers '+' and '/', so the low nibble picks those offsets.
constexpr array<uint8_t, 16> kDecodeLowNibbleSpecialOffsets = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 19, 0, 0, 0, 16};

constexpr size_t kMaxDecodeOutputLanes = 192;

// Maps each output lane to the left 6-bit input value used for repacking.
constexpr array<uint8_t, 256> make_decode_lhs_idx_lut() {
    array<uint8_t, 256> lut{};
    for (size_t lane = 0; lane < kMaxDecodeOutputLanes; ++lane) {
        const size_t group = lane / 3;
        const size_t position = lane % 3;
        lut[lane] = static_cast<uint8_t>((group * 4) + position);
    }
    return lut;
}

// Maps each output lane to the right 6-bit input value used for repacking.
constexpr array<uint8_t, 256> make_decode_rhs_idx_lut() {
    array<uint8_t, 256> lut{};
    for (size_t lane = 0; lane < kMaxDecodeOutputLanes; ++lane) {
        const size_t group = lane / 3;
        const size_t position = lane % 3;
        lut[lane] = static_cast<uint8_t>((group * 4) + position + 1);
    }
    return lut;
}

// Supplies the per-lane left shift count for rebuilding decoded bytes.
constexpr array<uint8_t, 256> make_decode_lhs_shift_lut() {
    array<uint8_t, 256> lut{};
    for (size_t lane = 0; lane < kMaxDecodeOutputLanes; ++lane) {
        const size_t position = lane % 3;
        lut[lane] = static_cast<uint8_t>((position == 0)   ? 2
                                         : (position == 1) ? 4
                                                           : 6);
    }
    return lut;
}

// Supplies the per-lane right shift count for rebuilding decoded bytes.
constexpr array<uint8_t, 256> make_decode_rhs_shift_lut() {
    array<uint8_t, 256> lut{};
    for (size_t lane = 0; lane < kMaxDecodeOutputLanes; ++lane) {
        const size_t position = lane % 3;
        lut[lane] = static_cast<uint8_t>((position == 0)   ? 4
                                         : (position == 1) ? 2
                                                           : 0);
    }
    return lut;
}

constexpr auto kDecodeLhsIdxLut = make_decode_lhs_idx_lut();
constexpr auto kDecodeRhsIdxLut = make_decode_rhs_idx_lut();
constexpr auto kDecodeLhsShiftLut = make_decode_lhs_shift_lut();
constexpr auto kDecodeRhsShiftLut = make_decode_rhs_shift_lut();

INLINE svuint8_t encode_indices(svuint8_t input, svbool_t pg) {
    // Scalar equivalent for each output lane:
    // group = lane / 4;
    // position = lane % 4;
    const svuint8_t lane = svindex_u8(0, 1);
    const svuint8_t group = svlsr_n_u8_x(pg, lane, 2);
    const svuint8_t position = svand_n_u8_x(pg, lane, 0x03);

    // Scalar equivalent for each output lane:
    // idx0 = group * 3;
    // idx1 = idx0 + 1;
    // idx2 = idx0 + 2;
    const svuint8_t idx0 = svmul_n_u8_x(pg, group, 3);
    const svuint8_t idx1 = svadd_n_u8_x(pg, idx0, 1);
    const svuint8_t idx2 = svadd_n_u8_x(pg, idx0, 2);

    // Scalar equivalent for each output lane:
    // b0 = input[group * 3 + 0];
    // b1 = input[group * 3 + 1];
    // b2 = input[group * 3 + 2];
    const svuint8_t b0 = svtbl_u8(input, idx0);
    const svuint8_t b1 = svtbl_u8(input, idx1);
    const svuint8_t b2 = svtbl_u8(input, idx2);

    // Scalar equivalent for each 3-byte group:
    // out0 = b0 >> 2;
    // out1 = ((b0 & 0x03) << 4) | (b1 >> 4);
    // out2 = ((b1 & 0x0F) << 2) | (b2 >> 6);
    // out3 = b2 & 0x3F;
    const svuint8_t out0 = svlsr_n_u8_x(pg, b0, 2);
    const svuint8_t out1 =
        svorr_u8_x(pg, svlsl_n_u8_x(pg, svand_n_u8_x(pg, b0, 0x03), 4),
                   svlsr_n_u8_x(pg, b1, 4));
    const svuint8_t out2 =
        svorr_u8_x(pg, svlsl_n_u8_x(pg, svand_n_u8_x(pg, b1, 0x0F), 2),
                   svlsr_n_u8_x(pg, b2, 6));
    const svuint8_t out3 = svand_n_u8_x(pg, b2, 0x3F);

    svuint8_t indices = out3;
    indices = svsel_u8(svcmpeq_n_u8(pg, position, 2), out2, indices);
    indices = svsel_u8(svcmpeq_n_u8(pg, position, 1), out1, indices);
    indices = svsel_u8(svcmpeq_n_u8(pg, position, 0), out0, indices);
    return svand_n_u8_x(pg, indices, 0x3F);
}

INLINE svuint8_t encode_indices_shift(svuint8_t indices, svbool_t pg) {
    // Collapse the 64 possible Base64 indices into the small shift LUT:
    // 26..51 -> 0, 52..61 -> 1..10, 62 -> 11, 63 -> 12.
    const svbool_t ge51 = svcmpge_n_u8(pg, indices, 51);
    svuint8_t reduced =
        svsel_u8(ge51, svsub_n_u8_x(pg, indices, 51), svdup_u8(0));

    // 0..25 map to LUT entry 13. The final ASCII byte is index + shift.
    reduced = svsel_u8(svcmplt_n_u8(pg, indices, 26), svdup_u8(13), reduced);

    // Enable exactly 16 byte lanes so we load the whole shift LUT,
    // including the two unused padding entries at positions 14 and 15.
    const svbool_t pg_lut =
        svwhilelt_b8(static_cast<uint64_t>(0), static_cast<uint64_t>(16));

    // Load the signed ASCII shifts, then use reduced as vector lookup indices:
    // shifts[lane] = kEncodeShiftLut[reduced[lane]].
    const svint8_t shift_lut = svld1_s8(pg_lut, kEncodeShiftLut);
    const svint8_t shifts = svtbl_s8(shift_lut, reduced);

    // Reinterpret the unsigned Base64 indices as signed bytes so they can be
    // added to signed shifts, then reinterpret the ASCII bytes back to uint8_t.
    return svreinterpret_u8_s8(
        svadd_s8_x(pg, svreinterpret_s8_u8(indices), shifts));
}

INLINE svuint8_t encode_indices_chunked_tables(svuint8_t indices, svbool_t pg) {
    constexpr char table0[] = "ABCDEFGHIJKLMNOP";
    constexpr char table1[] = "QRSTUVWXYZabcdef";
    constexpr char table2[] = "ghijklmnopqrstuv";
    constexpr char table3[] = "wxyz0123456789+/";

    const svbool_t pg_lut =
        svwhilelt_b8(static_cast<uint64_t>(0), static_cast<uint64_t>(16));
    const svuint8_t lut0 =
        svld1_u8(pg_lut, reinterpret_cast<const uint8_t *>(table0));
    const svuint8_t lut1 =
        svld1_u8(pg_lut, reinterpret_cast<const uint8_t *>(table1));
    const svuint8_t lut2 =
        svld1_u8(pg_lut, reinterpret_cast<const uint8_t *>(table2));
    const svuint8_t lut3 =
        svld1_u8(pg_lut, reinterpret_cast<const uint8_t *>(table3));

    svuint8_t out = svtbl_u8(lut0, indices);
    out = svsel_u8(svcmpge_n_u8(pg, indices, 16),
                   svtbl_u8(lut1, svsub_n_u8_x(pg, indices, 16)), out);
    out = svsel_u8(svcmpge_n_u8(pg, indices, 32),
                   svtbl_u8(lut2, svsub_n_u8_x(pg, indices, 32)), out);
    out = svsel_u8(svcmpge_n_u8(pg, indices, 48),
                   svtbl_u8(lut3, svsub_n_u8_x(pg, indices, 48)), out);
    return out;
}

enum class EncodeVariant {
    shift,
    chunked,
};

template <EncodeVariant variant>
INLINE svuint8_t encode_indices_to_ascii(svuint8_t indices, svbool_t pg) {
    if constexpr (variant == EncodeVariant::shift) {
        return encode_indices_shift(indices, pg);
    } else {
        return encode_indices_chunked_tables(indices, pg);
    }
}

INLINE svuint8_t decode_ascii_to_values_shift(svuint8_t input, svbool_t pg) {
    const svbool_t pg_lut =
        svwhilelt_b8(static_cast<uint64_t>(0), static_cast<uint64_t>(16));
    const svuint8_t high_offset_lut =
        svld1_u8(pg_lut, kDecodeHighNibbleOffsets.data());
    const svuint8_t low_special_lut =
        svld1_u8(pg_lut, kDecodeLowNibbleSpecialOffsets.data());

    const svuint8_t high_nibble = svlsr_n_u8_x(pg, input, 4);
    const svuint8_t low_nibble = svand_n_u8_x(pg, input, 0x0F);

    svuint8_t offset = svtbl_u8(high_offset_lut, high_nibble);
    const svuint8_t special_offset = svtbl_u8(low_special_lut, low_nibble);
    offset = svsel_u8(svcmpeq_n_u8(pg, high_nibble, 2), special_offset, offset);

    return svadd_u8_x(pg, input, offset);
}

INLINE svuint8_t decode_ascii_to_values_mask(svuint8_t input, svbool_t pg) {
    const svint8_t in_s8 = svreinterpret_s8_u8(input);

    const svbool_t mask_AZ = svand_b_z(pg, svcmpgt_n_s8(pg, in_s8, 'A' - 1),
                                       svcmplt_n_s8(pg, in_s8, 'Z' + 1));
    const svbool_t mask_az = svand_b_z(pg, svcmpgt_n_s8(pg, in_s8, 'a' - 1),
                                       svcmplt_n_s8(pg, in_s8, 'z' + 1));
    const svbool_t mask_09 = svand_b_z(pg, svcmpgt_n_s8(pg, in_s8, '0' - 1),
                                       svcmplt_n_s8(pg, in_s8, '9' + 1));
    const svbool_t mask_plus = svcmpeq_n_u8(pg, input, '+');
    const svbool_t mask_slash = svcmpeq_n_u8(pg, input, '/');

    svint8_t offset = svdup_s8(0);
    offset = svsel_s8(mask_AZ, svdup_s8(-65), offset);
    offset = svsel_s8(mask_az, svdup_s8(-71), offset);
    offset = svsel_s8(mask_09, svdup_s8(4), offset);
    offset = svsel_s8(mask_plus, svdup_s8(19), offset);
    offset = svsel_s8(mask_slash, svdup_s8(16), offset);

    return svreinterpret_u8_s8(svadd_s8_x(pg, in_s8, offset));
}

enum class DecodeVariant {
    shift,
    mask,
};

template <DecodeVariant variant>
INLINE svuint8_t decode_ascii_to_values(svuint8_t input, svbool_t pg) {
    if constexpr (variant == DecodeVariant::shift) {
        return decode_ascii_to_values_shift(input, pg);
    } else {
        return decode_ascii_to_values_mask(input, pg);
    }
}

template <EncodeVariant variant>
size_t base64_sve_encode_impl(const uint8_t *input, size_t input_len,
                              char *output) {
    // Use all byte lanes for operations that produce a full SVE vector.
    const svbool_t pg_all = svptrue_b8();
    const size_t vl = svcntb();

    // One Base64 group produces 4 output bytes, so this is the number
    // of complete groups that fit in the current hardware vector length.
    const size_t groups = vl / 4;
    if (groups == 0) {
        return base64_scalar_encode(input, input_len, output);
    }

    // Each vector chunk consumes groups * 3 input bytes and emits
    // groups * 4 ASCII bytes.
    const size_t in_chunk = groups * 3;
    const size_t out_chunk = groups * 4;

    // Predicates keep loads/stores inside the meaningful bytes of each chunk.
    const svbool_t pg_in =
        svwhilelt_b8(static_cast<uint64_t>(0), static_cast<uint64_t>(in_chunk));
    const svbool_t pg_out = svwhilelt_b8(static_cast<uint64_t>(0),
                                         static_cast<uint64_t>(out_chunk));

    size_t i = 0;
    size_t j = 0;

    // Process four independent vector chunks per loop to reduce branch
    // overhead.
    constexpr size_t unroll_factor = 4;
    const size_t unrolled_in_chunk = in_chunk * unroll_factor;
    const size_t unrolled_out_chunk = out_chunk * unroll_factor;

    // Convert bytes -> 6-bit Base64 indices -> printable ASCII for each chunk.
    for (; i + unrolled_in_chunk <= input_len;
         i += unrolled_in_chunk, j += unrolled_out_chunk) {
        const svuint8_t in0 = svld1_u8(pg_in, input + i);
        const svuint8_t in1 = svld1_u8(pg_in, input + i + in_chunk);
        const svuint8_t in2 = svld1_u8(pg_in, input + i + (2 * in_chunk));
        const svuint8_t in3 = svld1_u8(pg_in, input + i + (3 * in_chunk));

        const svuint8_t out0 = encode_indices_to_ascii<variant>(
            encode_indices(in0, pg_all), pg_all);
        const svuint8_t out1 = encode_indices_to_ascii<variant>(
            encode_indices(in1, pg_all), pg_all);
        const svuint8_t out2 = encode_indices_to_ascii<variant>(
            encode_indices(in2, pg_all), pg_all);
        const svuint8_t out3 = encode_indices_to_ascii<variant>(
            encode_indices(in3, pg_all), pg_all);

        svst1_u8(pg_out, reinterpret_cast<uint8_t *>(output + j), out0);
        svst1_u8(pg_out, reinterpret_cast<uint8_t *>(output + j + out_chunk),
                 out1);
        svst1_u8(pg_out,
                 reinterpret_cast<uint8_t *>(output + j + (2 * out_chunk)),
                 out2);
        svst1_u8(pg_out,
                 reinterpret_cast<uint8_t *>(output + j + (3 * out_chunk)),
                 out3);
    }

    // After the unrolled loop, process any remaining whole vector chunks.
    for (; i + in_chunk <= input_len; i += in_chunk, j += out_chunk) {
        const svuint8_t in = svld1_u8(pg_in, input + i);
        const svuint8_t out = encode_indices_to_ascii<variant>(
            encode_indices(in, pg_all), pg_all);
        svst1_u8(pg_out, reinterpret_cast<uint8_t *>(output + j), out);
    }

    // The scalar path handles the final partial 3-byte group and any padding.
    j += base64_scalar_encode(input + i, input_len - i, output + j);
    return j;
}

size_t base64_sve_encode(const uint8_t *input, size_t input_len, char *output) {
    return base64_sve_encode_impl<EncodeVariant::shift>(input, input_len,
                                                        output);
}

size_t base64_sve_encode_naive(const uint8_t *input, size_t input_len,
                               char *output) {
    return base64_sve_encode_impl<EncodeVariant::chunked>(input, input_len,
                                                          output);
}

template <DecodeVariant variant>
size_t base64_sve_decode_impl(const char *input, size_t input_len,
                              uint8_t *output) {
    size_t len = input_len;

    // Ignore trailing Base64 padding before vector decoding; the scalar tail
    // still knows how to handle the remaining short group.
    while (len > 0 && input[len - 1] == '=') {
        --len;
    }

    constexpr size_t scalar_decode_threshold = 1024;
    if (len < scalar_decode_threshold) {
        return base64_scalar_decode(input, len, output);
    }

    // Decode works on input groups of 4 Base64 characters.
    const svbool_t pg_all = svptrue_b8();
    const size_t vl = svcntb();
    const size_t groups = vl / 4;
    if (groups == 0) {
        return base64_scalar_decode(input, len, output);
    }

    // Each vector chunk consumes groups * 4 Base64 characters and emits
    // groups * 3 decoded bytes.
    const size_t in_chunk = groups * 4;
    const size_t out_chunk = groups * 3;

    // Separate predicates describe valid input and output lanes.
    const svbool_t pg_in =
        svwhilelt_b8(static_cast<uint64_t>(0), static_cast<uint64_t>(in_chunk));
    const svbool_t pg_out = svwhilelt_b8(static_cast<uint64_t>(0),
                                         static_cast<uint64_t>(out_chunk));

    // Load the shuffle/shift tables once; they are reused for every chunk.
    const svuint8_t lhs_idx_vec = svld1_u8(pg_all, kDecodeLhsIdxLut.data());
    const svuint8_t rhs_idx_vec = svld1_u8(pg_all, kDecodeRhsIdxLut.data());
    const svuint8_t lhs_shift_vec = svld1_u8(pg_all, kDecodeLhsShiftLut.data());
    const svuint8_t rhs_shift_vec = svld1_u8(pg_all, kDecodeRhsShiftLut.data());

    size_t i = 0;
    size_t j = 0;

    for (; i + in_chunk <= len; i += in_chunk, j += out_chunk) {
        const svuint8_t in =
            svld1_u8(pg_in, reinterpret_cast<const uint8_t *>(input + i));
        const svuint8_t values = decode_ascii_to_values<variant>(in, pg_all);

        // svtbl_u8 gathers the left and right 6-bit values needed for each
        // decoded output byte, according to the precomputed index vectors.
        const svuint8_t lhs = svtbl_u8(values, lhs_idx_vec);
        const svuint8_t rhs = svtbl_u8(values, rhs_idx_vec);

        // Rebuild each decoded byte from two neighboring 6-bit values.
        const svuint8_t out =
            svorr_u8_x(pg_all, svlsl_u8_x(pg_all, lhs, lhs_shift_vec),
                       svlsr_u8_x(pg_all, rhs, rhs_shift_vec));

        svst1_u8(pg_out, output + j, out);
    }

    // Finish any incomplete 4-character group with the scalar decoder.
    j += base64_scalar_decode(input + i, len - i, output + j);
    return j;
}

size_t base64_sve_decode(const char *input, size_t input_len, uint8_t *output) {
    return base64_sve_decode_impl<DecodeVariant::shift>(input, input_len,
                                                        output);
}

size_t base64_sve_decode_naive(const char *input, size_t input_len,
                               uint8_t *output) {
    return base64_sve_decode_impl<DecodeVariant::mask>(input, input_len,
                                                       output);
}
#else
#warning SVE not available on this target: base64 will have no SVE implementation !
#endif
} // namespace base64
