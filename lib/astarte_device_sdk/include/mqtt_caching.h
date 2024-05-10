/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MQTT_CACHING_H
#define MQTT_CACHING_H

/**
 * @file mqtt_caching.h
 * @brief Utility to be used to cache messages and timestamps binded to MQTT message IDs.
 */

#include "astarte_device_sdk/astarte.h"

#include <zephyr/sys/hash_map.h>

#include "mqtt.h"

/** @brief Different types of MQTT messages to be placed in the caching structure. */
enum mqtt_caching_message_type
{
    /** @brief Cache entry is an MQTT subscription message. */
    MQTT_CACHING_SUBSCRIPTION_ENTRY = 0,
    /** @brief Cache entry is an MQTT publish message. */
    MQTT_CACHING_PUBLISH_ENTRY,
    /** @brief Cache entry is an MQTT PUBREC message. */
    MQTT_CACHING_PUBREC_ENTRY,
};

/** @brief Generic MQTT caching hashmap entry. */
typedef struct
{
    /** @brief Type for the map entry */
    enum mqtt_caching_message_type type;
    /** @brief Topic of the message, can be NULL. */
    char *topic;
    /** @brief Data of the message, can be NULL. */
    void *data;
    /** @brief Size of data of the message, can be zero. */
    size_t data_size;
    /** @brief Quality of service or maximum allowed quality of service depending on message type */
    int qos;
} mqtt_caching_message_t;

/** @brief Function pointer to be used for signaling a message requires retransmission. */
typedef void (*mqtt_caching_retransmit_cbk_t)(
    astarte_mqtt_t *astarte_mqtt, uint16_t message_id, mqtt_caching_message_t message);

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get a non cached message ID, to be used for transmission.
 *
 * @param[in] map The hashmap in which to check if the message is present.
 * @return The message ID to be used for transmission.
 */
uint16_t mqtt_caching_get_available_message_id(struct sys_hashmap *map);
/**
 * @brief Insert a message in an hashmap.
 *
 * @param[inout] map The map in which to insert the message.
 * @param[in] identifier Identifier for the message to cache.
 * @param[in] message Message to cache.
 */
void mqtt_caching_insert_message(
    struct sys_hashmap *map, uint16_t identifier, mqtt_caching_message_t message);
/**
 * @brief Find a message in the hashmap.
 *
 * @param[in] map The map to use for the operation.
 * @param[in] message_id Message ID for the message.
 * @return True if the message has been found in the hashmap, false otherwise.
 */
bool mqtt_caching_find_message(struct sys_hashmap *map, uint16_t message_id);
/**
 * @brief Check if any message has timed out. For any timeout call the retransmission callback.
 *
 * @param[inout] map The map to use for the operation.
 * @param[in] astarte_mqtt Handle to the Astarte MQTT client instance.
 * @param[in] retransmit_cbk Callback to notify the message has timed out.
 */
void mqtt_caching_check_message_expiry(struct sys_hashmap *map, astarte_mqtt_t *astarte_mqtt,
    mqtt_caching_retransmit_cbk_t retransmit_cbk);
/**
 * @brief Reset a message expiration time.
 *
 * @param[inout] map The map to use for the operation.
 * @param[in] message_id Message ID for the message.
 */
void mqtt_caching_update_message_expiry(struct sys_hashmap *map, uint16_t message_id);
/**
 * @brief Remove a message from the cache.
 *
 * @param[inout] map The map to use for the operation.
 * @param[in] message_id Message ID for the message.
 */
void mqtt_caching_remove_message(struct sys_hashmap *map, uint16_t message_id);
/**
 * @brief Remove all the meassages from the hashmap.
 *
 * @param[inout] map The map to use for the operation.
 */
void mqtt_caching_clear_messages(struct sys_hashmap *map);

#ifdef __cplusplus
}
#endif

#endif // MQTT_CACHING_H
