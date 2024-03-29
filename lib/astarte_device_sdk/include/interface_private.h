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

/**
 * @brief Get the mapping corresponding to a path, if present in the interface.
 *
 * @param[in] interface Interface to use for the search.
 * @param[in] path Path to use to find the correct mapping.
 * @param[out] mapping Set to a pointer to the mapping if found.
 * @retval ASTARTE_RESULT_OK On success.
 * @retval ASTARTE_RESULT_MAPPING_NOT_IN_INTERFACE When the mapping is not found in the interface.
 * @retval ASTARTE_RESULT_INTERNAL_ERROR If an internal error took place.
 */
astarte_result_t astarte_interface_get_mapping(
    const astarte_interface_t *interface, const char *path, const astarte_mapping_t **mapping);

#ifdef __cplusplus
}
#endif

#endif // INTERFACE_PRIVATE_H
