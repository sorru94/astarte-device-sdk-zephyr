# (C) Copyright 2024, SECO Mind Srl
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

set(REQUIRED_BOARD "native_sim")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT BOARD STREQUAL REQUIRED_BOARD)
  message(FATAL_ERROR "End to end tests cannot run on any other board besides ${REQUIRED_BOARD}.")
endif()

set(COMMON_DIR ../samples/common)

include(${COMMON_DIR}/CommonConfig.cmake)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(e2e_test)

include(${COMMON_DIR}/CommonComponents.cmake)

# handle generating certificate inc file
include(certificate.cmake)

# add astarte private headers
target_include_directories(app PRIVATE ${CMAKE_SOURCE_DIR}/../lib/astarte_device_sdk/include)

# add sources
FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})
target_include_directories(app PRIVATE include)
