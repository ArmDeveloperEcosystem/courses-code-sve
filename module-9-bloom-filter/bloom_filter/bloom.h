// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#pragma once

#include "SVE-task-based-course.h"
#include "bloom_config.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

static_assert(BLOOM_BITS > 0, "BLOOM_BITS must be positive");
static_assert(BLOOM_NUM_HASHES > 0, "BLOOM_NUM_HASHES must be positive");
static_assert((BLOOM_BITS & (BLOOM_BITS - 1u)) == 0,
              "BLOOM_BITS must be a power of two");
static_assert((BLOOM_NUM_HASHES & (BLOOM_NUM_HASHES - 1u)) == 0,
              "BLOOM_NUM_HASHES must be a power of two");
static_assert((BLOOM_BITS % BLOOM_NUM_HASHES) == 0,
              "BLOOM_BITS must be divisible by BLOOM_NUM_HASHES");
static_assert((BLOOM_SLICE_BITS % BLOOM_WORD_BITS) == 0,
              "Each hash slice must contain a whole number of uint64_t words");

namespace Bloom {

// Each position identifies a bit in the filter, rather than a raw hash value.
using BitPositions_t = std::array<std::uint32_t, BLOOM_NUM_HASHES>;
using Key_t = std::string_view;
using HashFn_t = BitPositions_t (*)(Key_t);

// Shared bit storage and runtime interface for all implementations.
// Derived classes choose how to calculate positions and access the bits.
class Filter {
  public:
    Filter() = default;
    virtual ~Filter() = default;

    // Reset the entire filter; individual keys cannot be removed safely.
    void clear() { words.fill(0); }

    // Set the bits associated with the key. Repeated insertions are harmless.
    virtual void add(Key_t key) noexcept = 0;

    // False means definitely absent; true means possibly present. Different
    // keys can set the same bits, so a positive answer may be a false positive.
    [[nodiscard]] virtual bool maybe_contains(Key_t key) const noexcept = 0;

    // Used by tests to compare implementations with the same bit layout.
    // Equality does not establish that the same set of keys was inserted.
    static bool compare_words(const Filter &a, const Filter &b) {
        return a.words == b.words;
    }

  protected:
    // Call from derived constructor bodies, where virtual add dispatches to
    // the concrete implementation. The filter does not retain the key strings.
    void populate(const std::vector<std::string> &keys) noexcept {
        for (const auto &key : keys)
            add(key);
    }

    std::array<std::uint64_t, BLOOM_WORDS> words{};
};

// HashFn is selected at compile time and returns all bit positions. This
// class shares scalar bit access between scalar and SVE hash implementations.
// Partitioned SVE filters instead fuse hashing with vector memory access.
template <HashFn_t HashFn> class FilterWithHash final : public Filter {
  public:
    FilterWithHash() = default;

    explicit FilterWithHash(const std::vector<std::string> &keys) noexcept {
        populate(keys);
    }

    void add(Key_t key) noexcept override {
        for (std::uint32_t bit : HashFn(key))
            set(bit);
    }

    [[nodiscard]] bool maybe_contains(Key_t key) const noexcept override {
        for (std::uint32_t bit : HashFn(key))
            if (!test(bit))
                return false;
        return true;
    }

  private:
    INLINE void set(uint32_t bit) noexcept {
        // The upper bits select a 64-bit word; the lower six select its bit.
        words[bit >> 6] |= 1ULL << (bit & 63u);
    }

    [[nodiscard]] INLINE bool test(uint32_t bit) const noexcept {
        return (words[bit >> 6] >> (bit & 63u)) & 1ULL;
    }
};

BitPositions_t hashScalar64(Key_t key);
BitPositions_t hashScalar32(Key_t key);
BitPositions_t hashScalar64Partitioned(Key_t key);
BitPositions_t hashScalar32Partitioned(Key_t key);

using BloomFilterScalar64 = FilterWithHash<hashScalar64>;
using BloomFilterScalar32 = FilterWithHash<hashScalar32>;
using BloomFilterScalar64Partitioned = FilterWithHash<hashScalar64Partitioned>;
using BloomFilterScalar32Partitioned = FilterWithHash<hashScalar32Partitioned>;

#ifdef __ARM_FEATURE_SVE
BitPositions_t hashSVE64(Key_t key);
BitPositions_t hashSVE32(Key_t key);

using BloomFilterSVE64 = FilterWithHash<hashSVE64>;
using BloomFilterSVE32 = FilterWithHash<hashSVE32>;

// Each hash owns a separate word-aligned slice of the filter. The fused SVE
// operations keep positions in registers and gather/scatter the filter words,
// without exposing a separate function that returns bit positions.
class BloomFilterSVE64Partitioned final : public Filter {
  public:
    BloomFilterSVE64Partitioned() = default;
    explicit BloomFilterSVE64Partitioned(
        const std::vector<std::string> &keys) noexcept {
        populate(keys);
    }

    void add(Key_t key) noexcept override;
    [[nodiscard]] bool maybe_contains(Key_t key) const noexcept override;
};

class BloomFilterSVE32Partitioned final : public Filter {
  public:
    BloomFilterSVE32Partitioned() = default;
    explicit BloomFilterSVE32Partitioned(
        const std::vector<std::string> &keys) noexcept {
        populate(keys);
    }

    void add(Key_t key) noexcept override;
    [[nodiscard]] bool maybe_contains(Key_t key) const noexcept override;
};
#endif

} // namespace Bloom
