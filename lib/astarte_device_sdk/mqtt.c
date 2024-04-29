/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "mqtt.h"

#include <zephyr/net/socket.h>
#include <zephyr/net/tls_credentials.h>

#include "device_private.h"
#include "log.h"

ASTARTE_LOG_MODULE_REGISTER(astarte_mqtt, CONFIG_ASTARTE_DEVICE_SDK_MQTT_LOG_LEVEL);

/************************************************
 *       Checks over configuration values       *
 ***********************************************/

#if defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_MQTT)
#warning "TLS has been disabled (unsafe)!"
#endif /* defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_MQTT) */

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

/* Buffers for MQTT client. */
#define MQTT_RX_TX_BUFFER_SIZE 256U
static uint8_t mqtt_rx_buffer[MQTT_RX_TX_BUFFER_SIZE];
static uint8_t mqtt_tx_buffer[MQTT_RX_TX_BUFFER_SIZE];

static sec_tag_t sec_tag_list[] = {
#if !defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_MQTT)
    CONFIG_ASTARTE_DEVICE_SDK_MQTTS_CA_CERT_TAG,
#endif
    CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG,
};

/** @brief Size for the application message buffer, used to store incoming messages */
#define MQTT_MAX_MSG_SIZE 4096U

/************************************************
 *         Static functions declaration         *
 ***********************************************/

/**
 * @brief Handle a CONNACK reception event.
 *
 * @param[in] astarte_mqtt Handle to the Astarte MQTT client instance.
 * @param[in] connack Received CONNACK data in the MQTT client format.
 */
static void handle_connack_event(astarte_mqtt_t *astarte_mqtt, struct mqtt_connack_param connack);
/**
 * @brief Handle a disconnection event.
 *
 * @param[in] astarte_mqtt Handle to the Astarte MQTT client instance.
 */
static void handle_disconnection_event(astarte_mqtt_t *astarte_mqtt);
/**
 * @brief Handle a PUBLISH reception event.
 *
 * @param[in] astarte_mqtt Handle to the Astarte MQTT client instance.
 * @param[in] publish Received PUBLISH data in the MQTT client format.
 */
static void handle_publish_event(astarte_mqtt_t *astarte_mqtt, struct mqtt_publish_param publish);
/**
 * @brief Handle a PUBREL reception event.
 *
 * @param[in] astarte_mqtt Handle to the Astarte MQTT client instance.
 * @param[in] pubrel Received PUBREL data in the MQTT client format.
 */
static void handle_pubrel_event(astarte_mqtt_t *astarte_mqtt, struct mqtt_pubrel_param pubrel);
/**
 * @brief Handle a PUBACK reception event.
 *
 * @param[in] astarte_mqtt Handle to the Astarte MQTT client instance.
 * @param[in] puback Received PUBACK data in the MQTT client format.
 */
static void handle_puback_event(astarte_mqtt_t *astarte_mqtt, struct mqtt_puback_param puback);
/**
 * @brief Handle a PUBREC reception event.
 *
 * @param[in] astarte_mqtt Handle to the Astarte MQTT client instance.
 * @param[in] pubrec Received PUBREC data in the MQTT client format.
 */
static void handle_pubrec_event(astarte_mqtt_t *astarte_mqtt, struct mqtt_pubrec_param pubrec);
/**
 * @brief Handle a PUBCOMP reception event.
 *
 * @param[in] astarte_mqtt Handle to the Astarte MQTT client instance.
 * @param[in] pubcomp Received PUBCOMP data in the MQTT client format.
 */
static void handle_pubcomp_event(astarte_mqtt_t *astarte_mqtt, struct mqtt_pubcomp_param pubcomp);
/**
 * @brief Handle a SUBACK reception event.
 *
 * @param[in] astarte_mqtt Handle to the Astarte MQTT client instance.
 * @param[in] suback Received SUBACK data in the MQTT client format.
 */
static void handle_suback_event(astarte_mqtt_t *astarte_mqtt, struct mqtt_suback_param suback);

/************************************************
 *       Callbacks declaration/definition       *
 ***********************************************/

