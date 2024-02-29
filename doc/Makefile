# (C) Copyright 2024, SECO Mind Srl
#
# SPDX-License-Identifier: Apache-2.0

# ------------------------------------------------------------------------------
# Configuration options

BUILDDIR ?= _build
DOC_TAG ?= development
ASTARTE_DEVICE_SDK_BASE ?=

# ------------------------------------------------------------------------------
# Documentation targets

.PHONY: configure clean doxygen

doxygen: configure
	cmake --build ${BUILDDIR} --target $@

configure:
	cmake \
		-GNinja \
		-B${BUILDDIR} \
		-S. \
		-DDOC_TAG=${DOC_TAG} \
		-DASTARTE_DEVICE_SDK_BASE=${ASTARTE_DEVICE_SDK_BASE} \
		-DASTARTE_DEVICE_SDK_EXTENDED_DOCS=${ASTARTE_DEVICE_SDK_EXTENDED_DOCS}

clean:
	cmake --build ${BUILDDIR} --target clean
