# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia

cmake_minimum_required(VERSION 3.31.6)

project(Resource VERSION 0.1.0 LANGUAGES CXX)

if (PROJECT_IS_TOP_LEVEL)
    include(CTest)
endif()

message(CHECK_START "loading Canon")

if (COMMAND canon_apply_target)
    message(CHECK_PASS "already loaded")
elseif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/canon/CMakeLists.txt")
    add_subdirectory(external/canon EXCLUDE_FROM_ALL)
    message(CHECK_PASS "using vendored copy")
else()
    find_package(Canon 0.1 CONFIG REQUIRED)
    message(CHECK_PASS "using installed package")
endif()

add_subdirectory(src/resource)
