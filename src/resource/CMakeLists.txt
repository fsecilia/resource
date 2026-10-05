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
    canon_require_googletest()

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

    add_test(
        NAME resource.integration.install
        COMMAND
            "${CMAKE_COMMAND}"
            "-DRESOURCE_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            "-DRESOURCE_TEST_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/integration/install"
            "-DRESOURCE_GENERATOR=${CMAKE_GENERATOR}"
            "-DRESOURCE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
            -P "${PROJECT_SOURCE_DIR}/test/install.cmake"
    )
    set_tests_properties(resource.integration.install PROPERTIES LABELS integration TIMEOUT 120)
endif()

canon_add_documentation(src)
