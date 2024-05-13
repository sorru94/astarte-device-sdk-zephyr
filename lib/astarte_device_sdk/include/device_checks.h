/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DEVICE_CHECKS_H
#define DEVICE_CHECKS_H

/**
 * @file device_checks.h
 * @brief Utility functions used to validate data and timestamps in transmission and reception.
 */

#include "astarte_device_sdk/astarte.h"
#include "astarte_device_sdk/result.h"
#include "astarte_device_sdk/value.h"

#include "introspection.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Validate data for an individual datastream against the device introspection.
 *
 * @param[in] introspection Introspection to use for validation.
 * @param[in] interface_name Interface name to validate.
 * @param[in] path Path to validate.
 * @param[in] value Astarte value to validate.
 * @param[in] timestamp Timestamp to validate, it might be NULL.
 * @param[out] qos Quality of service for the mapping.
 * @return ASTARTE_RESULT_OK when validation is successful, an error otherwise.
 */
astarte_result_t device_checks_individual_datastream(introspection_t *introspection,
    const char *interface_name, const char *path, astarte_value_t value, const int64_t *timestamp,
    int *qos);

/**
 * @brief Validate data for an aggregated datastream against the device introspection.
 *
 * @param[in] introspection Introspection to use for validation.
 * @param[in] interface_name Interface name to validate.
 * @param[in] path Path to validate.
 * @param[in] values Array of Astarte value pairs to validate.
 * @param[in] values_length Size of the array of Astarte value pairs to validate.
 * @param[in] timestamp Timestamp to validate, it might be NULL.
 * @param[out] qos Quality of service for the mapping.
 * @return ASTARTE_RESULT_OK when validation is successful, an error otherwise.
 */
astarte_result_t device_checks_aggregated_datastream(introspection_t *introspection,
    const char *interface_name, const char *path, astarte_value_pair_t *values,
    size_t values_length, const int64_t *timestamp, int *qos);

/**
 * @brief Validate data for setting a device property against the device introspection.
 *
 * @param[in] introspection Introspection to use for validation.
 * @param[in] interface_name Interface name to validate.
 * @param[in] path Path to validate.
 * @param[in] value Astarte value to validate.
 * @return ASTARTE_RESULT_OK when validation is successful, an error otherwise.
 */
astarte_result_t device_checks_set_property(introspection_t *introspection,
    const char *interface_name, const char *path, astarte_value_t value);

/**
 * @brief Validate data for unsetting a device property against the device introspection.
 *
 * @param[in] introspection Introspection to use for validation.
 * @param[in] interface_name Interface name to validate.
 * @param[in] path Path to validate.
 * @return ASTARTE_RESULT_OK when validation is successful, an error otherwise.
 */
astarte_result_t device_checks_unset_property(
    introspection_t *introspection, const char *interface_name, const char *path);

#ifdef __cplusplus
}
#endif

#endif // DEVICE_CHECKS_H
