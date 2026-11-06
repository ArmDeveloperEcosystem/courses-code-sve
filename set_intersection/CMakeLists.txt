# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
# SPDX-License-Identifier: BSD-3-Clause-Clear

stbc_add_task(
  LIB_SOURCES
    set_intersection.cpp
    set_intersection.h
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(vector_report_dir "${CMAKE_CURRENT_BINARY_DIR}/vector-report")
  set(vector_report_file "${vector_report_dir}/gcc-vectorization-report.txt")
  set(vector_report_object "${vector_report_dir}/set_intersection.cpp.o")

  add_custom_target(vector-report-set_intersection
    COMMAND ${CMAKE_COMMAND} -E make_directory "${vector_report_dir}"
    COMMAND ${CMAKE_COMMAND}
      "-DCOMPILER:FILEPATH=${CMAKE_CXX_COMPILER}"
      "-DSOURCE:FILEPATH=${CMAKE_CURRENT_SOURCE_DIR}/set_intersection.cpp"
      "-DINCLUDE_DIR:PATH=${CMAKE_CURRENT_SOURCE_DIR}"
      "-DCOURSE_INCLUDE_DIR:PATH=${PROJECT_SOURCE_DIR}/include"
      "-DOBJECT:FILEPATH=${vector_report_object}"
      "-DREPORT:FILEPATH=${vector_report_file}"
      "-DNATIVE_FLAG:STRING=${STBC_NATIVE_COMPILE_FLAG}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/generate_vector_report.cmake"
    BYPRODUCTS
      "${vector_report_file}"
      "${vector_report_object}"
    COMMENT "Generating the GCC set-intersection vectorization report"
    VERBATIM
  )
endif()
