/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "pairing/core.h"

#include "astarte_device_sdk/pairing.h"
#include "pairing/serdes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "crypto.h"
#include "http.h"
#include "log.h"

ASTARTE_LOG_MODULE_REGISTER(astarte_pairing, CONFIG_ASTARTE_DEVICE_SDK_PAIRING_LOG_LEVEL);

/************************************************
 *       Checks over configuration values       *
 ***********************************************/

BUILD_ASSERT(sizeof(CONFIG_ASTARTE_DEVICE_SDK_REALM_NAME) != 1, "Missing realm name");

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

// Payload will be a json like: {"data":{"hw_id":"<DEVICE_ID>"}}
// The device ID will be no longer than 128 bits encoded in base64 resulting in 22 useful chars.
#define REGISTER_DEVICE_PAYLOAD_MAX_SIZE 50
// Response will be a json like: {"data":{"credentials_secret":"<CRED_SECR>"}}
// The credential secret will be 44 chars long.
#define REGISTER_DEVICE_RESPONSE_MAX_SIZE (50 + ASTARTE_PAIRING_CRED_SECR_LEN)

// Correct response will be a json like: {"data":{"protocols":{"astarte_mqtt_v1":"<BROKER_URL>"}}}
#define GET_BROKER_INFO_RESPONSE_MAX_SIZE (50 + ASTARTE_PAIRING_MAX_BROKER_URL_LEN)

// Payload will be a json like: {"data":{"csr":"<CSR>"}}
#define GET_CLIENT_CRT_PAYLOAD_MAX_SIZE (25 + ASTARTE_TLS_CREDENTIALS_CSR_BUFFER_SIZE)
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

/** @brief Generic URL prefix to be used for all https calls to the pairing APIs. */
#define PAIRING_URL_PREFIX "/pairing/v1/" CONFIG_ASTARTE_DEVICE_SDK_REALM_NAME
/** @brief URL for https calls to the device registration utility of the pairing APIs. */
#define PAIRING_REGISTRATION_URL PAIRING_URL_PREFIX "/agent/devices"
/** @brief Generic URL prefix for https calls to the device mgmt utility of the pairing APIs. */
#define PAIRING_DEVICE_MGMT_URL_PREFIX PAIRING_URL_PREFIX "/devices/"
/** @brief URL suffix for the https call to the device certificate generation utility */
#define PAIRING_DEVICE_CERT_URL_SUFFIX "/protocols/astarte_mqtt_v1/credentials"
/** @brief URL suffix for the https call to the device certificate verification utility */
#define PAIRING_DEVICE_CERT_CHECK_URL_SUFFIX PAIRING_DEVICE_CERT_URL_SUFFIX "/verify"

/** @brief Size in chars of the #PAIRING_DEVICE_MGMT_URL_PREFIX string. */
#define PAIRING_DEVICE_MGMT_URL_PREFIX_LEN (sizeof(PAIRING_DEVICE_MGMT_URL_PREFIX) - 1)
/** @brief Size in chars of the URL for a 'get broker info' HTTPs request to the pairing APIs. */
#define PAIRING_DEVICE_GET_BROKER_INFO_URL_LEN                                                     \
    (PAIRING_DEVICE_MGMT_URL_PREFIX_LEN + ASTARTE_DEVICE_ID_LEN)
/** @brief Size in chars of the #PAIRING_DEVICE_CERT_URL_SUFFIX string. */
#define PAIRING_DEVICE_CERT_URL_SUFFIX_LEN (sizeof(PAIRING_DEVICE_CERT_URL_SUFFIX) - 1)
/** @brief Size in chars of the URL for a 'get device cert' HTTPs request to the pairing APIs. */
#define PAIRING_DEVICE_GET_DEVICE_CERT_URL_LEN                                                     \
    (PAIRING_DEVICE_MGMT_URL_PREFIX_LEN + ASTARTE_DEVICE_ID_LEN                                    \
        + PAIRING_DEVICE_CERT_URL_SUFFIX_LEN)
/** @brief Size in chars of the #PAIRING_DEVICE_CERT_CHECK_URL_SUFFIX string. */
#define PAIRING_DEVICE_CERT_CHECK_URL_SUFFIX_LEN (sizeof(PAIRING_DEVICE_CERT_CHECK_URL_SUFFIX) - 1)
/** @brief Size in chars of the URL for a 'verify device cert' HTTPs request to the pairing APIs. */
#define PAIRING_DEVICE_CERT_CHECK_URL_LEN                                                          \
    (PAIRING_DEVICE_MGMT_URL_PREFIX_LEN + ASTARTE_DEVICE_ID_LEN                                    \
        + PAIRING_DEVICE_CERT_CHECK_URL_SUFFIX_LEN)

/************************************************
 *         Static functions declaration         *
 ***********************************************/

