// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#pragma once

#include <concepts>
#include <cstdint>
#include <span>

namespace sve_match {

template <typename T>
concept MatchElement =
    std::same_as<T, std::uint8_t> || std::same_as<T, std::uint16_t>;

/// Return true when any haystack element equals any key.
template <MatchElement T>
[[nodiscard]] bool search_scalar(std::span<const T> haystack,
                                 std::span<const T> keys) noexcept;

#ifdef __ARM_FEATURE_SVE2
/// Search with SVE2 MATCH, processing one scalable vector per iteration.
template <MatchElement T>
[[nodiscard]] bool search_sve2(std::span<const T> haystack,
                               std::span<const T> keys) noexcept;

/// Search with SVE2 MATCH, processing four scalable vectors per iteration.
template <MatchElement T>
[[nodiscard]] bool search_sve2_unrolled(std::span<const T> haystack,
                                        std::span<const T> keys) noexcept;
#endif

} // namespace sve_match
