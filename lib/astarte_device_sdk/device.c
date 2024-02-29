/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "astarte_device_sdk/device.h"

#include <stdlib.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/tls_credentials.h>

#include "astarte_device_sdk/bson_serializer.h"
#include "astarte_device_sdk/error.h"
#include "astarte_device_sdk/interface.h"
#include "astarte_device_sdk/pairing.h"
#include "astarte_device_sdk/type.h"
#include "crypto.h"
#include "introspection.h"
#include "pairing_private.h"

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
#define MQTT_RX_TX_BUFFER_SIZE 256U
static uint8_t mqtt_rx_buffer[MQTT_RX_TX_BUFFER_SIZE];
static uint8_t mqtt_tx_buffer[MQTT_RX_TX_BUFFER_SIZE];

static sec_tag_t sec_tag_list[] = {
#if !defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_DISABLE_OR_IGNORE_TLS)
    CONFIG_ASTARTE_DEVICE_SDK_CA_CERT_TAG,
#endif
    CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG,
};

/* The base MQTT topic length should never match this size. */
#define MAX_MQTT_BASE_TOPIC_SIZE 128
/* The total MQTT topic length should never match this size. */
#define MAX_MQTT_TOPIC_SIZE 512
/* Size for the application message buffer, used to store incoming messages */
#define APP_MSG_BUFFER_SIZE 4096U

/**
 * @brief Internal struct for an instance of an Astarte device.
 *
 * @warning Users should not modify the content of this struct directly
 */
struct astarte_device
{
    /** @brief Timeout for socket polls before connection to an MQTT broker. */
    int32_t mqtt_connection_timeout_ms;
    /** @brief Timeout for socket polls on an already connected MQTT broker. */
    int32_t mqtt_connected_timeout_ms;
    /** @brief MQTT broker hostname. */
    char broker_hostname[ASTARTE_MAX_MQTT_BROKER_HOSTNAME_LEN + 1];
    /** @brief MQTT broker port. */
    char broker_port[ASTARTE_MAX_MQTT_BROKER_PORT_LEN + 1];
    /** @brief Base topic for MQTT connection, will be in the format: REALM/DEVICE ID. */
    char base_topic[MAX_MQTT_BASE_TOPIC_SIZE];
    /** @brief MQTT client handle. */
    struct mqtt_client mqtt_client;
    /** @brief Last transmitted message ID. */
    uint16_t mqtt_message_id;
    /** @brief Device introspection. */
    introspection_t introspection;
    /** @brief Flag representing if the device is connected to the MQTT broker. */
    bool mqtt_is_connected;
    /** @brief (optional) User callback for connection events. */
    astarte_device_connection_cbk_t connection_cbk;
    /** @brief (optional) User callback for disconnection events. */
    astarte_device_disconnection_cbk_t disconnection_cbk;
    /** @brief (optional) User callback for incoming data events. */
    astarte_device_data_cbk_t data_cbk;
    /** @brief (optional) User callback for incoming property unset events. */
    astarte_device_unset_cbk_t unset_cbk;
    /** @brief (optional) User data to pass to all the set callbacks. */
    void *cbk_user_data;
};

/************************************************
 *         Static functions declaration         *
 ***********************************************/

/**
 * @brief This function should be called each time a connection event is received.
 *
 * @param[in] device Handle to the device instance.
 * @param[in] connack_param CONNACK parameter, containing the session present flag.
 */
static void on_connected(astarte_device_handle_t device, struct mqtt_connack_param connack_param);
/**
 * @brief This function should be called each time a disconnection event is received.
 *
 * @param[in] device Handle to the device instance.
 */
static void on_disconnected(astarte_device_handle_t device);
/**
 * @brief Helper function to parse an incoming published message.
 *
 * @param[in] device Handle to the device instance.
 * @param[in] pub Received published data in the MQTT client format.
 * @return The number of bytes received upon success, a negative error (errno.h) otherwise.
 */
static ssize_t handle_published_message(
    astarte_device_handle_t device, const struct mqtt_publish_param *pub);
/**
 * @brief This function should be called each time an MQTT publish message is received.
 *
 * @param[in] device Handle to the device instance.
 * @param[in] topic Topic on which the publish message has been received.
 * @param[in] topic_len Length of the topic string (not including NULL terminator).
 * @param[in] data Payload for the received data.
 * @param[in] data_len Length of the payload (not including NULL terminator).
 */
