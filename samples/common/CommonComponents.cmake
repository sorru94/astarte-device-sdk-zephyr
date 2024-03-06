# (C) Copyright 2024, SECO Mind Srl
#
# SPDX-License-Identifier: Apache-2.0

# create source files list
FILE(GLOB app_common_sources ${CMAKE_CURRENT_LIST_DIR}/src/*.c)
if(NOT CONFIG_WIFI)
    LIST(REMOVE_ITEM app_common_sources ${CMAKE_CURRENT_LIST_DIR}/src/wifi.c)
endif()

# add sources and includes
target_sources(app PRIVATE ${app_common_sources})
target_include_directories(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)
