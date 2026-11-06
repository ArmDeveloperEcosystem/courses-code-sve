# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
# SPDX-License-Identifier: BSD-3-Clause-Clear

set(EXAMPLE_DEFINE
    "1"
    CACHE STRING "Integer value for the example task's EXAMPLE_DEFINE compile definition")

stbc_add_task(
  LIB_SOURCES
    example.cpp
    example.h
  ASM_SOURCES
    add_one.S
  DEFINES
    EXAMPLE_DEFINE=${EXAMPLE_DEFINE}
)
