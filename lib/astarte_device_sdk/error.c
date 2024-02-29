/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "astarte_device_sdk/error.h"

#include <stdlib.h>

#define ERR_TBL_IT(err)                                                                            \
    {                                                                                              \
        err, #err                                                                                  \
    }

/** @brief Astarte error mesage struct. Matches a code to a string. */
typedef struct
{
    /** @brief Code for the error. */
    astarte_err_t code;
    /** @brief Text for the error. */
    const char *msg;
} astarte_err_msg_t;

static const astarte_err_msg_t astarte_err_msg_table[] = {
    ERR_TBL_IT(ASTARTE_OK),
    ERR_TBL_IT(ASTARTE_ERR),
    ERR_TBL_IT(ASTARTE_ERR_OUT_OF_MEMORY),
    ERR_TBL_IT(ASTARTE_ERR_CONFIGURATION),
    ERR_TBL_IT(ASTARTE_ERR_INVALID_PARAM),
    ERR_TBL_IT(ASTARTE_ERR_SOCKET),
    ERR_TBL_IT(ASTARTE_ERR_HTTP_REQUEST),
    ERR_TBL_IT(ASTARTE_ERR_JSON),
    ERR_TBL_IT(ASTARTE_ERR_MBEDTLS),
    ERR_TBL_IT(ASTARTE_ERR_NOT_FOUND),
    ERR_TBL_IT(ASTARTE_ERR_INTERFACE_ALREADY_PRESENT),
    ERR_TBL_IT(ASTARTE_ERR_INTERFACE_NOT_FOUND),
    ERR_TBL_IT(ASTARTE_ERR_INTERFACE_INVALID_VERSION_ZERO),
    ERR_TBL_IT(ASTARTE_ERR_INTERFACE_CONFLICTING),
    ERR_TBL_IT(ASTARTE_ERR_TLS),
    ERR_TBL_IT(ASTARTE_ERR_MQTT),
    ERR_TBL_IT(ASTARTE_ERR_TIMEOUT),
    ERR_TBL_IT(ASTARTE_ERR_BSON_SERIALIZER),
};

static const char astarte_unknown_msg[] = "ERROR";

const char *astarte_err_to_name(astarte_err_t code)
{
    for (size_t i = 0; i < sizeof(astarte_err_msg_table) / sizeof(astarte_err_msg_table[0]); ++i) {
        if (astarte_err_msg_table[i].code == code) {
            return astarte_err_msg_table[i].msg;
        }
    }

    return astarte_unknown_msg;
}
