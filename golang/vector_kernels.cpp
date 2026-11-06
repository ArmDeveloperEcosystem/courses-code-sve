// clang-format off
//go:build linux && arm64 && cgo
// clang-format on

// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on

#include "vector_kernels.h"

#include <cstddef>

#include <arm_sve.h>

#if defined(__GNUC__) || defined(__clang__)
#define SVE_TARGET __attribute__((target("+sve")))
#define SVE2_TARGET __attribute__((target("+sve2")))
#else
#error "the SVE kernels require GCC or Clang target attributes"
#endif

namespace {

// 64 printable Base64 (6-bit) characters. The encode table uses char because
// there is no native 6-bit data type.
constexpr char base64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Shift Base64 indices into their ASCII ranges after reducing them to a
// 4-bit LUT index. Since ASCII is mostly contiguous, this can be a small LUT
// of shifts instead of a full 64-byte encode table.
// Examples: 0 -> 'A' uses +65, 26 -> 'a' uses +71, 52 -> '0' uses -4,
// 62 -> '+' uses -19, and 63 -> '/' uses -16. Entries 14 and 15 are unused
// padding so the SVE table load is 16 bytes.
constexpr int8_t base64_shift_lut[16] = {71, -4, -4, -4,  -4,  -4, -4, -4,
                                         -4, -4, -4, -19, -16, 65, 0,  0};

std::size_t base64_scalar_encode(const uint8_t *input, std::size_t input_len,
                                 char *output) {
    std::size_t i = 0;
    std::size_t j = 0;

    // Process the input in chunks of three bytes.
    for (; i + 2 < input_len; i += 3) {
        // Bits 24 through 31 are unused. Pack three 8-bit values into a
        // 32-bit value, then map each 6-bit index to a Base64 character.
        const uint32_t value = (static_cast<uint32_t>(input[i]) << 16) |
                               (static_cast<uint32_t>(input[i + 1]) << 8) |
                               static_cast<uint32_t>(input[i + 2]);
        output[j++] = base64_table[(value >> 18) & 0x3f];
        output[j++] = base64_table[(value >> 12) & 0x3f];
        output[j++] = base64_table[(value >> 6) & 0x3f];
        output[j++] = base64_table[value & 0x3f];
    }

    // Handle the remaining one or two bytes with padding.
    if (i < input_len) {
        uint32_t value = static_cast<uint32_t>(input[i]) << 16;
        if (i + 1 < input_len) {
            value |= static_cast<uint32_t>(input[i + 1]) << 8;
        }
        output[j++] = base64_table[(value >> 18) & 0x3f];
        output[j++] = base64_table[(value >> 12) & 0x3f];
        // '=' is a special Base64 symbol and must not be interpreted as data.
        output[j++] =
            i + 1 < input_len ? base64_table[(value >> 6) & 0x3f] : '=';
        output[j++] = '=';
    }
    return j;
}

SVE_TARGET svuint8_t base64_indices(svuint8_t input, svbool_t predicate) {
    // Scalar equivalent for each output lane:
    // group = lane / 4;
    // position = lane % 4;
    const svuint8_t lane = svindex_u8(0, 1);
    const svuint8_t group = svlsr_n_u8_x(predicate, lane, 2);
    const svuint8_t position = svand_n_u8_x(predicate, lane, 3);

    // Scalar equivalent for each output lane:
    // index0 = group * 3;
    // index1 = index0 + 1;
    // index2 = index0 + 2;
    const svuint8_t index0 = svmul_n_u8_x(predicate, group, 3);
    const svuint8_t index1 = svadd_n_u8_x(predicate, index0, 1);
    const svuint8_t index2 = svadd_n_u8_x(predicate, index0, 2);

    // Scalar equivalent for each output lane:
    // byte0 = input[group * 3 + 0];
    // byte1 = input[group * 3 + 1];
    // byte2 = input[group * 3 + 2];
    const svuint8_t byte0 = svtbl_u8(input, index0);
    const svuint8_t byte1 = svtbl_u8(input, index1);
    const svuint8_t byte2 = svtbl_u8(input, index2);

    // Scalar equivalent for each 3-byte group:
    // output0 = byte0 >> 2;
    // output1 = ((byte0 & 0x03) << 4) | (byte1 >> 4);
    // output2 = ((byte1 & 0x0f) << 2) | (byte2 >> 6);
    // output3 = byte2 & 0x3f;
    const svuint8_t output0 = svlsr_n_u8_x(predicate, byte0, 2);
    const svuint8_t output1 = svorr_u8_x(
        predicate,
        svlsl_n_u8_x(predicate, svand_n_u8_x(predicate, byte0, 3), 4),
        svlsr_n_u8_x(predicate, byte1, 4));
    const svuint8_t output2 = svorr_u8_x(
        predicate,
        svlsl_n_u8_x(predicate, svand_n_u8_x(predicate, byte1, 0x0f), 2),
        svlsr_n_u8_x(predicate, byte2, 6));
    svuint8_t indices = svand_n_u8_x(predicate, byte2, 0x3f);
    indices = svsel_u8(svcmpeq_n_u8(predicate, position, 2), output2, indices);
    indices = svsel_u8(svcmpeq_n_u8(predicate, position, 1), output1, indices);
    return svsel_u8(svcmpeq_n_u8(predicate, position, 0), output0, indices);
}

SVE_TARGET svuint8_t base64_ascii(svuint8_t indices, svbool_t predicate) {
    // Collapse the 64 possible Base64 indices into the small shift LUT:
    // 26..51 -> 0, 52..61 -> 1..10, 62 -> 11, 63 -> 12.
    svuint8_t reduced =
        svsel_u8(svcmpge_n_u8(predicate, indices, 51),
                 svsub_n_u8_x(predicate, indices, 51), svdup_u8(0));

    // 0..25 map to LUT entry 13. The final ASCII byte is index + shift.
    reduced =
        svsel_u8(svcmplt_n_u8(predicate, indices, 26), svdup_u8(13), reduced);

    // Enable exactly 16 byte lanes so the whole shift LUT is loaded, including
    // the two unused padding entries at positions 14 and 15.
    const svbool_t lut_predicate = svwhilelt_b8(uint64_t{0}, uint64_t{16});

    // Load the signed ASCII shifts, then use reduced as vector lookup indices:
    // shifts[lane] = base64_shift_lut[reduced[lane]].
    const svint8_t shifts =
        svtbl_s8(svld1_s8(lut_predicate, base64_shift_lut), reduced);

    // Reinterpret the unsigned Base64 indices as signed bytes so they can be
    // added to signed shifts, then reinterpret the ASCII bytes as uint8_t.
    return svreinterpret_u8_s8(
        svadd_s8_x(predicate, svreinterpret_s8_u8(indices), shifts));
}

} // namespace

