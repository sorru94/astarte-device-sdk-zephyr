/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "log.h"
#include "mqtt/caching.h"
#include "mqtt/core.h"

ASTARTE_LOG_MODULE_DECLARE(astarte_mqtt, CONFIG_ASTARTE_DEVICE_SDK_MQTT_LOG_LEVEL);

/************************************************
 *       Callbacks declaration/definition       *
 ***********************************************/

void astarte_mqtt_subscribe(
    astarte_mqtt_t *astarte_mqtt, const char *topic, int max_qos, uint16_t *out_message_id)
{
    // Lock the mutex for the Astarte MQTT wrapper
    int mutex_rc = sys_mutex_lock(&astarte_mqtt->mutex, K_FOREVER);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex lock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    uint16_t message_id = astarte_mqtt_caching_get_available_message_id(&astarte_mqtt->out_msg_map);

    astarte_mqtt_caching_message_t message = {
        .type = MQTT_CACHING_SUBSCRIPTION_ENTRY,
        .topic = (char *) topic,
        .data = NULL,
        .data_size = 0,
        .qos = max_qos,
    };
    astarte_mqtt_caching_insert_message(&astarte_mqtt->out_msg_map, message_id, message);

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
        message_id = astarte_mqtt_caching_get_available_message_id(&astarte_mqtt->out_msg_map);
    }

    if (qos > 0) {
        astarte_mqtt_caching_message_t message = {
            .type = MQTT_CACHING_PUBLISH_ENTRY,
            .topic = (char *) topic,
            .data = data,
            .data_size = data_size,
            .qos = qos,
        };
        astarte_mqtt_caching_insert_message(&astarte_mqtt->out_msg_map, message_id, message);
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

bool astarte_mqtt_has_pending_outgoing(astarte_mqtt_t *astarte_mqtt)
{
    return !sys_hashmap_is_empty(&astarte_mqtt->out_msg_map);
}

void astarte_mqtt_clear_all_pending(astarte_mqtt_t *astarte_mqtt)
{
    astarte_mqtt_caching_clear_messages(&astarte_mqtt->in_msg_map);
    astarte_mqtt_caching_clear_messages(&astarte_mqtt->out_msg_map);
}
