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
#include <zephyr/net/mqtt.h>
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

/* Buffers for MQTT client. */
#define MQTT_RX_TX_BUFFER_SIZE 256
static uint8_t mqtt_rx_buffer[MQTT_RX_TX_BUFFER_SIZE];
static uint8_t mqtt_tx_buffer[MQTT_RX_TX_BUFFER_SIZE];

static sec_tag_t sec_tag_list[] = {
#if !defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_DISABLE_OR_IGNORE_TLS)
    CONFIG_ASTARTE_DEVICE_SDK_CA_CERT_TAG,
#endif
    CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG,
};

// Placed here as there is no way to pass user data to the MQTT callback
static bool mqtt_is_connected;

/************************************************
 *         Static functions declaration         *
 ***********************************************/

/**
 * @brief Print content of addrinfo struct.
 *
 * @param[in] input_addinfo addrinfo struct to print.
 */
static void dump_addrinfo(const struct addrinfo *input_addinfo);

/************************************************
 *       Callbacks declaration/definition       *
 ***********************************************/

// NOLINTNEXTLINE(readability-function-size, hicpp-function-size)
static void mqtt_evt_handler(struct mqtt_client *const client, const struct mqtt_evt *evt)
{
    switch (evt->type) {
        case MQTT_EVT_CONNACK:
            if (evt->result != 0) {
                LOG_ERR("MQTT connect failed %d", evt->result); // NOLINT
                break;
            }
            LOG_DBG("MQTT client connected"); // NOLINT
            mqtt_is_connected = true;
            break;

        case MQTT_EVT_DISCONNECT:
            LOG_DBG("MQTT client disconnected %d", evt->result); // NOLINT
            mqtt_is_connected = false;
            break;

        case MQTT_EVT_PUBACK:
            if (evt->result != 0) {
                LOG_ERR("MQTT PUBACK error %d", evt->result); // NOLINT
                break;
            }
            LOG_DBG("PUBACK packet id: %u", evt->param.puback.message_id); // NOLINT
            break;

        case MQTT_EVT_PUBREC:
            if (evt->result != 0) {
                LOG_ERR("MQTT PUBREC error %d", evt->result); // NOLINT
                break;
            }
            LOG_DBG("PUBREC packet id: %u", evt->param.pubrec.message_id); // NOLINT
            const struct mqtt_pubrel_param rel_param
                = { .message_id = evt->param.pubrec.message_id };
            int err = mqtt_publish_qos2_release(client, &rel_param);
            if (err != 0) {
                LOG_ERR("Failed to send MQTT PUBREL: %d", err); // NOLINT
            }
            break;

        case MQTT_EVT_PUBCOMP:
            if (evt->result != 0) {
                LOG_ERR("MQTT PUBCOMP error %d", evt->result); // NOLINT
                break;
            }
            LOG_DBG("PUBCOMP packet id: %u", evt->param.pubcomp.message_id); // NOLINT
            break;

        case MQTT_EVT_PINGRESP:
            LOG_DBG("PINGRESP packet"); // NOLINT
            break;

        default:
            LOG_WRN("Unhandled MQTT event: %d", evt->type); // NOLINT
            break;
    }
}

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
        return res;
    }

    int strncmp_rc = strncmp(broker_url, "mqtts://", strlen("mqtts://"));
    if (strncmp_rc != 0) {
        LOG_ERR("MQTT broker URL is malformed"); // NOLINT
        return ASTARTE_ERR_HTTP_REQUEST;
    }
    char *broker_url_token = strtok(&broker_url[strlen("mqtts://")], ":"); // NOLINT
    if (!broker_url_token) {
        LOG_ERR("MQTT broker URL is malformed"); // NOLINT
        return ASTARTE_ERR_HTTP_REQUEST;
    }
    strncpy(device->broker_hostname, broker_url_token, ASTARTE_MAX_MQTT_BROKER_HOSTNAME_LEN + 1);
    broker_url_token = strtok(NULL, "/");
    if (!broker_url_token) {
        LOG_ERR("MQTT broker URL is malformed"); // NOLINT
        return ASTARTE_ERR_HTTP_REQUEST;
    }
    strncpy(device->broker_port, broker_url_token, ASTARTE_MAX_MQTT_BROKER_PORT_LEN + 1);

    res = astarte_pairing_get_client_certificate(cfg->http_timeout_ms, cfg->cred_secr, privkey_pem,
        sizeof(privkey_pem), crt_pem, sizeof(crt_pem));
    if (res != ASTARTE_OK) {
        return res;
    }

    LOG_DBG("Client certificate (PEM): \n%s", crt_pem); // NOLINT
    LOG_DBG("Client private key (PEM): \n%s", privkey_pem); // NOLINT

    int tls_rc = tls_credential_add(CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG,
        TLS_CREDENTIAL_SERVER_CERTIFICATE, crt_pem, strlen(crt_pem) + 1);
    if (tls_rc != 0) {
        LOG_ERR("Failed adding client crt to credentials %d.", tls_rc); // NOLINT
        return ASTARTE_ERR_TLS;
    }

    tls_rc = tls_credential_add(CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG,
        TLS_CREDENTIAL_PRIVATE_KEY, privkey_pem, strlen(privkey_pem) + 1);
    if (tls_rc != 0) {
        LOG_ERR("Failed adding client private key to credentials %d.", tls_rc); // NOLINT
        return ASTARTE_ERR_TLS;
    }

    device->mqtt_connection_timeout_ms = cfg->mqtt_connection_timeout_ms;
    device->mqtt_connected_timeout_ms = cfg->mqtt_connected_timeout_ms;
    mqtt_is_connected = false;

    return res;
}

