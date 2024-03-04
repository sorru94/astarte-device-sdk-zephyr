/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "astarte_device_sdk/pairing.h"
#include "pairing_private.h"

#include <zephyr/data/json.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "crypto.h"
#include "http.h"

LOG_MODULE_REGISTER(astarte_pairing, CONFIG_ASTARTE_DEVICE_SDK_PAIRING_LOG_LEVEL); // NOLINT

/************************************************
 *       Checks over configuration values       *
 ***********************************************/

#if !defined(CONFIG_ASTARTE_DEVICE_SDK_GENERATE_DEVICE_ID)
#define DEVICE_ID_BASE64_LEN 22
BUILD_ASSERT(sizeof(CONFIG_ASTARTE_DEVICE_SDK_DEVICE_ID) == (DEVICE_ID_BASE64_LEN + 1),
    "Configured device ID has incorrect size");
#else
#error "Dynamic generation of device ID is not yet supported"
#endif /* !defined(CONFIG_ASTARTE_DEVICE_SDK_GENERATE_DEVICE_ID) */

BUILD_ASSERT(sizeof(CONFIG_ASTARTE_DEVICE_SDK_REALM_NAME) != 1, "Missing realm name");

// The JSON parser module for Zephyr does not support the null keyword
// We'll replace the null keyword with a substitute string before parsing.
#define JSON_NULL "null"
#define JSON_NULL_REPLACEMENT "\"..\""
BUILD_ASSERT(sizeof(JSON_NULL) == sizeof(JSON_NULL_REPLACEMENT),
    "JSON null replacement string has incorrect size.");

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

// For more information on the Astarte pairing API, such as possible responses, see the Astarte
// documentation.

// Payload will be a json like: {"data":{"hw_id":"<DEVICE_ID>"}}
// The device ID will be no longer than 128 bits encoded in base64 resulting in 22 useful chars.
#define REGISTER_DEVICE_PAYLOAD_MAX_SIZE 50
// Response will be a json like: {"data":{"credentials_secret":"<CRED_SECR>"}}
// The credential secret will be 44 chars long.
#define REGISTER_DEVICE_RESPONSE_MAX_SIZE (50 + ASTARTE_PAIRING_CRED_SECR_LEN)

// Correct response will be a json like: {"data":{"protocols":{"astarte_mqtt_v1":"<BROKER_URL>"}}}
#define GET_BROKER_URL_RESPONSE_MAX_SIZE (50 + ASTARTE_PAIRING_MAX_BROKER_URL_LEN)

// Payload will be a json like: {"data":{"csr":"<CSR>"}}
#define GET_CLIENT_CRT_PAYLOAD_MAX_SIZE (25 + ASTARTE_CRYPTO_CSR_BUFFER_SIZE)
// Correct response will be a json like: {"data":{"client_crt":"<CLIENT_CRT>"}}
// The maximum size of the client certificate may vary depending on the server configuration.
#define GET_CLIENT_CRT_RESPONSE_MAX_SIZE                                                           \
    (50 + CONFIG_ASTARTE_DEVICE_SDK_ADVANCED_CLIENT_CRT_BUFFER_SIZE)

// Correct payload will be a json like: {"data":{"client_crt":"<CLIENT_CRT>"}}
// The maximum size of the client certificate may vary depending on the server configuration.
#define VERIFY_CLIENT_CRT_PAYLOAD_MAX_SIZE                                                         \
    (50 + CONFIG_ASTARTE_DEVICE_SDK_ADVANCED_CLIENT_CRT_BUFFER_SIZE)
// Correct response will be a json like:
// {"data":{"timestamp":"2024-02-27 14:03:47.229Z","until":"2024-02-27 14:08:00.000Z","valid":true}}
// {"data":{"cause":"EXPIRED","details":null,"timestamp":"2024-02-27 14:15:56.480Z","valid":false}}
#define VERIFY_CLIENT_CRT_RESPONSE_MAX_SIZE 128

// Standard authorization header start string.
#define AUTH_HEADER_BEARER_STR_START "Authorization: Bearer "
// Standard authorization header end string.
#define AUTH_HEADER_BEARER_STR_END "\r\n"
// Authorization header fixed size when used with a credential secret.
// Composed by: "Authorization: Bearer " (23) + <CRED_SECRET> (44) + "\r\n" (3) + null term (1)
#define AUTH_HEADER_CRED_SECRET_SIZE                                                               \
    (sizeof(AUTH_HEADER_BEARER_STR_START) + ASTARTE_PAIRING_CRED_SECR_LEN                          \
        + sizeof(AUTH_HEADER_BEARER_STR_END) - 1)