extern "C" SVE_TARGET void sve_vector_addition_i32(int32_t *dst,
                                                   const int32_t *x,
                                                   const int32_t *y,
                                                   size_t count) {
    size_t index = 0;
    while (index < count) {
        const svbool_t active = svwhilelt_b32(index, count);
        const svint32_t x_values = svld1_s32(active, x + index);
        const svint32_t y_values = svld1_s32(active, y + index);
        svst1_s32(active, dst + index, svadd_s32_x(active, x_values, y_values));
        index += svcntw();
    }
}

extern "C" SVE_TARGET std::size_t
base64_sve_encode(const uint8_t *input, std::size_t input_len, char *output) {
    // Use all byte lanes for operations that produce a full SVE vector.
    const svbool_t predicate = svptrue_b8();

    // One Base64 group produces four output bytes, so this is the number of
    // complete groups that fit in the current hardware vector length.
    const std::size_t groups = svcntb() / 4;

    // Each vector chunk consumes groups * 3 input bytes and emits groups * 4
    // ASCII bytes.
    const std::size_t input_chunk = groups * 3;
    const std::size_t output_chunk = groups * 4;

    // Predicates keep loads and stores inside the meaningful bytes of a chunk.
    const svbool_t input_predicate =
        svwhilelt_b8(uint64_t{0}, static_cast<uint64_t>(input_chunk));
    const svbool_t output_predicate =
        svwhilelt_b8(uint64_t{0}, static_cast<uint64_t>(output_chunk));

    // Process four independent vector chunks per loop to reduce branch
    // overhead.
    constexpr std::size_t unroll = 4;
    const std::size_t unrolled_input = input_chunk * unroll;
    const std::size_t unrolled_output = output_chunk * unroll;
    std::size_t i = 0;
    std::size_t j = 0;

    // Convert bytes to 6-bit Base64 indices and then to printable ASCII for
    // each chunk.
    for (; i + unrolled_input <= input_len;
         i += unrolled_input, j += unrolled_output) {
        const svuint8_t input0 = svld1_u8(input_predicate, input + i);
        const svuint8_t input1 =
            svld1_u8(input_predicate, input + i + input_chunk);
        const svuint8_t input2 =
            svld1_u8(input_predicate, input + i + 2 * input_chunk);
        const svuint8_t input3 =
            svld1_u8(input_predicate, input + i + 3 * input_chunk);
        svst1_u8(output_predicate, reinterpret_cast<uint8_t *>(output + j),
                 base64_ascii(base64_indices(input0, predicate), predicate));
        svst1_u8(output_predicate,
                 reinterpret_cast<uint8_t *>(output + j + output_chunk),
                 base64_ascii(base64_indices(input1, predicate), predicate));
        svst1_u8(output_predicate,
                 reinterpret_cast<uint8_t *>(output + j + 2 * output_chunk),
                 base64_ascii(base64_indices(input2, predicate), predicate));
        svst1_u8(output_predicate,
                 reinterpret_cast<uint8_t *>(output + j + 3 * output_chunk),
                 base64_ascii(base64_indices(input3, predicate), predicate));
    }

    // After the unrolled loop, process any remaining whole vector chunks.
    for (; i + input_chunk <= input_len; i += input_chunk, j += output_chunk) {
        const svuint8_t value = svld1_u8(input_predicate, input + i);
        svst1_u8(output_predicate, reinterpret_cast<uint8_t *>(output + j),
                 base64_ascii(base64_indices(value, predicate), predicate));
    }

    // The scalar path handles the final partial 3-byte group and any padding.
    return j + base64_scalar_encode(input + i, input_len - i, output + j);
}

extern "C" SVE2_TARGET void
sve2_histogram_u32(uint32_t *histogram, const uint32_t *records, size_t count) {
    size_t index = 0;
    while (index < count) {
        const svbool_t active = svwhilelt_b32(index, count);
        const svuint32_t indices = svld1_u32(active, records + index);
        const svuint32_t old_counts =
            svld1_gather_u32index_u32(active, histogram, indices);
        const svuint32_t duplicate_counts =
            svhistcnt_u32_z(active, indices, indices);
        const svuint32_t new_counts =
            svadd_u32_m(active, old_counts, duplicate_counts);
        svst1_scatter_u32index_u32(active, histogram, indices, new_counts);
        index += svcntw();
    }
}
