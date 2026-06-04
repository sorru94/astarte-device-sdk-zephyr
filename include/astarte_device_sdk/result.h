/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ASTARTE_DEVICE_SDK_RESULT_H
#define ASTARTE_DEVICE_SDK_RESULT_H

/**
 * @file result.h
 * @brief Astarte result types.
 */

/**
 * @defgroup results Results
 * @brief Astarte result types.
 * @ingroup astarte_device_sdk
 * @{
 */

#include "astarte_device_sdk/astarte.h"

/**
 * @brief Astarte Device SDK return codes.
 *
 * @details Astarte Device SDK return codes. ASTARTE_RESULT_OK is always returned when no errors
 * occurred.
 */
typedef enum
{
    /** @brief No errors. */
    ASTARTE_RESULT_OK = 0,
    /** @brief BSON deserialization error. */
    ASTARTE_RESULT_BSON_DESERIALIZER_ERROR = 1,
    /** @brief A BSON document with elements of incompatible type has been received. */
    ASTARTE_RESULT_BSON_DESERIALIZER_TYPES_ERROR = 2,
    /** @brief A BSON array with elements of different type has been received. */
    ASTARTE_RESULT_BSON_EMPTY_ARRAY_ERROR = 3,
    /** @brief An empty BSON document has been received. */
    ASTARTE_RESULT_BSON_EMPTY_DOCUMENT_ERROR = 4,
    /** @brief BSON serialization error. */
    ASTARTE_RESULT_BSON_SERIALIZER_ERROR = 5,
    /** @brief Astarte marked the device client certificate as invalid. */
    ASTARTE_RESULT_CLIENT_CERT_INVALID = 6,
    /** @brief An outdated introspection has been found in cache. */
    ASTARTE_RESULT_DEVICE_CACHING_OUTDATED_INTROSPECTION = 7,
    /** @brief Calling a device API while the device is not ready. */
    ASTARTE_RESULT_DEVICE_NOT_READY = 8,
    /** @brief An HTTP request could not be processed. */
    ASTARTE_RESULT_HTTP_REQUEST_ERROR = 9,
    /** @brief A partial aggregated object has been found. */
    ASTARTE_RESULT_INCOMPLETE_AGGREGATION_OBJECT = 10,
    /** @brief Interface is already present in the introspection */
    ASTARTE_RESULT_INTERFACE_ALREADY_PRESENT = 11,
    /** @brief Trying to add an interface that conflicts with the previous one */
    ASTARTE_RESULT_INTERFACE_CONFLICTING = 12,
    /** @brief Trying to add an interface with both major an minor set to 0 */
    ASTARTE_RESULT_INTERFACE_INVALID_VERSION = 13,
    /** @brief Interface not found in the introspection */
    ASTARTE_RESULT_INTERFACE_NOT_FOUND = 14,
    /** @brief A generic error occurred. This is usually an internal error in the SDK. */
    ASTARTE_RESULT_INTERNAL_ERROR = 15,
    /** @brief Invalid configuration for the required operation. */
    ASTARTE_RESULT_INVALID_CONFIGURATION = 16,
    /** @brief A function has been called with incorrect parameters. */
    ASTARTE_RESULT_INVALID_PARAM = 17,
    /** @brief Attempting to parse/encode a malformed JSON document. */
    ASTARTE_RESULT_JSON_ERROR = 18,
    /** @brief Astarte key-value storage is full. */
    ASTARTE_RESULT_KEY_VALUE_FULL = 19,
    /** @brief Found key-value storage from an incompatible version. */
    ASTARTE_RESULT_KEY_VALUE_INCOMPATIBLE_VERSION = 20,
    /** @brief Recovery of corrupted key-value storage failed. */
    ASTARTE_RESULT_KEY_VALUE_RECOVERY_FAILED = 21,
    /** @brief The provided Astarte data is not compatible with a mapping. */
    ASTARTE_RESULT_MAPPING_DATA_INCOMPATIBLE = 22,
    /** @brief The specified mapping does not support an explicit timestamp. */
    ASTARTE_RESULT_MAPPING_EXPLICIT_TIMESTAMP_NOT_SUPPORTED = 23,
    /** @brief The specified mapping requires an explicit timestamp. */
    ASTARTE_RESULT_MAPPING_EXPLICIT_TIMESTAMP_REQUIRED = 24,
    /** @brief Could not find the mapping corresponding to a path in an interface. */
    ASTARTE_RESULT_MAPPING_NOT_IN_INTERFACE = 25,
    /** @brief The provided path does not match the mapping endpoint. */
    ASTARTE_RESULT_MAPPING_PATH_MISMATCH = 26,
    /** @brief The specified mapping does not support properties unsetting. */
    ASTARTE_RESULT_MAPPING_UNSET_NOT_ALLOWED = 27,
    /** @brief Internal error from the MBEDTLS library. */
    ASTARTE_RESULT_MBEDTLS_ERROR = 28,
    /** @brief Negative result for compare functions. */
    ASTARTE_RESULT_MISMATCH = 29,
    /** @brief Attempting to connect an MQTT client already connected. */
    ASTARTE_RESULT_MQTT_CLIENT_ALREADY_CONNECTED = 30,
    /** @brief Attempting to connect an MQTT client already connecting. */
    ASTARTE_RESULT_MQTT_CLIENT_ALREADY_CONNECTING = 31,
    /** @brief Calling an MQTT API while the client is not ready. */
    ASTARTE_RESULT_MQTT_CLIENT_NOT_READY = 32,
    /** @brief Internal error from the MQTT library. */
    ASTARTE_RESULT_MQTT_ERROR = 33,
    /** @brief The resource was not found. */
    ASTARTE_RESULT_NOT_FOUND = 34,
    /** @brief The operation caused an out of memory error */
    ASTARTE_RESULT_OUT_OF_MEMORY = 35,
    /** @brief Error during TCP socket creation. */
    ASTARTE_RESULT_SOCKET_ERROR = 36,
    /** @brief Operation timed out. */
    ASTARTE_RESULT_TIMEOUT = 37,
    /** @brief Error from the TLS credential zephyr module. */
    ASTARTE_RESULT_TLS_ERROR = 38,
    /** @brief Error while reading/writing an ZMS entry. */
    ASTARTE_RESULT_ZMS_ERROR = 39
} astarte_result_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Returns string for result codes.
 *
 * @details This function finds the result code in a pre-generated lookup-table and returns its
 * string representation.
 *
 * @param[in] code Result code
 * @return String result message
 */
const char *astarte_result_to_name(astarte_result_t code);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif // ASTARTE_DEVICE_SDK_RESULT_H
