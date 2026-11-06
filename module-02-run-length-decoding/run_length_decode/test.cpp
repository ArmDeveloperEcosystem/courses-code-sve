// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "run_length_decoding.h"

#include "gtest/gtest.h"

#include <memory>
#include <string>

using std::string;

TEST(run_length_decode_scalar,
     scalarCharPtrTwoPassHandlesValidEmptyAndNullInputs) {
    const std::unique_ptr<char[]> decoded{
        runLengthDecode::decodeScalarTwoPass("3a12b1c5d")};
    EXPECT_STREQ("aaabbbbbbbbbbbbcddddd", decoded.get());

    const std::unique_ptr<char[]> emptyDecoded{
        runLengthDecode::decodeScalarTwoPass("")};
    EXPECT_STREQ("", emptyDecoded.get())
        << "failed to parse empty string" << '\n';

    const std::unique_ptr<char[]> nullDecoded{
        runLengthDecode::decodeScalarTwoPass(nullptr)};
    EXPECT_STREQ("", nullDecoded.get()) << "failed to parse null input" << '\n';
}

TEST(run_length_decode_scalar,
     scalarCharPtrSinglePassHandlesValidEmptyAndNullInputs) {
    const std::unique_ptr<char[]> decoded{
        runLengthDecode::decodeScalarSinglePass("3a12b1c5d")};
    EXPECT_STREQ("aaabbbbbbbbbbbbcddddd", decoded.get());

    const std::unique_ptr<char[]> emptyDecoded{
        runLengthDecode::decodeScalarSinglePass("")};
    EXPECT_STREQ("", emptyDecoded.get())
        << "failed to parse empty string" << '\n';

    const std::unique_ptr<char[]> nullDecoded{
        runLengthDecode::decodeScalarSinglePass(nullptr)};
    EXPECT_STREQ("", nullDecoded.get()) << "failed to parse null input" << '\n';
}

TEST(run_length_decode_scalar,
     scalarStdStringSinglePassHandlesValidAndEmptyInputs) {
    EXPECT_EQ("aaabbbbbbbbbbbbcddddd",
              runLengthDecode::decodeScalarSinglePass(string{"3a12b1c5d"}));
    EXPECT_EQ("", runLengthDecode::decodeScalarSinglePass(string{}))
        << "failed to parse empty string" << '\n';
}

#ifdef __ARM_FEATURE_SVE
TEST(run_length_decode_sve, sveCharPtrTwoPassHandlesValidEmptyAndNullInputs) {
    const std::unique_ptr<char[]> decoded{
        runLengthDecode::decodeSVETwoPass("3a12b1c5d")};
    EXPECT_STREQ("aaabbbbbbbbbbbbcddddd", decoded.get());

    const std::unique_ptr<char[]> emptyDecoded{
        runLengthDecode::decodeSVETwoPass("")};
    EXPECT_STREQ("", emptyDecoded.get())
        << "failed to parse empty string" << '\n';

    const std::unique_ptr<char[]> nullDecoded{
        runLengthDecode::decodeSVETwoPass(nullptr)};
    EXPECT_STREQ("", nullDecoded.get()) << "failed to parse null input" << '\n';
}

TEST(run_length_decode_sve,
     sveCharPtrSinglePassHandlesValidEmptyAndNullInputs) {
    const std::unique_ptr<char[]> decoded{
        runLengthDecode::decodeSVESinglePass("3a12b1c5d")};
    EXPECT_STREQ("aaabbbbbbbbbbbbcddddd", decoded.get());

    const std::unique_ptr<char[]> emptyDecoded{
        runLengthDecode::decodeSVESinglePass("")};
    EXPECT_STREQ("", emptyDecoded.get())
        << "failed to parse empty string" << '\n';

    const std::unique_ptr<char[]> nullDecoded{
        runLengthDecode::decodeSVESinglePass(nullptr)};
    EXPECT_STREQ("", nullDecoded.get()) << "failed to parse null input" << '\n';
}
#endif // __ARM_FEATURE_SVE