static void mqtt_evt_handler(struct mqtt_client *const client, const struct mqtt_evt *evt)
{
    astarte_mqtt_t *astarte_mqtt = CONTAINER_OF(client, astarte_mqtt_t, client);

    switch (evt->type) {
        case MQTT_EVT_CONNACK:
            if (evt->result != 0) {
                ASTARTE_LOG_ERR("MQTT CONNACK event error: %s", strerror(-evt->result));
                break;
            }
            handle_connack_event(astarte_mqtt, evt->param.connack);
            break;
        case MQTT_EVT_DISCONNECT:
            if (evt->result != 0) {
                ASTARTE_LOG_ERR("MQTT disconnection event error: %s", strerror(-evt->result));
            }
            handle_disconnection_event(astarte_mqtt);
            break;
        case MQTT_EVT_PUBLISH:
            if (evt->result != 0) {
                ASTARTE_LOG_ERR("MQTT PUBLISH event error: %s", strerror(-evt->result));
                break;
            }
            handle_publish_event(astarte_mqtt, evt->param.publish);
            break;
        case MQTT_EVT_PUBREL:
            if (evt->result != 0) {
                ASTARTE_LOG_ERR("MQTT PUBREL event error: %s", strerror(-evt->result));
                break;
            }
            handle_pubrel_event(astarte_mqtt, evt->param.pubrel);
            break;
        case MQTT_EVT_PUBACK:
            if (evt->result != 0) {
                ASTARTE_LOG_ERR("MQTT PUBACK event error: %s", strerror(-evt->result));
                break;
            }
            handle_puback_event(astarte_mqtt, evt->param.puback);
            break;
        case MQTT_EVT_PUBREC:
            if (evt->result != 0) {
                ASTARTE_LOG_ERR("MQTT PUBREC event error: %s", strerror(-evt->result));
                break;
            }
            handle_pubrec_event(astarte_mqtt, evt->param.pubrec);
            break;
        case MQTT_EVT_PUBCOMP:
            if (evt->result != 0) {
                ASTARTE_LOG_ERR("MQTT PUBCOMP event error: %s", strerror(-evt->result));
                break;
            }
            handle_pubcomp_event(astarte_mqtt, evt->param.pubcomp);
            break;
        case MQTT_EVT_SUBACK:
            if (evt->result != 0) {
                ASTARTE_LOG_ERR("MQTT SUBACK event error: %s", strerror(-evt->result));
                break;
            }
            handle_suback_event(astarte_mqtt, evt->param.suback);
            break;
        case MQTT_EVT_PINGRESP:
            if (evt->result != 0) {
                ASTARTE_LOG_ERR("MQTT PINGRESP event error: %s", strerror(-evt->result));
                break;
            }
            ASTARTE_LOG_DBG("Received PINGRESP packet");
            break;
        default:
            ASTARTE_LOG_WRN("Unhandled MQTT event: %d", evt->type);
            break;
    }
}

/************************************************
 *         Global functions definitions         *
 ***********************************************/

void astarte_mqtt_init(astarte_mqtt_config_t *cfg, astarte_mqtt_t *astarte_mqtt)
{
    *astarte_mqtt = (astarte_mqtt_t){ 0 };
    astarte_mqtt->connecting_timeout_ms = cfg->connecting_timeout_ms;
    astarte_mqtt->connected_timeout_ms = cfg->connected_timeout_ms;
    astarte_mqtt->message_id = 1U;
    astarte_mqtt->is_connected = false;
    memcpy(
        astarte_mqtt->broker_hostname, cfg->broker_hostname, sizeof(astarte_mqtt->broker_hostname));
    memcpy(astarte_mqtt->broker_port, cfg->broker_port, sizeof(astarte_mqtt->broker_port));
    memcpy(astarte_mqtt->client_id, cfg->client_id, sizeof(astarte_mqtt->client_id));
}

