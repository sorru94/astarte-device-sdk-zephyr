/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "mqtt/caching.h"

#include "alloc.h"
#include "key_value/core.h"
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
    astarte_storage_mqtt_message_t message;
};

/************************************************
 *         Global functions definitions         *
 ***********************************************/

#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
void astarte_mqtt_caching_init(astarte_mqtt_caching_t *caching, astarte_storage_data_t *storage,
    enum astarte_storage_mqtt_message_direction direction)
#else
void astarte_mqtt_caching_init(astarte_mqtt_caching_t *caching)
#endif
{
    // NOLINTNEXTLINE
    caching->map_config = (const struct sys_hashmap_config) SYS_HASHMAP_CONFIG(
        CONFIG_ASTARTE_DEVICE_SDK_ADVANCED_MQTT_CACHING_HASMAPS_SIZE,
        SYS_HASHMAP_DEFAULT_LOAD_FACTOR);
    caching->map = (struct sys_hashmap){
        .api = &sys_hashmap_sc_api,
        .config = &caching->map_config,
        .data = &caching->map_data,
        .hash_func = sys_hash32,
        .alloc_func = SYS_HASHMAP_DEFAULT_ALLOCATOR,
    };
#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
    caching->storage = storage;
    caching->direction = direction;
    astarte_mqtt_caching_restore_from_flash(caching);
#endif
}

uint16_t astarte_mqtt_caching_get_available_message_id(astarte_mqtt_caching_t *caching)
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
    } while (sys_hashmap_contains_key(&caching->map, last_message_id));
    return last_message_id;
}

void astarte_mqtt_caching_insert_message(
    astarte_mqtt_caching_t *caching, uint16_t identifier, astarte_storage_mqtt_message_t message)
{
    char *topic_cpy = NULL;
    uint8_t *data_cpy = NULL;
    struct mqtt_caching_map_entry *map_entry = NULL;
    ASTARTE_LOG_DBG("Adding message to map, id: %d.", identifier);

    if (sys_hashmap_contains_key(&caching->map, identifier)) {
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

    int ret = sys_hashmap_insert(&caching->map, identifier, POINTER_TO_UINT(map_entry), NULL);
    if (ret != 1) {
        ASTARTE_LOG_ERR("Failed adding entry to the hashmap. Err: %d", ret);
        goto error;
    }

#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
    // TODO: we need to know the direction
    astarte_result_t ares
        = astarte_storage_mqtt_insert(caching->storage, caching->direction, identifier, &message);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed to persist message %d to flash. Err: %d", identifier, ares);
    }
#endif

    return;

error:
    astarte_free(topic_cpy);
    astarte_free(data_cpy);
    astarte_free(map_entry);
}

bool astarte_mqtt_caching_find_message(astarte_mqtt_caching_t *caching, uint16_t message_id)
{
    return sys_hashmap_contains_key(&caching->map, message_id);
}

void astarte_mqtt_caching_check_message_expiry(
    astarte_mqtt_caching_t *caching, astarte_mqtt_caching_retransmit_cbk_t retransmit_cbk)
{
    // Loop over all the messages in the hashmap
    struct sys_hashmap_iterator iter = { 0 };
    caching->map.api->iter(&caching->map, &iter);
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
            retransmit_cbk(caching, message_id, map_entry->message);
        }
    }
}

void astarte_mqtt_caching_update_message_expiry(
    astarte_mqtt_caching_t *caching, uint16_t message_id)
{
    ASTARTE_LOG_DBG("Updating message expiration in hashmap, id: %d.", message_id);

    // Check if message ID is contained in the hash map
    uint64_t value = 0;
    if (sys_hashmap_get(&caching->map, message_id, &value)) {
        // Replace the timestamp for the message ID with a fresh one
        // NOLINTNEXTLINE(performance-no-int-to-ptr) Unavoidable due to the hashmap structure
        struct mqtt_caching_map_entry *map_entry = UINT_TO_POINTER(value);
        map_entry->end_of_validity = sys_timepoint_calc(K_SECONDS(CONFIG_MQTT_KEEPALIVE));
    } else {
        ASTARTE_LOG_ERR("Message ID (%d) not found in hashmap.", message_id);
    }
}