/************************************************
 *       Callbacks declaration/definition       *
 ***********************************************/

/************************************************
 *         Static functions declaration         *
 ***********************************************/

/**
 * @brief Encode the payload for the device registration HTTP request.
 *
 * @param[in] device_id Device ID to be encoded in the payload.
 * @param[out] out_buff Buffer where to store the encoded payload.
 * @param[in] out_buff_size Size of the output buffer.
 * @return ASTARTE_OK if successful, otherwise an error code.
 */
static astarte_err_t encode_register_device_payload(
    const char *device_id, char *out_buff, size_t out_buff_size);

/**
 * @brief Parse the response from the device registration HTTP request.
 *
 * @param[in] resp_buf Response to parse.
 * @param[out] out_cred_secr Returned credential secret.
 * @return ASTARTE_OK if successful, otherwise an error code.
 */
static astarte_err_t parse_register_device_response(char *resp_buf, char *out_cred_secr);

/**
 * @brief Parse the response from the get broker url HTTP request.
 *
 * @param[in] resp_buf Response to parse.
 * @param[out] out_url Returned MQTT broker URL.
 * @return ASTARTE_OK if successful, otherwise an error code.
 */
static astarte_err_t parse_get_borker_url_response(char *resp_buf, char *out_url);

/**
 * @brief Encode the payload for the get client certificate HTTP request.
 *
 * @param[in] csr Certificate signing request to be encoded in the payload.
 * @param[out] out_buff Buffer where to store the encoded payload.
 * @param[in] out_buff_size Size of the output buffer.
 * @return ASTARTE_OK if successful, otherwise an error code.
 */
static astarte_err_t encode_get_client_certificate_payload(
    const char *csr, char *out_buff, size_t out_buff_size);

/**
 * @brief Parse the response from the get client certificate HTTP request.
 *
 * @param[in] resp_buf Response to parse.
 * @param[out] out_deb Returned deb certificate.
 * @return ASTARTE_OK if successful, otherwise an error code.
 */
static astarte_err_t parse_get_client_certificate_response(char *resp_buf, char *out_deb);

/**
 * @brief Encode the payload for the verify client certificate HTTP request.
 *
 * @param[in] crt_pem Certificate to be encoded in the payload.
 * @param[out] out_buff Buffer where to store the encoded payload.
 * @param[in] out_buff_size Size of the output buffer.
 * @return ASTARTE_OK if successful, otherwise an error code.
 */
static astarte_err_t encode_verify_client_certificate_payload(
    const char *crt_pem, char *out_buff, size_t out_buff_size);

/**
 * @brief Parse the response from the verify client certificate HTTP request.
 *
 * @param[in] resp_buf Response to parse.
 * @return ASTARTE_OK if successful, otherwise an error code.
 */
static astarte_err_t parse_verify_client_certificate_response(char *resp_buf);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

// NOLINTNEXTLINE(hicpp-function-size)
astarte_err_t astarte_pairing_register_device(
    int32_t timeout_ms, char *out_cred_secr, size_t out_cred_secr_size)
{
    astarte_err_t astarte_err = ASTARTE_OK;
    // Step 1: check the configuration and input parameters
    if (sizeof(CONFIG_ASTARTE_DEVICE_SDK_PAIRING_JWT) <= 1) {
        LOG_ERR("Registration of a device requires a valid pairing JWT"); // NOLINT
        return ASTARTE_ERR_CONFIGURATION;
    }
    if (out_cred_secr_size <= ASTARTE_PAIRING_CRED_SECR_LEN) {
        LOG_ERR("Insufficient output buffer size for credential secret."); // NOLINT
        return ASTARTE_ERR_INVALID_PARAM;
    }

    // Step 2: register the device and get the credential secret
    char url[] = "/pairing/v1/" CONFIG_ASTARTE_DEVICE_SDK_REALM_NAME "/agent/devices";
    char auth_header[] = { AUTH_HEADER_BEARER_STR_START CONFIG_ASTARTE_DEVICE_SDK_PAIRING_JWT
            AUTH_HEADER_BEARER_STR_END };
    const char *header_fields[] = { auth_header, NULL };
    char payload[REGISTER_DEVICE_PAYLOAD_MAX_SIZE] = { 0 };
    astarte_err = encode_register_device_payload(
        CONFIG_ASTARTE_DEVICE_SDK_DEVICE_ID, payload, REGISTER_DEVICE_PAYLOAD_MAX_SIZE);
    if (astarte_err != ASTARTE_OK) {
        return astarte_err;
    }
    char resp_buf[REGISTER_DEVICE_RESPONSE_MAX_SIZE] = { 0 };
    astarte_err
        = astarte_http_post(url, header_fields, payload, timeout_ms, resp_buf, sizeof(resp_buf));
    if (astarte_err != ASTARTE_OK) {
        return astarte_err;
    }

    // Step 3: process the result
    return parse_register_device_response(resp_buf, out_cred_secr);
}

