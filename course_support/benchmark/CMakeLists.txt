# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
# SPDX-License-Identifier: BSD-3-Clause-Clear

# ===================================================================
# Benchmarking
# -------------------------------------------------------------------
get_property(ALL_TASKS_BENCHMARK_SOURCES GLOBAL PROPERTY STBC_TASK_BENCHMARK_SOURCES)
get_property(ALL_TASKS_LIBS GLOBAL PROPERTY STBC_TASK_LIBS)
get_property(ALL_TASKS GLOBAL PROPERTY STBC_TASKS)

add_executable(benchmark
  main.cpp
  ${ALL_TASKS_BENCHMARK_SOURCES}
)
stbc_apply_source_defines(benchmark STBC_TASK_BENCHMARK_SOURCE_DEFINES)

target_compile_options(benchmark PRIVATE
  $<$<COMPILE_LANGUAGE:CXX>:-Wall>
)

target_link_libraries(benchmark benchmark::benchmark ${ALL_TASKS_LIBS})

add_custom_target(benchmark-all
   COMMAND ${STBC_EMULATOR_COMMAND} benchmark --benchmark_color=true --benchmark_out=benchmark-all.json --benchmark_out_format=json
   DEPENDS benchmark
)
foreach(TASK ${ALL_TASKS})
  add_custom_target(benchmark-${TASK}
    COMMAND ${STBC_EMULATOR_COMMAND} benchmark --benchmark_color=true --benchmark_filter="${TASK}_.*"
    DEPENDS benchmark
  )
endforeach()