astarte_err_t astarte_device_connect(astarte_device_t *device)
{
    struct zsock_addrinfo *broker_addrinfo = NULL;
    struct zsock_addrinfo hints;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    int sock_rc
        = zsock_getaddrinfo(device->broker_hostname, device->broker_port, &hints, &broker_addrinfo);
    if (sock_rc != 0) {
        LOG_ERR("Unable to resolve broker address %d", sock_rc); // NOLINT
        LOG_ERR("Errno: %s", strerror(errno)); // NOLINT
        return ASTARTE_ERR_SOCKET;
    }

    dump_addrinfo(broker_addrinfo);

    // MQTT client configuration
    mqtt_client_init(&device->mqtt_client);
    device->mqtt_client.broker = broker_addrinfo->ai_addr;
    device->mqtt_client.evt_cb = mqtt_evt_handler;
    device->mqtt_client.client_id.utf8 = (uint8_t *) "zephyr_mqtt_client";
    device->mqtt_client.client_id.size = sizeof("zephyr_mqtt_client") - 1;
    device->mqtt_client.password = NULL;
    device->mqtt_client.user_name = NULL;
    device->mqtt_client.protocol_version = MQTT_VERSION_3_1_1;
    device->mqtt_client.transport.type = MQTT_TRANSPORT_SECURE;

    // MQTT TLS configuration
    struct mqtt_sec_config *tls_config = &(device->mqtt_client.transport.tls.config);
#if !defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_DISABLE_OR_IGNORE_TLS)
    tls_config->peer_verify = TLS_PEER_VERIFY_REQUIRED;
#else
    tls_config->peer_verify = TLS_PEER_VERIFY_NONE;
#endif
    tls_config->cipher_list = NULL;
    tls_config->sec_tag_list = sec_tag_list;
    tls_config->sec_tag_count = ARRAY_SIZE(sec_tag_list);
    tls_config->hostname = device->broker_hostname;

    // MQTT buffers configuration
    device->mqtt_client.rx_buf = mqtt_rx_buffer;
    device->mqtt_client.rx_buf_size = sizeof(mqtt_rx_buffer);
    device->mqtt_client.tx_buf = mqtt_tx_buffer;
    device->mqtt_client.tx_buf_size = sizeof(mqtt_tx_buffer);

    // Request connection to broker
    int mqtt_rc = mqtt_connect(&device->mqtt_client);
    if (mqtt_rc != 0) {
        LOG_ERR("MQTT connection error (%d)", mqtt_rc); // NOLINT
        return ASTARTE_ERR_MQTT;
    }

    return ASTARTE_OK;
}

astarte_err_t astarte_device_poll(astarte_device_t *device)
{
    // Poll the socket
    struct zsock_pollfd socket_fds[1];
    int socket_nfds = 1;
    socket_fds[0].fd = device->mqtt_client.transport.tls.sock;
    socket_fds[0].events = ZSOCK_POLLIN;
    int32_t timeout = (mqtt_is_connected) ? device->mqtt_connected_timeout_ms
                                          : device->mqtt_connection_timeout_ms;
    int32_t keepalive = mqtt_keepalive_time_left(&device->mqtt_client);
    int socket_rc = zsock_poll(socket_fds, socket_nfds, MIN(timeout, keepalive));
    if (socket_rc < 0) {
        LOG_ERR("Poll error: %d", errno); // NOLINT
        return ASTARTE_ERR_SOCKET;
    }
    if (socket_rc == 0) {
        LOG_DBG("Poll operation timed out."); // NOLINT
        return ASTARTE_ERR_TIMEOUT;
    }
    // Process the MQTT response
    int mqtt_rc = mqtt_input(&device->mqtt_client);
    if (mqtt_rc != 0) {
        LOG_ERR("MQTT input failed (%d)", mqtt_rc); // NOLINT
        return ASTARTE_ERR_MQTT;
    }
    // Keep alive the connection
    mqtt_rc = mqtt_live(&device->mqtt_client);
    if ((mqtt_rc != 0) && (mqtt_rc != -EAGAIN)) {
        LOG_ERR("Failed to keep alive MQTT: %d", mqtt_rc); // NOLINT
        return ASTARTE_ERR_MQTT;
    }
    return ASTARTE_OK;
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

// NOLINTBEGIN Just used for logging during development
static void dump_addrinfo(const struct addrinfo *input_addinfo)
{
    char dst[16] = { 0 };
    inet_ntop(
        AF_INET, &((struct sockaddr_in *) input_addinfo->ai_addr)->sin_addr, dst, sizeof(dst));
    LOG_INF("addrinfo @%p: ai_family=%d, ai_socktype=%d, ai_protocol=%d, "
            "sa_family=%d, sin_port=%x, ip_addr=%s",
        input_addinfo, input_addinfo->ai_family, input_addinfo->ai_socktype,
        input_addinfo->ai_protocol, input_addinfo->ai_addr->sa_family,
        ((struct sockaddr_in *) input_addinfo->ai_addr)->sin_port, dst);
}
// NOLINTEND
