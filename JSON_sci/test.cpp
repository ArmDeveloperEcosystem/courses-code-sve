// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "JSON.h"

#include <string>
#include <vector>

#include "gtest/gtest.h"

using std::string;
using std::vector;

using IndicesTy = JSON::SCI::IndicesTy;

namespace {

struct SciCase {
    const char *name;
    const string json;
    const IndicesTy expected_indices;
};

const auto sci_cases = testing::Values(
    SciCase{.name = "empty", .json = "", .expected_indices = {}},
    SciCase{.name = "all_structural",
            .json = "{},:[]",
            .expected_indices = {0, 1, 2, 3, 4, 5}},
    SciCase{.name = "with_strings",
            .json = R"({"key": "{}[]:,\"", "arr": [1, 2, 3]})",
            .expected_indices = {0, 6, 18, 25, 27, 29, 32, 35, 36}},
    SciCase{.name = "with_strings_esc",
            .json = R"({"ke\"y": "val\}ue"})",
            .expected_indices = {0, 8, 19}},
    SciCase{.name = "with_backslash_parity",
            .json = R"({"even":"\\","odd":"\"","after":[1]})",
            .expected_indices = {0, 7, 12, 18, 23, 31, 32, 34, 35}});

string test_name(const testing::TestParamInfo<SciCase> &info) {
    return info.param.name;
}

class reference : public testing::TestWithParam<SciCase> {};

TEST_P(reference, matches_expected_indices) {
    const auto &test_case = GetParam();
    EXPECT_EQ(JSON::SCI(test_case.json).get_ref(), test_case.expected_indices);
}

INSTANTIATE_TEST_SUITE_P(JSON_sci, reference, sci_cases, test_name);

#ifdef __ARM_FEATURE_SVE
class sve : public testing::TestWithParam<SciCase> {};

TEST_P(sve, matches_expected_indices) {
    const auto &test_case = GetParam();
    EXPECT_EQ(JSON::SCI(test_case.json).get_sve(), test_case.expected_indices);
}

INSTANTIATE_TEST_SUITE_P(JSON_sci, sve, sci_cases, test_name);
#endif // __ARM_FEATURE_SVE

#ifdef __ARM_FEATURE_SVE2
class sve2 : public testing::TestWithParam<SciCase> {};

TEST_P(sve2, matches_expected_indices) {
    const auto &test_case = GetParam();
    EXPECT_EQ(JSON::SCI(test_case.json).get_sve2(), test_case.expected_indices);
}

INSTANTIATE_TEST_SUITE_P(JSON_sci, sve2, sci_cases, test_name);
#endif // __ARM_FEATURE_SVE2

#ifdef __ARM_FEATURE_SVE2p2
class sve2p2 : public testing::TestWithParam<SciCase> {};

TEST_P(sve2p2, matches_expected_indices) {
    const auto &test_case = GetParam();
    EXPECT_EQ(JSON::SCI(test_case.json).get_sve2p2(),
              test_case.expected_indices);
}

INSTANTIATE_TEST_SUITE_P(JSON_sci, sve2p2, sci_cases, test_name);
#endif // __ARM_FEATURE_SVE2p2

} // namespace
