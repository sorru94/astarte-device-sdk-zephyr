/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "mqtt.h"
#include "mqtt_caching.h"

#include <zephyr/net/socket.h>
#include <zephyr/net/tls_credentials.h>

#include "getaddrinfo.h"
#include "heap.h"
#include "log.h"

ASTARTE_LOG_MODULE_REGISTER(astarte_mqtt, CONFIG_ASTARTE_DEVICE_SDK_MQTT_LOG_LEVEL);

/************************************************
 *       Checks over configuration values       *
 ***********************************************/

#if defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_MQTT)
#warning "TLS has been disabled (unsafe)!"
#endif /* defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_MQTT) */

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

    if ((astarte_mqtt->connection_state == ASTARTE_MQTT_CONNECTING)
        && (evt->type != MQTT_EVT_CONNACK) && (evt->type != MQTT_EVT_DISCONNECT)) {
        ASTARTE_LOG_ERR("Received MQTT packet before CONNACK during connection.");
        return;
    }
    if ((astarte_mqtt->connection_state == ASTARTE_MQTT_DISCONNECTING)
        && (evt->type != MQTT_EVT_DISCONNECT)) {
        ASTARTE_LOG_ERR("Received MQTT packet before disconnection event during disconnection.");
        return;
    }

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
static void mqtt_caching_retransmit_out_msg_handler(
    astarte_mqtt_t *astarte_mqtt, uint16_t message_id, mqtt_caching_message_t message)
{
    int ret = 0;
    switch (message.type) {
        case MQTT_CACHING_PUBLISH_ENTRY:
            ASTARTE_LOG_DBG("Retransmitting MQTT publish message: %d", message_id);
            struct mqtt_publish_param msg = { 0 };
            msg.retain_flag = 0U;
            msg.message.topic.topic.utf8 = message.topic;
            msg.message.topic.topic.size = strlen(message.topic);
            msg.message.topic.qos = message.qos;
            msg.message.payload.data = message.data;
            msg.message.payload.len = message.data_size;
            msg.message_id = message_id;
            msg.dup_flag = 1U;
            ret = mqtt_publish(&astarte_mqtt->client, &msg);
            if (ret != 0) {
                ASTARTE_LOG_ERR("MQTT publish failed (message ID %d), err: %d", message_id, ret);
            } else {
                ASTARTE_LOG_DBG("PUBLISHED on topic \"%s\" [ id: %u qos: %u ], payload: %u B",
                    message.topic, msg.message_id, msg.message.topic.qos, message.data_size);
                ASTARTE_LOG_HEXDUMP_DBG(message.data, message.data_size, "Published payload:");
            }
            break;
        case MQTT_CACHING_SUBSCRIPTION_ENTRY:
            ASTARTE_LOG_DBG("Retransmitting MQTT subscribe message: %d", message_id);
            struct mqtt_topic topics[] = { {
                .topic = { .utf8 = message.topic, .size = strlen(message.topic) },
                .qos = message.qos,
            } };
            const struct mqtt_subscription_list ctrl_sub_list = {
                .list = topics,
                .list_count = ARRAY_SIZE(topics),
                .message_id = message_id,
            };
            int ret = mqtt_subscribe(&astarte_mqtt->client, &ctrl_sub_list);
            if (ret != 0) {
                ASTARTE_LOG_ERR("MQTT subscription failed: %s, %d", strerror(-ret), ret);
            } else {
                ASTARTE_LOG_DBG("SUBSCRIBED to %s", message.topic);
            }
            break;

        default:
            ASTARTE_LOG_ERR("Invalid map entry.");
            break;
    }
}
static void mqtt_caching_retransmit_in_msg_handler(
    astarte_mqtt_t *astarte_mqtt, uint16_t message_id, mqtt_caching_message_t message)
{
    (void) message;
    struct mqtt_pubrec_param pubrec = { .message_id = message_id };
    int ret = mqtt_publish_qos2_receive(&astarte_mqtt->client, &pubrec);
    if (ret != 0) {
        ASTARTE_LOG_ERR("MQTT PUBREC failed (message ID %d), err: %d", message_id, ret);
    }
}

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_mqtt_init(astarte_mqtt_config_t *cfg, astarte_mqtt_t *astarte_mqtt)
{
    if (!cfg->refresh_client_cert_cbk) {
        ASTARTE_LOG_ERR("Refresh client certificate callback is required.");
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    *astarte_mqtt = (astarte_mqtt_t) { 0 };
    astarte_mqtt->clean_session = cfg->clean_session;
    astarte_mqtt->connection_timeout_ms = cfg->connection_timeout_ms;
    astarte_mqtt->poll_timeout_ms = cfg->poll_timeout_ms;
    memcpy(
        astarte_mqtt->broker_hostname, cfg->broker_hostname, sizeof(astarte_mqtt->broker_hostname));
    memcpy(astarte_mqtt->broker_port, cfg->broker_port, sizeof(astarte_mqtt->broker_port));
    memcpy(astarte_mqtt->client_id, cfg->client_id, sizeof(astarte_mqtt->client_id));
    astarte_mqtt->refresh_client_cert_cbk = cfg->refresh_client_cert_cbk;
    astarte_mqtt->on_delivered_cbk = cfg->on_delivered_cbk;
    astarte_mqtt->on_subscribed_cbk = cfg->on_subscribed_cbk;
    astarte_mqtt->on_connected_cbk = cfg->on_connected_cbk;
    astarte_mqtt->on_disconnected_cbk = cfg->on_disconnected_cbk;
    astarte_mqtt->on_incoming_cbk = cfg->on_incoming_cbk;

    // Initialize the timepoint to an infinite future date
    astarte_mqtt->connection_timepoint = sys_timepoint_calc(K_FOREVER);

    // Initialize the backoff context
    backoff_context_init(&astarte_mqtt->backoff_ctx,
        CONFIG_ASTARTE_DEVICE_SDK_RECONNECTION_MQTT_BACKOFF_INITIAL_MS,
        CONFIG_ASTARTE_DEVICE_SDK_RECONNECTION_MQTT_BACKOFF_MAX_MS, false);

    // Initialize the reconnection timepoint
    astarte_mqtt->reconnection_timepoint = sys_timepoint_calc(K_NO_WAIT);

    // Initialize the caching hashmaps
    // NOLINTNEXTLINE
    astarte_mqtt->out_msg_map_config = (const struct sys_hashmap_config) SYS_HASHMAP_CONFIG(
        CONFIG_ASTARTE_DEVICE_SDK_ADVANCED_MQTT_CACHING_HASMAPS_SIZE,
        SYS_HASHMAP_DEFAULT_LOAD_FACTOR);
    astarte_mqtt->out_msg_map = (struct sys_hashmap) {
        .api = &sys_hashmap_sc_api,
        .config = &astarte_mqtt->out_msg_map_config,
        .data = &astarte_mqtt->out_msg_map_data,
        .hash_func = sys_hash32,
        .alloc_func = SYS_HASHMAP_DEFAULT_ALLOCATOR,
    };
    // NOLINTNEXTLINE
    astarte_mqtt->in_msg_map_config = (const struct sys_hashmap_config) SYS_HASHMAP_CONFIG(
        CONFIG_ASTARTE_DEVICE_SDK_ADVANCED_MQTT_CACHING_HASMAPS_SIZE,
        SYS_HASHMAP_DEFAULT_LOAD_FACTOR);
    astarte_mqtt->in_msg_map = (struct sys_hashmap) {
        .api = &sys_hashmap_sc_api,
        .config = &astarte_mqtt->in_msg_map_config,
        .data = &astarte_mqtt->in_msg_map_data,
        .hash_func = sys_hash32,
        .alloc_func = SYS_HASHMAP_DEFAULT_ALLOCATOR,
    };

    // Initialize the mutex
    sys_mutex_init(&astarte_mqtt->mutex);

    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_mqtt_connect(astarte_mqtt_t *astarte_mqtt)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    struct zsock_addrinfo *broker_addrinfo = NULL;

    // Lock the mutex for the Astarte MQTT wrapper
    int mutex_rc = sys_mutex_lock(&astarte_mqtt->mutex, K_FOREVER);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex lock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    if ((astarte_mqtt->connection_state != ASTARTE_MQTT_DISCONNECTED)
        && (astarte_mqtt->connection_state != ASTARTE_MQTT_CONNECTION_ERROR)) {
        ASTARTE_LOG_ERR("Connection request while the client is non idle will be ignored.");
        ares = ASTARTE_RESULT_MQTT_CLIENT_NOT_READY;
        goto exit;
    }

    ares = astarte_mqtt->refresh_client_cert_cbk(astarte_mqtt);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Refreshing client certificate failed");
        goto exit;
    }

    // Get broker address info
    struct zsock_addrinfo hints = { 0 };
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    int getaddrinfo_rc = astarte_getaddrinfo(
        astarte_mqtt->broker_hostname, astarte_mqtt->broker_port, &hints, &broker_addrinfo);
    if (getaddrinfo_rc != 0) {
        ASTARTE_LOG_ERR("Unable to resolve broker address %s", zsock_gai_strerror(getaddrinfo_rc));
        if (getaddrinfo_rc == DNS_EAI_SYSTEM) {
            ASTARTE_LOG_ERR("Errno: %s", strerror(errno));
        }
        ares = ASTARTE_RESULT_SOCKET_ERROR;
        goto exit;
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
    astarte_mqtt->client.clean_session = (astarte_mqtt->clean_session) ? 1 : 0;

    // MQTT TLS configuration
    sec_tag_t sec_tag_list[] = {
#if !defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_MQTT)
        CONFIG_ASTARTE_DEVICE_SDK_MQTTS_CA_CERT_TAG,
#endif
        CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG,
    };

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
    astarte_mqtt->client.rx_buf = astarte_mqtt->rx_buffer;
    astarte_mqtt->client.rx_buf_size = ARRAY_SIZE(astarte_mqtt->rx_buffer);
    astarte_mqtt->client.tx_buf = astarte_mqtt->tx_buffer;
    astarte_mqtt->client.tx_buf_size = ARRAY_SIZE(astarte_mqtt->tx_buffer);

    // Request connection to broker
    int mqtt_rc = mqtt_connect(&astarte_mqtt->client);
    if (mqtt_rc != 0) {
        ASTARTE_LOG_ERR("MQTT connection error (%d)", mqtt_rc);
        ares = ASTARTE_RESULT_MQTT_ERROR;
        goto exit;
    }

    // Set a timepoint to be used to check if the connection timeout will have elapsed
    astarte_mqtt->connection_timepoint
        = sys_timepoint_calc(K_MSEC(astarte_mqtt->connection_timeout_ms));

    // Set connecting flag
    astarte_mqtt->connection_state = ASTARTE_MQTT_CONNECTING;

exit:
    // Free the broker address info
    if (broker_addrinfo) {
        astarte_freeaddrinfo(broker_addrinfo);
    }
    // Unlock the mutex
    mutex_rc = sys_mutex_unlock(&astarte_mqtt->mutex);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex unlock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);
    return ares;
}

