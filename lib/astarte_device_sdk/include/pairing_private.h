/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PAIRING_PRIVATE_H
#define PAIRING_PRIVATE_H

/**
 * @file pairing_private.h
 * @brief Private pairing APIs for new Astarte devices
 */

#include "astarte_device_sdk/astarte.h"
#include "astarte_device_sdk/result.h"

/** @brief Maximum length for the MQTT broker URL in chars.
 *
 * @details It is the sum of:
 * - Size of the string `mqtts://` (8)
 * - Maximum length for a hostname (DNS) (253)
 * - Size of the string `:` (1)
 * - Maximum length for a port number (5)
 * - Size of the string `/` (1)
 *
 * Total: 268 characters
 */
#define ASTARTE_PAIRING_MAX_BROKER_URL_LEN 268

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Fetch the MQTT broker URL from Astarte.
 *
 * @param[in] timeout_ms Timeout to use for the HTTP operations in ms.
 * @param[in] cred_secr Credential secret to use as authorization token.
 * @param[out] out_url Output buffer where to store the fetched ULR.
 * @param[in] out_url_size Size of the output buffer for the URL.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_pairing_get_broker_url(
    int32_t timeout_ms, const char *cred_secr, char *out_url, size_t out_url_size);

/**
 * @brief Fetch the client x509 certificate from Astarte.
 *
 * @warning This is often a very memory intensive operation, more than 20kB of stack are required.
 *
 * @param[in] timeout_ms Timeout to use for the HTTP operations in ms.
 * @param[in] cred_secr Credential secret to use as authorization token.
 * @param[out] privkey_pem Buffer where to store the computed private key, in the PEM format.
 * @param[in] privkey_pem_size Size of preallocated privkey_pem buffer.
 * @param[out] crt_pem Output buffer where to store the fetched PEM certificate.
 * @param[in] crt_pem_size Size of the output buffer for the PEM certificate.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_pairing_get_client_certificate(int32_t timeout_ms, const char *cred_secr,
    unsigned char *privkey_pem, size_t privkey_pem_size, char *crt_pem, size_t crt_pem_size);

/**
 * @brief Fetch the client x509 certificate from Astarte.
 *
 * @warning This is often a very memory intensive operation, more than 20kB of stack are required.
 *
 * @param[in] timeout_ms Timeout to use for the HTTP operations in ms.
 * @param[in] cred_secr Credential secret to use as authorization token.
 * @param[in] crt_pem Input buffer containing the PEM certificate to verify.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_pairing_verify_client_certificate(
    int32_t timeout_ms, const char *cred_secr, const char *crt_pem);

#ifdef __cplusplus
}
#endif

#endif /* PAIRING_PRIVATE_H */
