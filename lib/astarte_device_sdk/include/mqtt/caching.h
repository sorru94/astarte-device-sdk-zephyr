/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MQTT_CACHING_H
#define MQTT_CACHING_H

/**
 * @file mqtt/caching.h
 * @brief Utility to be used to cache messages and timestamps binded to MQTT message IDs.
 */

#include "astarte_device_sdk/astarte.h"
#include "astarte_device_sdk/result.h"

#include <zephyr/sys/hash_map.h>

#include "storage/core.h"
#include "storage/mqtt.h"

/** @brief Data struct for MQTT caching. */
typedef struct
{
    /** @brief Configuration struct for the hashmap used to cache MQTT messages. */
    struct sys_hashmap_config map_config;
    /** @brief Data struct for the hashmap used to cache MQTT messages. */
    struct sys_hashmap_data map_data;
    /** @brief Main struct for the hashmap used to cache MQTT messages. */
    struct sys_hashmap map;
#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
    /** @brief Handle to the permanent storage. */
    astarte_storage_data_t *storage;
    /** @brief Direction (incoming/outgoing) of the messages being cached. */
    enum astarte_storage_mqtt_message_direction direction;
#endif
} astarte_mqtt_caching_t;

/** @brief Function pointer to be used for signaling a message requires retransmission. */
typedef void (*astarte_mqtt_caching_retransmit_cbk_t)(
    astarte_mqtt_caching_t *caching, uint16_t message_id, astarte_storage_mqtt_message_t message);

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the MQTT caching structure.
 *
 * @param[inout] caching Pointer to the caching instance to initialize.
 */
#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
void astarte_mqtt_caching_init(astarte_mqtt_caching_t *caching, astarte_storage_data_t *storage,
    enum astarte_storage_mqtt_message_direction direction);
#else
void astarte_mqtt_caching_init(astarte_mqtt_caching_t *caching);
#endif

/**
 * @brief Get a non-cached message ID to be used for transmission.
 *
 * @param[in] caching The caching structure in which to check if the message ID is available.
 * @return The message ID to be used for transmission.
 */
uint16_t astarte_mqtt_caching_get_available_message_id(astarte_mqtt_caching_t *caching);

/**
 * @brief Insert a message into the cache.
 *
 * @param[inout] caching The caching structure in which to insert the message.
 * @param[in] identifier Identifier for the message to cache.
 * @param[in] message Message to cache.
 */
void astarte_mqtt_caching_insert_message(
    astarte_mqtt_caching_t *caching, uint16_t identifier, astarte_storage_mqtt_message_t message);

/**
 * @brief Find a message in the cache.
 *
 * @param[in] caching The caching structure to use for the operation.
 * @param[in] message_id Message ID for the message.
 * @return True if the message has been found in the cache, false otherwise.
 */
bool astarte_mqtt_caching_find_message(astarte_mqtt_caching_t *caching, uint16_t message_id);

/**
 * @brief Check if any message has timed out. For any timeout call the retransmission callback.
 *
 * @param[inout] caching The caching structure to use for the operation.
 * @param[in] retransmit_cbk Callback to notify the message has timed out.
 */
void astarte_mqtt_caching_check_message_expiry(
    astarte_mqtt_caching_t *caching, astarte_mqtt_caching_retransmit_cbk_t retransmit_cbk);

/**
 * @brief Reset a message expiration time.
 *
 * @param[inout] caching The caching structure to use for the operation.
 * @param[in] message_id Message ID for the message.
 */
void astarte_mqtt_caching_update_message_expiry(
    astarte_mqtt_caching_t *caching, uint16_t message_id);

/**
 * @brief Remove a message from the cache.
 *
 * @param[inout] caching The caching structure to use for the operation.
 * @param[in] message_id Message ID for the message.
 */
void astarte_mqtt_caching_remove_message(astarte_mqtt_caching_t *caching, uint16_t message_id);

/**
 * @brief Remove all messages from the cache.
 *
 * @param[inout] caching The caching structure to use for the operation.
 */
void astarte_mqtt_caching_clear_messages(astarte_mqtt_caching_t *caching);

/**
 * @brief Restore cached messages from permanent flash storage into RAM.
 *
 * @param[inout] caching The caching structure to populate from flash.
 */
void astarte_mqtt_caching_restore_from_flash(astarte_mqtt_caching_t *caching);
#ifdef __cplusplus
}
#endif

#endif // MQTT_CACHING_H
