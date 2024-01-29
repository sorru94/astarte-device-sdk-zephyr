/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file error.h
 * @brief Astarte error types.
 */

#ifndef ASTARTE_DEVICE_SDK_ERROR_H
#define ASTARTE_DEVICE_SDK_ERROR_H

/**
 * @defgroup errors Errors
 * @ingroup astarte_device_sdk
 * @{
 */

#include "astarte_device_sdk/astarte.h"

// clang-format off

/**
* @brief Astarte Device SDK return codes.
*
* @details ASTARTE_OK is always returned when no errors occurred.
*/
typedef enum
{
   ASTARTE_OK = 0, /**< No errors. */
   ASTARTE_ERR = 1, /**< A generic error occurred. This is usually an internal error in the SDK. */
   ASTARTE_ERR_OUT_OF_MEMORY = 2, /**< The operation caused an out of memory error */
   ASTARTE_ERR_CONFIGURATION = 3, /**< Invalid configuration for the required operation. */
   ASTARTE_ERR_INVALID_PARAM = 4, /**< A function has been called with incorrect parameters. */
   ASTARTE_ERR_SOCKET = 5, /**< Error during TCP socket creation. */
   ASTARTE_ERR_HTTP_REQUEST = 6, /**< An HTTP request could not be processed. */
   ASTARTE_ERR_JSON = 7, /**< Attempting to parse/encode a malformed JSON document. */
   ASTARTE_ERR_MBEDTLS = 8, /**< Internal error from the MBEDTLS library. */
} astarte_err_t;

// clang-format on

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Returns string for astarte_err_t error codes.
 *
 * @details This function finds the error code in a pre-generated lookup-table and returns its
 * string representation.
 *
 * @param[in] code Error code
 * @return String error message
 */
const char *astarte_err_to_name(astarte_err_t code);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif // ASTARTE_DEVICE_SDK_ERROR_H
