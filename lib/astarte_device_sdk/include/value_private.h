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
#include "astarte_device_sdk/value.h"
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
    astarte_bson_serializer_handle_t bson, const char *key, astarte_value_t value);

/**
 * @brief Appends the content of an array of #astarte_value_pair_t to a BSON document.
 *
 * @param[in,out] bson a valid handle for the serializer instance.
 * @param[in] values Array of key-values pairs.
 * @param[in] values_size Number of elements for the values array.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_value_pair_serialize(
    astarte_bson_serializer_handle_t bson, astarte_value_pair_t *values, size_t values_size);

#ifdef __cplusplus
}
#endif

#endif /* VALUE_PRIVATE_H */
