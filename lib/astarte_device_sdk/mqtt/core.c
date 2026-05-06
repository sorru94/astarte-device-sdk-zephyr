/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "mqtt/core.h"

#include "mqtt/caching.h"
#include "mqtt/events.h"

#include <zephyr/net/socket.h>
#include <zephyr/net/tls_credentials.h>

#include "log.h"

ASTARTE_LOG_MODULE_REGISTER(astarte_mqtt, CONFIG_ASTARTE_DEVICE_SDK_MQTT_LOG_LEVEL);

/************************************************
 *       Checks over configuration values       *
 ***********************************************/

#ifdef CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_MQTT
#warning "TLS has been disabled (unsafe)!"
#endif /* defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_MQTT) */

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
            astarte_mqtt_handle_connack_event(astarte_mqtt, evt->param.connack);
            break;
        case MQTT_EVT_DISCONNECT:
            if (evt->result != 0) {
                ASTARTE_LOG_ERR("MQTT disconnection event error: %s", strerror(-evt->result));
            }
            astarte_mqtt_handle_disconnection_event(astarte_mqtt);
            break;
        case MQTT_EVT_PUBLISH:
            if (evt->result != 0) {
                ASTARTE_LOG_ERR("MQTT PUBLISH event error: %s", strerror(-evt->result));
                break;
            }
            astarte_mqtt_handle_publish_event(astarte_mqtt, evt->param.publish);
            break;
        case MQTT_EVT_PUBREL:
            if (evt->result != 0) {
                ASTARTE_LOG_ERR("MQTT PUBREL event error: %s", strerror(-evt->result));
                break;
            }
            astarte_mqtt_handle_pubrel_event(astarte_mqtt, evt->param.pubrel);
            break;
        case MQTT_EVT_PUBACK:
            if (evt->result != 0) {
                ASTARTE_LOG_ERR("MQTT PUBACK event error: %s", strerror(-evt->result));
                break;
            }
            astarte_mqtt_handle_puback_event(astarte_mqtt, evt->param.puback);
            break;
        case MQTT_EVT_PUBREC:
            if (evt->result != 0) {
                ASTARTE_LOG_ERR("MQTT PUBREC event error: %s", strerror(-evt->result));
                break;
            }
            astarte_mqtt_handle_pubrec_event(astarte_mqtt, evt->param.pubrec);
            break;
        case MQTT_EVT_PUBCOMP:
            if (evt->result != 0) {
                ASTARTE_LOG_ERR("MQTT PUBCOMP event error: %s", strerror(-evt->result));
                break;
            }
            astarte_mqtt_handle_pubcomp_event(astarte_mqtt, evt->param.pubcomp);
            break;
        case MQTT_EVT_SUBACK:
            if (evt->result != 0) {
                ASTARTE_LOG_ERR("MQTT SUBACK event error: %s", strerror(-evt->result));
                break;
            }
            astarte_mqtt_handle_suback_event(astarte_mqtt, evt->param.suback);
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
    astarte_mqtt_t *astarte_mqtt, uint16_t message_id, astarte_mqtt_caching_message_t message)
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
            int ret_sub = mqtt_subscribe(&astarte_mqtt->client, &ctrl_sub_list);
            if (ret_sub != 0) {
                ASTARTE_LOG_ERR("MQTT subscription failed: %s, %d", strerror(-ret_sub), ret_sub);
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
    astarte_mqtt_t *astarte_mqtt, uint16_t message_id, astarte_mqtt_caching_message_t message)
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

    *astarte_mqtt = (astarte_mqtt_t){ 0 };
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
    backoff_init(&astarte_mqtt->backoff_ctx,
        CONFIG_ASTARTE_DEVICE_SDK_RECONNECTION_BACKOFF_MULT_COEFF_MS,
        CONFIG_ASTARTE_DEVICE_SDK_RECONNECTION_BACKOFF_CUTOFF_COEFF_MS);

    // Initialize the reconnection timepoint
    astarte_mqtt->reconnection_timepoint = sys_timepoint_calc(K_NO_WAIT);

    // Initialize the caching hashmaps
    // NOLINTNEXTLINE
    astarte_mqtt->out_msg_map_config = (const struct sys_hashmap_config) SYS_HASHMAP_CONFIG(
        CONFIG_ASTARTE_DEVICE_SDK_ADVANCED_MQTT_CACHING_HASMAPS_SIZE,
        SYS_HASHMAP_DEFAULT_LOAD_FACTOR);
    astarte_mqtt->out_msg_map = (struct sys_hashmap){
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
    astarte_mqtt->in_msg_map = (struct sys_hashmap){
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
    int getaddrinfo_rc = zsock_getaddrinfo(
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
#ifndef CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_MQTT
        CONFIG_ASTARTE_DEVICE_SDK_MQTTS_CA_CERT_TAG,
#endif
        CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG,
    };

    struct mqtt_sec_config *tls_config = &(astarte_mqtt->client.transport.tls.config);
#ifndef CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_MQTT
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
        zsock_freeaddrinfo(broker_addrinfo);
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
    int mqtt_rc = mqtt_disconnect(&astarte_mqtt->client, NULL);
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
        mqtt_disconnect(&astarte_mqtt->client, NULL);
        ASTARTE_LOG_ERR("Connection attempt has timed out!");
        goto exit;
    }

    // If the device is recovering from an unexpected disconnection and backoff time has elapsed
    // try to reconnect
    if ((astarte_mqtt->connection_state == ASTARTE_MQTT_CONNECTION_ERROR)
        && K_TIMEOUT_EQ(sys_timepoint_timeout(astarte_mqtt->reconnection_timepoint), K_NO_WAIT)) {

        // Update reconnection timepoint to the next backoff value
        uint64_t next_backoff_ms = backoff_get_next_delay(&astarte_mqtt->backoff_ctx);
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
            astarte_mqtt_caching_retransmit_cbk_t retransmit_out_msg_cbk
                = mqtt_caching_retransmit_out_msg_handler;
            astarte_mqtt_caching_check_message_expiry(
                &astarte_mqtt->out_msg_map, astarte_mqtt, retransmit_out_msg_cbk);
            astarte_mqtt_caching_retransmit_cbk_t retransmit_in_msg_cbk
                = mqtt_caching_retransmit_in_msg_handler;
            astarte_mqtt_caching_check_message_expiry(
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
