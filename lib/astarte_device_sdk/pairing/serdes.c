/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "pairing/serdes.h"

#include <stdio.h>
#include <string.h>
#include <zephyr/data/json.h>

#include "log.h"
#include "pairing/core.h"

ASTARTE_LOG_MODULE_DECLARE(astarte_pairing, CONFIG_ASTARTE_DEVICE_SDK_PAIRING_LOG_LEVEL);

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

// The JSON parser module for Zephyr does not support the null keyword
// We'll replace the null keyword with a substitute string before parsing.
#define JSON_NULL "null"
#define JSON_NULL_REPLACEMENT "\"..\""
BUILD_ASSERT(sizeof(JSON_NULL) == sizeof(JSON_NULL_REPLACEMENT),
    "JSON null replacement string has incorrect size.");

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_pairing_serialize_register_device_payload(
    const char *device_id, char *out_buff, size_t out_buff_size)
{
    struct payload_s
    {
        struct data_s
        {
            const char *hw_id;
        } data;
    };

    static const struct json_obj_descr data_descr[] = {
        JSON_OBJ_DESCR_PRIM(struct data_s, hw_id, JSON_TOK_STRING),
    };

    static const struct json_obj_descr payload_descr[] = {
        JSON_OBJ_DESCR_OBJECT(struct payload_s, data, data_descr),
    };

    struct payload_s payload = { .data = { .hw_id = device_id } };

    ssize_t len = json_calc_encoded_len(payload_descr, ARRAY_SIZE(payload_descr), &payload);
    if ((len <= 0) || (len > out_buff_size)) {
        ASTARTE_LOG_ERR("Insufficient buffer, requiring %zd bytes", len);
        return ASTARTE_RESULT_JSON_ERROR;
    }

    int ret = json_obj_encode_buf(
        payload_descr, ARRAY_SIZE(payload_descr), &payload, out_buff, out_buff_size);
    if (ret != 0) {
        ASTARTE_LOG_ERR("Error encoding payload: %d", ret);
        return ASTARTE_RESULT_JSON_ERROR;
    }
    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_pairing_deserialize_register_device_response(
    char *resp_buf, char *out_cred_secr, size_t out_cred_secr_size)
{
    struct full_json_s
    {
        struct data_s
        {
            const char *credentials_secret;
        } data;
    };
    static const struct json_obj_descr data_descr[] = {
        JSON_OBJ_DESCR_PRIM(struct data_s, credentials_secret, JSON_TOK_STRING),
    };
    static const struct json_obj_descr full_json_descr[] = {
        JSON_OBJ_DESCR_OBJECT(struct full_json_s, data, data_descr),
    };

    // This is only for the outside level, nested elements should be checked individually.
    int expected_return_code = (1U << (size_t) ARRAY_SIZE(full_json_descr)) - 1;
    struct full_json_s parsed_json = { .data.credentials_secret = NULL };
    int64_t ret = json_obj_parse(
        resp_buf, strlen(resp_buf) + 1, full_json_descr, ARRAY_SIZE(full_json_descr), &parsed_json);

    if (ret != expected_return_code) {
        ASTARTE_LOG_ERR("JSON Parse Error: %lld", ret);
        return ASTARTE_RESULT_JSON_ERROR;
    }
    if (!parsed_json.data.credentials_secret) {
        ASTARTE_LOG_ERR("Parsed JSON is missing the credentials_secret field.");
        return ASTARTE_RESULT_JSON_ERROR;
    }
    if (strlen(parsed_json.data.credentials_secret) != ASTARTE_PAIRING_CRED_SECR_LEN) {
        ASTARTE_LOG_ERR("Received credential secret of unexpected length: '%s'.",
            parsed_json.data.credentials_secret);
        return ASTARTE_RESULT_JSON_ERROR;
    }

    int snprintf_rc
        = snprintf(out_cred_secr, out_cred_secr_size, "%s", parsed_json.data.credentials_secret);
    if ((snprintf_rc < 0) || (snprintf_rc >= out_cred_secr_size)) {
        ASTARTE_LOG_ERR("Error extracting the credential secret from the parsed json.");
        return ASTARTE_RESULT_JSON_ERROR;
    }
    ASTARTE_LOG_DBG("Received credentials secret: %s", out_cred_secr);
    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_pairing_deserialize_get_broker_url_response(
    char *resp_buf, char *out_url, size_t out_url_size)
{
    struct full_json_s
    {
        struct data_s
        {
            struct protocols_s
            {
                struct astarte_mqtt_v1_s
                {
                    const char *broker_url;
                } astarte_mqtt_v1;
            } protocols;
        } data;
    };
    static const struct json_obj_descr astarte_mqtt_v1_descr[] = {
        JSON_OBJ_DESCR_PRIM(struct astarte_mqtt_v1_s, broker_url, JSON_TOK_STRING),
    };
    static const struct json_obj_descr protocols_descr[] = {
        JSON_OBJ_DESCR_OBJECT(struct protocols_s, astarte_mqtt_v1, astarte_mqtt_v1_descr),
    };
    static const struct json_obj_descr data_descr[] = {
        JSON_OBJ_DESCR_OBJECT(struct data_s, protocols, protocols_descr),
    };
    static const struct json_obj_descr full_json_descr[] = {
        JSON_OBJ_DESCR_OBJECT(struct full_json_s, data, data_descr),
    };

    int expected_return_code = (1U << (size_t) ARRAY_SIZE(full_json_descr)) - 1;
    struct full_json_s parsed_json = { .data.protocols.astarte_mqtt_v1.broker_url = NULL };
    int64_t ret = json_obj_parse(
        resp_buf, strlen(resp_buf) + 1, full_json_descr, ARRAY_SIZE(full_json_descr), &parsed_json);

    if (ret != expected_return_code) {
        ASTARTE_LOG_ERR("JSON Parse Error: %lld", ret);
        return ASTARTE_RESULT_JSON_ERROR;
    }
    if (!parsed_json.data.protocols.astarte_mqtt_v1.broker_url) {
        ASTARTE_LOG_ERR("Parsed JSON is missing the broker URL field.");
        return ASTARTE_RESULT_JSON_ERROR;
    }
    if (strlen(parsed_json.data.protocols.astarte_mqtt_v1.broker_url)
        > ASTARTE_PAIRING_MAX_BROKER_URL_LEN) {
        ASTARTE_LOG_ERR("Received a broker URL exceeding the maximum allowed length: '%s'.",
            parsed_json.data.protocols.astarte_mqtt_v1.broker_url);
        return ASTARTE_RESULT_JSON_ERROR;
    }

    int snprintf_rc = snprintf(
        out_url, out_url_size, "%s", parsed_json.data.protocols.astarte_mqtt_v1.broker_url);
    if ((snprintf_rc < 0) || (snprintf_rc >= out_url_size)) {
        ASTARTE_LOG_ERR("Error extracting the broker URL from the parsed json.");
        return ASTARTE_RESULT_JSON_ERROR;
    }
    ASTARTE_LOG_DBG("Received broker URL: %s", out_url);
    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_pairing_serialize_get_client_certificate_payload(
    const char *csr, char *out_buff, size_t out_buff_size)
{
    struct payload_s
    {
        struct data_s
        {
            const char *csr;
        } data;
    };

    static const struct json_obj_descr data_descr[] = {
        JSON_OBJ_DESCR_PRIM(struct data_s, csr, JSON_TOK_STRING),
    };

    static const struct json_obj_descr payload_descr[] = {
        JSON_OBJ_DESCR_OBJECT(struct payload_s, data, data_descr),
    };

    struct payload_s payload = { .data = { .csr = csr } };

    ssize_t len = json_calc_encoded_len(payload_descr, ARRAY_SIZE(payload_descr), &payload);
    if ((len <= 0) || (len > out_buff_size)) {
        ASTARTE_LOG_ERR("Insufficient buffer, requiring %zd bytes", len);
        return ASTARTE_RESULT_JSON_ERROR;
    }

    int ret = json_obj_encode_buf(
        payload_descr, ARRAY_SIZE(payload_descr), &payload, out_buff, out_buff_size);
    if (ret != 0) {
        ASTARTE_LOG_ERR("Error encoding payload: %d", ret);
        return ASTARTE_RESULT_JSON_ERROR;
    }
    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_pairing_deserialize_get_client_certificate_response(
    char *resp_buf, char *out_deb, size_t out_deb_size)
{
    struct full_json_s
    {
        struct data_s
        {
            const char *client_crt;
        } data;
    };
    static const struct json_obj_descr data_descr[] = {
        JSON_OBJ_DESCR_PRIM(struct data_s, client_crt, JSON_TOK_STRING),
    };
    static const struct json_obj_descr full_json_descr[] = {
        JSON_OBJ_DESCR_OBJECT(struct full_json_s, data, data_descr),
    };

    int expected_return_code = (1U << (size_t) ARRAY_SIZE(full_json_descr)) - 1;
    struct full_json_s parsed_json = { .data.client_crt = NULL };
    int64_t ret = json_obj_parse(
        resp_buf, strlen(resp_buf) + 1, full_json_descr, ARRAY_SIZE(full_json_descr), &parsed_json);

    if (ret != expected_return_code) {
        ASTARTE_LOG_ERR("JSON Parse Error: %lld", ret);
        return ASTARTE_RESULT_JSON_ERROR;
    }
    if (!parsed_json.data.client_crt) {
        ASTARTE_LOG_ERR("Parsed JSON is missing the client_crt field.");
        return ASTARTE_RESULT_JSON_ERROR;
    }
    if (strlen(parsed_json.data.client_crt) == 0) {
        ASTARTE_LOG_ERR("Received empty client certificate");
        return ASTARTE_RESULT_JSON_ERROR;
    }

    if (strlen(parsed_json.data.client_crt)
        >= CONFIG_ASTARTE_DEVICE_SDK_ADVANCED_CLIENT_CRT_BUFFER_SIZE) {
        ASTARTE_LOG_ERR("Received client certificate is too long.");
        return ASTARTE_RESULT_INVALID_PARAM;
    }
    int snprintf_rc = snprintf(out_deb, out_deb_size, "%s", parsed_json.data.client_crt);
    if ((snprintf_rc < 0) || (snprintf_rc >= out_deb_size)) {
        ASTARTE_LOG_ERR("Error extracting the client certificate from the parsed json.");
        return ASTARTE_RESULT_JSON_ERROR;
    }
    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_pairing_serialize_verify_client_certificate_payload(
    const char *crt_pem, char *out_buff, size_t out_buff_size)
{
    struct payload_s
    {
        struct data_s
        {
            const char *client_crt;
        } data;
    };

    static const struct json_obj_descr data_descr[] = {
        JSON_OBJ_DESCR_PRIM(struct data_s, client_crt, JSON_TOK_STRING),
    };

    static const struct json_obj_descr payload_descr[] = {
        JSON_OBJ_DESCR_OBJECT(struct payload_s, data, data_descr),
    };

    struct payload_s payload = { .data = { .client_crt = crt_pem } };

    ssize_t len = json_calc_encoded_len(payload_descr, ARRAY_SIZE(payload_descr), &payload);
    if ((len <= 0) || (len > out_buff_size)) {
        ASTARTE_LOG_ERR("Insufficient buffer, requiring %zd bytes", len);
        return ASTARTE_RESULT_JSON_ERROR;
    }

    int ret = json_obj_encode_buf(
        payload_descr, ARRAY_SIZE(payload_descr), &payload, out_buff, out_buff_size);
    if (ret != 0) {
        ASTARTE_LOG_ERR("Error encoding payload: %d", ret);
        return ASTARTE_RESULT_JSON_ERROR;
    }
    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_pairing_deserialize_verify_client_certificate_response(char *resp_buf)
{
    // Zephyr JSON parser does not support null values in parsing. Since a certificate invalid
    // response contains a null value, it should be replaced with an empty string.
    char *details_null = strstr(resp_buf, "\"details\":" JSON_NULL);
    if (details_null) {
        char details_null_replacement[] = "\"details\":" JSON_NULL_REPLACEMENT;
        // We perform a memcpy if a string excluding the null termination char on purpose as
        // this is only a substring of an already null terminated string
        // NOLINTNEXTLINE(bugprone-not-null-terminated-result)
        memcpy(details_null, details_null_replacement, strlen(details_null_replacement));
    }

    struct full_json_s
    {
        struct data_s
        {
            const char *timestamp;
            bool valid;
            const char *until;
            const char *details;
            const char *cause;
        } data;
    };

    static const struct json_obj_descr data_descr[] = {
        JSON_OBJ_DESCR_PRIM(struct data_s, timestamp, JSON_TOK_STRING),
        JSON_OBJ_DESCR_PRIM(struct data_s, valid, JSON_TOK_TRUE),
        JSON_OBJ_DESCR_PRIM(struct data_s, until, JSON_TOK_STRING),
        JSON_OBJ_DESCR_PRIM(struct data_s, details, JSON_TOK_STRING),
        JSON_OBJ_DESCR_PRIM(struct data_s, cause, JSON_TOK_STRING),
    };
    static const struct json_obj_descr full_json_descr[] = {
        JSON_OBJ_DESCR_OBJECT(struct full_json_s, data, data_descr),
    };

    int expected_return_code = (1U << (size_t) ARRAY_SIZE(full_json_descr)) - 1;
    struct full_json_s parsed_json = { .data.timestamp = NULL,
        .data.valid = false,
        .data.until = NULL,
        .data.details = NULL,
        .data.cause = NULL };

    int64_t ret = json_obj_parse(
        resp_buf, strlen(resp_buf) + 1, full_json_descr, ARRAY_SIZE(full_json_descr), &parsed_json);

    if (ret != expected_return_code) {
        ASTARTE_LOG_ERR("JSON Parse Error: %lld", ret);
        return ASTARTE_RESULT_JSON_ERROR;
    }

    if (!parsed_json.data.valid) {
        if (!parsed_json.data.cause) {
            ASTARTE_LOG_ERR("Parsed JSON is missing the cause field.");
            return ASTARTE_RESULT_JSON_ERROR;
        }
        if (strlen(parsed_json.data.cause) == 0) {
            ASTARTE_LOG_ERR("Received empty cause field.");
            return ASTARTE_RESULT_JSON_ERROR;
        }
        ASTARTE_LOG_WRN("Invalid certificate, reason: %s", parsed_json.data.cause);
        return ASTARTE_RESULT_CLIENT_CERT_INVALID;
    }
    return ASTARTE_RESULT_OK;
}
