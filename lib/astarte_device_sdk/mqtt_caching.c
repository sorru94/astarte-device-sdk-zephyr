/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "mqtt_caching.h"

#include "heap.h"
#include "log.h"

ASTARTE_LOG_MODULE_DECLARE(astarte_mqtt, CONFIG_ASTARTE_DEVICE_SDK_MQTT_LOG_LEVEL);

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

/** @brief Generic MQTT caching hashmap entry. */
struct mqtt_caching_map_entry
{
    /** @brief End of validity for this map entry. */
    k_timepoint_t end_of_validity;
    /** @brief MQTT cached message */
    mqtt_caching_message_t message;
};

/************************************************
 *         Global functions definitions         *
 ***********************************************/

uint16_t mqtt_caching_get_available_message_id(struct sys_hashmap *map)
{
    // The message ID, also known as packet ID, can't be zero (MQTT Version 3.1.1 section 2.3.1).
    static uint16_t last_message_id = 0U;
    // Increment the message ID untill it's not contained in any hashmap
    do {
        // Wrap around skipping 0
        if (last_message_id == UINT16_MAX) {
            last_message_id = 0U;
        }
        last_message_id++;
    } while (sys_hashmap_contains_key(map, last_message_id));
    return last_message_id;
}

void mqtt_caching_insert_message(
    struct sys_hashmap *map, uint16_t identifier, mqtt_caching_message_t message)
{
    char *topic_cpy = NULL;
    uint8_t *data_cpy = NULL;
    struct mqtt_caching_map_entry *map_entry = NULL;
    ASTARTE_LOG_DBG("Adding message to map, id: %d.", identifier);

    if (sys_hashmap_contains_key(map, identifier)) {
        ASTARTE_LOG_ERR("Message already cached, id: %d.", identifier);
        goto error;
    }

    map_entry = astarte_calloc(1, sizeof(struct mqtt_caching_map_entry));
    if (!map_entry) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        goto error;
    }

    if (message.topic) {
        topic_cpy = astarte_calloc(strlen(message.topic) + 1, sizeof(char));
        if (!topic_cpy) {
            ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
            goto error;
        }
        strncpy(topic_cpy, message.topic, strlen(message.topic) + 1);
    }

    if (message.data && (message.data_size != 0)) {
        data_cpy = astarte_calloc(message.data_size, sizeof(uint8_t));
        if (!data_cpy) {
            ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
            goto error;
        }
        memcpy(data_cpy, message.data, message.data_size);
    }

    map_entry->end_of_validity = sys_timepoint_calc(K_SECONDS(CONFIG_MQTT_KEEPALIVE));
    map_entry->message.type = message.type;
    map_entry->message.topic = topic_cpy;
    map_entry->message.data = data_cpy;
    map_entry->message.data_size = message.data_size;
    map_entry->message.qos = message.qos;

    int ret = sys_hashmap_insert(map, identifier, POINTER_TO_UINT(map_entry), NULL);
    if (ret != 1) {
        ASTARTE_LOG_ERR("Failed adding entry to the hashmap. Err: %d", ret);
        goto error;
    }

    return;

error:
    astarte_free(topic_cpy);
    astarte_free(data_cpy);
    astarte_free(map_entry);
}

bool mqtt_caching_find_message(struct sys_hashmap *map, uint16_t message_id)
{
    return sys_hashmap_contains_key(map, message_id);
}

void mqtt_caching_check_message_expiry(struct sys_hashmap *map, astarte_mqtt_t *astarte_mqtt,
    mqtt_caching_retransmit_cbk_t retransmit_cbk)
{
    // Loop over all the messages in the hashmap
    struct sys_hashmap_iterator iter = { 0 };
    map->api->iter(map, &iter);
    while (sys_hashmap_iterator_has_next(&iter)) {
        iter.next(&iter);
        // Check if the message has expired
        uint16_t message_id = iter.key;
        // NOLINTNEXTLINE(performance-no-int-to-ptr) Unavoidable due to the hashmap structure
        struct mqtt_caching_map_entry *map_entry = UINT_TO_POINTER(iter.value);
        if (K_TIMEOUT_EQ(sys_timepoint_timeout(map_entry->end_of_validity), K_NO_WAIT)) {
            ASTARTE_LOG_ERR("Message ID (%d) has timed out, it will be retransmitted.", message_id);
            // Update end of validity for this message
            map_entry->end_of_validity = sys_timepoint_calc(K_SECONDS(CONFIG_MQTT_KEEPALIVE));
            // Re-send the message
            retransmit_cbk(astarte_mqtt, message_id, map_entry->message);
        }
    }
}

void mqtt_caching_update_message_expiry(struct sys_hashmap *map, uint16_t message_id)
{
    ASTARTE_LOG_DBG("Updating message expiration in hashmap, id: %d.", message_id);

    // Check if message ID is contained in the hash map
    uint64_t value = 0;
    if (sys_hashmap_get(map, message_id, &value)) {
        // Replace the timestamp for the message ID with a fresh one
        // NOLINTNEXTLINE(performance-no-int-to-ptr) Unavoidable due to the hashmap structure
        struct mqtt_caching_map_entry *map_entry = UINT_TO_POINTER(value);
        map_entry->end_of_validity = sys_timepoint_calc(K_SECONDS(CONFIG_MQTT_KEEPALIVE));
    } else {
        ASTARTE_LOG_ERR("Message ID (%d) not found in hashmap.", message_id);
    }
}

void mqtt_caching_remove_message(struct sys_hashmap *map, uint16_t message_id)
{
    ASTARTE_LOG_DBG("Removing message from hashmap (%d)", message_id);

    uint64_t value = 0;
    if (sys_hashmap_remove(map, message_id, &value)) {
        // NOLINTNEXTLINE(performance-no-int-to-ptr) Unavoidable due to the hashmap structure
        struct mqtt_caching_map_entry *map_entry = UINT_TO_POINTER(value);
        if (map_entry->message.topic) {
            astarte_free(map_entry->message.topic);
        }
        if (map_entry->message.data) {
            astarte_free(map_entry->message.data);
        }
        astarte_free(map_entry);
    } else {
        ASTARTE_LOG_ERR("Message ID (%d) not found in hashmap.", message_id);
    }
}

void mqtt_caching_clear_messages(struct sys_hashmap *map)
{
    sys_hashmap_clear(map, NULL, NULL);
}
