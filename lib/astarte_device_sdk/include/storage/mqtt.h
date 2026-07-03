/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef STORAGE_MQTT_H
#define STORAGE_MQTT_H

/**
 * @file storage/mqtt.h
 * @brief Storage functions for the Astarte device MQTT wrapper.
 */

#include "astarte_device_sdk/astarte.h"
#include "astarte_device_sdk/result.h"

#include "storage/core.h"

/** @brief Direction for the messages to store. */
enum astarte_storage_mqtt_message_direction
{
    /** @brief Entry to store is an MQTT outgoing message. */
    STORAGE_MQTT_MSG_OUTGOING = 0,
    /** @brief Entry to store is an MQTT incoming message. */
    STORAGE_MQTT_MSG_INCOMING,
};

/** @brief Different types of MQTT messages to be placed in storage. */
enum astarte_storage_mqtt_message_type
{
    /** @brief Entry is an MQTT subscription message. */
    STORAGE_MQTT_SUBSCRIPTION_ENTRY = 0,
    /** @brief Entry is an MQTT publish message. */
    STORAGE_MQTT_PUBLISH_ENTRY,
    /** @brief Entry is an MQTT PUBREC message. */
    STORAGE_MQTT_PUBREC_ENTRY,
};

/** @brief Generic MQTT storage entry. */
typedef struct
{
    /** @brief Type for the entry */
    enum astarte_storage_mqtt_message_type type;
    /** @brief Topic of the message, can be NULL. */
    char *topic;
    /** @brief Data of the message, can be NULL. */
    void *data;
    /** @brief Size of data of the message, can be zero. */
    size_t data_size;
    /** @brief Quality of service or maximum allowed quality of service depending on message type */
    int qos;
} astarte_storage_mqtt_message_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Insert an MQTT message into storage.
 *
 * @param[inout] handle Pointer to the storage data handle.
 * @param[in] direction Direction of the message to store (incoming or outgoing).
 * @param[in] identifier Unique message identifier.
 * @param[in] message Pointer to the message data to be stored.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_storage_mqtt_insert(astarte_storage_data_t *handle,
    enum astarte_storage_mqtt_message_direction direction, uint16_t identifier,
    const astarte_storage_mqtt_message_t *message);

/**
 * @brief Find and allocate an MQTT message from storage.
 *
 * @param[inout] handle Pointer to the storage data handle.
 * @param[in] direction Direction of the message to search for.
 * @param[in] identifier Unique message identifier.
 * @param[out] message Pointer to an unallocated message struct where the found data will be placed.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_storage_mqtt_find_alloc(astarte_storage_data_t *handle,
    enum astarte_storage_mqtt_message_direction direction, uint16_t identifier,
    astarte_storage_mqtt_message_t *message);

/**
 * @brief Free the resources allocated by a previous find operation.
 *
 * @param[inout] message Pointer to the message whose data payload needs to be freed.
 */
void astarte_storage_mqtt_find_free(astarte_storage_mqtt_message_t *message);

/**
 * @brief Delete an MQTT message from permanent storage.
 *
 * @param[inout] handle Pointer to the storage data handle.
 * @param[in] direction Direction of the message to delete.
 * @param[in] identifier Unique message identifier.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_storage_mqtt_delete(astarte_storage_data_t *handle,
    enum astarte_storage_mqtt_message_direction direction, uint16_t identifier);

#ifdef __cplusplus
}
#endif

#endif // STORAGE_INTROSPECTION_H
