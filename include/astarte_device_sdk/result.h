/*
 * (C) Copyright 2026, SECO Mind Srl
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

/**
 * @brief Master list of all Astarte results.
 * @details Format: X(enum_name, integer_value)
 */
#define ASTARTE_RESULT_MAP(X)                                                                      \
    X(ASTARTE_RESULT_OK, 0)                                                                        \
    X(ASTARTE_RESULT_INTERNAL_ERROR, 1)                                                            \
    X(ASTARTE_RESULT_OUT_OF_MEMORY, 2)                                                             \
    X(ASTARTE_RESULT_INVALID_CONFIGURATION, 3)                                                     \
    X(ASTARTE_RESULT_INVALID_PARAM, 4)                                                             \
    X(ASTARTE_RESULT_SOCKET_ERROR, 5)                                                              \
    X(ASTARTE_RESULT_HTTP_REQUEST_ERROR, 6)                                                        \
    X(ASTARTE_RESULT_JSON_ERROR, 7)                                                                \
    X(ASTARTE_RESULT_MBEDTLS_ERROR, 8)                                                             \
    X(ASTARTE_RESULT_NOT_FOUND, 9)                                                                 \
    X(ASTARTE_RESULT_INTERFACE_ALREADY_PRESENT, 10)                                                \
    X(ASTARTE_RESULT_INTERFACE_NOT_FOUND, 11)                                                      \
    X(ASTARTE_RESULT_INTERFACE_INVALID_VERSION, 12)                                                \
    X(ASTARTE_RESULT_INTERFACE_CONFLICTING, 13)                                                    \
    X(ASTARTE_RESULT_TLS_ERROR, 14)                                                                \
    X(ASTARTE_RESULT_MQTT_ERROR, 15)                                                               \
    X(ASTARTE_RESULT_TIMEOUT, 16)                                                                  \
    X(ASTARTE_RESULT_BSON_SERIALIZER_ERROR, 17)                                                    \
    X(ASTARTE_RESULT_BSON_DESERIALIZER_ERROR, 18)                                                  \
    X(ASTARTE_RESULT_BSON_DESERIALIZER_TYPES_ERROR, 19)                                            \
    X(ASTARTE_RESULT_BSON_EMPTY_ARRAY_ERROR, 20)                                                   \
    X(ASTARTE_RESULT_BSON_EMPTY_DOCUMENT_ERROR, 21)                                                \
    X(ASTARTE_RESULT_CLIENT_CERT_INVALID, 22)                                                      \
    X(ASTARTE_RESULT_MAPPING_PATH_MISMATCH, 23)                                                    \
    X(ASTARTE_RESULT_MAPPING_DATA_INCOMPATIBLE, 24)                                                \
    X(ASTARTE_RESULT_MAPPING_NOT_IN_INTERFACE, 25)                                                 \
    X(ASTARTE_RESULT_MAPPING_UNSET_NOT_ALLOWED, 26)                                                \
    X(ASTARTE_RESULT_MAPPING_EXPLICIT_TIMESTAMP_REQUIRED, 27)                                      \
    X(ASTARTE_RESULT_MAPPING_EXPLICIT_TIMESTAMP_NOT_SUPPORTED, 28)                                 \
    X(ASTARTE_RESULT_MQTT_CLIENT_NOT_READY, 29)                                                    \
    X(ASTARTE_RESULT_MQTT_CLIENT_ALREADY_CONNECTED, 30)                                            \
    X(ASTARTE_RESULT_MQTT_CLIENT_ALREADY_CONNECTING, 31)                                           \
    X(ASTARTE_RESULT_DEVICE_NOT_READY, 32)                                                         \
    X(ASTARTE_RESULT_INCOMPLETE_AGGREGATION_OBJECT, 33)                                            \
    X(ASTARTE_RESULT_NVS_ERROR, 34)                                                                \
    X(ASTARTE_RESULT_KV_STORAGE_FULL, 35)                                                          \
    X(ASTARTE_RESULT_DEVICE_CACHING_OUTDATED_INTROSPECTION, 36)

/** @brief Astarte Device SDK return codes. */
typedef enum
{
/**
 * @brief Internal helper macro to expand the result map.
 *
 * @param name The enum identifier.
 * @param value The integer value.
 */
#define X_ENUM(name, value) name = value,
    ASTARTE_RESULT_MAP(X_ENUM)
#undef X_ENUM
} astarte_result_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Converts each result to a meaningful string.
 *
 * @param[in] code Result code
 * @return Result code as a string
 */
const char *astarte_result_to_name(astarte_result_t code);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif // ASTARTE_DEVICE_SDK_RESULT_H
