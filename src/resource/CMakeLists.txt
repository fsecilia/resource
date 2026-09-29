# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia

add_library(Resource INTERFACE)
target_sources(
    Resource
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS "${CMAKE_SOURCE_DIR}/src"
        FILES resource.hpp
)
canon_apply_library(Resource)

include(CTest)
if (BUILD_TESTING)
    message(CHECK_START "finding googletest")
    if (NOT TARGET GTest::gmock_main)
        if (EXISTS "${CMAKE_SOURCE_DIR}/external/googletest/CMakeLists.txt")
            message(CHECK_PASS "using vendored googletest")
            add_subdirectory("${CMAKE_SOURCE_DIR}/external/googletest"
                "${CMAKE_BINARY_DIR}/external/googletest"
                EXCLUDE_FROM_ALL
            )
        else()
            message(CHECK_PASS "using host googletest")
            find_package(GTest "1.16.0...<2.0.0" CONFIG)
        endif()
    endif()
    if (NOT TARGET GTest::gmock_main)
        message(CHECK_FAIL "compatible system googletest not found")
        message(FATAL_ERROR
            "dependency 'googletest' unavailable; initialize submodule "
            "'external/googletest' or install a compatible host package"
        )
    endif()

    add_executable(ResourceTest)
    target_sources(ResourceTest PRIVATE
        resource_test.cpp
    )
    target_link_libraries(ResourceTest PRIVATE
        GTest::gmock
        GTest::gmock_main
    )
    canon_apply_executable(ResourceTest)
endif()

canon_add_documentation()