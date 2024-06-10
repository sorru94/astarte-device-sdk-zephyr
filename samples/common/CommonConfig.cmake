# (C) Copyright 2024, SECO Mind Srl
#
# SPDX-License-Identifier: Apache-2.0

include(${CMAKE_CURRENT_LIST_DIR}/Utils.cmake)

# add common condifuration file
list(APPEND EXTRA_CONF_FILE ${CMAKE_CURRENT_LIST_DIR}/prj.conf)
# add private configuration for common modules
concat_if_exists(${CMAKE_CURRENT_LIST_DIR}/private.conf EXTRA_CONF_FILE)
# add also the sample specific private configuration
concat_if_exists(${CMAKE_SOURCE_DIR}/private.conf EXTRA_CONF_FILE)

# conditional add specific board overlays
concat_if_exists(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}.overlay EXTRA_DTC_OVERLAY_FILE)

# conditional add specific board configuration file
concat_if_exists(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}.conf EXTRA_CONF_FILE)

include(${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}.cmake OPTIONAL)
