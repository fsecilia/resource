# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia

cmake_minimum_required(VERSION 3.31.6)

project(Resource VERSION 0.1.0 LANGUAGES CXX)

if (NOT COMMAND canon_apply_target)
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/canon/CMakeLists.txt")
        add_subdirectory(external/canon EXCLUDE_FROM_ALL)
    else()
        find_package(Canon 0.1 CONFIG REQUIRED)
    endif()
endif()

add_subdirectory(src/resource)
