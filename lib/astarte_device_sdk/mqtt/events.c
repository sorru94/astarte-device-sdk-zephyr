/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "mqtt/events.h"

#include <stdlib.h>

#include "alloc.h"
#include "log.h"
#include "mqtt/caching.h"

ASTARTE_LOG_MODULE_DECLARE(astarte_mqtt, CONFIG_ASTARTE_DEVICE_SDK_MQTT_LOG_LEVEL);

/************************************************
 *         Static functions declaration         *
 ***********************************************/

static int read_publish_payload(astarte_mqtt_t *astarte_mqtt, char *msg_buffer, size_t alloc_size,
    uint32_t message_size, bool discarded);
static int acknowledge_qos(astarte_mqtt_t *astarte_mqtt, uint16_t message_id, uint8_t qos);

/************************************************
 *       Callbacks declaration/definition       *
 ***********************************************/

void astarte_mqtt_handle_connack_event(
    astarte_mqtt_t *astarte_mqtt, struct mqtt_connack_param connack)
{
    ASTARTE_LOG_DBG("Received CONNACK packet, session present: %d", connack.session_present_flag);
    // Reset the backoff context for the next connection failure
    backoff_init(&astarte_mqtt->backoff_ctx,
        CONFIG_ASTARTE_DEVICE_SDK_RECONNECTION_BACKOFF_MULT_COEFF_MS,
        CONFIG_ASTARTE_DEVICE_SDK_RECONNECTION_BACKOFF_CUTOFF_COEFF_MS);

    if (astarte_mqtt->connection_state == ASTARTE_MQTT_CONNECTING) {
        astarte_mqtt->connection_state = ASTARTE_MQTT_CONNECTED;
    }

    if (connack.session_present_flag == 0) {
        astarte_mqtt_caching_clear_messages(&astarte_mqtt->in_msgs);
        astarte_mqtt_caching_clear_messages(&astarte_mqtt->out_msgs);
    }

    if (astarte_mqtt->on_connected_cbk) {
        astarte_mqtt->on_connected_cbk(astarte_mqtt, connack);
    }
}

void astarte_mqtt_handle_disconnection_event(astarte_mqtt_t *astarte_mqtt)
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
    if (astarte_mqtt->on_disconnected_cbk) {
        astarte_mqtt->on_disconnected_cbk(astarte_mqtt);
    }
}

void astarte_mqtt_handle_publish_event(
    astarte_mqtt_t *astarte_mqtt, struct mqtt_publish_param publish)
{
    uint16_t message_id = publish.message_id;
    uint32_t message_size = publish.message.payload.len;
    char *topic = NULL;
    char *msg_buffer = NULL;

    ASTARTE_LOG_DBG("Received PUBLISH packet (%u)", message_id);

    // Safety limit to prevent unbounded allocations
    const bool discarded = message_size > CONFIG_ASTARTE_DEVICE_SDK_MQTT_MAX_MSG_SIZE;
    size_t alloc_size = discarded ? CONFIG_ASTARTE_DEVICE_SDK_MQTT_MAX_MSG_SIZE : message_size;

    // Safely handle 0-byte payloads
    if (alloc_size > 0) {
        msg_buffer = astarte_calloc(alloc_size, sizeof(char));
        if (!msg_buffer) {
            ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
            goto exit;
        }
    }

    ASTARTE_LOG_DBG("RECEIVED on topic \"%.*s\" [ id: %u qos: %u ] payload: %u / %u B",
        publish.message.topic.topic.size, (const char *) publish.message.topic.topic.utf8,
        message_id, publish.message.topic.qos, message_size,
        CONFIG_ASTARTE_DEVICE_SDK_MQTT_MAX_MSG_SIZE);

    if (read_publish_payload(astarte_mqtt, msg_buffer, alloc_size, message_size, discarded) < 0) {
        goto exit;
    }

    ASTARTE_LOG_HEXDUMP_DBG(msg_buffer, MIN(message_size, 256U), "Received payload:");

    if (acknowledge_qos(astarte_mqtt, message_id, publish.message.topic.qos) < 0) {
        goto exit;
    }

    size_t topic_len = publish.message.topic.topic.size;
    topic = astarte_calloc(topic_len + 1, sizeof(char));
    if (!topic) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        goto exit;
    }
    memcpy(topic, publish.message.topic.topic.utf8, topic_len);

    if (astarte_mqtt->on_incoming_cbk) {
        astarte_mqtt->on_incoming_cbk(astarte_mqtt, topic, topic_len, msg_buffer, message_size);
    }

exit:
    astarte_free(topic);
    astarte_free(msg_buffer);
}

void astarte_mqtt_handle_pubrel_event(astarte_mqtt_t *astarte_mqtt, struct mqtt_pubrel_param pubrel)
{
    uint16_t message_id = pubrel.message_id;
    ASTARTE_LOG_DBG("Received PUBREL packet (%u)", message_id);

    astarte_mqtt_caching_remove_message(&astarte_mqtt->in_msgs, message_id);

    struct mqtt_pubcomp_param pubcomp = { .message_id = message_id };
    int res = mqtt_publish_qos2_complete(&astarte_mqtt->client, &pubcomp);
    if (res != 0) {
        ASTARTE_LOG_ERR("MQTT PUBCOMP transmission error %d", res);
    }
}

