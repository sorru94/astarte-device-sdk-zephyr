# (C) Copyright 2024, SECO Mind Srl
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr COMPONENTS unittest REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(astarte_device_sdk_unit_individual)

target_include_directories(testbinary PRIVATE
    ${ZEPHYR_BASE}/../astarte-device-sdk-zephyr/include
    ${ZEPHYR_BASE}/../astarte-device-sdk-zephyr
    ${ZEPHYR_BASE}/../astarte-device-sdk-zephyr/lib/astarte_device_sdk/include
)

target_sources(testbinary PRIVATE
    ${ZEPHYR_BASE}/../astarte-device-sdk-zephyr/lib/astarte_device_sdk/interface.c
    ${ZEPHYR_BASE}/../astarte-device-sdk-zephyr/lib/astarte_device_sdk/mapping.c
    ${ZEPHYR_BASE}/../astarte-device-sdk-zephyr/lib/astarte_device_sdk/bson_deserializer.c
    ${ZEPHYR_BASE}/../astarte-device-sdk-zephyr/lib/astarte_device_sdk/bson_serializer.c
    ${ZEPHYR_BASE}/../astarte-device-sdk-zephyr/lib/astarte_device_sdk/result.c
)


FILE(GLOB test_sources src/*.c)
target_sources(testbinary PRIVATE ${test_sources})
