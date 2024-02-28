# (C) Copyright 2024, SECO Mind Srl
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

include(../common/CommonConfig.cmake)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sample_aggregates)

include(../common/CommonComponents.cmake)

FILE(GLOB app_sources src/*.c)

target_sources(app PRIVATE ${app_sources})
target_include_directories(app PRIVATE include)
