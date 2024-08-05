# (C) Copyright 2024, SECO Mind Srl
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
project(Astarte-Device-SDK-Doc LANGUAGES)

set(MIN_WEST_VERSION 1.0.0)

set(Astarte_Device_SDK_VERSION 0.7.0)

message(STATUS "Zephyr base: ${ASTARTE_DEVICE_SDK_BASE}")
message(STATUS "Documentation tag: ${DOC_TAG}")

#-------------------------------------------------------------------------------
# Options

set(DOC_TAG "development" CACHE STRING "Documentation tag")

#-------------------------------------------------------------------------------
# Dependencies

find_package(Doxygen REQUIRED dot)

#-------------------------------------------------------------------------------
# Environment & Paths

set(DOCS_CFG_DIR ${CMAKE_CURRENT_LIST_DIR})
set(DOCS_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(DOCS_SRC_DIR ${CMAKE_CURRENT_BINARY_DIR}/src)
set(DOCS_HTML_DIR ${CMAKE_CURRENT_BINARY_DIR}/html)

if(WIN32)
  set(SEP $<SEMICOLON>)
else()
  set(SEP :)
endif()

#-------------------------------------------------------------------------------
# Doxygen (standalone)

set(DOXY_OUT ${CMAKE_CURRENT_BINARY_DIR}/doxygen)

if("${ASTARTE_DEVICE_SDK_EXTENDED_DOCS}" STREQUAL "yes")
message(STATUS "Generating extended documentation.")
set(DOXYFILE_IN ${CMAKE_CURRENT_LIST_DIR}/astarte-device-sdk.extended.doxyfile.in)
else()
set(DOXYFILE_IN ${CMAKE_CURRENT_LIST_DIR}/astarte-device-sdk.doxyfile.in)
endif()

set(DOXYFILE_OUT ${CMAKE_CURRENT_BINARY_DIR}/zephyr.doxyfile)
set(ASTARTE_DEVICE_SDK_VERSION "${Astarte_Device_SDK_VERSION}")

configure_file(${DOXYFILE_IN} ${DOXYFILE_OUT} @ONLY)

add_custom_target(
  doxygen
  COMMAND
    ${DOXYGEN_EXECUTABLE} ${DOXYFILE_OUT}
  COMMENT "Running Doxygen..."
)

set_target_properties(
  doxygen
  PROPERTIES
    ADDITIONAL_CLEAN_FILES "${DOXY_OUT}"
)