static void on_incoming(astarte_device_handle_t device, const char *topic, size_t topic_len,
    const char *data, size_t data_len);
/**
 * @brief Setup all the MQTT subscriptions for the device.
 *
 * @param[in] device Handle to the device instance.
 */
static void setup_subscriptions(astarte_device_handle_t device);
/**
 * @brief Send the emptycache message to Astarte.
 *
 * @param[in] device Handle to the device instance.
 */
static void send_emptycache(astarte_device_handle_t device);
/**
 * @brief Send the introspection for the device.
 *
 * @param[in] device Handle to the device instance.
 */
static void send_introspection(astarte_device_handle_t device);
/**
 * @brief Publish data.
 *
 * @param[in] device Handle to the device instance.
 * @param[in] interface_name Interface where to publish data.
 * @param[in] path Path where to publish data.
 * @param[in] data Data to publish.
 * @param[in] data_size Size of data to publish.
 * @param[in] qos Quality of service for MQTT publish.
 * @return ASTARTE_OK if publish has been successful, an error code otherwise.
 */
static astarte_err_t publish_data(astarte_device_handle_t device, const char *interface_name,
    const char *path, void *data, int data_size, int qos);

/************************************************
 *       Callbacks declaration/definition       *
 ***********************************************/

// NOLINTNEXTLINE(readability-function-size, hicpp-function-size)
static void mqtt_evt_handler(struct mqtt_client *const client, const struct mqtt_evt *evt)
{
    int res = 0;
    struct astarte_device *device = CONTAINER_OF(client, struct astarte_device, mqtt_client);

    switch (evt->type) {
        case MQTT_EVT_CONNACK:
            if (evt->result != 0) {
                LOG_ERR("MQTT connect failed %d", evt->result); // NOLINT
                break;
            }
            LOG_DBG("MQTT client connected"); // NOLINT
            on_connected(device, evt->param.connack);
            break;

        case MQTT_EVT_DISCONNECT:
            LOG_DBG("MQTT client disconnected %d", evt->result); // NOLINT
            on_disconnected(device);
            break;

        case MQTT_EVT_PUBLISH:
            if (evt->result != 0) {
                LOG_ERR("MQTT publish reception failed %d", evt->result); // NOLINT
                break;
            }

            const struct mqtt_publish_param *pub = &evt->param.publish;
            res = handle_published_message(device, pub);
            if ((res < 0) || (res != pub->message.payload.len)) {
                LOG_ERR("MQTT published incoming data parsing error %d", res); // NOLINT
                break;
            }

            break;

        case MQTT_EVT_PUBREL:
            if (evt->result != 0) {
                LOG_ERR("MQTT PUBREL error %d", evt->result); // NOLINT
                break;
            }
            LOG_DBG("PUBREL packet id: %u", evt->param.pubrel.message_id); // NOLINT

            struct mqtt_pubcomp_param pubcomp = { .message_id = evt->param.pubrel.message_id };
            res = mqtt_publish_qos2_complete(&device->mqtt_client, &pubcomp);
            if (res != 0) {
                LOG_ERR("MQTT PUBCOMP transmission error %d", res); // NOLINT
            }

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

        case MQTT_EVT_SUBACK:
            if (evt->result != 0) {
                LOG_ERR("MQTT SUBACK error %d", evt->result); // NOLINT
                break;
            }
            LOG_DBG("SUBACK packet id: %u", evt->param.suback.message_id); // NOLINT
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

astarte_err_t astarte_device_new(astarte_device_config_t *cfg, astarte_device_handle_t *handle)
{
    astarte_err_t res = ASTARTE_OK;

    astarte_device_handle_t device = calloc(1, sizeof(struct astarte_device));
    if (!device) {
        LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__); // NOLINT
        res = ASTARTE_ERR_OUT_OF_MEMORY;
        goto failure;
    }

    char broker_url[ASTARTE_PAIRING_MAX_BROKER_URL_LEN + 1];
    res = astarte_pairing_get_broker_url(
        cfg->http_timeout_ms, cfg->cred_secr, broker_url, ASTARTE_PAIRING_MAX_BROKER_URL_LEN + 1);
    if (res != ASTARTE_OK) {
        LOG_ERR("Failed in obtaining the MQTT broker URL"); // NOLINT
        goto failure;
    }

    int strncmp_rc = strncmp(broker_url, "mqtts://", strlen("mqtts://"));
    if (strncmp_rc != 0) {
        LOG_ERR("MQTT broker URL is malformed"); // NOLINT
        res = ASTARTE_ERR_HTTP_REQUEST;
        goto failure;
    }
    char *broker_url_token = strtok(&broker_url[strlen("mqtts://")], ":"); // NOLINT
    if (!broker_url_token) {
        LOG_ERR("MQTT broker URL is malformed"); // NOLINT
        res = ASTARTE_ERR_HTTP_REQUEST;
        goto failure;
    }
    strncpy(device->broker_hostname, broker_url_token, ASTARTE_MAX_MQTT_BROKER_HOSTNAME_LEN + 1);
    broker_url_token = strtok(NULL, "/");
    if (!broker_url_token) {
        LOG_ERR("MQTT broker URL is malformed"); // NOLINT
        res = ASTARTE_ERR_HTTP_REQUEST;
        goto failure;
    }
    strncpy(device->broker_port, broker_url_token, ASTARTE_MAX_MQTT_BROKER_PORT_LEN + 1);

    res = astarte_pairing_get_client_certificate(cfg->http_timeout_ms, cfg->cred_secr, privkey_pem,
        sizeof(privkey_pem), crt_pem, sizeof(crt_pem));
    if (res != ASTARTE_OK) {
        goto failure;
    }

    // The base topic for this device is returned by Astarte in the common name of the certificate
    // It will be usually be in the format: <REALM>/<DEVICE ID>
    res = astarte_crypto_get_certificate_common_name(
        crt_pem, device->base_topic, MAX_MQTT_BASE_TOPIC_SIZE);
    if ((res != ASTARTE_OK) || (strlen(device->base_topic) == 0)) {
        LOG_ERR("Error in certificate common name extraction."); // NOLINT
        goto failure;
    }

    LOG_DBG("Client certificate (PEM): \n%s", crt_pem); // NOLINT
    LOG_DBG("Client private key (PEM): \n%s", privkey_pem); // NOLINT
    LOG_DBG("Received device topic is: %s", device->base_topic); // NOLINT

    int tls_rc = tls_credential_add(CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG,
        TLS_CREDENTIAL_SERVER_CERTIFICATE, crt_pem, strlen(crt_pem) + 1);
    if (tls_rc != 0) {
        LOG_ERR("Failed adding client crt to credentials %d.", tls_rc); // NOLINT
        res = ASTARTE_ERR_TLS;
        goto failure;
    }

    tls_rc = tls_credential_add(CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG,
        TLS_CREDENTIAL_PRIVATE_KEY, privkey_pem, strlen(privkey_pem) + 1);
    if (tls_rc != 0) {
        LOG_ERR("Failed adding client private key to credentials %d.", tls_rc); // NOLINT
        res = ASTARTE_ERR_TLS;
        goto failure;
    }

    LOG_DBG("Initializing introspection"); // NOLINT
    res = introspection_init(&device->introspection);
    if (res != ASTARTE_OK) {
        LOG_ERR("Introspection initialization failure %s.", astarte_err_to_name(res)); // NOLINT
        goto failure;
    }
    for (size_t i = 0; i < cfg->interfaces_size; i++) {
        res = introspection_add(&device->introspection, cfg->interfaces[i]);
        if (res != ASTARTE_OK) {
            LOG_ERR("Introspection add failure %s.", astarte_err_to_name(res)); // NOLINT
            introspection_free(device->introspection);
            goto failure;
        }
    }

    device->mqtt_connection_timeout_ms = cfg->mqtt_connection_timeout_ms;
    device->mqtt_connected_timeout_ms = cfg->mqtt_connected_timeout_ms;
    device->connection_cbk = cfg->connection_cbk;
    device->disconnection_cbk = cfg->disconnection_cbk;
    device->data_cbk = cfg->data_cbk;
    device->unset_cbk = cfg->unset_cbk;
    device->cbk_user_data = cfg->cbk_user_data;
    device->mqtt_is_connected = false;
    device->mqtt_message_id = 1U;

    *handle = device;

    return res;

failure:
    free(device);
    return res;
}

astarte_err_t astarte_device_destroy(astarte_device_handle_t handle)
{
    if (handle->mqtt_is_connected) {
        int res = mqtt_disconnect(&handle->mqtt_client);
        if (res < 0) {
            LOG_ERR("Device disconnection failure %d", res); // NOLINT
            return ASTARTE_ERR_MQTT;
        }
    }

    int tls_rc = tls_credential_delete(
        CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG, TLS_CREDENTIAL_SERVER_CERTIFICATE);
    if (tls_rc != 0) {
        LOG_ERR("Failed removing the client certificate from credentials %d.", tls_rc); // NOLINT
        return ASTARTE_ERR_TLS;
    }

    tls_rc = tls_credential_delete(
        CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG, TLS_CREDENTIAL_PRIVATE_KEY);
    if (tls_rc != 0) {
        LOG_ERR("Failed removing the client private key from credentials %d.", tls_rc); // NOLINT
        return ASTARTE_ERR_TLS;
    }

    free(handle);
    return ASTARTE_OK;
}

astarte_err_t astarte_device_connect(astarte_device_handle_t device)
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

astarte_err_t astarte_device_poll(astarte_device_handle_t device)
{
    // Poll the socket
    struct zsock_pollfd socket_fds[1];
    int socket_nfds = 1;
    socket_fds[0].fd = device->mqtt_client.transport.tls.sock;
    socket_fds[0].events = ZSOCK_POLLIN;
    int32_t timeout = (device->mqtt_is_connected) ? device->mqtt_connected_timeout_ms
                                                  : device->mqtt_connection_timeout_ms;
    int32_t keepalive = mqtt_keepalive_time_left(&device->mqtt_client);
    int socket_rc = zsock_poll(socket_fds, socket_nfds, MIN(timeout, keepalive));
    if (socket_rc < 0) {
        LOG_ERR("Poll error: %d", errno); // NOLINT
        return ASTARTE_ERR_SOCKET;
    }
    if (socket_rc != 0) {
        // Process the MQTT response
        int mqtt_rc = mqtt_input(&device->mqtt_client);
        if (mqtt_rc != 0) {
            LOG_ERR("MQTT input failed (%d)", mqtt_rc); // NOLINT
            return ASTARTE_ERR_MQTT;
        }
    }
    // Keep alive the connection
    int mqtt_rc = mqtt_live(&device->mqtt_client);
    if ((mqtt_rc != 0) && (mqtt_rc != -EAGAIN)) {
        LOG_ERR("Failed to keep alive MQTT: %d", mqtt_rc); // NOLINT
        return ASTARTE_ERR_MQTT;
    }
    return (socket_rc == 0) ? ASTARTE_ERR_TIMEOUT : ASTARTE_OK;
}

astarte_err_t astarte_device_stream_individual(astarte_device_handle_t device, char *interface_name,
    char *path, astarte_value_t value, const int64_t *timestamp, uint8_t qos)
{
    bson_serializer_handle_t bson = bson_serializer_new();
    if (!bson) {
        LOG_ERR("Could not initialize the bson serializer"); // NOLINT
        return ASTARTE_ERR_OUT_OF_MEMORY;
    }
    astarte_err_t exit_code = astarte_value_serialize(bson, "v", value);
    if (exit_code != ASTARTE_OK) {
        goto exit;
    }

    if (timestamp) {
        bson_serializer_append_datetime(bson, "t", *timestamp);
    }
    bson_serializer_append_end_of_document(bson);

    int len = 0;
    void *data = (void *) bson_serializer_get_document(bson, &len);
    if (!data) {
        LOG_ERR("Error during BSON serialization"); // NOLINT
        exit_code = ASTARTE_ERR_BSON_SERIALIZER;
        goto exit;
    }
    if (len < 0) {
        LOG_ERR("BSON document is too long for MQTT publish."); // NOLINT
        LOG_ERR("Interface: %s, path: %s", interface_name, path); // NOLINT

        exit_code = ASTARTE_ERR_BSON_SERIALIZER;
        goto exit;
    }

    exit_code = publish_data(device, interface_name, path, data, len, qos);

exit:
    bson_serializer_destroy(bson);

    return exit_code;
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static void on_connected(astarte_device_handle_t device, struct mqtt_connack_param connack_param)
{
    device->mqtt_is_connected = true;

    if (device->connection_cbk) {
        astarte_device_connection_event_t event = {
            .device = device,
            .session_present = connack_param.session_present_flag,
            .user_data = device->cbk_user_data,
        };

        device->connection_cbk(&event);
    }

    if (connack_param.session_present_flag != 0) {
        return;
    }

    setup_subscriptions(device);
    send_introspection(device);
    send_emptycache(device);
    // TODO: send device owned props
}

static void on_disconnected(astarte_device_handle_t device)
{
    device->mqtt_is_connected = false;

    if (device->disconnection_cbk) {
        astarte_device_disconnection_event_t event = {
            .device = device,
            .user_data = device->cbk_user_data,
        };

        device->disconnection_cbk(&event);
    }
}

static ssize_t handle_published_message(
    astarte_device_handle_t device, const struct mqtt_publish_param *pub)
{
    int ret = 0U;
    size_t received = 0U;
    uint32_t message_size = pub->message.payload.len;
    uint8_t msg_buffer[APP_MSG_BUFFER_SIZE];
    const bool discarded = message_size > APP_MSG_BUFFER_SIZE;

    // NOLINTNEXTLINE
    LOG_DBG("RECEIVED on topic \"%s\" [ id: %u qos: %u ] payload: %u / %u B",
        (const char *) pub->message.topic.topic.utf8, pub->message_id, pub->message.topic.qos,
        message_size, APP_MSG_BUFFER_SIZE);

    while (received < message_size) {
        uint8_t *pkt = discarded ? msg_buffer : &msg_buffer[received];

        ret = mqtt_read_publish_payload_blocking(&device->mqtt_client, pkt, APP_MSG_BUFFER_SIZE);
        if (ret < 0) {
            return ret;
        }

        received += ret;
    }

    if (pub->message.topic.qos == MQTT_QOS_1_AT_LEAST_ONCE) {
        struct mqtt_puback_param puback = { .message_id = pub->message_id };
        ret = mqtt_publish_qos1_ack(&device->mqtt_client, &puback);
        if (ret != 0) {
            LOG_ERR("MQTT PUBACK transmission error %d", ret); // NOLINT
        }
    }
    if (pub->message.topic.qos == MQTT_QOS_2_EXACTLY_ONCE) {
        struct mqtt_pubrec_param pubrec = { .message_id = pub->message_id };
        ret = mqtt_publish_qos2_receive(&device->mqtt_client, &pubrec);
        if (ret != 0) {
            LOG_ERR("MQTT PUBREC transmission error %d", ret); // NOLINT
        }
    }

    if (!discarded) {
        LOG_HEXDUMP_DBG(msg_buffer, MIN(message_size, 256u), "Received payload:"); // NOLINT

        const char *topic = (const char *) pub->message.topic.topic.utf8;
        on_incoming(device, topic, strlen(topic), (const char *) msg_buffer, message_size);
    }

    return discarded ? -ENOMEM : (ssize_t) received;
}

static void on_incoming(astarte_device_handle_t device, const char *topic, size_t topic_len,
    const char *data, size_t data_len)
{
    if (!device->data_cbk) {
        LOG_ERR("data_event_callback not set"); // NOLINT
        return;
    }

    if (strstr(topic, device->base_topic) != topic) {
        LOG_ERR("Incoming message topic doesn't begin with base topic: %s", topic); // NOLINT
        return;
    }

    char control_prefix[MAX_MQTT_TOPIC_SIZE] = { 0 };
    int ret = snprintf(control_prefix, MAX_MQTT_TOPIC_SIZE, "%s/control", device->base_topic);
    if ((ret < 0) || (ret >= MAX_MQTT_TOPIC_SIZE)) {
        LOG_ERR("Error encoding control prefix"); // NOLINT
        return;
    }

    // Control message
    size_t control_prefix_len = strlen(control_prefix);
    if (strstr(topic, control_prefix)) {
        const char *control_topic = topic + control_prefix_len;
        LOG_DBG("Received control message on control topic %s", control_topic); // NOLINT
        // TODO correctly process control messages
        // on_control_message(device, control_topic, data, data_len);
        return;
    }

    // Data message
    if (topic_len < strlen(device->base_topic) + strlen("/")
        || topic[strlen(device->base_topic)] != '/') {
        LOG_ERR("No / after device_topic, can't find interface: %s", topic); // NOLINT
        return;
    }

    const char *interface_name_begin = topic + strlen(device->base_topic) + strlen("/");
    char *path_begin = strchr(interface_name_begin, '/');
    if (!path_begin) {
        LOG_ERR("No / after interface_name, can't find path: %s", topic); // NOLINT
        return;
    }

    int interface_name_len = path_begin - interface_name_begin;
    char interface_name[INTERFACE_NAME_MAX_SIZE] = { 0 };
    ret = snprintf(
        interface_name, INTERFACE_NAME_MAX_SIZE, "%.*s", interface_name_len, interface_name_begin);
    if ((ret < 0) || (ret >= INTERFACE_NAME_MAX_SIZE)) {
        LOG_ERR("Error encoding interface name"); // NOLINT
        return;
    }

    size_t path_len = topic_len - strlen(device->base_topic) - strlen("/") - interface_name_len;
    char path[MAX_MQTT_TOPIC_SIZE] = { 0 };
    ret = snprintf(path, MAX_MQTT_TOPIC_SIZE, "%.*s", path_len, path_begin);
    if ((ret < 0) || (ret >= MAX_MQTT_TOPIC_SIZE)) {
        LOG_ERR("Error encoding path"); // NOLINT
        return;
    }

    if (!data && data_len == 0) {
        if (device->unset_cbk) {
            astarte_device_unset_event_t event = {
                .device = device,
                .interface_name = interface_name,
                .path = path,
                .user_data = device->cbk_user_data,
            };
            device->unset_cbk(&event);
        } else {
            LOG_ERR("Unset data for %s received, but unset cbk is not defined", path); // NOLINT
        }
        return;
    }

    if (!bson_deserializer_check_validity(data, data_len)) {
        LOG_ERR("Invalid BSON document in data"); // NOLINT
        return;
    }

    bson_document_t full_document = bson_deserializer_init_doc(data);
    bson_element_t v_elem;
    if (bson_deserializer_element_lookup(full_document, "v", &v_elem) != ASTARTE_OK) {
        LOG_ERR("Cannot retrieve BSON value from data"); // NOLINT
        return;
    }

    astarte_device_data_event_t event = {
        .device = device,
        .interface_name = interface_name,
        .path = path,
        .bson_element = v_elem,
        .user_data = device->cbk_user_data,
    };

    device->data_cbk(&event);
}

static void setup_subscriptions(astarte_device_handle_t device)
{
    char topic_str[MAX_MQTT_TOPIC_SIZE] = { 0 };
    int ret = snprintf(
        topic_str, MAX_MQTT_TOPIC_SIZE, "%s/control/consumer/properties", device->base_topic);
    if ((ret < 0) || (ret >= MAX_MQTT_TOPIC_SIZE)) {
        LOG_ERR("Error encoding MQTT topic"); // NOLINT
        return;
    }

    struct mqtt_topic ctrl_topics[] = { {
        .topic = { .utf8 = topic_str, .size = strlen(topic_str) },
        .qos = 2,
    } };
    const struct mqtt_subscription_list ctrl_sub_list = {
        .list = ctrl_topics,
        .list_count = ARRAY_SIZE(ctrl_topics),
        .message_id = device->mqtt_message_id++,
    };

    LOG_DBG("Subscribing to %s", topic_str); // NOLINT

    ret = mqtt_subscribe(&device->mqtt_client, &ctrl_sub_list);
    if (ret != 0) {
        LOG_ERR("Failed to subscribe to control topic: %d", ret); // NOLINT
        return;
    }

    for (introspection_node_t *iterator = introspection_iter(&device->introspection);
         iterator != NULL; iterator = introspection_iter_next(&device->introspection, iterator)) {
        const astarte_interface_t *interface = iterator->interface;

        if (interface->ownership == OWNERSHIP_SERVER) {
            // Subscribe to server interface subtopics
            ret = snprintf(
                topic_str, MAX_MQTT_TOPIC_SIZE, "%s/%s/#", device->base_topic, interface->name);
            if ((ret < 0) || (ret >= MAX_MQTT_TOPIC_SIZE)) {
                LOG_ERR("Error encoding MQTT topic"); // NOLINT
                continue;
            }

            struct mqtt_topic topics[] = { {
                .topic = { .utf8 = topic_str, .size = strlen(topic_str) },
                .qos = 2,
            } };
            const struct mqtt_subscription_list sub_list = {
                .list = topics,
                .list_count = ARRAY_SIZE(topics),
                .message_id = device->mqtt_message_id++,
            };

            LOG_DBG("Subscribing to %s", topic_str); // NOLINT

            ret = mqtt_subscribe(&device->mqtt_client, &sub_list);
            if (ret != 0) {
                LOG_ERR("Failed to subscribe to %s: %d", topic_str, ret); // NOLINT
                return;
            }
        }
    }
}

static void send_introspection(astarte_device_handle_t device)
{
    size_t introspection_str_size = introspection_get_string_size(&device->introspection);

    // if introspection size is > 4KiB print a warning
    const size_t introspection_size_warn_level = 4096;
    if (introspection_str_size > introspection_size_warn_level) {
        LOG_WRN("The introspection size is > 4KiB"); // NOLINT
    }

    char *introspection_str = calloc(introspection_str_size, sizeof(char));
    if (!introspection_str) {
        LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__); // NOLINT
        return;
    }
    introspection_fill_string(&device->introspection, introspection_str, introspection_str_size);

    LOG_DBG("Publishing introspection: %s", introspection_str); // NOLINT

    struct mqtt_publish_param msg;
    msg.retain_flag = 0U;
    msg.message.topic.topic.utf8 = device->base_topic;
    msg.message.topic.topic.size = strlen(device->base_topic);
    msg.message.topic.qos = 2;
    msg.message.payload.data = introspection_str;
    msg.message.payload.len = strlen(introspection_str);
    msg.message_id = device->mqtt_message_id++;
    int res = mqtt_publish(&device->mqtt_client, &msg);
    if (res != 0) {
        LOG_ERR("MQTT publish failed during send_introspection."); // NOLINT
    }

    free(introspection_str);
}

static void send_emptycache(astarte_device_handle_t device)
{
    char topic[MAX_MQTT_TOPIC_SIZE] = { 0 };
    int ret = snprintf(topic, MAX_MQTT_TOPIC_SIZE, "%s/control/emptyCache", device->base_topic);
    if ((ret < 0) || (ret >= MAX_MQTT_TOPIC_SIZE)) {
        LOG_ERR("Error encoding topic"); // NOLINT
        return;
    }

    LOG_DBG("Sending emptyCache to %s", topic); // NOLINT

    struct mqtt_publish_param msg;
    msg.retain_flag = 0U;
    msg.message.topic.topic.utf8 = topic;
    msg.message.topic.topic.size = strlen(topic);
    msg.message.topic.qos = 2;
    msg.message.payload.data = "1";
    msg.message.payload.len = strlen("1");
    msg.message_id = device->mqtt_message_id++;
    int res = mqtt_publish(&device->mqtt_client, &msg);
    if (res != 0) {
        LOG_ERR("MQTT publish failed during send_introspection."); // NOLINT
    }
}

static astarte_err_t publish_data(astarte_device_handle_t device, const char *interface_name,
    const char *path, void *data, int data_size, int qos)
{
    if (path[0] != '/') {
        LOG_ERR("Invalid path: %s (must be start with /)", path); // NOLINT
        return ASTARTE_ERR_INVALID_PARAM;
    }

    if (qos < 0 || qos > 2) {
        LOG_ERR("Invalid QoS: %d (must be 0, 1 or 2)", qos); // NOLINT
        return ASTARTE_ERR_INVALID_PARAM;
    }

    char topic[MAX_MQTT_TOPIC_SIZE] = { 0 };
    int print_ret
        = snprintf(topic, MAX_MQTT_TOPIC_SIZE, "%s/%s%s", device->base_topic, interface_name, path);
    if ((print_ret < 0) || (print_ret >= MAX_MQTT_TOPIC_SIZE)) {
        LOG_ERR("Error encoding topic"); // NOLINT
        return ASTARTE_ERR;
    }

    struct mqtt_publish_param msg;
    msg.retain_flag = 0U;
    msg.message.topic.topic.utf8 = topic;
    msg.message.topic.topic.size = strlen(topic);
    msg.message.topic.qos = qos;
    msg.message.payload.data = data;
    msg.message.payload.len = data_size;
    msg.message_id = device->mqtt_message_id++;

    int ret = mqtt_publish(&device->mqtt_client, &msg);
    if (ret != 0) {
        LOG_ERR("Failed to publish message: %d", ret); // NOLINT
        return ASTARTE_ERR_MQTT;
    }

    // NOLINTNEXTLINE
    LOG_INF("PUBLISHED on topic \"%s\" [ id: %u qos: %u ], payload: %u B", topic, msg.message_id,
        msg.message.topic.qos, data_size);
    LOG_HEXDUMP_DBG(data, data_size, "Published payload:"); // NOLINT

    return ASTARTE_OK;
}