// NOLINTNEXTLINE(hicpp-function-size)
astarte_err_t astarte_pairing_get_broker_url(
    int32_t timeout_ms, const char *cred_secr, char *out_url, size_t out_url_size)
{
    astarte_err_t astarte_err = ASTARTE_OK;
    // Step 1: check the input parameters
    if (strlen(cred_secr) != ASTARTE_PAIRING_CRED_SECR_LEN) {
        LOG_ERR("Incorrect length for credential secret."); // NOLINT
        return ASTARTE_ERR_INVALID_PARAM;
    }
    if (out_url_size <= ASTARTE_PAIRING_MAX_BROKER_URL_LEN) {
        LOG_ERR("Insufficient output buffer size for broker URL."); // NOLINT
        return ASTARTE_ERR_INVALID_PARAM;
    }

    // Step 2: Get the MQTT broker URL
    char auth_header[AUTH_HEADER_CRED_SECRET_SIZE] = { 0 };
    // NOLINTNEXTLINE (cert-err33-c) Above checks ensure auth_header is large enough for this string
    snprintf(auth_header, AUTH_HEADER_CRED_SECRET_SIZE,
        AUTH_HEADER_BEARER_STR_START "%s" AUTH_HEADER_BEARER_STR_END, cred_secr);
    const char *header_fields[] = { auth_header, NULL };

    char url[] = "/pairing/v1/" CONFIG_ASTARTE_DEVICE_SDK_REALM_NAME
                 "/devices/" CONFIG_ASTARTE_DEVICE_SDK_DEVICE_ID;
    char resp_buf[GET_BROKER_URL_RESPONSE_MAX_SIZE] = { 0 };
    astarte_err = astarte_http_get(url, header_fields, timeout_ms, resp_buf, sizeof(resp_buf));
    if (astarte_err != ASTARTE_OK) {
        return astarte_err;
    }

    // Step 3: process the result
    return parse_get_borker_url_response(resp_buf, out_url);
}

// NOLINTNEXTLINE(hicpp-function-size)
astarte_err_t astarte_pairing_get_client_certificate(int32_t timeout_ms, const char *cred_secr,
    unsigned char *privkey_pem, size_t privkey_pem_size, char *crt_pem, size_t crt_pem_size)
{
    astarte_err_t astarte_err = ASTARTE_OK;
    // Step 1: check the configuration and input parameters
    if (strlen(cred_secr) != ASTARTE_PAIRING_CRED_SECR_LEN) {
        LOG_ERR("Incorrect length for credential secret."); // NOLINT
        return ASTARTE_ERR_INVALID_PARAM;
    }
    if (crt_pem_size < CONFIG_ASTARTE_DEVICE_SDK_ADVANCED_CLIENT_CRT_BUFFER_SIZE) {
        LOG_ERR("Insufficient output buffer size for client certificate."); // NOLINT
        return ASTARTE_ERR_INVALID_PARAM;
    }

    // Step 2: create a private key and a CSR
    astarte_err_t crypto_rc = astarte_crypto_create_key(privkey_pem, privkey_pem_size);
    if (crypto_rc != ASTARTE_OK) {
        LOG_ERR("Failed in creating a private key."); // NOLINT
        return crypto_rc;
    }

    unsigned char csr_buf[ASTARTE_CRYPTO_CSR_BUFFER_SIZE];
    crypto_rc = astarte_crypto_create_csr(privkey_pem, csr_buf, sizeof(csr_buf));
    if (crypto_rc != ASTARTE_OK) {
        LOG_ERR("Failed in creating a CSR."); // NOLINT
        return crypto_rc;
    }

    // Step 3: get the client certificate from the server
    char url[]
        = "/pairing/v1/" CONFIG_ASTARTE_DEVICE_SDK_REALM_NAME
          "/devices/" CONFIG_ASTARTE_DEVICE_SDK_DEVICE_ID "/protocols/astarte_mqtt_v1/credentials";
    char auth_header[AUTH_HEADER_CRED_SECRET_SIZE] = { 0 };
    // NOLINTNEXTLINE (cert-err33-c) Above checks ensure auth_header is large enough for this string
    snprintf(auth_header, AUTH_HEADER_CRED_SECRET_SIZE,
        AUTH_HEADER_BEARER_STR_START "%s" AUTH_HEADER_BEARER_STR_END, cred_secr);
    const char *header_fields[] = { auth_header, NULL };
    char payload[GET_CLIENT_CRT_PAYLOAD_MAX_SIZE] = { 0 };
    astarte_err
        = encode_get_client_certificate_payload(csr_buf, payload, GET_CLIENT_CRT_PAYLOAD_MAX_SIZE);
    if (astarte_err != ASTARTE_OK) {
        return astarte_err;
    }
    char resp_buf[GET_CLIENT_CRT_RESPONSE_MAX_SIZE] = { 0 };
    astarte_err
        = astarte_http_post(url, header_fields, payload, timeout_ms, resp_buf, sizeof(resp_buf));
    if (astarte_err != ASTARTE_OK) {
        return astarte_err;
    }

    // Step 4: process the result
    astarte_err = parse_get_client_certificate_response(resp_buf, crt_pem);
    if (astarte_err != ASTARTE_OK) {
        return astarte_err;
    }

    // Step 5: convert the received certificate to a valid PEM certificate
    // Replace "\n" with newlines chars
    char *tmp = NULL;
    while ((tmp = strstr(crt_pem, "\\n")) != NULL) {
        tmp[0] = '\n';
        tmp[1] = '\n';
    }

    LOG_DBG("Received client certificate: %s", crt_pem); // NOLINT

    return ASTARTE_OK;
}