void astarte_mqtt_caching_remove_message(astarte_mqtt_caching_t *caching, uint16_t message_id)
{
    ASTARTE_LOG_DBG("Removing message from hashmap (%d)", message_id);

    uint64_t value = 0;
    if (sys_hashmap_remove(&caching->map, message_id, &value)) {
        // NOLINTNEXTLINE(performance-no-int-to-ptr) Unavoidable due to the hashmap structure
        struct mqtt_caching_map_entry *map_entry = UINT_TO_POINTER(value);
        astarte_free(map_entry->message.topic);
        astarte_free(map_entry->message.data);
        astarte_free(map_entry);
#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
        // Delete also from flash
        astarte_result_t ares
            = astarte_storage_mqtt_delete(caching->storage, caching->direction, message_id);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Failed to delete message %d from flash. Err: %d", message_id, ares);
        } else {
            ASTARTE_LOG_DBG("Message %d deleted from flash.", message_id);
        }
#endif
    } else {
        ASTARTE_LOG_ERR("Message ID (%d) not found in hashmap.", message_id);
    }
}

void astarte_mqtt_caching_clear_messages(astarte_mqtt_caching_t *caching)
{
    ASTARTE_LOG_DBG("Removing all messages from hashmap and flash.");

    // Loop over all the messages in the hashmap
    struct sys_hashmap_iterator iter = { 0 };
    caching->map.api->iter(&caching->map, &iter);
    while (sys_hashmap_iterator_has_next(&iter)) {
        iter.next(&iter);
        // NOLINTNEXTLINE(performance-no-int-to-ptr) Unavoidable due to the hashmap structure
        struct mqtt_caching_map_entry *map_entry = UINT_TO_POINTER(iter.value);
        astarte_free(map_entry->message.topic);
        astarte_free(map_entry->message.data);
        astarte_free(map_entry);
#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
        // Remove also from flash
        astarte_result_t ares
            = astarte_storage_mqtt_delete(caching->storage, caching->direction, iter.key);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Failed to delete message %llu from flash. Err: %d", iter.key, ares);
        } else {
            ASTARTE_LOG_DBG("Message %llu deleted from flash.", iter.key);
        }
#endif
    }

    // Clear the internal structures of the map
    sys_hashmap_clear(&caching->map, NULL, NULL);
}

void astarte_mqtt_caching_restore_from_flash(astarte_mqtt_caching_t *caching)
{
#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
    ASTARTE_LOG_INF("Scanning flash to restore MQTT cache...");
    int restored_count = 0;

    // The message ID cannot be zero according to MQTT Version 3.1.1
    for (uint32_t i = 1; i <= UINT16_MAX; i++) {
        uint16_t message_id = (uint16_t) i;

        astarte_storage_mqtt_message_t message = { 0 };
        astarte_result_t ares = astarte_storage_mqtt_find_alloc(
            caching->storage, caching->direction, message_id, &message);
        if (ares == ASTARTE_RESULT_NOT_FOUND) {
            continue;
        }
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Failed to find the message %d in flash. Err: %d", message_id, ares);
            continue;
        }

        // Re-insert into the RAM hashmap.
        struct mqtt_caching_map_entry *map_entry
            = astarte_calloc(1, sizeof(struct mqtt_caching_map_entry));
        if (!map_entry) {
            ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
            astarte_storage_mqtt_find_free(&message);
            continue;
        }

        // Set expiry to fire immediately so it gets retransmitted on the first poll
        map_entry->end_of_validity = sys_timepoint_calc(K_NO_WAIT);
        map_entry->message = message;

        // TODO: there is no check if this operation succeeds or fails!
        int ret = sys_hashmap_insert(&caching->map, message_id, POINTER_TO_UINT(map_entry), NULL);
        if (ret != 1) {
            ASTARTE_LOG_ERR("Failed adding entry to the hashmap. Err: %d", ret);
            astarte_storage_mqtt_find_free(&message);
            free(map_entry);
            continue;
        }
        restored_count++;
    }

    ASTARTE_LOG_INF("MQTT cache restoration complete. Restored %d messages.", restored_count);
#endif
}
