/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DATA_DESERIALIZE_SCALAR_H
#define DATA_DESERIALIZE_SCALAR_H

/**
 * @file data/deserialize_scalar.h
 * @brief Deserialization functions for Astarte data types (scalar types)
 */
#include "data/deserialize.h"

#include "bson/deserializer.h"

#include "interface_private.h"
#include "mapping_private.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Deserialize a scalar bson element into an Astarte data object.
 *
 * @param[in] bson_elem Scalar BSON element to deserialize.
 * @param[in] type The expected type for the Astarte data.
 * @param[out] data The Astarte data where to store the deserialized data.
 * @return ASTARTE_RESULT_OK upon success, an error code otherwise.
 */
astarte_result_t astarte_data_deserialize_scalar(
    astarte_bson_element_t bson_elem, astarte_mapping_type_t type, astarte_data_t *data);

#ifdef __cplusplus
}
#endif

#endif /* DATA_DESERIALIZE_SCALAR_H */