// NOLINTNEXTLINE(hicpp-function-size)
astarte_err_t astarte_pairing_verify_client_certificate(
    int32_t timeout_ms, const char *cred_secr, const char *crt_pem)
{
    astarte_err_t astarte_err = ASTARTE_OK;
    // Step 1: check the input parameters
    if (strlen(cred_secr) != ASTARTE_PAIRING_CRED_SECR_LEN) {
        LOG_ERR("Incorrect length for credential secret."); // NOLINT
        return ASTARTE_ERR_INVALID_PARAM;
    }

    // Step 2: register the device and get the credential secret
    char url[] = "/pairing/v1/" CONFIG_ASTARTE_DEVICE_SDK_REALM_NAME
                 "/devices/" CONFIG_ASTARTE_DEVICE_SDK_DEVICE_ID
                 "/protocols/astarte_mqtt_v1/credentials/verify";
    char auth_header[AUTH_HEADER_CRED_SECRET_SIZE] = { 0 };
    // NOLINTNEXTLINE (cert-err33-c) Above checks ensure auth_header is large enough for this string
    snprintf(auth_header, AUTH_HEADER_CRED_SECRET_SIZE,
        AUTH_HEADER_BEARER_STR_START "%s" AUTH_HEADER_BEARER_STR_END, cred_secr);
    const char *header_fields[] = { auth_header, NULL };
    char payload[VERIFY_CLIENT_CRT_PAYLOAD_MAX_SIZE] = { 0 };
    astarte_err = encode_verify_client_certificate_payload(
        crt_pem, payload, VERIFY_CLIENT_CRT_PAYLOAD_MAX_SIZE);
    if (astarte_err != ASTARTE_OK) {
        return astarte_err;
    }
    char resp_buf[VERIFY_CLIENT_CRT_RESPONSE_MAX_SIZE] = { 0 };
    astarte_err
        = astarte_http_post(url, header_fields, payload, timeout_ms, resp_buf, sizeof(resp_buf));
    if (astarte_err != ASTARTE_OK) {
        return astarte_err;
    }

    // Step 3: process the result
    return parse_verify_client_certificate_response(resp_buf);
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static astarte_err_t encode_register_device_payload(
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
        LOG_ERR("Insufficient buffer, requiring %zd bytes", len); // NOLINT
        return ASTARTE_ERR_JSON;
    }

    int ret = json_obj_encode_buf(
        payload_descr, ARRAY_SIZE(payload_descr), &payload, out_buff, out_buff_size);
    if (ret != 0) {
        LOG_ERR("Error encoding payload: %d", ret); // NOLINT
        return ASTARTE_ERR_JSON;
    }
    return ASTARTE_OK;
}

