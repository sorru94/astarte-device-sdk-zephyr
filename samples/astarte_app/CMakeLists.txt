# (C) Copyright 2024, SECO Mind Srl
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

function(concat_if_exists FILE_PATH STRING_NAME)
    # Check if the file exists
    file(GLOB FILE_TO_ADD ${FILE_PATH})

    # If the file exists, add it to the list
    if(EXISTS ${FILE_TO_ADD})
        set(${STRING_NAME} "${${STRING_NAME}};${FILE_PATH}" PARENT_SCOPE)
    endif()
endfunction()

# add the sample specific private configuration
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
