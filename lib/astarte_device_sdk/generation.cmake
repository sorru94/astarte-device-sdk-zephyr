# (C) Copyright 2024, SECO Mind Srl
#
# SPDX-License-Identifier: Apache-2.0

set(CODE_GENERATION_VAR "CONFIG_ASTARTE_DEVICE_SDK_ADVANCED_CODE_GENERATION")
set(INTERFACE_PATH_VAR "CONFIG_ASTARTE_DEVICE_SDK_ADVANCED_CODE_GENERATION_INTERFACE_DIRECTORY")

# ${INTERFACE_PATH_VAR} depends on ${CODE_GENERATION_VAR}, if the first one is defined then both are defined.

if(NOT DEFINED ${INTERFACE_PATH_VAR} OR "${${INTERFACE_PATH_VAR}}" STREQUAL "")
  message(STATUS "The interfaces directory variable '${INTERFACE_PATH_VAR}' is not defined, no interface will be generated")
  return()
endif()

# Get the absolute path by concatenating "CMAKE_SOURCE_DIR" (path of user dir) to the user variable
get_filename_component(interfaces_dir ${CMAKE_SOURCE_DIR}/${${INTERFACE_PATH_VAR}} ABSOLUTE)

if(NOT EXISTS ${interfaces_dir})
  message(SEND_ERROR "The path '${interfaces_dir}' specified in '${INTERFACE_PATH_VAR}' does not exist")
  return()
endif()

if(NOT IS_DIRECTORY ${interfaces_dir})
  message(SEND_ERROR "The path '${interfaces_dir}' specified in '${INTERFACE_PATH_VAR}' is not a valid directory")
  return()
endif()

# actual generation of the interfaces
set(output_directory ${ZEPHYR_BINARY_DIR}/include/generated/)
set(output_file_prefix "astarte_")
set(output_file_name "${output_file_prefix}generated_interfaces")
set(interfaces_output_source ${output_directory}/${output_file_name}.c)
set(interfaces_output_files ${output_directory}/${output_file_name}.h ${interfaces_output_source})

# Add custom target that depends on the output files of the command
add_custom_target(astarte_generate_interfaces DEPENDS ${interfaces_output_files})

# Add a custom command that generates the output files and depends on the interfaces directory defined by the user
add_custom_command(
  OUTPUT ${interfaces_output_files}
  COMMAND
  west generate-interfaces
  -d ${output_directory}
  -p ${output_file_prefix}
  ${interfaces_dir}
  DEPENDS ${interfaces_dir}
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )

# Add the generated source file to the app target
target_sources(app PRIVATE ${interfaces_output_source})

# add astarte_generate_interfaces target as a dependency of the app target
add_dependencies(app astarte_generate_interfaces)
