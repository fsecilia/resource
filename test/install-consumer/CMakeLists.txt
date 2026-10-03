# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Frank Secilia

cmake_minimum_required(VERSION 3.31.6)

project(ResourceInstallConsumer LANGUAGES CXX)

find_package(Resource 0.1 CONFIG REQUIRED)

add_executable(resource_install_consumer main.cpp)
set_property(TARGET resource_install_consumer PROPERTY CXX_SCAN_FOR_MODULES FALSE)
target_link_libraries(resource_install_consumer PRIVATE Resource::Resource)

enable_testing()
add_test(NAME resource_install_consumer COMMAND resource_install_consumer)
