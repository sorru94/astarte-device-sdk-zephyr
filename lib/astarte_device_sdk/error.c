/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "astarte_device_sdk/error.h"

#define ERR_TBL_IT(err)                                                                            \
    {                                                                                              \
        err, #err                                                                                  \
    }

/** @brief Astarte error mesage struct. Matches a code to a string. */
typedef struct
{
    /** @brief Code for the error. */
    astarte_error_t code;
    /** @brief Text for the error. */
    const char *msg;
} astarte_err_msg_t;

static const astarte_err_msg_t astarte_err_msg_table[] = {
    ERR_TBL_IT(ASTARTE_OK),
    ERR_TBL_IT(ASTARTE_ERROR),
    ERR_TBL_IT(ASTARTE_ERROR_OUT_OF_MEMORY),
    ERR_TBL_IT(ASTARTE_ERROR_CONFIGURATION),
    ERR_TBL_IT(ASTARTE_ERROR_INVALID_PARAM),
    ERR_TBL_IT(ASTARTE_ERROR_SOCKET),
    ERR_TBL_IT(ASTARTE_ERROR_HTTP_REQUEST),
    ERR_TBL_IT(ASTARTE_ERROR_JSON),
    ERR_TBL_IT(ASTARTE_ERROR_MBEDTLS),
    ERR_TBL_IT(ASTARTE_ERROR_NOT_FOUND),
    ERR_TBL_IT(ASTARTE_ERROR_INTERFACE_ALREADY_PRESENT),
    ERR_TBL_IT(ASTARTE_ERROR_INTERFACE_NOT_FOUND),
    ERR_TBL_IT(ASTARTE_ERROR_INTERFACE_INVALID_VERSION_ZERO),
    ERR_TBL_IT(ASTARTE_ERROR_INTERFACE_CONFLICTING),
    ERR_TBL_IT(ASTARTE_ERROR_TLS),
    ERR_TBL_IT(ASTARTE_ERROR_MQTT),
    ERR_TBL_IT(ASTARTE_ERROR_TIMEOUT),
    ERR_TBL_IT(ASTARTE_ERROR_BSON_SERIALIZER),
    ERR_TBL_IT(ASTARTE_ERROR_CLIENT_CERT_INVALID),
};

static const char astarte_unknown_msg[] = "ERROR";

const char *astarte_error_to_name(astarte_error_t code)
{
    for (size_t i = 0; i < sizeof(astarte_err_msg_table) / sizeof(astarte_err_msg_table[0]); ++i) {
        if (astarte_err_msg_table[i].code == code) {
            return astarte_err_msg_table[i].msg;
        }
    }

    return astarte_unknown_msg;
}
