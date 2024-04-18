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
 * @brief Helper function to parse an incoming published message.
 *
 * @param[in] astarte_mqtt Handle to the Astarte MQTT client instance.
 * @param[in] pub Received published data in the MQTT client format.
 * @return The number of bytes received upon success, a negative error (errno.h) otherwise.
 */
static ssize_t handle_published_message(
    astarte_mqtt_t *astarte_mqtt, const struct mqtt_publish_param *pub);

/************************************************
 *       Callbacks declaration/definition       *
 ***********************************************/

static void mqtt_evt_handler(struct mqtt_client *const client, const struct mqtt_evt *evt)
{
    int res = 0;
    astarte_mqtt_t *astarte_mqtt = CONTAINER_OF(client, astarte_mqtt_t, client);

    switch (evt->type) {
        case MQTT_EVT_CONNACK:
            if (evt->result != 0) {
                ASTARTE_LOG_ERR("MQTT connect failed %d", evt->result);
                break;
            }
            ASTARTE_LOG_DBG("MQTT client connected");
            astarte_mqtt->is_connected = true;
            on_connected(astarte_mqtt, evt->param.connack);
            break;

        case MQTT_EVT_DISCONNECT:
            ASTARTE_LOG_DBG("MQTT client disconnected %d", evt->result);
            astarte_mqtt->is_connected = false;
            on_disconnected(astarte_mqtt);
            break;

        case MQTT_EVT_PUBLISH:
            if (evt->result != 0) {
                ASTARTE_LOG_ERR("MQTT publish reception failed %d", evt->result);
                break;
            }

            const struct mqtt_publish_param *pub = &evt->param.publish;
            res = handle_published_message(astarte_mqtt, pub);
            if ((res < 0) || (res != pub->message.payload.len)) {
                ASTARTE_LOG_ERR("MQTT published incoming data parsing error %d", res);
                break;
            }

            break;

        case MQTT_EVT_PUBREL:
            if (evt->result != 0) {
                ASTARTE_LOG_ERR("MQTT PUBREL error %d", evt->result);
                break;
            }
            ASTARTE_LOG_DBG("PUBREL packet id: %u", evt->param.pubrel.message_id);

            struct mqtt_pubcomp_param pubcomp = { .message_id = evt->param.pubrel.message_id };
            res = mqtt_publish_qos2_complete(client, &pubcomp);
            if (res != 0) {
                ASTARTE_LOG_ERR("MQTT PUBCOMP transmission error %d", res);
            }

            break;

        case MQTT_EVT_PUBACK:
            if (evt->result != 0) {
                ASTARTE_LOG_ERR("MQTT PUBACK error %d", evt->result);
                break;
            }
            ASTARTE_LOG_DBG("PUBACK packet id: %u", evt->param.puback.message_id);
            break;

        case MQTT_EVT_PUBREC:
            if (evt->result != 0) {
                ASTARTE_LOG_ERR("MQTT PUBREC error %d", evt->result);
                break;
            }
            ASTARTE_LOG_DBG("PUBREC packet id: %u", evt->param.pubrec.message_id);
            const struct mqtt_pubrel_param rel_param
                = { .message_id = evt->param.pubrec.message_id };
            int err = mqtt_publish_qos2_release(client, &rel_param);
            if (err != 0) {
                ASTARTE_LOG_ERR("Failed to send MQTT PUBREL: %d", err);
            }
            break;

        case MQTT_EVT_PUBCOMP:
            if (evt->result != 0) {
                ASTARTE_LOG_ERR("MQTT PUBCOMP error %d", evt->result);
                break;
            }
            ASTARTE_LOG_DBG("PUBCOMP packet id: %u", evt->param.pubcomp.message_id);
            break;

        case MQTT_EVT_SUBACK:
            if (evt->result != 0) {
                ASTARTE_LOG_ERR("MQTT SUBACK error %d", evt->result);
                break;
            }
            ASTARTE_LOG_DBG("SUBACK packet id: %u", evt->param.suback.message_id);
            break;

        case MQTT_EVT_PINGRESP:
            ASTARTE_LOG_DBG("PINGRESP packet");
            break;

        default:
            ASTARTE_LOG_WRN("Unhandled MQTT event: %d", evt->type);
            break;
    }
}