void astarte_mqtt_handle_puback_event(astarte_mqtt_t *astarte_mqtt, struct mqtt_puback_param puback)
{
    uint16_t message_id = puback.message_id;
    ASTARTE_LOG_DBG("Received PUBACK packet (%u)", message_id);

    astarte_mqtt_caching_remove_message(&astarte_mqtt->out_msgs, message_id);

    if (astarte_mqtt->on_delivered_cbk) {
        astarte_mqtt->on_delivered_cbk(astarte_mqtt, message_id);
    }
}

void astarte_mqtt_handle_pubrec_event(astarte_mqtt_t *astarte_mqtt, struct mqtt_pubrec_param pubrec)
{
    uint16_t message_id = pubrec.message_id;
    ASTARTE_LOG_DBG("Received PUBREC packet (%u)", message_id);

    astarte_mqtt_caching_update_message_expiry(&astarte_mqtt->out_msgs, message_id);

    // Transmit a PUBREL
    const struct mqtt_pubrel_param rel_param = { .message_id = message_id };
    int err = mqtt_publish_qos2_release(&astarte_mqtt->client, &rel_param);
    if (err != 0) {
        ASTARTE_LOG_ERR("Failed to send MQTT PUBREL: %d", err);
    }
}

void astarte_mqtt_handle_pubcomp_event(
    astarte_mqtt_t *astarte_mqtt, struct mqtt_pubcomp_param pubcomp)
{
    uint16_t message_id = pubcomp.message_id;
    ASTARTE_LOG_DBG("Received PUBCOMP packet (%u)", message_id);

    astarte_mqtt_caching_remove_message(&astarte_mqtt->out_msgs, message_id);

    if (astarte_mqtt->on_delivered_cbk) {
        astarte_mqtt->on_delivered_cbk(astarte_mqtt, message_id);
    }
}

void astarte_mqtt_handle_suback_event(astarte_mqtt_t *astarte_mqtt, struct mqtt_suback_param suback)
{
    uint16_t message_id = suback.message_id;
    ASTARTE_LOG_DBG("Received SUBACK packet (%u)", message_id);

    astarte_mqtt_caching_remove_message(&astarte_mqtt->out_msgs, message_id);

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

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static int read_publish_payload(astarte_mqtt_t *astarte_mqtt, char *msg_buffer, size_t alloc_size,
    uint32_t message_size, bool discarded)
{
    int ret = 0U;
    size_t received = 0U;

    while (received < message_size) {
        char *pkt = discarded ? msg_buffer : &msg_buffer[received];
        size_t capacity = discarded ? alloc_size : (alloc_size - received);

        ret = mqtt_read_publish_payload_blocking(&astarte_mqtt->client, pkt, capacity);
        if (ret < 0) {
            ASTARTE_LOG_ERR("Error reading payload of PUBLISH packet %s", strerror(ret));
            return -1;
        }

        received += ret;
    }

    if (received != message_size) {
        ASTARTE_LOG_ERR("Incoherent expected and read MQTT publish payload lengths.");
        return -1;
    }

    if (discarded) {
        ASTARTE_LOG_ERR("Insufficient space in the Astarte MQTT reception buffer.");
        return -1;
    }

    return 0;
}

static int acknowledge_qos(astarte_mqtt_t *astarte_mqtt, uint16_t message_id, uint8_t qos)
{
    int ret = 0U;

    if (qos == MQTT_QOS_1_AT_LEAST_ONCE) {
        struct mqtt_puback_param puback = { .message_id = message_id };
        ret = mqtt_publish_qos1_ack(&astarte_mqtt->client, &puback);
        if (ret != 0) {
            ASTARTE_LOG_ERR("MQTT PUBACK transmission error %d", ret);
        }
    } else if (qos == MQTT_QOS_2_EXACTLY_ONCE) {
        struct mqtt_pubrec_param pubrec = { .message_id = message_id };
        ret = mqtt_publish_qos2_receive(&astarte_mqtt->client, &pubrec);
        if (ret != 0) {
            ASTARTE_LOG_ERR("MQTT PUBREC transmission error %d", ret);
        }

        if (astarte_mqtt_caching_find_message(&astarte_mqtt->in_msgs, message_id)) {
            ASTARTE_LOG_WRN("Received duplicated PUBLISH QoS 2 with message ID (%d).", message_id);
            return -1;
        }

        astarte_storage_mqtt_message_t message = {
            .type = STORAGE_MQTT_PUBREC_ENTRY,
            .topic = NULL,
            .data = NULL,
            .data_size = 0,
            .qos = 2,
        };
        astarte_mqtt_caching_insert_message(&astarte_mqtt->in_msgs, message_id, message);
    }

    return 0;
}