astarte_result_t astarte_mqtt_connect(astarte_mqtt_t *astarte_mqtt)
{
    // Get broker address info
    struct zsock_addrinfo *broker_addrinfo = NULL;
    struct zsock_addrinfo hints = { 0 };
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    int getaddrinfo_rc = zsock_getaddrinfo(
        astarte_mqtt->broker_hostname, astarte_mqtt->broker_port, &hints, &broker_addrinfo);
    if (getaddrinfo_rc != 0) {
        ASTARTE_LOG_ERR("Unable to resolve broker address %s", gai_strerror(getaddrinfo_rc));
        if (getaddrinfo_rc == EAI_SYSTEM) {
            ASTARTE_LOG_ERR("Errno: %s", strerror(errno));
        }
        return ASTARTE_RESULT_SOCKET_ERROR;
    }

    // MQTT client configuration
    mqtt_client_init(&astarte_mqtt->client);
    astarte_mqtt->client.broker = broker_addrinfo->ai_addr;
    astarte_mqtt->client.evt_cb = mqtt_evt_handler;
    astarte_mqtt->client.client_id.utf8 = (uint8_t *) astarte_mqtt->client_id;
    astarte_mqtt->client.client_id.size = strlen(astarte_mqtt->client_id);
    astarte_mqtt->client.password = NULL;
    astarte_mqtt->client.user_name = NULL;
    astarte_mqtt->client.protocol_version = MQTT_VERSION_3_1_1;
    astarte_mqtt->client.transport.type = MQTT_TRANSPORT_SECURE;

    // MQTT TLS configuration
    struct mqtt_sec_config *tls_config = &(astarte_mqtt->client.transport.tls.config);
#if !defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_MQTT)
    tls_config->peer_verify = TLS_PEER_VERIFY_REQUIRED;
#else
    tls_config->peer_verify = TLS_PEER_VERIFY_NONE;
#endif
    tls_config->cipher_list = NULL;
    tls_config->sec_tag_list = sec_tag_list;
    tls_config->sec_tag_count = ARRAY_SIZE(sec_tag_list);
    tls_config->hostname = astarte_mqtt->broker_hostname;

    // MQTT buffers configuration
    astarte_mqtt->client.rx_buf = mqtt_rx_buffer;
    astarte_mqtt->client.rx_buf_size = sizeof(mqtt_rx_buffer);
    astarte_mqtt->client.tx_buf = mqtt_tx_buffer;
    astarte_mqtt->client.tx_buf_size = sizeof(mqtt_tx_buffer);

    // Request connection to broker
    int mqtt_rc = mqtt_connect(&astarte_mqtt->client);
    if (mqtt_rc != 0) {
        ASTARTE_LOG_ERR("MQTT connection error (%d)", mqtt_rc);
        zsock_freeaddrinfo(broker_addrinfo);
        return ASTARTE_RESULT_MQTT_ERROR;
    }

    zsock_freeaddrinfo(broker_addrinfo);
    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_mqtt_disconnect(astarte_mqtt_t *astarte_mqtt)
{
    if (astarte_mqtt->is_connected) {
        int res = mqtt_disconnect(&astarte_mqtt->client);
        if (res < 0) {
            ASTARTE_LOG_ERR("Device disconnection failure %d", res);
            return ASTARTE_RESULT_MQTT_ERROR;
        }
    }
    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_mqtt_subscribe(astarte_mqtt_t *astarte_mqtt, const char *topic)
{
    ASTARTE_LOG_DBG("Subscribing to %s", topic);

    struct mqtt_topic topics[] = { {
        .topic = { .utf8 = topic, .size = strlen(topic) },
        .qos = 2,
    } };
    const struct mqtt_subscription_list ctrl_sub_list = {
        .list = topics,
        .list_count = ARRAY_SIZE(topics),
        .message_id = astarte_mqtt->message_id++,
    };

    int ret = mqtt_subscribe(&astarte_mqtt->client, &ctrl_sub_list);
    if (ret != 0) {
        ASTARTE_LOG_ERR("Failed to subscribe to control topic: %d", ret);
        return ASTARTE_RESULT_MQTT_ERROR;
    }

    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_mqtt_publish(
    astarte_mqtt_t *astarte_mqtt, const char *topic, void *data, size_t data_size, int qos)
{
    struct mqtt_publish_param msg = { 0 };
    msg.retain_flag = 0U;
    msg.message.topic.topic.utf8 = topic;
    msg.message.topic.topic.size = strlen(topic);
    msg.message.topic.qos = qos;
    msg.message.payload.data = data;
    msg.message.payload.len = data_size;
    msg.message_id = astarte_mqtt->message_id++;
    int res = mqtt_publish(&astarte_mqtt->client, &msg);
    if (res != 0) {
        ASTARTE_LOG_ERR("MQTT publish failed during astarte_mqtt_publish.");
        return ASTARTE_RESULT_MQTT_ERROR;
    }

    ASTARTE_LOG_DBG("PUBLISHED on topic \"%s\" [ id: %u qos: %u ], payload: %u B", topic,
        msg.message_id, msg.message.topic.qos, data_size);
    ASTARTE_LOG_HEXDUMP_DBG(data, data_size, "Published payload:");

    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_mqtt_poll(astarte_mqtt_t *astarte_mqtt)
{
    // Poll the socket
    struct zsock_pollfd socket_fds[1];
    int socket_nfds = 1;
    socket_fds[0].fd = astarte_mqtt->client.transport.tls.sock;
    socket_fds[0].events = ZSOCK_POLLIN;
    int32_t timeout = (astarte_mqtt->is_connected) ? astarte_mqtt->connected_timeout_ms
                                                   : astarte_mqtt->connecting_timeout_ms;
    int32_t keepalive = mqtt_keepalive_time_left(&astarte_mqtt->client);
    int socket_rc = zsock_poll(socket_fds, socket_nfds, MIN(timeout, keepalive));
    if (socket_rc < 0) {
        ASTARTE_LOG_ERR("Poll error: %d", errno);
        return ASTARTE_RESULT_SOCKET_ERROR;
    }
    if (socket_rc != 0) {
        // Process the MQTT response
        int mqtt_rc = mqtt_input(&astarte_mqtt->client);
        if (mqtt_rc != 0) {
            ASTARTE_LOG_ERR("MQTT input failed (%d)", mqtt_rc);
            return ASTARTE_RESULT_MQTT_ERROR;
        }
    }
    // Keep alive the connection
    int mqtt_rc = mqtt_live(&astarte_mqtt->client);
    if ((mqtt_rc != 0) && (mqtt_rc != -EAGAIN)) {
        ASTARTE_LOG_ERR("Failed to keep alive MQTT: %d", mqtt_rc);
        return ASTARTE_RESULT_MQTT_ERROR;
    }
    return (socket_rc == 0) ? ASTARTE_RESULT_TIMEOUT : ASTARTE_RESULT_OK;
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

// NOLINTNEXTLINE(misc-unused-parameters)
static void handle_connack_event(
    astarte_mqtt_t *astarte_mqtt, const struct mqtt_connack_param connack)
{
    ASTARTE_LOG_DBG("Received CONNACK packet");
    astarte_mqtt->is_connected = true;
    on_connected(astarte_mqtt, connack);
}
// NOLINTNEXTLINE(misc-unused-parameters)
static void handle_disconnection_event(astarte_mqtt_t *astarte_mqtt)
{
    ASTARTE_LOG_DBG("MQTT client disconnected");
    astarte_mqtt->is_connected = false;
    on_disconnected(astarte_mqtt);
}
// NOLINTNEXTLINE(misc-unused-parameters)
static void handle_publish_event(astarte_mqtt_t *astarte_mqtt, struct mqtt_publish_param publish)
{
    uint16_t message_id = publish.message_id;
    ASTARTE_LOG_DBG("Received PUBLISH packet (%u)", message_id);
    int ret = 0U;
    size_t received = 0U;
    uint32_t message_size = publish.message.payload.len;
    char msg_buffer[MQTT_MAX_MSG_SIZE] = { 0 };
    const bool discarded = message_size > MQTT_MAX_MSG_SIZE;

    ASTARTE_LOG_DBG("RECEIVED on topic \"%.*s\" [ id: %u qos: %u ] payload: %u / %u B",
        publish.message.topic.topic.size, (const char *) publish.message.topic.topic.utf8,
        message_id, publish.message.topic.qos, message_size, MQTT_MAX_MSG_SIZE);

    while (received < message_size) {
        char *pkt = discarded ? msg_buffer : &msg_buffer[received];

        ret = mqtt_read_publish_payload_blocking(&astarte_mqtt->client, pkt, MQTT_MAX_MSG_SIZE);
        if (ret < 0) {
            ASTARTE_LOG_ERR("Error reading payload of PUBLISH packet %s", strerror(ret));
            return;
        }

        received += ret;
    }

    if (publish.message.topic.qos == MQTT_QOS_1_AT_LEAST_ONCE) {
        struct mqtt_puback_param puback = { .message_id = message_id };
        ret = mqtt_publish_qos1_ack(&astarte_mqtt->client, &puback);
        if (ret != 0) {
            ASTARTE_LOG_ERR("MQTT PUBACK transmission error %d", ret);
        }
    }
    if (publish.message.topic.qos == MQTT_QOS_2_EXACTLY_ONCE) {
        struct mqtt_pubrec_param pubrec = { .message_id = message_id };
        ret = mqtt_publish_qos2_receive(&astarte_mqtt->client, &pubrec);
        if (ret != 0) {
            ASTARTE_LOG_ERR("MQTT PUBREC transmission error %d", ret);
        }
    }

    if (received != publish.message.payload.len) {
        ASTARTE_LOG_ERR("Incoherent expected and read MQTT publish payload lengths.");
        return;
    }

    if (discarded) {
        ASTARTE_LOG_ERR("Insufficient space in the Astarte MQTT reception buffer.");
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        return;
    }

    ASTARTE_LOG_HEXDUMP_DBG(msg_buffer, MIN(message_size, 256U), "Received payload:");

    // This copy is necessary due to the Zephyr MQTT library not null terminating the topic.
    size_t topic_len = publish.message.topic.topic.size;
    char *topic = calloc(topic_len + sizeof(char), sizeof(char));
    if (!topic) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        return;
    }
    memcpy(topic, publish.message.topic.topic.utf8, topic_len);
    on_incoming(astarte_mqtt, topic, topic_len, msg_buffer, message_size);
    free(topic);
}
// NOLINTNEXTLINE(misc-unused-parameters)
static void handle_pubrel_event(astarte_mqtt_t *astarte_mqtt, struct mqtt_pubrel_param pubrel)
{
    uint16_t message_id = pubrel.message_id;
    ASTARTE_LOG_DBG("Received PUBREL packet (%u)", message_id);

    struct mqtt_pubcomp_param pubcomp = { .message_id = message_id };
    int res = mqtt_publish_qos2_complete(&astarte_mqtt->client, &pubcomp);
    if (res != 0) {
        ASTARTE_LOG_ERR("MQTT PUBCOMP transmission error %d", res);
    }
}
// NOLINTNEXTLINE(misc-unused-parameters)
static void handle_puback_event(astarte_mqtt_t *astarte_mqtt, struct mqtt_puback_param puback)
{
    ASTARTE_LOG_DBG("Received PUBACK packet (%u)", puback.message_id);
}
// NOLINTNEXTLINE(misc-unused-parameters)
static void handle_pubrec_event(astarte_mqtt_t *astarte_mqtt, struct mqtt_pubrec_param pubrec)
{
    uint16_t message_id = pubrec.message_id;
    ASTARTE_LOG_DBG("Received PUBREC packet (%u)", message_id);

    // Transmit a PUBREL
    const struct mqtt_pubrel_param rel_param = { .message_id = message_id };
    int err = mqtt_publish_qos2_release(&astarte_mqtt->client, &rel_param);
    if (err != 0) {
        ASTARTE_LOG_ERR("Failed to send MQTT PUBREL: %d", err);
    }
}
// NOLINTNEXTLINE(misc-unused-parameters)
static void handle_pubcomp_event(astarte_mqtt_t *astarte_mqtt, struct mqtt_pubcomp_param pubcomp)
{
    ASTARTE_LOG_DBG("Received PUBCOMP packet (%u)", pubcomp.message_id);
}
// NOLINTNEXTLINE(misc-unused-parameters)
static void handle_suback_event(astarte_mqtt_t *astarte_mqtt, struct mqtt_suback_param suback)
{
    ASTARTE_LOG_DBG("Received SUBACK packet (%u)", suback.message_id);
}
