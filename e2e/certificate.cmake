# (C) Copyright 2024, SECO Mind Srl
#
# SPDX-License-Identifier: Apache-2.0

set(CERT_CONF_FILE ${APPLICATION_SOURCE_DIR}/CERTIFICATE)

if(NOT DEFINED APPLICATION_SOURCE_DIR OR NOT EXISTS ${CERT_CONF_FILE})
  return()
endif()

file(READ ${CERT_CONF_FILE} cert_conf)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CERT_CONF_FILE})

string(REGEX MATCH "CERTIFICATE_PATH = (.*)\n" _ ${cert_conf})
get_filename_component(certificate_file ${CMAKE_MATCH_1} ABSOLUTE)

if(EXISTS ${certificate_file})
  add_compile_definitions(CMAKE_CUSTOM_GENERATED_CERTIFICATE)

  set(gen_dir ${ZEPHYR_BINARY_DIR}/include/generated/)
  generate_inc_file_for_target(
      app
      ${certificate_file}
      ${gen_dir}/ca_certificate.inc)
else()
  message(WARNING "The specified path in CERTIFICATE: '${certificate_file}' does not exist, the include file won't be generated")
endif()
