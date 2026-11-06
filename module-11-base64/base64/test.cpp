// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "base64.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "gtest/gtest.h"

using std::size_t;
using std::uint8_t;

using std::array;
using std::string_view;
using std::vector;

class base64Test : public ::testing::Test {
  protected:
    // ideal input data that is a clean multiple of 3 bytes
    array<uint8_t, 3> threeByteInput{'a', 'a', 'a'};
    array<char, 4> encodedState;
    array<uint8_t, 3> decodedState;
    array<char, 4> threeByteAnswer{'Y', 'W', 'F', 'h'};

    // input data that is not a clean multiple of 24 bits
    array<uint8_t, 13> sentence{'S', 'V', 'E', '2', 'i', 's', 'a',
                                'm', 'a', 'z', 'i', 'n', 'g'};
    array<char, 20> encodedSentence;
    array<uint8_t, 13> decodedSentence;
    array<char, 20> sentenceAnswer{'U', '1', 'Z', 'F', 'M', 'm', 'l',
                                   'z', 'Y', 'W', '1', 'h', 'e', 'm',
                                   'l', 'u', 'Z', 'w', '=', '='};

    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(base64Test, scalarFunctionalTest) {

    base64::base64_scalar_encode(threeByteInput.data(), threeByteInput.size(),
                                 encodedState.data());
    base64::base64_scalar_decode(encodedState.data(), encodedState.size(),
                                 decodedState.data());
    EXPECT_EQ(threeByteAnswer, encodedState);
    EXPECT_EQ(threeByteInput, decodedState);
}

TEST_F(base64Test, scalarSentenceFunctionalTest) {

    base64::base64_scalar_encode(sentence.data(), sentence.size(),
                                 encodedSentence.data());
    base64::base64_scalar_decode(encodedSentence.data(), encodedSentence.size(),
                                 decodedSentence.data());
    EXPECT_EQ(sentenceAnswer, encodedSentence);
    EXPECT_EQ(sentence, decodedSentence);
}

#ifdef __ARM_FEATURE_SVE
using SveEncodeFn = size_t (*)(const uint8_t *, size_t, char *);
using SveDecodeFn = size_t (*)(const char *, size_t, uint8_t *);

TEST_F(base64Test, sveEncodeVariantsFunctionalTest) {
    for (const SveEncodeFn encode :
         {base64::base64_sve_encode, base64::base64_sve_encode_naive}) {
        array<char, 4> encoded;
        encode(threeByteInput.data(), threeByteInput.size(), encoded.data());
        EXPECT_EQ(threeByteAnswer, encoded);

        array<char, 20> encodedSentenceVariant;
        encode(sentence.data(), sentence.size(), encodedSentenceVariant.data());
        EXPECT_EQ(sentenceAnswer, encodedSentenceVariant);
    }
}

TEST_F(base64Test, sveDecodeVariantsFunctionalTest) {
    for (const SveDecodeFn decode :
         {base64::base64_sve_decode, base64::base64_sve_decode_naive}) {
        array<uint8_t, 3> decoded;
        decode(threeByteAnswer.data(), threeByteAnswer.size(), decoded.data());
        EXPECT_EQ(threeByteInput, decoded);

        array<uint8_t, 13> decodedSentenceVariant;
        decode(sentenceAnswer.data(), sentenceAnswer.size(),
               decodedSentenceVariant.data());
        EXPECT_EQ(sentence, decodedSentenceVariant);
    }
}

TEST_F(base64Test, sveDecodeVariantsMatchScalarForAlphabetClasses) {
    constexpr string_view alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    vector<char> encoded;
    encoded.reserve(alphabet.size() * 16);
    for (size_t repeat = 0; repeat < 16; ++repeat) {
        encoded.insert(encoded.end(), alphabet.begin(), alphabet.end());
    }

    vector<uint8_t> scalarDecoded(encoded.size());
    const size_t scalarSize = base64::base64_scalar_decode(
        encoded.data(), encoded.size(), scalarDecoded.data());
    scalarDecoded.resize(scalarSize);

    for (const SveDecodeFn decode :
         {base64::base64_sve_decode, base64::base64_sve_decode_naive}) {
        vector<uint8_t> sveDecoded(encoded.size());
        const size_t sveSize =
            decode(encoded.data(), encoded.size(), sveDecoded.data());
        sveDecoded.resize(sveSize);
        EXPECT_EQ(scalarSize, sveSize);
        EXPECT_EQ(scalarDecoded, sveDecoded);
    }
}

TEST_F(base64Test, sveVariantsMatchScalarAcrossSizes) {
    const array<size_t, 68> sizes = {
        1,  2,  3,  4,  5,  6,  7,  8,  9,   10,   11,   12,  13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23,  24,   25,   26,  27, 28,
        29, 30, 31, 32, 33, 34, 35, 36, 37,  38,   39,   40,  41, 42,
        43, 44, 45, 46, 47, 48, 49, 50, 51,  52,   53,   54,  55, 56,
        57, 58, 59, 60, 61, 62, 63, 64, 256, 1024, 4096, 8191};

    for (const size_t size : sizes) {
        vector<uint8_t> input(size);
        for (size_t i = 0; i < input.size(); ++i) {
            input[i] = static_cast<uint8_t>((i * 17 + 3) & 0xFF);
        }

        vector<char> scalarEncoded(((input.size() + 2) / 3) * 4);
        const size_t encodedSize = base64::base64_scalar_encode(
            input.data(), input.size(), scalarEncoded.data());

        for (const SveEncodeFn encode :
             {base64::base64_sve_encode, base64::base64_sve_encode_naive}) {
            vector<char> sveEncoded(scalarEncoded.size());
            const size_t sveEncodedSize =
                encode(input.data(), input.size(), sveEncoded.data());
            sveEncoded.resize(sveEncodedSize);
            EXPECT_EQ(vector<char>(scalarEncoded.begin(),
                                   scalarEncoded.begin() + encodedSize),
                      sveEncoded)
                << "size=" << size;
        }

        vector<uint8_t> scalarDecoded(input.size());
        const size_t scalarDecodedSize = base64::base64_scalar_decode(
            scalarEncoded.data(), encodedSize, scalarDecoded.data());
        scalarDecoded.resize(scalarDecodedSize);

        for (const SveDecodeFn decode :
             {base64::base64_sve_decode, base64::base64_sve_decode_naive}) {
            vector<uint8_t> sveDecoded(input.size());
            const size_t sveDecodedSize =
                decode(scalarEncoded.data(), encodedSize, sveDecoded.data());
            sveDecoded.resize(sveDecodedSize);
            EXPECT_EQ(scalarDecoded, sveDecoded) << "size=" << size;
        }
    }
}
#endif
