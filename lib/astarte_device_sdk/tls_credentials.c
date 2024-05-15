/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "tls_credentials.h"

#include <zephyr/kernel.h>
#include <zephyr/net/tls_credentials.h>

#include "log.h"

ASTARTE_LOG_MODULE_REGISTER(
    astarte_tls_credentials, CONFIG_ASTARTE_DEVICE_SDK_TLS_CREDENTIALS_LOG_LEVEL);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_tls_credential_add(astarte_tls_credentials_client_crt_t *client_crt)
{
    int tls_rc = tls_credential_add(CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG,
        TLS_CREDENTIAL_SERVER_CERTIFICATE, client_crt->crt_pem, strlen(client_crt->crt_pem) + 1);
    if (tls_rc != 0) {
        ASTARTE_LOG_ERR("Failed adding client crt to credentials %d.", tls_rc);
        memset(client_crt->privkey_pem, 0, ARRAY_SIZE(client_crt->privkey_pem));
        memset(client_crt->crt_pem, 0, ARRAY_SIZE(client_crt->crt_pem));
        return ASTARTE_RESULT_TLS_ERROR;
    }

    tls_rc = tls_credential_add(CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG,
        TLS_CREDENTIAL_PRIVATE_KEY, client_crt->privkey_pem, strlen(client_crt->privkey_pem) + 1);
    if (tls_rc != 0) {
        ASTARTE_LOG_ERR("Failed adding client private key to credentials %d.", tls_rc);
        tls_credential_delete(
            CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG, TLS_CREDENTIAL_SERVER_CERTIFICATE);
        memset(client_crt->privkey_pem, 0, ARRAY_SIZE(client_crt->privkey_pem));
        memset(client_crt->crt_pem, 0, ARRAY_SIZE(client_crt->crt_pem));
        return ASTARTE_RESULT_TLS_ERROR;
    }

    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_tls_credential_delete(void)
{
    int tls_rc = tls_credential_delete(
        CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG, TLS_CREDENTIAL_SERVER_CERTIFICATE);
    if ((tls_rc != 0) && (tls_rc != -ENOENT)) {
        ASTARTE_LOG_ERR("Failed removing the client certificate from credentials %d.", tls_rc);
        return ASTARTE_RESULT_TLS_ERROR;
    }

    tls_rc = tls_credential_delete(
        CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG, TLS_CREDENTIAL_PRIVATE_KEY);
    if ((tls_rc != 0) && (tls_rc != -ENOENT)) {
        ASTARTE_LOG_ERR("Failed removing the client private key from credentials %d.", tls_rc);
        return ASTARTE_RESULT_TLS_ERROR;
    }

    return ASTARTE_RESULT_OK;
}
