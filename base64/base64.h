// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#pragma once

#include <cstddef>
#include <cstdint>

namespace base64 {
/**
 * Scalar base64 encode function
 *
 * @param input byte stream buffer to be encoded into base64 symbols
 * @param input_len number of bytes to process
 * @param output Writes to `output` (caller must allocate at least ((input_len +
 * 2) / 3) * 4 bytes)
 * @return returns the number of encoded bytes written
 */
std::size_t base64_scalar_encode(const std::uint8_t *input,
                                 std::size_t input_len, char *output);

/**
 * Scalar base64 decode function
 *
 * @param input byte stream buffer of encoded base64 symbols
 * @param input_len number of bytes to process
 * @param output Writes to `output` (caller must allocate at least (input_len /
 * 4) * 3 bytes)
 * @return returns the number of decoded bytes written
 */
std::size_t base64_scalar_decode(const char *input, std::size_t input_len,
                                 std::uint8_t *output);

#ifdef __ARM_FEATURE_SVE
/**
 * SVE base64 encode function
 *
 * @param input byte stream buffer to be encoded into base64 symbols
 * @param input_len number of bytes to process
 * @param output Writes to `output` (caller must allocate at least ((input_len +
 * 2) / 3) * 4 bytes)
 * @return returns the number of encoded bytes written
 */
std::size_t base64_sve_encode(const std::uint8_t *input, std::size_t input_len,
                              char *output);

/**
 * Naive SVE base64 encode function
 *
 * @param input byte stream buffer to be encoded into base64 symbols
 * @param input_len number of bytes to process
 * @param output Writes to `output` (caller must allocate at least ((input_len +
 * 2) / 3) * 4 bytes)
 * @return returns the number of encoded bytes written
 */
std::size_t base64_sve_encode_naive(const std::uint8_t *input,
                                    std::size_t input_len, char *output);

/**
 * SVE base64 decode function
 *
 * @param input byte stream buffer of encoded base64 symbols
 * @param input_len number of bytes to process
 * @param output Writes to `output` (caller must allocate at least (input_len /
 * 4) * 3 bytes)
 * @return returns the number of decoded bytes written
 */
std::size_t base64_sve_decode(const char *input, std::size_t input_len,
                              std::uint8_t *output);

/**
 * Naive SVE base64 decode function
 *
 * @param input byte stream buffer of encoded base64 symbols
 * @param input_len number of bytes to process
 * @param output Writes to `output` (caller must allocate at least (input_len /
 * 4) * 3 bytes)
 * @return returns the number of decoded bytes written
 */
std::size_t base64_sve_decode_naive(const char *input, std::size_t input_len,
                                    std::uint8_t *output);
#endif
} // namespace base64
