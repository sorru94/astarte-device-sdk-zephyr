# (C) Copyright 2024, SECO Mind Srl
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
project(library-documentation LANGUAGES)

set(DOXYGEN_CMAKE_TARGET_NAME library_documentation)

#-------------------------------------------------------------------------------
# Project specific defaults

if(NOT DEFINED DOXYGEN_LIBRARY_NAME)
    set(DOXYGEN_LIBRARY_NAME "Astarte device API for Zephyr")
endif()

if(NOT DEFINED DOXYGEN_LIBRARY_VERSION)
    set(DOXYGEN_LIBRARY_VERSION "0.9.0")
endif()

if(NOT DEFINED DOXYGEN_LIBRARY_BRIEF)
    set(DOXYGEN_LIBRARY_BRIEF "Astarte device SDK for Zephyr RTOS")
endif()

#-------------------------------------------------------------------------------
# Options

message(STATUS "Library name: ${DOXYGEN_LIBRARY_NAME}")
message(STATUS "Library version: ${DOXYGEN_LIBRARY_VERSION}")
message(STATUS "Library brief: ${DOXYGEN_LIBRARY_BRIEF}")
message(STATUS "Library path: ${DOXYGEN_LIBRARY_PATH}")

#-------------------------------------------------------------------------------
# Dependencies

find_package(Doxygen REQUIRED dot)

#-------------------------------------------------------------------------------
# Doxygen

set(DOXY_OUT ${CMAKE_CURRENT_BINARY_DIR}/doxygen)

if("${EXTENDED_DOCS}" STREQUAL "yes")
  message(STATUS "Generating extended documentation.")
  set(DOXYFILE_IN ${CMAKE_CURRENT_LIST_DIR}/library.extended.doxyfile.in)
else()
  set(DOXYFILE_IN ${CMAKE_CURRENT_LIST_DIR}/library.doxyfile.in)
endif()

set(DOXYFILE_OUT ${CMAKE_CURRENT_BINARY_DIR}/zephyr.doxyfile)

configure_file(${DOXYFILE_IN} ${DOXYFILE_OUT} @ONLY)

add_custom_target(
  ${DOXYGEN_CMAKE_TARGET_NAME}
  COMMAND
    ${DOXYGEN_EXECUTABLE} ${DOXYFILE_OUT}
  COMMENT "Running Doxygen..."
)

set_target_properties(
  ${DOXYGEN_CMAKE_TARGET_NAME}
  PROPERTIES
    ADDITIONAL_CLEAN_FILES "${DOXY_OUT}"
)
