/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DATA_VALIDATION_H
#define DATA_VALIDATION_H

/**
 * @file data_validation.h
 * @brief Utility functions used to validate data and timestamps in transmission and reception.
 */

#include "astarte_device_sdk/astarte.h"
#include "astarte_device_sdk/result.h"
#include "astarte_device_sdk/value.h"

#include "interface_private.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Validate data for an individual datastream against the device introspection.
 *
 * @param[in] interface Interface to use for the operation.
 * @param[in] path Path to validate.
 * @param[in] value Astarte value to validate.
 * @param[in] timestamp Timestamp to validate, it might be NULL.
 * @return ASTARTE_RESULT_OK when validation is successful, an error otherwise.
 */
astarte_result_t data_validation_individual_datastream(const astarte_interface_t *interface,
    const char *path, astarte_value_t value, const int64_t *timestamp);

/**
 * @brief Validate data for an aggregated datastream against the device introspection.
 *
 * @param[in] interface Interface to use for the operation.
 * @param[in] path Path to validate.
 * @param[in] value_pair_array Astarte value pair array to validate.
 * @param[in] timestamp Timestamp to validate, it might be NULL.
 * @return ASTARTE_RESULT_OK when validation is successful, an error otherwise.
 */
astarte_result_t data_validation_aggregated_datastream(const astarte_interface_t *interface,
    const char *path, astarte_value_pair_array_t value_pair_array, const int64_t *timestamp);

/**
 * @brief Validate data for setting a device property against the device introspection.
 *
 * @param[in] interface Interface to use for the operation.
 * @param[in] path Path to validate.
 * @param[in] value Astarte value to validate.
 * @return ASTARTE_RESULT_OK when validation is successful, an error otherwise.
 */
astarte_result_t data_validation_set_property(
    const astarte_interface_t *interface, const char *path, astarte_value_t value);

/**
 * @brief Validate data for unsetting a device property against the device introspection.
 *
 * @param[in] interface Interface to use for the operation.
 * @param[in] path Path to validate.
 * @return ASTARTE_RESULT_OK when validation is successful, an error otherwise.
 */
astarte_result_t data_validation_unset_property(
    const astarte_interface_t *interface, const char *path);

#ifdef __cplusplus
}
#endif

#endif // DATA_VALIDATION_H