bool astarte_mqtt_is_connected(astarte_mqtt_t *astarte_mqtt)
{
    return astarte_mqtt->connection_state == ASTARTE_MQTT_CONNECTED;
}

astarte_result_t astarte_mqtt_disconnect(astarte_mqtt_t *astarte_mqtt)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    // Lock the mutex for the Astarte MQTT wrapper
    int mutex_rc = sys_mutex_lock(&astarte_mqtt->mutex, K_FOREVER);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex lock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    switch (astarte_mqtt->connection_state) {
        case ASTARTE_MQTT_CONNECTION_ERROR:
            astarte_mqtt->connection_state = ASTARTE_MQTT_DISCONNECTED;
            goto exit;
        case ASTARTE_MQTT_DISCONNECTED:
            ASTARTE_LOG_ERR("Disconnection request for a disconnected client will be ignored.");
            ares = ASTARTE_RESULT_MQTT_CLIENT_NOT_READY;
            goto exit;
        case ASTARTE_MQTT_DISCONNECTING:
            ASTARTE_LOG_ERR("Disconnection request for a disconnecting client will be ignored.");
            ares = ASTARTE_RESULT_MQTT_CLIENT_NOT_READY;
            goto exit;
        default:
            break;
    }

    // The only case in which a disconnection request is allowed is if the client is connected.
    int mqtt_rc = mqtt_disconnect(&astarte_mqtt->client);
    if (mqtt_rc < 0) {
        ASTARTE_LOG_ERR("Device disconnection failure %d", mqtt_rc);
        ares = ASTARTE_RESULT_MQTT_ERROR;
        goto exit;
    }
    astarte_mqtt->connection_state = ASTARTE_MQTT_DISCONNECTING;

