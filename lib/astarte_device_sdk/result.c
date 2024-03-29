/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "astarte_device_sdk/result.h"

#define RES_TBL_IT(res)                                                                            \
    {                                                                                              \
        res, #res                                                                                  \
    }

/** @brief Astarte result message struct. Matches a code to a string. */
typedef struct
{
    /** @brief Code for the error. */
    astarte_result_t code;
    /** @brief Text for the error. */
    const char *msg;
} astarte_res_msg_t;

static const astarte_res_msg_t astarte_res_msg_table[] = {
    RES_TBL_IT(ASTARTE_RESULT_OK),
    RES_TBL_IT(ASTARTE_RESULT_INTERNAL_ERROR),
    RES_TBL_IT(ASTARTE_RESULT_OUT_OF_MEMORY),
    RES_TBL_IT(ASTARTE_RESULT_INVALID_CONFIGURATION),
    RES_TBL_IT(ASTARTE_RESULT_INVALID_PARAM),
    RES_TBL_IT(ASTARTE_RESULT_SOCKET_ERROR),
    RES_TBL_IT(ASTARTE_RESULT_HTTP_REQUEST_ERROR),
    RES_TBL_IT(ASTARTE_RESULT_JSON_ERROR),
    RES_TBL_IT(ASTARTE_RESULT_MBEDTLS_ERROR),
    RES_TBL_IT(ASTARTE_RESULT_NOT_FOUND),
    RES_TBL_IT(ASTARTE_RESULT_INTERFACE_ALREADY_PRESENT),
    RES_TBL_IT(ASTARTE_RESULT_INTERFACE_NOT_FOUND),
    RES_TBL_IT(ASTARTE_RESULT_INTERFACE_INVALID_VERSION),
    RES_TBL_IT(ASTARTE_RESULT_INTERFACE_CONFLICTING),
    RES_TBL_IT(ASTARTE_RESULT_TLS_ERROR),
    RES_TBL_IT(ASTARTE_RESULT_MQTT_ERROR),
    RES_TBL_IT(ASTARTE_RESULT_TIMEOUT),
    RES_TBL_IT(ASTARTE_RESULT_BSON_SERIALIZER_ERROR),
    RES_TBL_IT(ASTARTE_RESULT_BSON_DESERIALIZER_ERROR),
    RES_TBL_IT(ASTARTE_RESULT_BSON_ARRAY_TYPES_ERROR),
    RES_TBL_IT(ASTARTE_RESULT_BSON_EMPTY_ARRAY_ERROR),
    RES_TBL_IT(ASTARTE_RESULT_BSON_EMPTY_DOCUMENT_ERROR),
    RES_TBL_IT(ASTARTE_RESULT_CLIENT_CERT_INVALID),
    RES_TBL_IT(ASTARTE_RESULT_MAPPING_PATH_MISMATCH),
};

static const char astarte_unknown_msg[] = "UNKNOWN RESULT CODE";

const char *astarte_result_to_name(astarte_result_t code)
{
    for (size_t i = 0; i < sizeof(astarte_res_msg_table) / sizeof(astarte_res_msg_table[0]); ++i) {
        if (astarte_res_msg_table[i].code == code) {
            return astarte_res_msg_table[i].msg;
        }
    }

    return astarte_unknown_msg;
}
