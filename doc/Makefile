# (C) Copyright 2024, SECO Mind Srl
#
# SPDX-License-Identifier: Apache-2.0

# ------------------------------------------------------------------------------
# Configuration options

BUILDDIR ?= _build
LIBRARY_PATH ?=
DOXYGEN_LIBRARY_NAME ?= "Astarte device API for Zephyr"
DOXYGEN_LIBRARY_VERSION ?= "0.9.0"
DOXYGEN_LIBRARY_BRIEF ?= "Astarte device SDK for Zephyr RTOS"

# ------------------------------------------------------------------------------
# Documentation targets

.PHONY: configure clean doxygen

doxygen: configure
	cmake --build ${BUILDDIR} --target library_documentation

configure:
	cmake \
		-GNinja \
		-B${BUILDDIR} \
		-S. \
		-DLIBRARY_PATH=${LIBRARY_PATH} \
		-DDOXYGEN_LIBRARY_NAME=${DOXYGEN_LIBRARY_NAME} \
		-DDOXYGEN_LIBRARY_VERSION=${DOXYGEN_LIBRARY_VERSION} \
		-DDOXYGEN_LIBRARY_BRIEF=${DOXYGEN_LIBRARY_BRIEF} \
		-DEXTENDED_DOCS=${EXTENDED_DOCS}

clean:
	cmake --build ${BUILDDIR} --target clean
