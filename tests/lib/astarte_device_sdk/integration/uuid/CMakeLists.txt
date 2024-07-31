# (C) Copyright 2024, SECO Mind Srl
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(astarte_device_sdk_integration_uuid)

target_include_directories(app PRIVATE
    ${ZEPHYR_BASE}/../astarte-device-sdk-zephyr/lib/astarte_device_sdk/include
)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})
