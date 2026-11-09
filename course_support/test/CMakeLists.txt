# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
# SPDX-License-Identifier: BSD-3-Clause-Clear

# ===================================================================
# Unit testing
# -------------------------------------------------------------------
get_property(ALL_TASKS_TEST_SOURCES GLOBAL PROPERTY STBC_TASK_TEST_SOURCES)
get_property(ALL_TASKS_LIBS GLOBAL PROPERTY STBC_TASK_LIBS)
get_property(ALL_TASKS GLOBAL PROPERTY STBC_TASKS)

add_executable(test main.cpp ${ALL_TASKS_TEST_SOURCES})
stbc_apply_source_defines(test STBC_TASK_TEST_SOURCE_DEFINES)

target_link_libraries(test GTest::gtest ${ALL_TASKS_LIBS})

add_custom_target(test-all
  COMMAND ${STBC_EMULATOR_COMMAND} $<TARGET_FILE:test> --gtest_color=yes --gtest_output=json:test-all.json
  DEPENDS test
)
foreach(TASK ${ALL_TASKS})
  add_custom_target(test-${TASK}
    COMMAND ${STBC_EMULATOR_COMMAND} $<TARGET_FILE:test> --gtest_color=yes --gtest_filter="${TASK}*"
    DEPENDS test
  )
endforeach()