exit:
    // Unlock the mutex
    mutex_rc = sys_mutex_unlock(&astarte_mqtt->mutex);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex unlock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);
    return ares;
}

void astarte_mqtt_subscribe(
    astarte_mqtt_t *astarte_mqtt, const char *topic, int max_qos, uint16_t *out_message_id)
{
    // Lock the mutex for the Astarte MQTT wrapper
    int mutex_rc = sys_mutex_lock(&astarte_mqtt->mutex, K_FOREVER);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex lock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    uint16_t message_id = mqtt_caching_get_available_message_id(&astarte_mqtt->out_msg_map);

    mqtt_caching_message_t message = {
        .type = MQTT_CACHING_SUBSCRIPTION_ENTRY,
        .topic = (char *) topic,
        .data = NULL,
        .data_size = 0,
        .qos = max_qos,
    };
    mqtt_caching_insert_message(&astarte_mqtt->out_msg_map, message_id, message);

    struct mqtt_topic topics[] = { {
        .topic = { .utf8 = topic, .size = strlen(topic) },
        .qos = max_qos,
    } };
    const struct mqtt_subscription_list ctrl_sub_list = {
        .list = topics,
        .list_count = ARRAY_SIZE(topics),
        .message_id = message_id,
    };

    if (out_message_id) {
        *out_message_id = message_id;
    }

    int ret = mqtt_subscribe(&astarte_mqtt->client, &ctrl_sub_list);
    if (ret != 0) {
        ASTARTE_LOG_ERR("MQTT subscription failed: %s, %d", strerror(-ret), ret);
    } else {
        ASTARTE_LOG_DBG("SUBSCRIBED to %s", topic);
    }

    // Unlock the mutex
    mutex_rc = sys_mutex_unlock(&astarte_mqtt->mutex);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex unlock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);
}

