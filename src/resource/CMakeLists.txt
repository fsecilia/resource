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
    add_test(NAME resource.unit COMMAND ResourceTest)
    canon_apply_target(ResourceTest)
endif()
