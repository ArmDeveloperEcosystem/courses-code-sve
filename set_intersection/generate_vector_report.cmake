# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
# SPDX-License-Identifier: BSD-3-Clause-Clear

foreach(required_variable IN ITEMS COMPILER SOURCE INCLUDE_DIR COURSE_INCLUDE_DIR
                                   OBJECT REPORT)
  if(NOT DEFINED ${required_variable})
    message(FATAL_ERROR "Missing required variable: ${required_variable}")
  endif()
endforeach()

set(compile_command
  "${COMPILER}"
  -std=gnu++20
  -O3
  -Wall
  -fdiagnostics-color=never
  -fopt-info-vec-optimized-missed
  "-I${INCLUDE_DIR}"
  "-I${COURSE_INCLUDE_DIR}"
)
if(NATIVE_FLAG)
  list(APPEND compile_command "${NATIVE_FLAG}")
endif()
list(APPEND compile_command -c "${SOURCE}" -o "${OBJECT}")

execute_process(
  COMMAND ${compile_command}
  RESULT_VARIABLE compile_result
  OUTPUT_VARIABLE compiler_output
  ERROR_VARIABLE vector_report
)

string(JOIN " " printable_command ${compile_command})
file(WRITE "${REPORT}"
  "GCC set-intersection vectorization report\n"
  "Command: ${printable_command}\n\n"
  "${vector_report}"
)

if(compiler_output)
  file(APPEND "${REPORT}" "\nCompiler standard output:\n${compiler_output}")
endif()

if(NOT compile_result EQUAL 0)
  message(FATAL_ERROR
    "GCC vector-report compilation failed with exit code ${compile_result}. "
    "See ${REPORT}"
  )
endif()

message(STATUS "Wrote GCC vectorization report to ${REPORT}")