void astarte_mqtt_publish(astarte_mqtt_t *astarte_mqtt, const char *topic, void *data,
    size_t data_size, int qos, uint16_t *out_message_id)
{
    // Lock the mutex for the Astarte MQTT wrapper
    int mutex_rc = sys_mutex_lock(&astarte_mqtt->mutex, K_FOREVER);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex lock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    uint16_t message_id = 0;
    if (qos > 0) {
        message_id = mqtt_caching_get_available_message_id(&astarte_mqtt->out_msg_map);
    }

    if (qos > 0) {
        mqtt_caching_message_t message = {
            .type = MQTT_CACHING_PUBLISH_ENTRY,
            .topic = (char *) topic,
            .data = data,
            .data_size = data_size,
            .qos = qos,
        };
        mqtt_caching_insert_message(&astarte_mqtt->out_msg_map, message_id, message);
    }

    if (out_message_id && (qos > 0)) {
        *out_message_id = message_id;
    }

    struct mqtt_publish_param msg = { 0 };
    msg.retain_flag = 0U;
    msg.message.topic.topic.utf8 = topic;
    msg.message.topic.topic.size = strlen(topic);
    msg.message.topic.qos = qos;
    msg.message.payload.data = data;
    msg.message.payload.len = data_size;
    msg.message_id = message_id;
    int ret = mqtt_publish(&astarte_mqtt->client, &msg);
    if (ret != 0) {
        ASTARTE_LOG_ERR("MQTT publish failed: %s, %d", strerror(-ret), ret);
    } else {
        ASTARTE_LOG_DBG("PUBLISHED on topic \"%s\" [ id: %u qos: %u ], payload: %u B", topic,
            msg.message_id, msg.message.topic.qos, data_size);
        ASTARTE_LOG_HEXDUMP_DBG(data, data_size, "Published payload:");
    }

    // Unlock the mutex
    mutex_rc = sys_mutex_unlock(&astarte_mqtt->mutex);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex unlock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);
}

