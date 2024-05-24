/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef VALUE_PRIVATE_H
#define VALUE_PRIVATE_H

/**
 * @file value_private.h
 * @brief Private definitions for Astarte values
 */

#include "astarte_device_sdk/astarte.h"
#include "astarte_device_sdk/interface.h"
#include "astarte_device_sdk/value.h"
#include "bson_deserializer.h"
#include "bson_serializer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Appends the content of an #astarte_value_t to a BSON document.
 *
 * @param[in,out] bson a valid handle for the serializer instance.
 * @param[in] key BSON key name, which is a C string.
 * @param[in] value the #astarte_value_t to serialize to the bson.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_value_serialize(
    astarte_bson_serializer_t *bson, const char *key, astarte_value_t value);

/**
 * @brief Appends the content of an array of #astarte_value_pair_t to a BSON document.
 *
 * @param[in,out] bson a valid handle for the serializer instance.
 * @param[in] values Array of key-values pairs.
 * @param[in] values_length Number of elements for the values array.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_value_pair_serialize(
    astarte_bson_serializer_t *bson, astarte_value_pair_t *values, size_t values_length);

/**
 * @brief Deserialize a BSON element to an #astarte_value_t.
 *
 * @warning This function might perform dynamic allocation, as such any value deserialized with this
 * function should be destroyed calling #astarte_value_destroy_deserialized.
 *
 * @note No encapsulation is permitted in the BSON element, the value should be directy accessible.
 *
 * @param[in] bson_elem The BSON element containing the data to deserialize.
 * @param[in] type An expected mapping type for the Astarte value.
 * @param[out] value The resulting value.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_value_deserialize(
    astarte_bson_element_t bson_elem, astarte_mapping_type_t type, astarte_value_t *value);

/**
 * @brief Destroy the data serialized with #astarte_value_deserialize.
 *
 * @warning This function will free all dynamically allocated memory allocated by
 * #astarte_value_deserialize. As so it should only be called on #astarte_value_t
 * that have been created with #astarte_value_deserialize.
 *
 * @note This function will have no effect on scalar #astarte_value_t, but only act on array types.
 *
 * @param[in] value Value of which the content will be destroyed.
 */
void astarte_value_destroy_deserialized(astarte_value_t value);

/**
 * @brief Deserialize a BSON element to an #astarte_value_pair_array_t.
 *
 * @warning This function might perform dynamic allocation, as such any value deserialized with this
 * function should be destroyed calling #astarte_value_pair_destroy_deserialized.
 *
 * @note The BSON element should respect a predefined structure. It should contain a document
 * with all its element deserializable with #astarte_value_deserialize.
 *
 * @param[in] bson_elem The BSON element containing the data to deserialize.
 * @param[in] interface The interface corresponding the the Astarte value to deserialize.
 * @param[in] path The path corresponding to the BSON element to deserialize.
 * @param[out] value_pair_array Deserialized Astarte value pair array.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_value_pair_deserialize(astarte_bson_element_t bson_elem,
    const astarte_interface_t *interface, const char *path,
    astarte_value_pair_array_t *value_pair_array);

/**
 * @brief Destroy the data serialized with #astarte_value_pair_deserialize.
 *
 * @warning This function will free all dynamically allocated memory allocated by
 * #astarte_value_pair_deserialize. As so it should only be called on #astarte_value_pair_array_t
 * that have been created with #astarte_value_pair_deserialize.
 *
 * @param[in] value_pair_array Astarte value pair array of which the content will be destroyed.
 */
void astarte_value_pair_destroy_deserialized(astarte_value_pair_array_t value_pair_array);

#ifdef __cplusplus
}
#endif

#endif /* VALUE_PRIVATE_H */
