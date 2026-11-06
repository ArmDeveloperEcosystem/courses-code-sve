// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#pragma once

#include <string>

namespace runLengthDecode {
char *decodeScalarTwoPass(const char *encodedString);
char *decodeScalarSinglePass(const char *encodedString);
std::string decodeScalarSinglePass(const std::string &encodedString);

#ifdef __ARM_FEATURE_SVE
char *decodeSVETwoPass(const char *encodedString);
char *decodeSVESinglePass(const char *encodedString);
#endif // __ARM_FEATURE_SVE
} // namespace runLengthDecode