astarte_result_t astarte_mqtt_poll(astarte_mqtt_t *astarte_mqtt)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    // Lock the mutex for the Astarte MQTT wrapper
    int mutex_rc = sys_mutex_lock(&astarte_mqtt->mutex, K_FOREVER);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex lock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    // If in the connecting phase check that the connection timeout has not elapsed
    if ((astarte_mqtt->connection_state == ASTARTE_MQTT_CONNECTING)
        && K_TIMEOUT_EQ(sys_timepoint_timeout(astarte_mqtt->connection_timepoint), K_NO_WAIT)) {
        astarte_mqtt->connection_state = ASTARTE_MQTT_CONNECTION_ERROR;
        mqtt_disconnect(&astarte_mqtt->client);
        ASTARTE_LOG_ERR("Connection attempt has timed out!");
        goto exit;
    }

    // If the device is recovering from an unexpected disconnection and backoff time has elapsed
    // try to reconnect
    if ((astarte_mqtt->connection_state == ASTARTE_MQTT_CONNECTION_ERROR)
        && K_TIMEOUT_EQ(sys_timepoint_timeout(astarte_mqtt->reconnection_timepoint), K_NO_WAIT)) {

        // Update reconnection timepoint to the next backoff value
        uint32_t next_backoff_ms = 0;
        backoff_get_next(&astarte_mqtt->backoff_ctx, &next_backoff_ms);
        astarte_mqtt->reconnection_timepoint = sys_timepoint_calc(K_MSEC(next_backoff_ms));

        // Attempt reconnection
        ASTARTE_LOG_INF("Attempting a reconnection");
        if (astarte_mqtt_connect(astarte_mqtt) != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Failed establishing a new connection!");
            goto exit; // Exit with ASTARTE_RESULT_OK.
        }
    }

    // Only poll if device is connecting, disconnecting or connected
    if ((astarte_mqtt->connection_state != ASTARTE_MQTT_CONNECTION_ERROR)
        && (astarte_mqtt->connection_state != ASTARTE_MQTT_DISCONNECTED)) {

        if (astarte_mqtt->connection_state == ASTARTE_MQTT_CONNECTED) {
            mqtt_caching_retransmit_cbk_t retransmit_out_msg_cbk
                = mqtt_caching_retransmit_out_msg_handler;
            mqtt_caching_check_message_expiry(
                &astarte_mqtt->out_msg_map, astarte_mqtt, retransmit_out_msg_cbk);
            mqtt_caching_retransmit_cbk_t retransmit_in_msg_cbk
                = mqtt_caching_retransmit_in_msg_handler;
            mqtt_caching_check_message_expiry(
                &astarte_mqtt->in_msg_map, astarte_mqtt, retransmit_in_msg_cbk);
        }

        // Check connection and ensure to periodically ping the broker using mqtt_live
        int mqtt_rc = mqtt_live(&astarte_mqtt->client);
        if ((mqtt_rc != 0) && (mqtt_rc != -EAGAIN)) {
            ASTARTE_LOG_WRN("Fail keep alive MQTT connection: %s, %d", strerror(-mqtt_rc), mqtt_rc);
        }

        // Poll the socket (unlocking the mutex)
        struct zsock_pollfd socket_fd
            = { .fd = astarte_mqtt->client.transport.tls.sock, .events = ZSOCK_POLLIN };
        int32_t keepalive = mqtt_keepalive_time_left(&astarte_mqtt->client);
        int32_t timeout = MIN(astarte_mqtt->poll_timeout_ms, keepalive);
        mutex_rc = sys_mutex_unlock(&astarte_mqtt->mutex);
        ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex unlock failed with %d", mutex_rc);
        __ASSERT_NO_MSG(mutex_rc == 0);
        int socket_rc = zsock_poll(&socket_fd, 1, timeout);
        mutex_rc = sys_mutex_lock(&astarte_mqtt->mutex, K_FOREVER);
        ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex lock failed with %d", mutex_rc);
        __ASSERT_NO_MSG(mutex_rc == 0);
        if (socket_rc < 0) {
            ASTARTE_LOG_ERR("Poll error: %d", errno);
            astarte_mqtt->connection_state = ASTARTE_MQTT_CONNECTION_ERROR;
            ares = ASTARTE_RESULT_SOCKET_ERROR;
            goto exit;
        }
        if (socket_rc != 0) {
            // Process the MQTT response
            int mqtt_rc = mqtt_input(&astarte_mqtt->client);
            if ((mqtt_rc != 0) && (mqtt_rc != -ENOTCONN)) {
                ASTARTE_LOG_ERR("MQTT input failed (%d)", mqtt_rc);
                ares = ASTARTE_RESULT_MQTT_ERROR;
                goto exit;
            }
        }
    }

