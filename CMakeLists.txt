# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
# SPDX-License-Identifier: BSD-3-Clause-Clear
cmake_minimum_required(VERSION 3.20)

project(SVE-task-based-course LANGUAGES C CXX ASM)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# ===================================================================
# Compiler version checks.
# This project requires C++20 and SVE support which need recent compilers.
# -------------------------------------------------------------------
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "13")
    message(FATAL_ERROR
      "GCC ${CMAKE_CXX_COMPILER_VERSION} is too old. "
      "This project requires GCC 13 or later for full C++20 and SVE support. "
      "Please install a newer GCC version (e.g. apt install g++-13).")
  endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "18")
    message(FATAL_ERROR
      "Clang ${CMAKE_CXX_COMPILER_VERSION} is too old. "
      "This project requires Clang 18 or later for full C++20 and SVE support. "
      "Please install a newer Clang version (e.g. apt install clang-18).")
  endif()
else()
  message(WARNING
    "Unsupported compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}. "
    "This project is tested with GCC >= 13 and Clang >= 18. "
    "Proceeding, but you may encounter build issues.")
endif()

# Cache builds by default if CCache is found. Do this as early as possible.
find_program(CCACHE_PROGRAM ccache)
set(ENABLE_CCACHE_BUILD ON CACHE BOOL "Set to OFF to disable ccache build")
if(ENABLE_CCACHE_BUILD)
  if(CCACHE_PROGRAM)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
  endif()
endif()

# ===================================================================
# Download, configure, build and install locally our external dependencies:
#   - google/googletest
#   - google/benchmark
# This is done once, at configuration time.
# -------------------------------------------------------------------

# Build CMake command line so that it will use the same CMake configuration than
# the one we have been invoked with (generator, compiler, build type, build directory)
set(EXTERNAL_PROJECT_CMAKE_ARGS
      -G ${CMAKE_GENERATOR}
      -DCMAKE_CXX_COMPILER:PATH=${CMAKE_CXX_COMPILER}
      -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_BINARY_DIR})

# Download and configure our external dependencies
execute_process(
  COMMAND ${CMAKE_COMMAND}
      -S ${CMAKE_SOURCE_DIR}/external
      -B ${CMAKE_BINARY_DIR}/external
      ${EXTERNAL_PROJECT_CMAKE_ARGS}
)

# Build our external dependencies.
execute_process(
  COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}/external
)

# Install our external dependencies.
execute_process(
  COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR}/external
)

# Import google/googletest package information (library names, paths, dependencies...)
set(GTest_DIR "${CMAKE_BINARY_DIR}/lib/cmake/GTest"
    CACHE PATH "Path to the googletest package configuration files")
find_package(GTest REQUIRED
  CONFIG
  NO_DEFAULT_PATH
  NO_PACKAGE_ROOT_PATH
  NO_SYSTEM_ENVIRONMENT_PATH
)

# Import google/benchmark package information (library names, paths, dependencies...)
set(benchmark_DIR "${CMAKE_BINARY_DIR}/lib/cmake/benchmark"
    CACHE PATH "Path to the google/benchmark package configuration files")
find_package(benchmark REQUIRED
  CONFIG
  NO_DEFAULT_PATH
  NO_PACKAGE_ROOT_PATH
  NO_SYSTEM_ENVIRONMENT_PATH
)

# ===================================================================
# Include the function to add a task, and the global properties to export
# task information to testing and benchmarking.
# -------------------------------------------------------------------
include(cmake/stbc.cmake)

# ===================================================================
# Cross-compilation option: build for SVE on a host that may not have SVE.
# When enabled, uses -march=armv8-a+sve+sve2 instead of -mcpu=native, and
# tests/benchmarks are run under QEMU user-mode emulation.
# -------------------------------------------------------------------
option(STBC_SVE_CROSS_COMPILE "Enable SVE cross-compilation (use -march=armv8-a+sve+sve2 and QEMU for execution)" OFF)

set(STBC_QEMU_EXECUTABLE "" CACHE FILEPATH
  "Path to qemu-aarch64 (or qemu-aarch64-static) for running SVE binaries on non-SVE hosts. Auto-detected if empty.")

# ===================================================================
# Feature test our platform's CPU.
# -------------------------------------------------------------------
include(CheckCXXCompilerFlag)

