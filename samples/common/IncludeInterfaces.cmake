# (C) Copyright 2024, SECO Mind Srl
#
# SPDX-License-Identifier: Apache-2.0

# add generated sources and includes for the interfaces
FILE(GLOB app_interfaces_sources ${CMAKE_CURRENT_LIST_DIR}/interfaces/*.c)
target_sources(app PRIVATE ${app_interfaces_sources})
target_include_directories(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/interfaces)
