/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "crypto.h"

#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/ecp.h>
#include <mbedtls/entropy.h>
#include <mbedtls/oid.h>
#include <mbedtls/pk.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/x509_csr.h>

LOG_MODULE_REGISTER(astarte_crypto, CONFIG_ASTARTE_DEVICE_SDK_CRYPTO_LOG_LEVEL); // NOLINT

/************************************************
 *       Checks over configuration values       *
 ***********************************************/

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

/************************************************
 *       Callbacks declaration/definition       *
 ***********************************************/

/************************************************
 *         Static functions declaration         *
 ***********************************************/

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_err_t astarte_crypto_create_key(unsigned char *privkey_pem, size_t privkey_pem_size)
{
    astarte_err_t exit_code = ASTARTE_ERR_MBEDTLS;

    mbedtls_pk_context key;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    const char *pers = "astarte_credentials_create_key";

    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_pk_init(&key);
    mbedtls_entropy_init(&entropy);

    LOG_DBG("Initializing entropy"); // NOLINT
    int ret = mbedtls_ctr_drbg_seed(
        &ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char *) pers, strlen(pers));
    if (ret != 0) {
        LOG_ERR("mbedtls_ctr_drbg_seed returned %d", ret); // NOLINT
        goto exit;
    }

    LOG_DBG("Generating the EC key (using curve secp256r1)"); // NOLINT

    ret = mbedtls_pk_setup(&key, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY));
    if (ret != 0) {
        LOG_ERR("mbedtls_pk_setup returned %d", ret); // NOLINT
        goto exit;
    }

    ret = mbedtls_ecp_gen_key(
        MBEDTLS_ECP_DP_SECP256R1, mbedtls_pk_ec(key), mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0) {
        LOG_ERR("mbedtls_ecp_gen_key returned %d", ret); // NOLINT
        goto exit;
    }

    LOG_DBG("Key succesfully generated"); // NOLINT

    ret = mbedtls_pk_write_key_pem(&key, privkey_pem, privkey_pem_size);
    if (ret != 0) {
        LOG_ERR("mbedtls_pk_write_key_pem returned %d", ret); // NOLINT
        goto exit;
    }

    LOG_DBG("%.*s", strlen((char *) privkey_pem), privkey_pem); // NOLINT

    exit_code = ASTARTE_OK;

exit:

    mbedtls_pk_free(&key);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);

    return exit_code;
}

astarte_err_t astarte_crypto_create_csr(
    const unsigned char *privkey_pem, unsigned char *csr_pem, size_t csr_pem_size)
{
    astarte_err_t exit_code = ASTARTE_ERR_MBEDTLS;

    mbedtls_pk_context key;
    mbedtls_x509write_csr req;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    const char *pers = "astarte_credentials_create_csr";

    mbedtls_x509write_csr_init(&req);
    mbedtls_pk_init(&key);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);

    mbedtls_x509write_csr_set_md_alg(&req, MBEDTLS_MD_SHA256);
    mbedtls_x509write_csr_set_ns_cert_type(&req, MBEDTLS_X509_NS_CERT_TYPE_SSL_CLIENT);

    // We set the CN to a temporary value, it's just a placeholder since Pairing API will change it
    int ret = mbedtls_x509write_csr_set_subject_name(&req, "CN=temporary");
    if (ret != 0) {
        LOG_ERR("mbedtls_x509write_csr_set_subject_name returned %d", ret); // NOLINT
        goto exit;
    }

    LOG_DBG("Initializing entropy"); // NOLINT
    ret = mbedtls_ctr_drbg_seed(
        &ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char *) pers, strlen(pers));
    if (ret != 0) {
        LOG_ERR("mbedtls_ctr_drbg_seed returned %d", ret); // NOLINT
        goto exit;
    }

    LOG_DBG("Creating the private key"); // NOLINT

    ret = mbedtls_pk_parse_key(
        &key, privkey_pem, strlen(privkey_pem) + 1, NULL, 0, mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0) {
        LOG_ERR("mbedtls_pk_parse_key returned %d", ret); // NOLINT
        goto exit;
    }

    mbedtls_x509write_csr_set_key(&req, &key);

    ret = mbedtls_x509write_csr_pem(
        &req, csr_pem, csr_pem_size, mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0) {
        LOG_ERR("mbedtls_x509write_csr_pem returned %d", ret); // NOLINT
        goto exit;
    }

    LOG_DBG("CSR succesfully created."); // NOLINT

    LOG_DBG("%.*s", strlen((char *) csr_pem), csr_pem); // NOLINT

    exit_code = ASTARTE_OK;

exit:

    mbedtls_x509write_csr_free(&req);
    mbedtls_pk_free(&key);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);

    return exit_code;
}

astarte_err_t astarte_crypto_get_certificate_info(
    const char *cert_pem, char *cert_cn, size_t cert_cn_size)
{
    astarte_err_t exit_code = ASTARTE_ERR_MBEDTLS;
    mbedtls_x509_crt crt;
    mbedtls_x509_crt_init(&crt);

    size_t cert_length = strlen(cert_pem) + 1; // + 1 for NULL terminator, as per documentation
    int ret = mbedtls_x509_crt_parse(&crt, (unsigned char *) cert_pem, cert_length);
    if (ret < 0) {
        LOG_ERR("mbedtls_x509_crt_parse_file returned %d", ret); // NOLINT
        goto exit;
    }

    mbedtls_x509_name *subj_it = &crt.subject;
    while (subj_it && (MBEDTLS_OID_CMP(MBEDTLS_OID_AT_CN, &subj_it->oid) != 0)) {
        subj_it = subj_it->next;
    }

    if (!subj_it) {
        LOG_ERR("CN not found in certificate"); // NOLINT
        exit_code = ASTARTE_ERR_NOT_FOUND;
        goto exit;
    }

    ret = snprintf(cert_cn, cert_cn_size, "%.*s", subj_it->val.len, subj_it->val.p);
    if ((ret < 0) || (ret >= cert_cn_size)) {
        LOG_ERR("Error encoding certificate common name"); // NOLINT
        exit_code = ASTARTE_ERR;
        goto exit;
    }

    exit_code = ASTARTE_OK;

exit:
    mbedtls_x509_crt_free(&crt);

    return exit_code;
}
/************************************************
 *         Static functions definitions         *
 ***********************************************/