static astarte_err_t parse_register_device_response(char *resp_buf, char *out_cred_secr)
{
    // Define the structure of the expected JSON file
    struct full_json_s
    {
        struct data_s
        {
            const char *credentials_secret;
        } data;
    };
    // Create the descriptors list containing each element of the struct
    static const struct json_obj_descr data_descr[] = {
        JSON_OBJ_DESCR_PRIM(struct data_s, credentials_secret, JSON_TOK_STRING),
    };
    static const struct json_obj_descr full_json_descr[] = {
        JSON_OBJ_DESCR_OBJECT(struct full_json_s, data, data_descr),
    };
    // This is only for the outside level, nested elements should be checked individually.
    int expected_return_code = (1U << (size_t) ARRAY_SIZE(full_json_descr)) - 1;
    // Parse the json into a structure
    struct full_json_s parsed_json = { .data.credentials_secret = NULL };
    int64_t ret = json_obj_parse(
        resp_buf, strlen(resp_buf) + 1, full_json_descr, ARRAY_SIZE(full_json_descr), &parsed_json);
    if (ret != expected_return_code) {
        LOG_ERR("JSON Parse Error: %lld", ret); // NOLINT
        return ASTARTE_ERR_JSON;
    }
    if (!parsed_json.data.credentials_secret) {
        LOG_ERR("Parsed JSON is missing the credentials_secret field."); // NOLINT
        return ASTARTE_ERR_JSON;
    }
    if (strlen(parsed_json.data.credentials_secret) != ASTARTE_PAIRING_CRED_SECR_LEN) {
        // NOLINTNEXTLINE
        LOG_ERR("Received credential secret of unexpected length: '%s'.",
            parsed_json.data.credentials_secret);
        return ASTARTE_ERR_JSON;
    }

    // Copy the received credential secret in the output buffer
    strncpy(out_cred_secr, parsed_json.data.credentials_secret, ASTARTE_PAIRING_CRED_SECR_LEN + 1);
    LOG_DBG("Received credentials secret: %s", out_cred_secr); // NOLINT
    return ASTARTE_OK;
}

static astarte_err_t parse_get_borker_url_response(char *resp_buf, char *out_url)
{
    // Define the structure of the expected JSON file
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
    // Create the descriptors list containing each element of the struct
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
    // This is only for the outside level, nested elements should be checked individually.
    int expected_return_code = (1U << (size_t) ARRAY_SIZE(full_json_descr)) - 1;
    // Parse the json into a structure
    struct full_json_s parsed_json = { .data.protocols.astarte_mqtt_v1.broker_url = NULL };
    int64_t ret = json_obj_parse(
        resp_buf, strlen(resp_buf) + 1, full_json_descr, ARRAY_SIZE(full_json_descr), &parsed_json);
    if (ret != expected_return_code) {
        LOG_ERR("JSON Parse Error: %lld", ret); // NOLINT
        return ASTARTE_ERR_JSON;
    }
    if (!parsed_json.data.protocols.astarte_mqtt_v1.broker_url) {
        LOG_ERR("Parsed JSON is missing the broker URL field."); // NOLINT
        return ASTARTE_ERR_JSON;
    }
    if (strlen(parsed_json.data.protocols.astarte_mqtt_v1.broker_url)
        > ASTARTE_PAIRING_MAX_BROKER_URL_LEN) {
        // NOLINTNEXTLINE
        LOG_ERR("Received a broker URL exceeding the maximum allowed length: '%s'.",
            parsed_json.data.protocols.astarte_mqtt_v1.broker_url);
        return ASTARTE_ERR_JSON;
    }

    // Copy the received credential secret in the output buffer
    strncpy(out_url, parsed_json.data.protocols.astarte_mqtt_v1.broker_url,
        ASTARTE_PAIRING_MAX_BROKER_URL_LEN + 1);
    LOG_DBG("Received broker URL: %s", out_url); // NOLINT
    return ASTARTE_OK;
}

static astarte_err_t encode_get_client_certificate_payload(
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
        LOG_ERR("Insufficient buffer, requiring %zd bytes", len); // NOLINT
        return ASTARTE_ERR_JSON;
    }

    int ret = json_obj_encode_buf(
        payload_descr, ARRAY_SIZE(payload_descr), &payload, out_buff, out_buff_size);
    if (ret != 0) {
        LOG_ERR("Error encoding payload: %d", ret); // NOLINT
        return ASTARTE_ERR_JSON;
    }
    return ASTARTE_OK;
}