if(STBC_SVE_CROSS_COMPILE)
  # When cross-compiling for SVE, use an explicit architecture flag instead of -mcpu=native.
  set(STBC_NATIVE_COMPILE_FLAG "-march=armv8-a+sve+sve2")
  message(STATUS "SVE cross-compilation enabled: using ${STBC_NATIVE_COMPILE_FLAG}")

  # Find QEMU for running the built binaries.
  if(NOT STBC_QEMU_EXECUTABLE)
    find_program(_STBC_QEMU_FOUND NAMES qemu-aarch64 qemu-aarch64-static)
    if(_STBC_QEMU_FOUND)
      set(STBC_QEMU_EXECUTABLE "${_STBC_QEMU_FOUND}" CACHE FILEPATH
        "Path to qemu-aarch64 (or qemu-aarch64-static) for running SVE binaries on non-SVE hosts. Auto-detected if empty." FORCE)
    endif()
  endif()
  if(NOT STBC_QEMU_EXECUTABLE)
    message(FATAL_ERROR
      "STBC_SVE_CROSS_COMPILE is enabled but qemu-aarch64 was not found. "
      "Please install QEMU user-mode emulation (e.g. apt install qemu-user) "
      "or set -DSTBC_QEMU_EXECUTABLE=/path/to/qemu-aarch64.")
  endif()
  message(STATUS "QEMU emulator for test/benchmark execution: ${STBC_QEMU_EXECUTABLE}")

  # Set the emulator command prefix for use by test and benchmark targets.
  # SVE vector length is controlled at runtime via the STBC_SVE_VL environment
  # variable (in bits: 128, 256, 512, 1024, or 2048). See cmake/qemu-wrapper.sh.
  configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/qemu-wrapper.sh.in"
    "${CMAKE_BINARY_DIR}/qemu-wrapper.sh"
    @ONLY
    FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
  )
  set(STBC_EMULATOR_COMMAND "${CMAKE_BINARY_DIR}/qemu-wrapper.sh" CACHE INTERNAL "Command prefix for running SVE binaries")
else()
  set(STBC_NATIVE_COMPILE_FLAG "")
  foreach(NATIVE_FLAG IN ITEMS "-mcpu=native" "-march=native")
      string(MAKE_C_IDENTIFIER "${NATIVE_FLAG}" NATIVE_FLAG_IDENTIFIER)
      check_cxx_compiler_flag("${NATIVE_FLAG}" "STBC_HAS_${NATIVE_FLAG_IDENTIFIER}")
      if(STBC_HAS_${NATIVE_FLAG_IDENTIFIER})
        set(STBC_NATIVE_COMPILE_FLAG "${NATIVE_FLAG}")
        break()
      endif()
  endforeach()

  # No emulator needed when running natively.
  set(STBC_EMULATOR_COMMAND "" CACHE INTERNAL "Command prefix for running SVE binaries")
endif()

set(STBC_REQUIRED_FLAGS_SAVE "${CMAKE_REQUIRED_FLAGS}")
if(STBC_NATIVE_COMPILE_FLAG)
  set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${STBC_NATIVE_COMPILE_FLAG}")
endif()
stbc_check_aarch64_feature(STBC_PLATFORM_HAS_SVE "SVE")
stbc_check_aarch64_feature(STBC_PLATFORM_HAS_SVE2 "SVE2")
stbc_check_aarch64_feature(STBC_PLATFORM_HAS_SVE2P1 "SVE2p1")
stbc_check_aarch64_feature(STBC_PLATFORM_HAS_SVE2P2 "SVE2p2")
set(CMAKE_REQUIRED_FLAGS "${STBC_REQUIRED_FLAGS_SAVE}")

if(NOT STBC_PLATFORM_HAS_SVE AND NOT STBC_SVE_CROSS_COMPILE)
  message(WARNING "Platform does not have SVE support - some tasks may not build or run correctly")
endif()

include_directories("${CMAKE_SOURCE_DIR}/include")
add_compile_options(-Wall)

if(STBC_NATIVE_COMPILE_FLAG)
  message(STATUS "Using compile flag: ${STBC_NATIVE_COMPILE_FLAG}")
  add_compile_options(
    $<$<OR:$<COMPILE_LANGUAGE:CXX>,$<COMPILE_LANGUAGE:C>,$<COMPILE_LANGUAGE:ASM>>:${STBC_NATIVE_COMPILE_FLAG}>
  )
endif()

# ===================================================================
# Add task based examples here.
# -------------------------------------------------------------------
add_subdirectory(example)
add_subdirectory(JSON_sci)
add_subdirectory(set_intersection)
add_subdirectory(VSS)
add_subdirectory(histogram)
add_subdirectory(sve_match)
add_subdirectory(bloom_filter)
add_subdirectory(match_and_visit)
add_subdirectory(base64)
add_subdirectory(strlen)
add_subdirectory(run_length_decode)
add_subdirectory(vector_examples)

# ===================================================================
# Unit testing & benchmarking --- must come last.
# -------------------------------------------------------------------
add_subdirectory(test)
add_subdirectory(benchmark)
