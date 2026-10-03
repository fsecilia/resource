# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia

foreach(_required_variable
    RESOURCE_SOURCE_DIR
    RESOURCE_TEST_BINARY_DIR
    RESOURCE_GENERATOR
    RESOURCE_CXX_COMPILER
)
    if (NOT DEFINED ${_required_variable} OR "${${_required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${_required_variable} is required")
    endif()
endforeach()

function(_resource_run DESCRIPTION)
    execute_process(
        COMMAND ${ARGN}
        RESULT_VARIABLE _result
        OUTPUT_VARIABLE _stdout
        ERROR_VARIABLE _stderr
    )

    if (NOT _result EQUAL 0)
        message(FATAL_ERROR
            "${DESCRIPTION} failed with exit code ${_result}\n"
            "stdout:\n${_stdout}\n"
            "stderr:\n${_stderr}"
        )
    endif()
endfunction()

file(REMOVE_RECURSE "${RESOURCE_TEST_BINARY_DIR}")
set(_resource_build_dir "${RESOURCE_TEST_BINARY_DIR}/resource-build")
set(_consumer_build_dir "${RESOURCE_TEST_BINARY_DIR}/consumer-build")
set(_install_prefix "${RESOURCE_TEST_BINARY_DIR}/install")

_resource_run(
    "Resource staging configure"
    "${CMAKE_COMMAND}"
    -S "${RESOURCE_SOURCE_DIR}"
    -B "${_resource_build_dir}"
    -G "${RESOURCE_GENERATOR}"
    "-DCMAKE_CXX_COMPILER=${RESOURCE_CXX_COMPILER}"
    -DCMAKE_BUILD_TYPE=Debug
    -DBUILD_TESTING=OFF
)
_resource_run(
    "Resource staging install"
    "${CMAKE_COMMAND}"
    --install "${_resource_build_dir}"
    --prefix "${_install_prefix}"
    --config Debug
)
_resource_run(
    "Resource installed consumer configure"
    "${CMAKE_COMMAND}"
    -S "${RESOURCE_SOURCE_DIR}/test/install-consumer"
    -B "${_consumer_build_dir}"
    -G "${RESOURCE_GENERATOR}"
    "-DCMAKE_CXX_COMPILER=${RESOURCE_CXX_COMPILER}"
    "-DCMAKE_PREFIX_PATH=${_install_prefix}"
    -DCMAKE_BUILD_TYPE=Debug
)
_resource_run(
    "Resource installed consumer build"
    "${CMAKE_COMMAND}"
    --build "${_consumer_build_dir}"
    --config Debug
)
_resource_run(
    "Resource installed consumer test"
    "${CMAKE_CTEST_COMMAND}"
    --test-dir "${_consumer_build_dir}"
    -C Debug
    --output-on-failure
)
