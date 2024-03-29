/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MAPPING_PRIVATE_H
#define MAPPING_PRIVATE_H

/**
 * @file mapping_private.h
 * @brief Private methods for the Astarte interfaces.
 */

#include "astarte_device_sdk/mapping.h"
#include "astarte_device_sdk/result.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Check if a path corresponds to the endpoint of a mapping.
 *
 * @note A path is an endpoint with all parametrization substituted by real values.
 *
 * @param[in] mapping Mapping to use for the comparison.
 * @param[in] path Path to use for comparison.
 * @retval ASTARTE_RESULT_OK On success.
 * @retval ASTARTE_RESULT_MAPPING_PATH_MISMATCH When the path does not matches the mapping endpoint.
 * @retval ASTARTE_RESULT_INTERNAL_ERROR If an internal error took place.
 */
astarte_result_t astarte_mapping_check_path(astarte_mapping_t mapping, const char *path);

#ifdef __cplusplus
}
#endif

#endif // MAPPING_PRIVATE_H