/************************************************
 *         Global functions definitions         *
 ***********************************************/

void astarte_mqtt_init(astarte_mqtt_t *astarte_mqtt, int32_t connecting_timeout_ms,
    int32_t connected_timeout_ms, const char *broker_hostname, const char *broker_port,
    const char *client_id)
{
    *astarte_mqtt = (astarte_mqtt_t){ 0 };
    astarte_mqtt->connecting_timeout_ms = connecting_timeout_ms;
    astarte_mqtt->connected_timeout_ms = connected_timeout_ms;
    astarte_mqtt->message_id = 1U;
    astarte_mqtt->is_connected = false;
    memcpy(astarte_mqtt->broker_hostname, broker_hostname, sizeof(astarte_mqtt->broker_hostname));
    memcpy(astarte_mqtt->broker_port, broker_port, sizeof(astarte_mqtt->broker_port));
    memcpy(astarte_mqtt->client_id, client_id, sizeof(astarte_mqtt->client_id));
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
    struct mqtt_topic topics[] = { {
        .topic = { .utf8 = topic, .size = strlen(topic) },
        .qos = 2,
    } };
    const struct mqtt_subscription_list ctrl_sub_list = {
        .list = topics,
        .list_count = ARRAY_SIZE(topics),
        .message_id = astarte_mqtt->message_id++,
    };

    ASTARTE_LOG_DBG("Subscribing to %s", topic);

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
    struct mqtt_publish_param msg;
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

static ssize_t handle_published_message(
    astarte_mqtt_t *astarte_mqtt, const struct mqtt_publish_param *pub)
{
    int ret = 0U;
    size_t received = 0U;
    uint32_t message_size = pub->message.payload.len;
    char msg_buffer[MQTT_MAX_MSG_SIZE] = { 0 };
    const bool discarded = message_size > MQTT_MAX_MSG_SIZE;

    ASTARTE_LOG_DBG("RECEIVED on topic \"%.*s\" [ id: %u qos: %u ] payload: %u / %u B",
        pub->message.topic.topic.size, (const char *) pub->message.topic.topic.utf8,
        pub->message_id, pub->message.topic.qos, message_size, MQTT_MAX_MSG_SIZE);

    while (received < message_size) {
        char *pkt = discarded ? msg_buffer : &msg_buffer[received];

        ret = mqtt_read_publish_payload_blocking(&astarte_mqtt->client, pkt, MQTT_MAX_MSG_SIZE);
        if (ret < 0) {
            return ret;
        }

        received += ret;
    }

    if (pub->message.topic.qos == MQTT_QOS_1_AT_LEAST_ONCE) {
        struct mqtt_puback_param puback = { .message_id = pub->message_id };
        ret = mqtt_publish_qos1_ack(&astarte_mqtt->client, &puback);
        if (ret != 0) {
            ASTARTE_LOG_ERR("MQTT PUBACK transmission error %d", ret);
        }
    }
    if (pub->message.topic.qos == MQTT_QOS_2_EXACTLY_ONCE) {
        struct mqtt_pubrec_param pubrec = { .message_id = pub->message_id };
        ret = mqtt_publish_qos2_receive(&astarte_mqtt->client, &pubrec);
        if (ret != 0) {
            ASTARTE_LOG_ERR("MQTT PUBREC transmission error %d", ret);
        }
    }

    if (!discarded) {
        ASTARTE_LOG_HEXDUMP_DBG(msg_buffer, MIN(message_size, 256U), "Received payload:");

        // This copy is necessary due to the Zephyr MQTT library not null terminating the topic.
        size_t topic_len = pub->message.topic.topic.size;
        char *topic = calloc(topic_len + sizeof(char), sizeof(char));
        if (!topic) {
            ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
            return -ENOMEM;
        }
        memcpy(topic, pub->message.topic.topic.utf8, topic_len);
        on_incoming(astarte_mqtt, topic, topic_len, msg_buffer, message_size);
        free(topic);
    }

    return discarded ? -ENOMEM : (ssize_t) received;
}
