/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef INTERFACE_PRIVATE_H
#define INTERFACE_PRIVATE_H

/**
 * @file interface_private.h
 * @brief Private methods for the Astarte interfaces.
 */

#include "astarte_device_sdk/interface.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Validate an Astarte interface.
 *
 * @param[in] interface Interface to validate.
 * @return ASTARTE_RESULT_OK on success, otherwise an error is returned.
 */
astarte_result_t astarte_interface_validate(const astarte_interface_t *interface);

#ifdef __cplusplus
}
#endif

#endif // INTERFACE_PRIVATE_H
