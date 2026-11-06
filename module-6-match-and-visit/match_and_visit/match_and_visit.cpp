// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "match_and_visit.h"
#include "SVE-task-based-course.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>

#ifdef __aarch64__
#include <arm_sve.h>
#else
#warning Platform is not AArch64: no SVE-accelerated implementations will be available !
#endif

using std::size_t;
using std::uint32_t;
using std::uint64_t;

using std::vector;

namespace matchAndVisit {

int matchAndVisitScalar(const vector<uint32_t> &inputData, uint32_t threshold,
                        const MatchCallback &callbackFunction) {
    for (unsigned int i : inputData) {
        if (i < threshold) {
            callbackFunction(i);
        }
    }

    return EXIT_SUCCESS;
}

#ifdef __ARM_FEATURE_SVE
namespace {

INLINE void processMatches(const uint32_t *matchedValues, size_t matchCount,
                           const MatchCallback &callbackFunction) {
    if (matchCount == 0) {
        return;
    }

    // This cutoff is a tuning heuristic. Direct dispatch for small counts
    // avoids the general callback loop.
    if (matchCount <= 3) {
        callbackFunction(matchedValues[0]);
        if (matchCount == 1) {
            return;
        }

        callbackFunction(matchedValues[1]);
        if (matchCount == 2) {
            return;
        }

        callbackFunction(matchedValues[2]);
        return;
    }

    for (size_t i = 0; i < matchCount; ++i) {
        callbackFunction(matchedValues[i]);
    }
}

INLINE void processPnextMatches(svuint32_t values, svbool_t matches,
                                size_t matchCount, uint32_t *matchedValues,
                                const MatchCallback &callbackFunction) {
    if (matchCount == 0) {
        return;
    }

    auto current = svpfalse_b();
    size_t i = 0;

    // This cutoff is a tuning heuristic; benchmark it for the target processor
    // and configuration.
    if (matchCount <= 3) {
        uint32_t firstValue = 0;
        uint32_t secondValue = 0;
        uint32_t thirdValue = 0;

        if (matchCount >= 1) {
            current = svpnext_b32(matches, current);
            firstValue = svlastb_u32(current, values);
        }
        if (matchCount >= 2) {
            current = svpnext_b32(matches, current);
            secondValue = svlastb_u32(current, values);
        }
        if (matchCount == 3) {
            current = svpnext_b32(matches, current);
            thirdValue = svlastb_u32(current, values);
        }

        callbackFunction(firstValue);
        if (matchCount == 1) {
            return;
        }

        callbackFunction(secondValue);
        if (matchCount == 2) {
            return;
        }

        callbackFunction(thirdValue);
        return;
    }

    // Knowing matchCount avoids svptest_any after each PNEXT. Unroll complete
    // groups of four to reduce extraction-loop control overhead.
    while (i + 4 <= matchCount) {
        current = svpnext_b32(matches, current);
        matchedValues[i] = svlastb_u32(current, values);

        current = svpnext_b32(matches, current);
        matchedValues[i + 1] = svlastb_u32(current, values);

        current = svpnext_b32(matches, current);
        matchedValues[i + 2] = svlastb_u32(current, values);

        current = svpnext_b32(matches, current);
        matchedValues[i + 3] = svlastb_u32(current, values);
        i += 4;
    }

    for (; i < matchCount; ++i) {
        current = svpnext_b32(matches, current);
        matchedValues[i] = svlastb_u32(current, values);
    }

    processMatches(matchedValues, matchCount, callbackFunction);
}

} // namespace

int matchAndVisitSVEBasic(const vector<uint32_t> &inputData, uint32_t threshold,
                          const MatchCallback &callbackFunction) {
    const auto laneCount = static_cast<size_t>(svcntw());

    for (size_t base = 0; base < inputData.size(); base += laneCount) {
        const auto active =
            svwhilelt_b32(static_cast<uint64_t>(base),
                          static_cast<uint64_t>(inputData.size()));
        const auto values = svld1_u32(active, inputData.data() + base);
        const auto matches = svcmplt_n_u32(active, values, threshold);
        auto current = svpfalse_b();

        while (svptest_any(matches, current = svpnext_b32(matches, current))) {
            const auto value = svlastb_u32(current, values);
            callbackFunction(value);
        }
    }

    return EXIT_SUCCESS;
}

int matchAndVisitSVEOptimized(const vector<uint32_t> &inputData,
                              uint32_t threshold,
                              const MatchCallback &callbackFunction) {
    const auto laneCount = static_cast<size_t>(svcntw());
    vector<uint32_t> matchedValues(laneCount);

    for (size_t base = 0; base < inputData.size(); base += laneCount) {
        const auto active =
            svwhilelt_b32(static_cast<uint64_t>(base),
                          static_cast<uint64_t>(inputData.size()));
        const auto values = svld1_u32(active, inputData.data() + base);
        const auto matches = svcmplt_n_u32(active, values, threshold);
        const auto matchCount =
            static_cast<size_t>(svcntp_b32(active, matches));
        if (matchCount == 0) {
            continue;
        }

        processPnextMatches(values, matches, matchCount, matchedValues.data(),
                            callbackFunction);
    }

    return EXIT_SUCCESS;
}

int matchAndVisitSVECompact(const vector<uint32_t> &inputData,
                            uint32_t threshold,
                            const MatchCallback &callbackFunction) {
    const auto laneCount = static_cast<size_t>(svcntw());
    vector<uint32_t> compactValues(laneCount);

    for (size_t base = 0; base < inputData.size(); base += laneCount) {
        const auto active =
            svwhilelt_b32(static_cast<uint64_t>(base),
                          static_cast<uint64_t>(inputData.size()));
        const auto values = svld1_u32(active, inputData.data() + base);
        const auto matches = svcmplt_n_u32(active, values, threshold);
        const auto matchCount =
            static_cast<size_t>(svcntp_b32(active, matches));
        if (matchCount == 0) {
            continue;
        }
        const auto packed =
            svwhilelt_b32(uint64_t{0}, static_cast<uint64_t>(matchCount));

        svst1_u32(packed, compactValues.data(), svcompact_u32(matches, values));

        processMatches(compactValues.data(), matchCount, callbackFunction);
    }

    return EXIT_SUCCESS;
}
#else
#warning SVE is not available on this target: match_and_visit will have no SVE implementations !
#endif
} // namespace matchAndVisit
