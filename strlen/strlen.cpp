// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "strlen.h"

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
using std::uintptr_t;

namespace str {

size_t strlen(const char *s) {
    const char *e = s;
    while (*e != '\0') {
        e++;
    }
    return e - s;
}

#ifdef __ARM_FEATURE_SVE
size_t strlenSVENaive(const char *s) {
    const auto *start = reinterpret_cast<const unsigned char *>(s);
    const unsigned char *cursor = start;
    const svbool_t pg = svptrue_b8();

    while (true) {
        svsetffr();
        const svuint8_t bytes = svldff1_u8(pg, cursor);
        const svbool_t loaded = svrdffr_z(pg);
        const svbool_t zeros = svcmpeq_n_u8(loaded, bytes, 0);

        if (svptest_any(loaded, zeros)) {
            const svbool_t before_zero = svbrkb_b_z(loaded, zeros);
            const auto index = static_cast<size_t>(svcntp_b8(pg, before_zero));
            return static_cast<size_t>(cursor - start) + index;
        }

        cursor += static_cast<size_t>(svcntp_b8(pg, loaded));
    }
}

size_t strlenSVEImproved(const char *s) {
    const auto *start = reinterpret_cast<const unsigned char *>(s);
    const unsigned char *cursor = start;
    const svbool_t pg = svptrue_b8();

    svsetffr();
    while (true) {
        const svuint8_t bytes = svldff1_u8(pg, cursor);
        const svbool_t loaded = svrdffr_z(pg);
        const svbool_t zeros = svcmpeq_n_u8(loaded, bytes, 0);

        if (svptest_any(loaded, zeros)) {
            const svbool_t before_zero = svbrkb_b_z(loaded, zeros);
            const auto index = static_cast<size_t>(svcntp_b8(pg, before_zero));
            return static_cast<size_t>(cursor - start) + index;
        }

        cursor += static_cast<size_t>(svcntp_b8(pg, loaded));
    }
}

size_t strlenSVESimdLoops(const char *s) {
    const auto *bytes = reinterpret_cast<const uint8_t *>(s);
    const svbool_t all = svptrue_b8();
    svbool_t loaded;
    svbool_t zeros;
    svuint8_t data;

    size_t index = 0;
    const size_t alignment = 16 - (reinterpret_cast<uintptr_t>(bytes) % 16);
    loaded = svwhilele_b8(static_cast<uint64_t>(0),
                          static_cast<uint64_t>(alignment));
    data = svld1_u8(loaded, bytes);
    zeros = svcmpeq_n_u8(loaded, data, 0);

    if (!svptest_any(zeros, zeros)) {
        index += alignment;
        svsetffr();

        while (true) {
            data = svldff1_u8(all, bytes + index);
            loaded = svrdffr_z(all);
            zeros = svcmpeq_n_u8(loaded, data, 0);

            if (svptest_any(zeros, zeros)) {
                break;
            }

            if (svptest_last(all, loaded)) {
                index += static_cast<size_t>(svcntb());
            } else {
                svsetffr();
                index += static_cast<size_t>(svcntp_b8(loaded, loaded));
            }
        }
    }

    zeros = svbrkb_b_z(all, zeros);
    index += static_cast<size_t>(svcntp_b8(zeros, zeros));
    return index;
}
#else
#warning SVE not available on this target: strlen will have no SVE implementation !
#endif

} // namespace str
