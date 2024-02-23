# (C) Copyright 2024, SECO Mind Srl
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(register)

FILE(GLOB app_sources src/*.c)
if(CONFIG_BOARD_NATIVE_SIM)
    LIST(REMOVE_ITEM app_sources ${CMAKE_SOURCE_DIR}/src/wifi.c)
endif()
target_sources(app PRIVATE ${app_sources})
target_include_directories(app PRIVATE include)
