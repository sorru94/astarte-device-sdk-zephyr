/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CRYPTO_H
#define CRYPTO_H

/**
 * @file crypto.h
 * @brief Functions used to generate a CSR (certificate signing request) and private key.
 *
 * @note This module relies on MbedTLS functionality.
 */

#include <psa/crypto.h>

#include "astarte_device_sdk/astarte.h"
#include "astarte_device_sdk/result.h"

#include "tls_credentials.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a private key to be used in a CSR.
 *
 * @param client_crt structure where to store the computed private key.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_crypto_create_key(astarte_tls_credentials_client_crt_t *client_crt);

/**
 * @brief Create a CSR (certificate signing request).
 *
 * @param[in] privkey Private key to use for CSR generation.
 * @param[out] csr_pem Buffer where to store the computed CSR, in the PEM format.
 * @param[in] csr_pem_size Size of preallocated CSR buffer.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_crypto_create_csr(
    const mbedtls_svc_key_id_t *privkey, unsigned char *csr_pem, size_t csr_pem_size);

/**
 * @brief Get the common name for a PEM certificate.
 *
 * @param[in] cert_pem Certificate in the PEM format.
 * @param[out] cert_cn Resulting common name.
 * @param[in] cert_cn_size Size of preallocated common name buffer
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_crypto_get_certificate_info(
    const char *cert_pem, char *cert_cn, size_t cert_cn_size);

#ifdef __cplusplus
}
#endif

#endif /* CRYPTO_H */