exit:
    // Unlock the mutex
    mutex_rc = sys_mutex_unlock(&astarte_mqtt->mutex);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex unlock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);
    return ares;
}

bool astarte_mqtt_has_pending_outgoing(astarte_mqtt_t *astarte_mqtt)
{
    return !sys_hashmap_is_empty(&astarte_mqtt->out_msg_map);
}

void astarte_mqtt_clear_all_pending(astarte_mqtt_t *astarte_mqtt)
{
    mqtt_caching_clear_messages(&astarte_mqtt->in_msg_map);
    mqtt_caching_clear_messages(&astarte_mqtt->out_msg_map);
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static void handle_connack_event(
    astarte_mqtt_t *astarte_mqtt, const struct mqtt_connack_param connack)
{
    ASTARTE_LOG_DBG("Received CONNACK packet, session present: %d", connack.session_present_flag);
    // Reset the backoff context for the next connection failure
    backoff_context_init(&astarte_mqtt->backoff_ctx,
        CONFIG_ASTARTE_DEVICE_SDK_RECONNECTION_MQTT_BACKOFF_INITIAL_MS,
        CONFIG_ASTARTE_DEVICE_SDK_RECONNECTION_MQTT_BACKOFF_MAX_MS, true);

    if (astarte_mqtt->connection_state == ASTARTE_MQTT_CONNECTING) {
        astarte_mqtt->connection_state = ASTARTE_MQTT_CONNECTED;
    }

    if (connack.session_present_flag == 0) {
        mqtt_caching_clear_messages(&astarte_mqtt->in_msg_map);
        mqtt_caching_clear_messages(&astarte_mqtt->out_msg_map);
    }

    astarte_mqtt->on_connected_cbk(astarte_mqtt, connack);
}
static void handle_disconnection_event(astarte_mqtt_t *astarte_mqtt)
{
    ASTARTE_LOG_DBG("MQTT client disconnected");

    switch (astarte_mqtt->connection_state) {
        case ASTARTE_MQTT_CONNECTING:
        case ASTARTE_MQTT_CONNECTED:
            astarte_mqtt->connection_state = ASTARTE_MQTT_CONNECTION_ERROR;
            break;
        case ASTARTE_MQTT_DISCONNECTING:
            astarte_mqtt->connection_state = ASTARTE_MQTT_DISCONNECTED;
            break;
        default:
            break;
    }
    astarte_mqtt->on_disconnected_cbk(astarte_mqtt);
}
static void handle_publish_event(astarte_mqtt_t *astarte_mqtt, struct mqtt_publish_param publish)
{
    uint16_t message_id = publish.message_id;
    ASTARTE_LOG_DBG("Received PUBLISH packet (%u)", message_id);
    int ret = 0U;
    size_t received = 0U;
    uint32_t message_size = publish.message.payload.len;
    char msg_buffer[CONFIG_ASTARTE_DEVICE_SDK_MQTT_MAX_MSG_SIZE] = { 0 };
    const bool discarded = message_size > CONFIG_ASTARTE_DEVICE_SDK_MQTT_MAX_MSG_SIZE;

    ASTARTE_LOG_DBG("RECEIVED on topic \"%.*s\" [ id: %u qos: %u ] payload: %u / %u B",
        publish.message.topic.topic.size, (const char *) publish.message.topic.topic.utf8,
        message_id, publish.message.topic.qos, message_size,
        CONFIG_ASTARTE_DEVICE_SDK_MQTT_MAX_MSG_SIZE);

    while (received < message_size) {
        char *pkt = discarded ? msg_buffer : &msg_buffer[received];

        ret = mqtt_read_publish_payload_blocking(
            &astarte_mqtt->client, pkt, CONFIG_ASTARTE_DEVICE_SDK_MQTT_MAX_MSG_SIZE);
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

        if (mqtt_caching_find_message(&astarte_mqtt->in_msg_map, message_id)) {
            ASTARTE_LOG_WRN("Received duplicated PUBLISH QoS 2 with message ID (%d).", message_id);
            return;
        }

        mqtt_caching_message_t message = {
            .type = MQTT_CACHING_PUBREC_ENTRY,
            .topic = NULL,
            .data = NULL,
            .data_size = 0,
            .qos = 2,
        };
        mqtt_caching_insert_message(&astarte_mqtt->in_msg_map, message_id, message);
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
    char *topic = astarte_calloc(topic_len + 1, sizeof(char));
    if (!topic) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        return;
    }
    memcpy(topic, publish.message.topic.topic.utf8, topic_len);
    astarte_mqtt->on_incoming_cbk(astarte_mqtt, topic, topic_len, msg_buffer, message_size);
    astarte_free(topic);
}
static void handle_pubrel_event(astarte_mqtt_t *astarte_mqtt, struct mqtt_pubrel_param pubrel)
{
    uint16_t message_id = pubrel.message_id;
    ASTARTE_LOG_DBG("Received PUBREL packet (%u)", message_id);

    mqtt_caching_remove_message(&astarte_mqtt->in_msg_map, message_id);

    struct mqtt_pubcomp_param pubcomp = { .message_id = message_id };
    int res = mqtt_publish_qos2_complete(&astarte_mqtt->client, &pubcomp);
    if (res != 0) {
        ASTARTE_LOG_ERR("MQTT PUBCOMP transmission error %d", res);
    }
}
static void handle_puback_event(astarte_mqtt_t *astarte_mqtt, struct mqtt_puback_param puback)
{
    uint16_t message_id = puback.message_id;
    ASTARTE_LOG_DBG("Received PUBACK packet (%u)", message_id);

    mqtt_caching_remove_message(&astarte_mqtt->out_msg_map, message_id);

    if (astarte_mqtt->on_delivered_cbk) {
        astarte_mqtt->on_delivered_cbk(astarte_mqtt, message_id);
    }
}
static void handle_pubrec_event(astarte_mqtt_t *astarte_mqtt, struct mqtt_pubrec_param pubrec)
{
    uint16_t message_id = pubrec.message_id;
    ASTARTE_LOG_DBG("Received PUBREC packet (%u)", message_id);

    mqtt_caching_update_message_expiry(&astarte_mqtt->out_msg_map, message_id);

    // Transmit a PUBREL
    const struct mqtt_pubrel_param rel_param = { .message_id = message_id };
    int err = mqtt_publish_qos2_release(&astarte_mqtt->client, &rel_param);
    if (err != 0) {
        ASTARTE_LOG_ERR("Failed to send MQTT PUBREL: %d", err);
    }
}
static void handle_pubcomp_event(astarte_mqtt_t *astarte_mqtt, struct mqtt_pubcomp_param pubcomp)
{
    uint16_t message_id = pubcomp.message_id;
    ASTARTE_LOG_DBG("Received PUBCOMP packet (%u)", message_id);

    mqtt_caching_remove_message(&astarte_mqtt->out_msg_map, message_id);

    if (astarte_mqtt->on_delivered_cbk) {
        astarte_mqtt->on_delivered_cbk(astarte_mqtt, message_id);
    }
}
static void handle_suback_event(astarte_mqtt_t *astarte_mqtt, struct mqtt_suback_param suback)
{
    uint16_t message_id = suback.message_id;
    ASTARTE_LOG_DBG("Received SUBACK packet (%u)", message_id);

    mqtt_caching_remove_message(&astarte_mqtt->out_msg_map, message_id);

    if (astarte_mqtt->on_subscribed_cbk) {
        enum mqtt_suback_return_code return_code = MQTT_SUBACK_FAILURE;
        if (suback.return_codes.len == 0) {
            ASTARTE_LOG_ERR("Missing return code for SUBACK message (%u)", message_id);
        } else {
            return_code = (enum mqtt_suback_return_code) * suback.return_codes.data;
        }
        astarte_mqtt->on_subscribed_cbk(astarte_mqtt, message_id, return_code);
    }
}