/**
 * @brief Fetch the MQTT broker URL from Astarte.
 *
 * @param[in] timeout_ms Timeout to use for the HTTP operations in ms.
 * @param[in] device_id Unique identifier to use to register the device instance.
 * @param[in] cred_secr Credential secret to use as authorization token.
 * @param[out] out_url Output buffer where to store the fetched ULR.
 * @param[in] out_url_size Size of the output buffer for the URL.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
static astarte_result_t get_broker_info(int32_t timeout_ms, const char *device_id,
    const char *cred_secr, char *out_url, size_t out_url_size);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_pairing_register_device(
    int32_t timeout_ms, const char *device_id, char *out_cred_secr, size_t out_cred_secr_size)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    // Step 1: check the configuration and input parameters
    if (sizeof(CONFIG_ASTARTE_DEVICE_SDK_PAIRING_JWT) <= 1) {
        ASTARTE_LOG_ERR("Registration of a device requires a valid pairing JWT");
        return ASTARTE_RESULT_INVALID_CONFIGURATION;
    }
    if (strlen(device_id) != ASTARTE_DEVICE_ID_LEN) {
        ASTARTE_LOG_ERR(
            "Device ID has incorrect length, should be %d chars.", ASTARTE_DEVICE_ID_LEN);
        return ASTARTE_RESULT_INVALID_PARAM;
    }
    if (out_cred_secr_size <= ASTARTE_PAIRING_CRED_SECR_LEN) {
        ASTARTE_LOG_ERR("Insufficient output buffer size for credential secret.");
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    // Step 2: register the device and get the credential secret
    char url[] = PAIRING_REGISTRATION_URL;
    char auth_header[] = { AUTH_HEADER_BEARER_STR_START CONFIG_ASTARTE_DEVICE_SDK_PAIRING_JWT
            AUTH_HEADER_BEARER_STR_END };
    const char *header_fields[] = { auth_header, NULL };
    char payload[REGISTER_DEVICE_PAYLOAD_MAX_SIZE] = { 0 };

    ares = astarte_pairing_serialize_register_device_payload(
        device_id, payload, REGISTER_DEVICE_PAYLOAD_MAX_SIZE);
    if (ares != ASTARTE_RESULT_OK) {
        return ares;
    }

    char resp_buf[REGISTER_DEVICE_RESPONSE_MAX_SIZE] = { 0 };
    ares = astarte_http_post(timeout_ms, url, header_fields, payload, resp_buf, sizeof(resp_buf));
    if (ares != ASTARTE_RESULT_OK) {
        return ares;
    }

    // Step 3: process the result
    return astarte_pairing_deserialize_register_device_response(
        resp_buf, out_cred_secr, out_cred_secr_size);
}

astarte_result_t astarte_pairing_get_mqtt_broker_hostname_and_port(int32_t http_timeout_ms,
    const char *device_id, const char *cred_secr,
    char broker_hostname[static ASTARTE_MQTT_MAX_BROKER_HOSTNAME_LEN + 1],
    char broker_port[static ASTARTE_MQTT_MAX_BROKER_PORT_LEN + 1])
{
    char broker_url[ASTARTE_PAIRING_MAX_BROKER_URL_LEN + 1] = { 0 };
    astarte_result_t ares
        = get_broker_info(http_timeout_ms, device_id, cred_secr, broker_url, sizeof(broker_url));
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed in obtaining the MQTT broker URL");
        return ares;
    }
    int strncmp_rc = strncmp(broker_url, "mqtts://", strlen("mqtts://"));
    if (strncmp_rc != 0) {
        ASTARTE_LOG_ERR("MQTT broker URL is malformed");
        return ASTARTE_RESULT_HTTP_REQUEST_ERROR;
    }
    char *saveptr = NULL;
    char *broker_url_token = strtok_r(&broker_url[strlen("mqtts://")], ":", &saveptr);
    if (!broker_url_token) {
        ASTARTE_LOG_ERR("MQTT broker URL is malformed");
        return ASTARTE_RESULT_HTTP_REQUEST_ERROR;
    }
    int ret = snprintf(
        broker_hostname, ASTARTE_MQTT_MAX_BROKER_HOSTNAME_LEN + 1, "%s", broker_url_token);
    if ((ret <= 0) || (ret > ASTARTE_MQTT_MAX_BROKER_HOSTNAME_LEN)) {
        ASTARTE_LOG_ERR("Error encoding broker hostname.");
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }
    broker_url_token = strtok_r(NULL, "/", &saveptr);
    if (!broker_url_token) {
        ASTARTE_LOG_ERR("MQTT broker URL is malformed");
        return ASTARTE_RESULT_HTTP_REQUEST_ERROR;
    }
    ret = snprintf(broker_port, ASTARTE_MQTT_MAX_BROKER_PORT_LEN + 1, "%s", broker_url_token);
    if ((ret <= 0) || (ret > ASTARTE_MQTT_MAX_BROKER_PORT_LEN)) {
        ASTARTE_LOG_ERR("Error encoding broker port.");
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }
    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_pairing_get_client_certificate(int32_t timeout_ms, const char *device_id,
    const char *cred_secr, astarte_tls_credentials_client_crt_t *client_crt)
{
    astarte_result_t ares = ASTARTE_RESULT_INVALID_PARAM;
    // Step 1: check the configuration and input parameters
    if (strlen(device_id) != ASTARTE_DEVICE_ID_LEN) {
        ASTARTE_LOG_ERR(
            "Device ID has incorrect length, should be %d chars.", ASTARTE_DEVICE_ID_LEN);
        goto error;
    }

    // Step 2: create a private key and a CSR
    ares = astarte_crypto_create_key(
        &client_crt->privkey, client_crt->privkey_pem, sizeof(client_crt->privkey_pem));
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed in creating a private key.");
        goto error;
    }

    unsigned char csr_buf[ASTARTE_TLS_CREDENTIALS_CSR_BUFFER_SIZE];
    ares = astarte_crypto_create_csr(&client_crt->privkey, csr_buf, sizeof(csr_buf));
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed in creating a CSR.");
        goto error;
    }

    // Step 3: get the client certificate from the server
    char auth_header[AUTH_HEADER_CRED_SECRET_SIZE] = { 0 };
    int snprintf_rc = snprintf(auth_header, AUTH_HEADER_CRED_SECRET_SIZE,
        AUTH_HEADER_BEARER_STR_START "%s" AUTH_HEADER_BEARER_STR_END, cred_secr);
    if (snprintf_rc != AUTH_HEADER_CRED_SECRET_SIZE - 1) {
        ASTARTE_LOG_ERR("Incorrect length/format for credential secret.");
        ares = ASTARTE_RESULT_INVALID_PARAM;
        goto error;
    }
    const char *header_fields[] = { auth_header, NULL };
    char payload[GET_CLIENT_CRT_PAYLOAD_MAX_SIZE] = { 0 };
    ares = astarte_pairing_serialize_get_client_certificate_payload(
        csr_buf, payload, GET_CLIENT_CRT_PAYLOAD_MAX_SIZE);
    if (ares != ASTARTE_RESULT_OK) {
        goto error;
    }

    char resp_buf[GET_CLIENT_CRT_RESPONSE_MAX_SIZE] = { 0 };
    char url[PAIRING_DEVICE_GET_DEVICE_CERT_URL_LEN + 1] = { 0 };
    snprintf_rc = snprintf(url, PAIRING_DEVICE_GET_DEVICE_CERT_URL_LEN + 1,
        PAIRING_DEVICE_MGMT_URL_PREFIX "%s" PAIRING_DEVICE_CERT_URL_SUFFIX, device_id);
    if (snprintf_rc != PAIRING_DEVICE_GET_DEVICE_CERT_URL_LEN) {
        ASTARTE_LOG_ERR("Error encoding URL for get client certificate request.");
        ares = ASTARTE_RESULT_INTERNAL_ERROR;
        goto error;
    }

    ares = astarte_http_post(timeout_ms, url, header_fields, payload, resp_buf, sizeof(resp_buf));
    if (ares != ASTARTE_RESULT_OK) {
        goto error;
    }

    // Step 4: process the result
    ares = astarte_pairing_deserialize_get_client_certificate_response(
        resp_buf, client_crt->crt_pem, ARRAY_SIZE(client_crt->crt_pem));
    if (ares != ASTARTE_RESULT_OK) {
        goto error;
    }

    // Step 5: convert the received certificate to a valid PEM certificate
    // Replace "\n" with newlines chars
    char *tmp = NULL;
    while ((tmp = strstr(client_crt->crt_pem, "\\n")) != NULL) {
        tmp[0] = '\n';
        // Shift the remaining part of the string left by 1 character
        size_t len = strlen(tmp + 2);
        memmove(tmp + 1, tmp + 2, len + 1);
    }

    ASTARTE_LOG_DBG("Received client certificate: %s", client_crt->crt_pem);
    return ASTARTE_RESULT_OK;

error:
    // clear the credentials
    psa_status_t psa_ret = psa_destroy_key(client_crt->privkey);
    if (psa_ret != PSA_SUCCESS) {
        ASTARTE_LOG_ERR("psa_destroy_key returned %d", psa_ret);
    }
    client_crt->privkey = PSA_KEY_ID_NULL;
    memset(client_crt->privkey_pem, 0, ARRAY_SIZE(client_crt->privkey_pem));
    memset(client_crt->crt_pem, 0, ARRAY_SIZE(client_crt->crt_pem));

    return ares;
}

astarte_result_t astarte_pairing_verify_client_certificate(
    int32_t timeout_ms, const char *device_id, const char *cred_secr, const char *crt_pem)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    // Step 1: check the configuration and input parameters
    if (strlen(device_id) != ASTARTE_DEVICE_ID_LEN) {
        ASTARTE_LOG_ERR(
            "Device ID has incorrect length, should be %d chars.", ASTARTE_DEVICE_ID_LEN);
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    // Step 2: register the device and get the credential secret
    char auth_header[AUTH_HEADER_CRED_SECRET_SIZE] = { 0 };
    int snprintf_rc = snprintf(auth_header, AUTH_HEADER_CRED_SECRET_SIZE,
        AUTH_HEADER_BEARER_STR_START "%s" AUTH_HEADER_BEARER_STR_END, cred_secr);
    if (snprintf_rc != AUTH_HEADER_CRED_SECRET_SIZE - 1) {
        ASTARTE_LOG_ERR("Incorrect length/format for credential secret.");
        return ASTARTE_RESULT_INVALID_PARAM;
    }
    const char *header_fields[] = { auth_header, NULL };
    char payload[VERIFY_CLIENT_CRT_PAYLOAD_MAX_SIZE] = { 0 };
    ares = astarte_pairing_serialize_verify_client_certificate_payload(
        crt_pem, payload, VERIFY_CLIENT_CRT_PAYLOAD_MAX_SIZE);
    if (ares != ASTARTE_RESULT_OK) {
        return ares;
    }

    char url[PAIRING_DEVICE_CERT_CHECK_URL_LEN + 1] = { 0 };
    snprintf_rc = snprintf(url, PAIRING_DEVICE_CERT_CHECK_URL_LEN + 1,
        PAIRING_DEVICE_MGMT_URL_PREFIX "%s" PAIRING_DEVICE_CERT_CHECK_URL_SUFFIX, device_id);
    if (snprintf_rc != PAIRING_DEVICE_CERT_CHECK_URL_LEN) {
        ASTARTE_LOG_ERR("Error encoding URL for verify client certificate request.");
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }

    char resp_buf[VERIFY_CLIENT_CRT_RESPONSE_MAX_SIZE] = { 0 };
    ares = astarte_http_post(timeout_ms, url, header_fields, payload, resp_buf, sizeof(resp_buf));
    if (ares != ASTARTE_RESULT_OK) {
        return ares;
    }

    // Step 3: process the result
    return astarte_pairing_deserialize_verify_client_certificate_response(resp_buf);
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static astarte_result_t get_broker_info(int32_t timeout_ms, const char *device_id,
    const char *cred_secr, char *out_url, size_t out_url_size)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    // Step 1: check the input parameters
    if (out_url_size <= ASTARTE_PAIRING_MAX_BROKER_URL_LEN) {
        ASTARTE_LOG_ERR("Insufficient output buffer size for broker URL.");
        return ASTARTE_RESULT_INVALID_PARAM;
    }
    if (strlen(device_id) != ASTARTE_DEVICE_ID_LEN) {
        ASTARTE_LOG_ERR(
            "Device ID has incorrect length, should be %d chars.", ASTARTE_DEVICE_ID_LEN);
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    // Step 2: Get the MQTT broker URL
    char auth_header[AUTH_HEADER_CRED_SECRET_SIZE] = { 0 };
    int snprintf_rc = snprintf(auth_header, AUTH_HEADER_CRED_SECRET_SIZE,
        AUTH_HEADER_BEARER_STR_START "%s" AUTH_HEADER_BEARER_STR_END, cred_secr);
    if (snprintf_rc != AUTH_HEADER_CRED_SECRET_SIZE - 1) {
        ASTARTE_LOG_ERR("Incorrect length/format for credential secret.");
        return ASTARTE_RESULT_INVALID_PARAM;
    }
    const char *header_fields[] = { auth_header, NULL };

    char url[PAIRING_DEVICE_GET_BROKER_INFO_URL_LEN + 1] = { 0 };
    snprintf_rc = snprintf(url, PAIRING_DEVICE_GET_BROKER_INFO_URL_LEN + 1,
        PAIRING_DEVICE_MGMT_URL_PREFIX "%s", device_id);
    if (snprintf_rc != PAIRING_DEVICE_GET_BROKER_INFO_URL_LEN) {
        ASTARTE_LOG_ERR("Error encoding URL for get client certificate request.");
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }

    char resp_buf[GET_BROKER_INFO_RESPONSE_MAX_SIZE] = { 0 };
    ares = astarte_http_get(timeout_ms, url, header_fields, resp_buf, sizeof(resp_buf));
    if (ares != ASTARTE_RESULT_OK) {
        return ares;
    }

    // Step 3: process the result
    return astarte_pairing_deserialize_get_broker_url_response(resp_buf, out_url, out_url_size);
}
