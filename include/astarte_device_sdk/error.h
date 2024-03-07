/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ASTARTE_DEVICE_SDK_ERROR_H
#define ASTARTE_DEVICE_SDK_ERROR_H

/**
 * @file error.h
 * @brief Astarte error types.
 */

/**
 * @defgroup errors Errors
 * @ingroup astarte_device_sdk
 * @{
 */

#include "astarte_device_sdk/astarte.h"

/**
 * @brief Astarte Device SDK return codes.
 *
 * @details Astarte Device SDK return codes. ASTARTE_OK is always returned when no errors occurred.
 */
typedef enum
{
    /** @brief No errors. */
    ASTARTE_OK = 0,
    /** @brief A generic error occurred. This is usually an internal error in the SDK. */
    ASTARTE_ERROR = 1,
    /** @brief The operation caused an out of memory error */
    ASTARTE_ERROR_OUT_OF_MEMORY = 2,
    /** @brief Invalid configuration for the required operation. */
    ASTARTE_ERROR_CONFIGURATION = 3,
    /** @brief A function has been called with incorrect parameters. */
    ASTARTE_ERROR_INVALID_PARAM = 4,
    /** @brief Error during TCP socket creation. */
    ASTARTE_ERROR_SOCKET = 5,
    /** @brief An HTTP request could not be processed. */
    ASTARTE_ERROR_HTTP_REQUEST = 6,
    /** @brief Attempting to parse/encode a malformed JSON document. */
    ASTARTE_ERROR_JSON = 7,
    /** @brief Internal error from the MBEDTLS library. */
    ASTARTE_ERROR_MBEDTLS = 8,
    /** @brief The resource was not found. */
    ASTARTE_ERROR_NOT_FOUND = 9,
    /** @brief Interface is already present in the introspection */
    ASTARTE_ERROR_INTERFACE_ALREADY_PRESENT = 10,
    /** @brief Interface not found in the introspection */
    ASTARTE_ERROR_INTERFACE_NOT_FOUND = 11,
    /** @brief Trying to add an interface with both major an minor set to 0 */
    ASTARTE_ERROR_INTERFACE_INVALID_VERSION_ZERO = 12,
    /** @brief Trying to add an interface that conflicts with the previous one */
    ASTARTE_ERROR_INTERFACE_CONFLICTING = 13,
    /** @brief Error from the TLS credential zephyr module. */
    ASTARTE_ERROR_TLS = 14,
    /** @brief Internal error from the MQTT library. */
    ASTARTE_ERROR_MQTT = 15,
    /** @brief Operation timed out. */
    ASTARTE_ERROR_TIMEOUT = 16,
    /** @brief BSON serialization error. */
    ASTARTE_ERROR_BSON_SERIALIZER = 17,
    /** @brief Astarte marked the device client certificate as invalid. */
    ASTARTE_ERROR_CLIENT_CERT_INVALID = 18
} astarte_error_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Returns string for astarte_error_t error codes.
 *
 * @details This function finds the error code in a pre-generated lookup-table and returns its
 * string representation.
 *
 * @param[in] code Error code
 * @return String error message
 */
const char *astarte_error_to_name(astarte_error_t code);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif // ASTARTE_DEVICE_SDK_ERROR_H
