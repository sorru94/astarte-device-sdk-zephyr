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
 * @param[out] key_id Pointer the the PSA private key.
 * @param[out] privkey_pem Buffer where to store the computed private key, in the PEM format.
 * @param[in] privkey_pem_size Size of preallocated private key buffer.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_crypto_create_key(
    mbedtls_svc_key_id_t *key_id, unsigned char *privkey_pem, size_t privkey_pem_size);

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

#ifdef __cplusplus
}
#endif

#endif /* CRYPTO_H */
