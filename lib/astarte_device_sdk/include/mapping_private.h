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
#include "astarte_device_sdk/value.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Convert an array mapping type to its corresponding scalar mapping type.
 *
 * @param[in] array_type Input mapping type. Has to be an array type.
 * @param[out] scalar_type The resulting scalar type.
 * @retval ASTARTE_RESULT_OK On success.
 * @retval ASTARTE_RESULT_INTERNAL_ERROR When the input type is not an array type or a mapping type.
 */
astarte_result_t astarte_mapping_array_to_scalar_type(
    astarte_mapping_type_t array_type, astarte_mapping_type_t *scalar_type);

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

/**
 * @brief Check if a value si compatible to the endpoint of a mapping.
 *
 * @param[in] mapping Mapping to use for the comparison.
 * @param[in] value Astarte value to use for comparison.
 * @retval ASTARTE_RESULT_OK On success.
 * @retval ASTARTE_RESULT_MAPPING_VALUE_INCOMPATIBLE When the value is not compatible with the
 * mapping.
 */
astarte_result_t astarte_mapping_check_value(
    const astarte_mapping_t *mapping, astarte_value_t value);

#ifdef __cplusplus
}
#endif

#endif // MAPPING_PRIVATE_H
