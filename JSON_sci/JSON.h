// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace JSON {

/// Our JSON structural-character indexer (SCI) implementation, which identifies
/// the indices of structural characters in a JSON string, while correctly
/// handling string literals and escaped characters.
class SCI {
  public:
    /// The type used for indices.
    using IndiceTy = std::uint64_t;
    using IndicesTy = std::vector<IndiceTy>;

    /// Construct an SCI instance with the given JSON string.
    explicit SCI(std::string_view json) : json(json) {
        // Reserve space for structural characters.
        result.reserve(json.size() / 2);
    }

    /// A reference implementation of a JSON structural-character indexer, for
    /// benchmarking and correctness purposes.
    IndicesTy get_ref() noexcept;

    /// SVE-accelerated implementations of the JSON structural-character
    /// indexer. These functions will only be defined if the code is being
    /// compiled on an AArch64 platform with SVE/SVE2/SVE2.2 support.
#ifdef __ARM_FEATURE_SVE
    IndicesTy get_sve() noexcept;
#endif
#ifdef __ARM_FEATURE_SVE2
    IndicesTy get_sve2() noexcept;
#endif
#ifdef __ARM_FEATURE_SVE2p2
    IndicesTy get_sve2p2() noexcept;
#endif

  private:
    /// Indices of structural characters found in the JSON string. Store it in
    /// the object, rather than locally in the indexing function to avoid the
    /// cost of allocating its memory while benchmarking, as well as the cost of
    /// reallocations during indexing.
    IndicesTy result;
    /// The JSON string to be indexed.
    std::string_view json;
    /// Indicates whether the current position is inside a string literal.
    bool in_string{false};

    /// Reset the state of the SCI instance, clearing the result and resetting
    /// the in_string flag.
    void reset() noexcept {
        result.clear();
        in_string = false;
    }
};

} // namespace JSON