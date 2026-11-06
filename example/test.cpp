// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "example.h"

#include "gtest/gtest.h"

TEST(example, add_one) {
  EXPECT_EQ(example::add_one(0), 1);
  EXPECT_EQ(example::add_one(41), 42);
  EXPECT_EQ(example::add_one(-2), -1);
}

TEST(example, foo) {
  EXPECT_FALSE(example::foo(true));
  EXPECT_TRUE(example::foo(false));
}
