/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DATA_DESERIALIZE_H
#define DATA_DESERIALIZE_H

/**
 * @file data_deserialize.h
 * @brief Deserialization functions for Astarte data types
 */
#include "astarte_device_sdk/data.h"

#include "bson_deserializer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Deserialize a BSON element to an #astarte_data_t.
 *
 * @warning This function might perform dynamic allocation, as such any data deserialized with
 * this function should be destroyed calling #data_destroy_deserialized.
 *
 * @note No encapsulation is permitted in the BSON element, the data should be directy accessible.
 *
 * @param[in] bson_elem The BSON element containing the data to deserialize.
 * @param[in] type An expected mapping type for the Astarte data.
 * @param[out] data The resulting data.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t data_deserialize(
    astarte_bson_element_t bson_elem, astarte_mapping_type_t type, astarte_data_t *data);

/**
 * @brief Destroy the data serialized with #data_deserialize.
 *
 * @warning This function will free all dynamically allocated memory allocated by
 * #data_deserialize. As so it should only be called on #astarte_data_t
 * that have been created with #data_deserialize.
 *
 * @note This function will have no effect on scalar #astarte_data_t, but only act on array
 * types.
 *
 * @param[in] data Data of which the content will be destroyed.
 */
void data_destroy_deserialized(astarte_data_t data);

#ifdef __cplusplus
}
#endif

#endif /* DATA_DESERIALIZE_H */
