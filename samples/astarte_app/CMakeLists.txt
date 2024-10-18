# (C) Copyright 2024, SECO Mind Srl
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# add the sample specific private configuration
include(${CMAKE_SOURCE_DIR}/Utils.cmake)
concat_if_exists(${CMAKE_SOURCE_DIR}/private.conf EXTRA_CONF_FILE)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(astarte_app)

# create source files list
FILE(GLOB app_common_sources ${CMAKE_CURRENT_LIST_DIR}/src/*.c)
target_sources(app PRIVATE ${app_common_sources})
target_include_directories(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)

# add generated sources and includes for the interfaces
FILE(GLOB app_interfaces_sources ${CMAKE_CURRENT_LIST_DIR}/interfaces/*.c)
target_sources(app PRIVATE ${app_interfaces_sources})
target_include_directories(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/interfaces)
