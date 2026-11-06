# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
# SPDX-License-Identifier: BSD-3-Clause-Clear

cmake_minimum_required(VERSION 3.10)

project(external LANGUAGES CXX)

# Get the functionality to configure, build and install external project
# from CMake module 'ExternalProject'.
include(ExternalProject)

# Use the same compiler, build type and installation directory than those
# from our caller.
set(EXTERNAL_PROJECT_CMAKE_ARGS
      -DCMAKE_CXX_COMPILER:PATH=${CMAKE_CXX_COMPILER}
      -DCMAKE_BUILD_TYPE:STRING=Release
      -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_INSTALL_PREFIX})

# Add 'googletest' as an external project, that will be cloned with git,
# from the official googletest repository, at version v1.17.0.
# We ask for a shallow clone, which is a clone with only the revision
# we are interested in rather than googletest's full history ---
# this makes the clone much faster (less data traffic), and uses much
# less disk space ; furthermore, as we are not developing googletest
# but just merely using it, we don't need the full history. It will be
# built and installed with our build configuration passed with CMAKE_ARGS.
ExternalProject_Add(googletest
    PREFIX "external"
    GIT_REPOSITORY "https://github.com/google/googletest"
    GIT_TAG "v1.17.0"
    GIT_SHALLOW TRUE
    CMAKE_GENERATOR "Ninja"
    CMAKE_ARGS ${EXTERNAL_PROJECT_CMAKE_ARGS}
)

# Add google/benchmark as an external project.
ExternalProject_Add(benchmark
    PREFIX "external"
    GIT_REPOSITORY "https://github.com/google/benchmark"
    GIT_TAG "v1.9.5"
    GIT_SHALLOW TRUE
    CMAKE_GENERATOR "Ninja"
    CMAKE_ARGS ${EXTERNAL_PROJECT_CMAKE_ARGS} -DGOOGLETEST_PATH:PATH=${CMAKE_BINARY_DIR}/external/src/googletest
)
