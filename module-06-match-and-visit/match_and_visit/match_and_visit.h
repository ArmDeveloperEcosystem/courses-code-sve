// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace matchAndVisit {
using MatchCallback = std::function<void(std::uint32_t)>;

int matchAndVisitScalar(const std::vector<std::uint32_t> &inputData,
                        std::uint32_t threshold,
                        const MatchCallback &callbackFunction);

#ifdef __ARM_FEATURE_SVE
int matchAndVisitSVEOptimized(const std::vector<std::uint32_t> &inputData,
                              std::uint32_t threshold,
                              const MatchCallback &callbackFunction);

int matchAndVisitSVEBasic(const std::vector<std::uint32_t> &inputData,
                          std::uint32_t threshold,
                          const MatchCallback &callbackFunction);

int matchAndVisitSVECompact(const std::vector<std::uint32_t> &inputData,
                            std::uint32_t threshold,
                            const MatchCallback &callbackFunction);
#endif
} // namespace matchAndVisit
