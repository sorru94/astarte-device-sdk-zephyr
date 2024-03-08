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
    /** @brief A generic error occurred. This is usually an internal error in the SDK. */
    ASTARTE_RESULT_INTERNAL_ERROR = 1,
    /** @brief The operation caused an out of memory error */
    ASTARTE_RESULT_OUT_OF_MEMORY = 2,
    /** @brief Invalid configuration for the required operation. */
    ASTARTE_RESULT_INVALID_CONFIGURATION = 3,
    /** @brief A function has been called with incorrect parameters. */
    ASTARTE_RESULT_INVALID_PARAM = 4,
    /** @brief Error during TCP socket creation. */
    ASTARTE_RESULT_SOCKET_ERROR = 5,
    /** @brief An HTTP request could not be processed. */
    ASTARTE_RESULT_HTTP_REQUEST_ERROR = 6,
    /** @brief Attempting to parse/encode a malformed JSON document. */
    ASTARTE_RESULT_JSON_ERROR = 7,
    /** @brief Internal error from the MBEDTLS library. */
    ASTARTE_RESULT_MBEDTLS_ERROR = 8,
    /** @brief The resource was not found. */
    ASTARTE_RESULT_NOT_FOUND = 9,
    /** @brief Interface is already present in the introspection */
    ASTARTE_RESULT_INTERFACE_ALREADY_PRESENT = 10,
    /** @brief Interface not found in the introspection */
    ASTARTE_RESULT_INTERFACE_NOT_FOUND = 11,
    /** @brief Trying to add an interface with both major an minor set to 0 */
    ASTARTE_RESULT_INTERFACE_INVALID_VERSION = 12,
    /** @brief Trying to add an interface that conflicts with the previous one */
    ASTARTE_RESULT_INTERFACE_CONFLICTING = 13,
    /** @brief Error from the TLS credential zephyr module. */
    ASTARTE_RESULT_TLS_ERROR = 14,
    /** @brief Internal error from the MQTT library. */
    ASTARTE_RESULT_MQTT_ERROR = 15,
    /** @brief Operation timed out. */
    ASTARTE_RESULT_TIMEOUT = 16,
    /** @brief BSON serialization error. */
    ASTARTE_RESULT_BSON_SERIALIZER_ERROR = 17,
    /** @brief Astarte marked the device client certificate as invalid. */
    ASTARTE_RESULT_CLIENT_CERT_INVALID = 18
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