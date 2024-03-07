/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef HTTP_H
#define HTTP_H

/**
 * @file http.h
 * @brief Low level connectivity functions
 */

#include "astarte_device_sdk/astarte.h"
#include "astarte_device_sdk/error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Perform an HTTP POST request to Astarte.
 *
 * @param[in] url Partial URL to use for the POST request. Hostname and port are taken from the
 * configuration.
 * @param[in] header_fields NULL terminated list of headers for the request.
 * @param[in] payload Payload to transmit.
 * @param[in] timeout_ms Timeout to use for the HTTP operations in ms.
 * @param[out] resp_buf Output buffer where to store the response from the server.
 * @param[in] resp_buf_size Size of the response output buffer.
 * @return ASTARTE_OK if successful, otherwise an error code.
 */
astarte_error_t astarte_http_post(const char *url, const char **header_fields, const char *payload,
    int32_t timeout_ms, uint8_t *resp_buf, size_t resp_buf_size);

/**
 * @brief Perform an HTTP GET request to Astarte.
 *
 * @param[in] url Partial URL to use for the POST request. Hostname and port are taken from the
 * configuration.
 * @param[in] header_fields NULL terminated list of headers for the request.
 * @param[in] timeout_ms Timeout to use for the HTTP operations in ms.
 * @param[out] resp_buf Output buffer where to store the response from the server.
 * @param[in] resp_buf_size Size of the response output buffer.
 * @return ASTARTE_OK if successful, otherwise an error code.
 */
astarte_error_t astarte_http_get(const char *url, const char **header_fields, int32_t timeout_ms,
    uint8_t *resp_buf, size_t resp_buf_size);

#ifdef __cplusplus
}
#endif

#endif /* HTTP_H */
