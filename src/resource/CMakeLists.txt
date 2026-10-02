# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia

add_library(Resource INTERFACE)
add_library(Resource::Resource ALIAS Resource)
target_sources(
    Resource
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS "${PROJECT_SOURCE_DIR}/src"
        FILES resource.hpp
)
canon_apply_library(Resource)

if (PROJECT_IS_TOP_LEVEL AND BUILD_TESTING)
    message(CHECK_START "finding googletest")
    if (NOT TARGET GTest::gtest_main)
        if (EXISTS "${PROJECT_SOURCE_DIR}/external/googletest/CMakeLists.txt")
            message(CHECK_PASS "using vendored googletest")
            add_subdirectory(
                "${PROJECT_SOURCE_DIR}/external/googletest"
                "${PROJECT_BINARY_DIR}/external/googletest"
                EXCLUDE_FROM_ALL
            )
        else()
            message(CHECK_PASS "using host googletest")
            find_package(GTest "1.16.0...<2.0.0" CONFIG)
        endif()
    endif()
    if (NOT TARGET GTest::gtest_main)
        message(CHECK_FAIL "compatible system googletest not found")
        message(FATAL_ERROR
            "dependency 'googletest' unavailable; initialize submodule "
            "'external/googletest' or install a compatible host package"
        )
    endif()

    add_executable(ResourceTest)
    set_target_properties(ResourceTest PROPERTIES OUTPUT_NAME resource_test)
    target_sources(ResourceTest PRIVATE
        resource_test.cpp
    )
    target_link_libraries(ResourceTest PRIVATE
        Resource::Resource
        GTest::gtest_main
    )
    add_test(NAME ResourceTest COMMAND ResourceTest)
    canon_apply_target(ResourceTest)

    function(_resource_expect_compile_failure NAME SOURCE)
        if (ARGC LESS 3)
            message(FATAL_ERROR
                "compile-fail fixture '${NAME}' requires an expected diagnostic"
            )
        endif()

        get_target_property(_cxx_standard ResourceTest CXX_STANDARD)
        get_target_property(
            _cxx_standard_required
            ResourceTest
            CXX_STANDARD_REQUIRED
        )

        set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

        try_compile(
            _compiled
            SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE}"
            NO_CACHE
            CMAKE_FLAGS
                "-DCMAKE_CXX_SCAN_FOR_MODULES=OFF"
            CXX_STANDARD "${_cxx_standard}"
            CXX_STANDARD_REQUIRED "${_cxx_standard_required}"
            OUTPUT_VARIABLE _output
        )

        if (_compiled)
            message(FATAL_ERROR
                "compile-fail fixture '${NAME}' compiled successfully:\n${_output}"
            )
        endif()

        foreach(_expected IN LISTS ARGN)
            string(FIND "${_output}" "${_expected}" _expected_index)
            if (_expected_index EQUAL -1)
                message(FATAL_ERROR
                    "compile-fail fixture '${NAME}' failed for an unexpected reason; "
                    "expected diagnostic fragment '${_expected}':\n${_output}"
                )
            endif()
        endforeach()
    endfunction()

    set_property(
        DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/resource.hpp"
    )

    _resource_expect_compile_failure(
        default-sentinel-fundamental
        default_sentinel_fundamental_compile_fail.cpp
        DefaultSentinel
        "constraints not satisfied"
    )
    _resource_expect_compile_failure(
        default-sentinel-pointer
        default_sentinel_pointer_compile_fail.cpp
        DefaultSentinel
        "constraints not satisfied"
    )
    _resource_expect_compile_failure(
        default-sentinel-late
        default_sentinel_late_compile_fail.cpp
        DefaultSentinel
        "after instantiation"
    )
    _resource_expect_compile_failure(
        sentinel-narrowing
        sentinel_narrowing_compile_fail.cpp
        Sentinel
        "constraints not satisfied"
    )
endif()

canon_add_documentation()
