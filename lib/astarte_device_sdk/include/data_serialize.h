/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DATA_SERIALIZE_H
#define DATA_SERIALIZE_H

/**
 * @file data_serialize.h
 * @brief Serialization functions for  Astarte data types
 */
#include "astarte_device_sdk/data.h"

#include "bson_serializer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Appends the content of an #astarte_data_t to a BSON document.
 *
 * @param[in,out] bson a valid handle for the serializer instance.
 * @param[in] key BSON key name, which is a C string.
 * @param[in] data the #astarte_data_t to serialize to the bson.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t data_serialize(
    astarte_bson_serializer_t *bson, const char *key, astarte_data_t data);

#ifdef __cplusplus
}
#endif

#endif /* DATA_SERIALIZE_H */
