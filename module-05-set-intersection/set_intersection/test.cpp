// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "set_intersection.h"

#include <algorithm>
#include <string>
#include <vector>

#include "gtest/gtest.h"

using SetIntersection::Value;
using std::string;
using std::vector;

namespace {

struct IntersectionCase {
    const char *name;
    vector<Value> lhs;
    vector<Value> rhs;
    vector<Value> expected;
};

const auto intersection_cases = testing::Values(
    IntersectionCase{"empty_lhs", {}, {1, 2, 3}, {}},
    IntersectionCase{"empty_rhs", {1, 2, 3}, {}, {}},
    IntersectionCase{"no_overlap", {1, 3, 5, 7}, {2, 4, 6, 8}, {}},
    IntersectionCase{
        "all_overlap", {1, 4, 7, 10}, {1, 4, 7, 10}, {1, 4, 7, 10}},
    IntersectionCase{
        "lhs_smaller", {3, 8, 21}, {1, 3, 5, 8, 13, 21, 34}, {3, 8, 21}},
    IntersectionCase{
        "rhs_smaller", {1, 3, 5, 8, 13, 21, 34}, {3, 8, 21}, {3, 8, 21}},
    IntersectionCase{"sparse_matches",
                     {4, 9, 16, 31, 64, 127, 256},
                     {0, 4, 8, 12, 16, 20, 24, 64, 128, 256},
                     {4, 16, 64, 256}});

string testName(const testing::TestParamInfo<IntersectionCase> &info) {
    return info.param.name;
}

vector<Value> runScalar(const IntersectionCase &test_case) {
    vector<Value> output(std::min(test_case.lhs.size(), test_case.rhs.size()));
    const auto output_count = SetIntersection::intersect_scalar(
        test_case.lhs.data(), test_case.lhs.size(), test_case.rhs.data(),
        test_case.rhs.size(), output.data());
    output.resize(output_count);
    return output;
}

vector<Value> runRestrict(const IntersectionCase &test_case) {
    vector<Value> output(std::min(test_case.lhs.size(), test_case.rhs.size()));
    const auto output_count = SetIntersection::intersect_restrict(
        test_case.lhs.data(), test_case.lhs.size(), test_case.rhs.data(),
        test_case.rhs.size(), output.data());
    output.resize(output_count);
    return output;
}

class scalar : public testing::TestWithParam<IntersectionCase> {};
class restrict_hint : public testing::TestWithParam<IntersectionCase> {};
} // namespace

TEST_P(scalar, matches_expected_values) {
    const auto &test_case = GetParam();
    EXPECT_EQ(runScalar(test_case), test_case.expected);
}

INSTANTIATE_TEST_SUITE_P(set_intersection, scalar, intersection_cases,
                         testName);

TEST_P(restrict_hint, matches_expected_values) {
    const auto &test_case = GetParam();
    EXPECT_EQ(runRestrict(test_case), test_case.expected);
}

INSTANTIATE_TEST_SUITE_P(set_intersection, restrict_hint, intersection_cases,
                         testName);

#ifdef __ARM_FEATURE_SVE
namespace {
vector<Value> runSveProbe(const IntersectionCase &test_case) {
    vector<Value> output(std::min(test_case.lhs.size(), test_case.rhs.size()));
    const auto output_count = SetIntersection::intersect_sve_probe(
        test_case.lhs.data(), test_case.lhs.size(), test_case.rhs.data(),
        test_case.rhs.size(), output.data());
    output.resize(output_count);
    return output;
}

vector<Value> runSveJammed(const IntersectionCase &test_case) {
    vector<Value> output(std::min(test_case.lhs.size(), test_case.rhs.size()));
    const auto output_count = SetIntersection::intersect_sve_jammed(
        test_case.lhs.data(), test_case.lhs.size(), test_case.rhs.data(),
        test_case.rhs.size(), output.data());
    output.resize(output_count);
    return output;
}

class sve_probe : public testing::TestWithParam<IntersectionCase> {};

class sve_jammed : public testing::TestWithParam<IntersectionCase> {};
} // namespace

TEST_P(sve_probe, matches_expected_values) {
    const auto &test_case = GetParam();
    EXPECT_EQ(runSveProbe(test_case), test_case.expected);
}

INSTANTIATE_TEST_SUITE_P(set_intersection, sve_probe, intersection_cases,
                         testName);

TEST_P(sve_jammed, matches_expected_values) {
    const auto &test_case = GetParam();
    EXPECT_EQ(runSveJammed(test_case), test_case.expected);
}

INSTANTIATE_TEST_SUITE_P(set_intersection, sve_jammed, intersection_cases,
                         testName);
#endif