static astarte_err_t parse_get_client_certificate_response(char *resp_buf, char *out_deb)
{
    // Define the structure of the expected JSON file
    struct full_json_s
    {
        struct data_s
        {
            const char *client_crt;
        } data;
    };
    // Create the descriptors list containing each element of the struct
    static const struct json_obj_descr data_descr[] = {
        JSON_OBJ_DESCR_PRIM(struct data_s, client_crt, JSON_TOK_STRING),
    };
    static const struct json_obj_descr full_json_descr[] = {
        JSON_OBJ_DESCR_OBJECT(struct full_json_s, data, data_descr),
    };
    // This is only for the outside level, nested elements should be checked individually.
    int expected_return_code = (1U << (size_t) ARRAY_SIZE(full_json_descr)) - 1;
    // Parse the json into a structure
    struct full_json_s parsed_json = { .data.client_crt = NULL };
    int64_t ret = json_obj_parse(
        resp_buf, strlen(resp_buf) + 1, full_json_descr, ARRAY_SIZE(full_json_descr), &parsed_json);
    if (ret != expected_return_code) {
        LOG_ERR("JSON Parse Error: %lld", ret); // NOLINT
        return ASTARTE_ERR_JSON;
    }
    if (!parsed_json.data.client_crt) {
        LOG_ERR("Parsed JSON is missing the client_crt field."); // NOLINT
        return ASTARTE_ERR_JSON;
    }
    if (strlen(parsed_json.data.client_crt) == 0) {
        LOG_ERR("Received empty client certificate"); // NOLINT
        return ASTARTE_ERR_JSON;
    }

    // Copy the received client certificate in the output buffer
    if (strlen(parsed_json.data.client_crt)
        >= CONFIG_ASTARTE_DEVICE_SDK_ADVANCED_CLIENT_CRT_BUFFER_SIZE) {
        LOG_ERR("Received client certificate is too long."); // NOLINT
        return ASTARTE_ERR_INVALID_PARAM;
    }
    strncpy(out_deb, parsed_json.data.client_crt,
        CONFIG_ASTARTE_DEVICE_SDK_ADVANCED_CLIENT_CRT_BUFFER_SIZE);
    return ASTARTE_OK;
}

static astarte_err_t encode_verify_client_certificate_payload(
    const char *crt_pem, char *out_buff, size_t out_buff_size)
{
    // Define the structure of the expected JSON file
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
        LOG_ERR("Insufficient buffer, requiring %zd bytes", len); // NOLINT
        return ASTARTE_ERR_JSON;
    }

    int ret = json_obj_encode_buf(
        payload_descr, ARRAY_SIZE(payload_descr), &payload, out_buff, out_buff_size);
    if (ret != 0) {
        LOG_ERR("Error encoding payload: %d", ret); // NOLINT
        return ASTARTE_ERR_JSON;
    }
    return ASTARTE_OK;
}

static astarte_err_t parse_verify_client_certificate_response(char *resp_buf)
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

    // Define the structure of the expected JSON file
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
    // Create the descriptors list containing each element of the struct
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
    // This is only for the outside level, nested elements should be checked individually.
    int expected_return_code = (1U << (size_t) ARRAY_SIZE(full_json_descr)) - 1;
    // Parse the json into a structure
    struct full_json_s parsed_json = { .data.timestamp = NULL,
        .data.valid = false,
        .data.until = NULL,
        .data.details = NULL,
        .data.cause = NULL };
    int64_t ret = json_obj_parse(
        resp_buf, strlen(resp_buf) + 1, full_json_descr, ARRAY_SIZE(full_json_descr), &parsed_json);
    if (ret != expected_return_code) {
        LOG_ERR("JSON Parse Error: %lld", ret); // NOLINT
        return ASTARTE_ERR_JSON;
    }

    // Check the certificate validity
    if (!parsed_json.data.valid) {
        if (!parsed_json.data.cause) {
            LOG_ERR("Parsed JSON is missing the cause field."); // NOLINT
            return ASTARTE_ERR_JSON;
        }
        if (strlen(parsed_json.data.cause) == 0) {
            LOG_ERR("Received empty cause field."); // NOLINT
            return ASTARTE_ERR_JSON;
        }
        LOG_WRN("Invalid certificate, reason: %s", parsed_json.data.cause); // NOLINT
        return ASTARTE_ERR_CLIENT_CERT_INVALID;
    }
    return ASTARTE_OK;
}
