/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ASTARTE_DEVICE_SDK_OBJECT_H
#define ASTARTE_DEVICE_SDK_OBJECT_H

/**
 * @file object.h
 * @brief Definitions for Astarte object data types
 */

/**
 * @defgroup objects Objects
 * @ingroup astarte_device_sdk
 * @{
 */

#include "astarte_device_sdk/astarte.h"
#include "astarte_device_sdk/individual.h"
#include "astarte_device_sdk/result.h"

/**
 * @brief Object entry data type.
 *
 *
 * @warning Do not inspect its content directly. Use the provided functions to modify it.
 * @details This struct is the base building block for an Astarte object. Concatenate multiple
 * #astarte_object_entry_t in an array to create the payload for an Astarte object transmission and
 * reception.
 *
 * See the following initialization/getter methods:
 *  - #astarte_object_entry_new
 *  - #astarte_object_entry_to_endpoint_and_individual
 */
typedef struct
{
    /** @brief Endpoint for the entry */
    const char *endpoint;
    /** @brief Individual for the entry */
    astarte_individual_t individual;
} astarte_object_entry_t;

/**
 * @brief Array of Astarte object entries.
 *
 * @warning Do not inspect its content directly. Use the provided functions to modify it.
 * @details This struct is intended to be used when transmitting and receiving Astarte objects.
 *
 * See the following initialization/getter methods:
 *  - #astarte_object_new
 *  - #astarte_object_to_entries
 */
typedef struct
{
    /** @brief Array of Astarte object entries. */
    astarte_object_entry_t *buf;
    /** @brief Size of the array of Astarte object entries. */
    size_t len;
} astarte_object_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize an Astarte object entry.
 *
 * @param[in] endpoint Endpoint to store in the entry.
 * @param[in] individual Individual data to store in the entry.
 * @return The Astarte object entry created from the inputs.
 */
astarte_object_entry_t astarte_object_entry_new(
    const char *endpoint, astarte_individual_t individual);

/**
 * @brief Initialize an Astarte object.
 *
 * @param[in] entries Array of entries to place in the object.
 * @param[in] len Size of the array of entries.
 * @return The Astarte object created from the inputs.
 */
astarte_object_t astarte_object_new(astarte_object_entry_t *entries, size_t len);

/**
 * @brief Convert an Astarte object entry to an Astarte individual and endpoint.
 *
 * @param[in] entry Input Astarte object entry.
 * @param[out] endpoint Endpoint extracted from the entry.
 * @param[out] individual Individual extracted from the entry.
 * @return ASTARTE_RESULT_OK if the conversion was successful, an error otherwise.
 */
astarte_result_t astarte_object_entry_to_endpoint_and_individual(
    astarte_object_entry_t entry, const char **endpoint, astarte_individual_t *individual);

/**
 * @brief Convert an Astarte object to an array of Astarte object entries.
 *
 * @param[in] object Input Astarte object.
 * @param[out] object_entries Extracted array of object entries.
 * @param[out] len Size of the extracted array.
 * @return ASTARTE_RESULT_OK if the conversion was successful, an error otherwise.
 */
astarte_result_t astarte_object_to_entries(
    astarte_object_t object, astarte_object_entry_t **object_entries, size_t *len);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* ASTARTE_DEVICE_SDK_OBJECT_H */
