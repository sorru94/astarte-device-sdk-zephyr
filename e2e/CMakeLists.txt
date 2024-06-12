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

include(${COMMON_DIR}/Utils.cmake)

# the order of this operations reflect their priority
# properties added later have higher priority
list(APPEND EXTRA_CONF_FILE ${COMMON_DIR}/prj.conf)
# add private conf file
concat_if_exists(private.conf EXTRA_CONF_FILE)
# add native_sim board overlay from the sample directory
concat_if_exists(${COMMON_DIR}/boards/${REQUIRED_BOARD}.conf EXTRA_CONF_FILE)
concat_if_exists(${COMMON_DIR}/boards/${REQUIRED_BOARD}.overlay EXTRA_DTC_OVERLAY_FILE)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(e2e_test)

# handle generating certificate inc file
include(certificate.cmake)

# add common source files
set(app_common_sources ${COMMON_DIR}/src/utils.c ${COMMON_DIR}/src/eth.c)
target_sources(app PRIVATE ${app_common_sources})
target_include_directories(app PRIVATE ${COMMON_DIR}/include)

# add generated interfaces file
FILE(GLOB app_interfaces_sources interfaces/*.c)
target_sources(app PRIVATE ${app_interfaces_sources})
target_include_directories(app PRIVATE interfaces)

# add astarte private headers
target_include_directories(app PRIVATE ${CMAKE_SOURCE_DIR}/../lib/astarte_device_sdk/include)

# add sources
FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})
target_include_directories(app PRIVATE include)
