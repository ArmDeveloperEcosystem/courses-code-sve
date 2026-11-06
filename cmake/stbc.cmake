# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
# SPDX-License-Identifier: BSD-3-Clause-Clear

function(stbc_prepend_dir output_var dir)
  set(result)
  foreach(file IN ITEMS ${ARGN})
    cmake_path(ABSOLUTE_PATH file BASE_DIRECTORY "${dir}" NORMALIZE OUTPUT_VARIABLE full_path)
    list(APPEND result "${full_path}")
  endforeach()
  set(${output_var} "${result}" PARENT_SCOPE)
endfunction()

function(stbc_export_source_defines source_property)
  set(multiValueArgs "SOURCES;DEFINES")
  cmake_parse_arguments(ARG "" "" "${multiValueArgs}" ${ARGN})

  if(NOT ARG_DEFINES)
    return()
  endif()

  foreach(source IN LISTS ARG_SOURCES)
    string(MD5 source_key "${source}")
    set_property(GLOBAL APPEND PROPERTY ${source_property} "${source_key}")
    set_property(GLOBAL PROPERTY "STBC_TASK_SOURCE_${source_key}" "${source}")
    set_property(GLOBAL PROPERTY "STBC_TASK_SOURCE_DEFINES_${source_key}" "${ARG_DEFINES}")
  endforeach()
endfunction()

function(stbc_apply_source_defines target source_property)
  get_property(source_keys GLOBAL PROPERTY ${source_property})

  foreach(source_key IN LISTS source_keys)
    get_property(source GLOBAL PROPERTY "STBC_TASK_SOURCE_${source_key}")
    get_property(defines GLOBAL PROPERTY "STBC_TASK_SOURCE_DEFINES_${source_key}")

    if(defines)
      set_source_files_properties(
        "${source}"
        TARGET_DIRECTORY "${target}"
        PROPERTIES
        COMPILE_DEFINITIONS "${defines}"
      )
    endif()
  endforeach()
endfunction()

include(CheckCXXSourceCompiles)
function(stbc_check_aarch64_feature output_var feature_macro)
  check_cxx_source_compiles(
    "
    #if !defined(__aarch64__)
    #error Target must be AArch64
    #endif
    #if !defined(__ARM_FEATURE_${feature_macro})
    #error Target does not provide ${feature_macro}
    #endif
    #include <arm_sve.h>
    int main() {
      return 0;
    }
    "
    ${output_var})
endfunction()

function(stbc_add_task)
  set(task_dir "${CMAKE_CURRENT_SOURCE_DIR}")
  cmake_path(GET task_dir FILENAME task_name)

  set(multiValueArgs "LIB_SOURCES;ASM_SOURCES;TEST_SOURCES;BENCHMARK_SOURCES;DEFINES")
  cmake_parse_arguments(ARG "" "" "${multiValueArgs}" ${ARGN})

  # Build the library for the task.
  stbc_prepend_dir(LIB_SOURCES "${task_dir}" ${ARG_LIB_SOURCES})
  stbc_prepend_dir(ASM_SOURCES "${task_dir}" ${ARG_ASM_SOURCES})
  if(ASM_SOURCES)
    set_source_files_properties(${ASM_SOURCES} PROPERTIES LANGUAGE ASM)
  endif()
  add_library(lib${task_name} ${LIB_SOURCES} ${ASM_SOURCES})
  target_include_directories(lib${task_name} PRIVATE "${task_dir}")
  if(ARG_DEFINES)
    target_compile_definitions(lib${task_name} PRIVATE ${ARG_DEFINES})
  endif()

  # Build the main executable for the task.
  add_executable(${task_name} main.cpp)
  target_link_libraries(${task_name} lib${task_name})
  if(ARG_DEFINES)
    target_compile_definitions(${task_name} PRIVATE ${ARG_DEFINES})
  endif()

  # If no test sources are specified, default to test.cpp in the same directory as the task.
  if(NOT ARG_TEST_SOURCES)
    set(ARG_TEST_SOURCES "test.cpp")
  endif()

  # Add the task's test sources to testing.
  stbc_prepend_dir(TEST_SOURCES "${task_dir}" ${ARG_TEST_SOURCES})
  set_property(GLOBAL APPEND PROPERTY STBC_TASK_TEST_SOURCES ${TEST_SOURCES})
  stbc_export_source_defines(
    STBC_TASK_TEST_SOURCE_DEFINES
    SOURCES ${TEST_SOURCES}
    DEFINES ${ARG_DEFINES}
  )

  # If no benchmark sources are specified, default to benchmark.cpp in the same directory as the task.
  if(NOT ARG_BENCHMARK_SOURCES)
    set(ARG_BENCHMARK_SOURCES "benchmark.cpp")
  endif()

  # Add the task's benchmark sources to benchmarking.
  stbc_prepend_dir(BENCHMARK_SOURCES "${task_dir}" ${ARG_BENCHMARK_SOURCES})
  set_property(GLOBAL APPEND PROPERTY STBC_TASK_BENCHMARK_SOURCES ${BENCHMARK_SOURCES})
  stbc_export_source_defines(
    STBC_TASK_BENCHMARK_SOURCE_DEFINES
    SOURCES ${BENCHMARK_SOURCES}
    DEFINES ${ARG_DEFINES}
  )

  # Export the task's library for testing and benchmarking.
  set_property(GLOBAL APPEND PROPERTY STBC_TASK_LIBS lib${task_name})

  # Export the task's name.
  set_property(GLOBAL APPEND PROPERTY STBC_TASKS ${task_name})

endfunction()
