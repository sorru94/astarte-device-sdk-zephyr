/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "astarte_device_sdk/device.h"

#include "astarte_device_sdk/pairing.h"
#include "crypto.h"
#include "pairing_private.h"

#include <stdlib.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/tls_credentials.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(astarte_device, CONFIG_ASTARTE_DEVICE_SDK_DEVICE_LOG_LEVEL); // NOLINT

/************************************************
 *       Checks over configuration values       *
 ***********************************************/

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

/* Buffers for private key and client certificate */
static unsigned char privkey_pem[ASTARTE_CRYPTO_PRIVKEY_BUFFER_SIZE];
static unsigned char crt_pem[ASTARTE_PAIRING_MAX_CLIENT_CRT_LEN + 1];

/************************************************
 *       Callbacks declaration/definition       *
 ***********************************************/

/************************************************
 *         Static functions declaration         *
 ***********************************************/

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_err_t astarte_device_init(astarte_device_config_t *cfg, astarte_device_t *device)
{
    astarte_err_t res = ASTARTE_OK;
    char broker_url[ASTARTE_PAIRING_MAX_BROKER_URL_LEN + 1];

    res = astarte_pairing_get_broker_url(
        cfg->http_timeout_ms, cfg->cred_secr, broker_url, ASTARTE_PAIRING_MAX_BROKER_URL_LEN + 1);
    if (res != ASTARTE_OK) {
        LOG_ERR("Failed in obtaining the MQTT broker URL"); // NOLINT
        goto end;
    }

    int strncmp_rc = strncmp(broker_url, "mqtts://", strlen("mqtts://"));
    if (strncmp_rc != 0) {
        LOG_ERR("MQTT broker URL is malformed"); // NOLINT
        res = ASTARTE_ERR_HTTP_REQUEST;
        goto end;
    }
    char *broker_url_token = strtok(&broker_url[strlen("mqtts://")], ":"); // NOLINT
    if (!broker_url_token) {
        LOG_ERR("MQTT broker URL is malformed"); // NOLINT
        res = ASTARTE_ERR_HTTP_REQUEST;
        goto end;
    }
    strncpy(device->broker_hostname, broker_url_token, ASTARTE_MAX_MQTT_BROKER_HOSTNAME_LEN + 1);
    broker_url_token = strtok(NULL, "/");
    if (!broker_url_token) {
        LOG_ERR("MQTT broker URL is malformed"); // NOLINT
        res = ASTARTE_ERR_HTTP_REQUEST;
        goto end;
    }
    strncpy(device->broker_port, broker_url_token, ASTARTE_MAX_MQTT_BROKER_PORT_LEN + 1);

    res = astarte_pairing_get_client_certificate(cfg->http_timeout_ms, cfg->cred_secr, privkey_pem,
        sizeof(privkey_pem), crt_pem, sizeof(crt_pem));
    if (res != ASTARTE_OK) {
        goto end;
    }

    LOG_DBG("Client certificate (PEM): \n%s", crt_pem); // NOLINT
    LOG_DBG("Client private key (PEM): \n%s", privkey_pem); // NOLINT

end:
    return res;
}
/************************************************
 *         Static functions definitions         *
 ***********************************************/
