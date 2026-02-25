/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "crypto.h"

#include <stdio.h>

#include <zephyr/net/socket.h>

#include <mbedtls/pk.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/x509_csr.h>
#include <psa/crypto.h>

#include "log.h"

ASTARTE_LOG_MODULE_REGISTER(astarte_crypto, CONFIG_ASTARTE_DEVICE_SDK_CRYPTO_LOG_LEVEL);

/************************************************
 *       Checks over configuration values       *
 ***********************************************/

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

static const size_t psa_key_bits = 256;
static const char *const csr_subject_name = "CN=temporary";

/************************************************
 *       Callbacks declaration/definition       *
 ***********************************************/

/************************************************
 *         Static functions declaration         *
 ***********************************************/

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_crypto_create_key(astarte_tls_credentials_client_crt_t *client_crt)
{
    astarte_result_t ares = ASTARTE_RESULT_MBEDTLS_ERROR;

    mbedtls_svc_key_id_t key_id = PSA_KEY_ID_NULL;
    mbedtls_pk_context key_ctx = { 0 };

    // initialize PSA
    psa_status_t psa_ret = psa_crypto_init();
    if (psa_ret != PSA_SUCCESS) {
        ASTARTE_LOG_ERR("psa_crypto_init returned %d", psa_ret);
        goto exit;
    }

    ASTARTE_LOG_DBG("Generating the EC key (using curve secp256r1)");

    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_algorithm(&attributes, PSA_ECC_FAMILY_SECP_R1);
    psa_set_key_usage_flags(
        &attributes, PSA_KEY_USAGE_SIGN_HASH | PSA_KEY_USAGE_SIGN_MESSAGE | PSA_KEY_USAGE_EXPORT);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
    psa_set_key_bits(&attributes, psa_key_bits);

    psa_ret = psa_generate_key(&attributes, &key_id);
    if (psa_ret != PSA_SUCCESS) {
        ASTARTE_LOG_ERR("psa_generate_key returned %d", psa_ret);
        if (psa_destroy_key(key_id) != PSA_SUCCESS) {
            ASTARTE_LOG_ERR("psa_destroy_key returned %d", psa_ret);
        }
        goto exit;
    }

    ASTARTE_LOG_DBG("EC key generated");
    client_crt->privkey = key_id;

    mbedtls_pk_init(&key_ctx);
    int pk_ret = mbedtls_pk_copy_from_psa(key_id, &key_ctx);
    if (pk_ret != 0) {
        ASTARTE_LOG_ERR("mbedtls_pk_copy_from_psa returned %d", pk_ret);
        goto exit;
    }

    ASTARTE_LOG_DBG("PEM key succesfully generated");

    pk_ret = mbedtls_pk_write_key_pem(
        &key_ctx, client_crt->privkey_pem, ARRAY_SIZE(client_crt->privkey_pem));
    if (pk_ret != 0) {
        ASTARTE_LOG_ERR("mbedtls_pk_write_key_pem returned %d", pk_ret);
        goto exit;
    }

    ASTARTE_LOG_DBG("%.*s", strlen((char *) client_crt->privkey_pem), client_crt->privkey_pem);

    ares = ASTARTE_RESULT_OK;

exit:
    mbedtls_pk_free(&key_ctx);
    return ares;
}

astarte_result_t astarte_crypto_create_csr(
    const mbedtls_svc_key_id_t *privkey, unsigned char *csr_pem, size_t csr_pem_size)
{
    astarte_result_t ares = ASTARTE_RESULT_MBEDTLS_ERROR;
    if (csr_pem_size < ASTARTE_CRYPTO_CSR_BUFFER_SIZE) {
        ASTARTE_LOG_ERR("Insufficient output buffer size for certificate signing request.");
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    // initialize PSA
    psa_status_t psa_ret = psa_crypto_init();
    if (psa_ret != PSA_SUCCESS) {
        ASTARTE_LOG_ERR("psa_crypto_init returned %d", psa_ret);
        return ASTARTE_RESULT_MBEDTLS_ERROR;
    }

    mbedtls_pk_context key_ctx = { 0 };
    mbedtls_x509write_csr csr_ctx = { 0 };
    mbedtls_pk_init(&key_ctx);
    mbedtls_x509write_csr_init(&csr_ctx);

    int res = mbedtls_pk_copy_from_psa(*privkey, &key_ctx);
    if (res != 0) {
        ASTARTE_LOG_ERR("mbedtls_pk_copy_from_psa returned %d", res);
        goto exit;
    }

    // configure the CSR
    mbedtls_x509write_csr_set_key(&csr_ctx, &key_ctx);
    mbedtls_x509write_csr_set_md_alg(&csr_ctx, MBEDTLS_MD_SHA256);
    res = mbedtls_x509write_csr_set_subject_name(&csr_ctx, csr_subject_name);
    if (res != 0) {
        ASTARTE_LOG_ERR("mbedtls_x509write_csr_set_subject_name returned %d", res);
        goto exit;
    }

    res = mbedtls_x509write_csr_pem(&csr_ctx, csr_pem, csr_pem_size, NULL, NULL);
    if (res != 0) {
        ASTARTE_LOG_ERR("mbedtls_x509write_csr_pem returned %d", res);
        goto exit;
    }

    ASTARTE_LOG_DBG("%.*s", strlen((char *) csr_pem), csr_pem);

    ares = ASTARTE_RESULT_OK;

exit:

    mbedtls_pk_free(&key_ctx);
    mbedtls_x509write_csr_free(&csr_ctx);

    return ares;
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/
