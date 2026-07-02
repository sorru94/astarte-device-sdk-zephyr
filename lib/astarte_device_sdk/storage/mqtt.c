/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "storage/mqtt.h"

#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "log.h"

ASTARTE_LOG_MODULE_DECLARE(astarte_storage, CONFIG_ASTARTE_DEVICE_SDK_STORAGE_LOG_LEVEL);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

/** @brief Helper function to map the MQTT message direction to the ZMS 'alternate' boolean flag. */
static bool direction_to_alternate(enum astarte_storage_mqtt_message_direction direction);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_storage_mqtt_insert(astarte_storage_data_t *handle,
    enum astarte_storage_mqtt_message_direction direction, uint16_t identifier,
    const astarte_storage_mqtt_message_t *message)
{
    if (!handle || !message) {
        ASTARTE_LOG_ERR("NULL parameters provided");
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    size_t topic_len = (message->topic != NULL) ? strlen(message->topic) : 0;

    // Calculate total buffer size required for serialization
    size_t total_size = sizeof(message->type) + sizeof(message->qos) + sizeof(topic_len)
        + sizeof(message->data_size) + topic_len + message->data_size;

    uint8_t *buffer = astarte_calloc(total_size, sizeof(uint8_t));
    if (!buffer) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        return ASTARTE_RESULT_OUT_OF_MEMORY;
    }

    // Serialize struct fields into the buffer
    size_t offset = 0;
    memcpy(buffer + offset, &message->type, sizeof(message->type));
    offset += sizeof(message->type);

    memcpy(buffer + offset, &message->qos, sizeof(message->qos));
    offset += sizeof(message->qos);

    memcpy(buffer + offset, &topic_len, sizeof(topic_len));
    offset += sizeof(topic_len);

    memcpy(buffer + offset, &message->data_size, sizeof(message->data_size));
    offset += sizeof(message->data_size);

    // Copy dynamic payload fields
    if (topic_len > 0) {
        memcpy(buffer + offset, message->topic, topic_len);
        offset += topic_len;
    }

    if (message->data_size > 0 && message->data != NULL) {
        memcpy(buffer + offset, message->data, message->data_size);
    }

    // Insert to ZMS using direct 16-bit key access
    astarte_result_t ares = astarte_key_value_direct_insert(
        &handle->zms_fs, direction_to_alternate(direction), identifier, buffer, total_size);

    astarte_free(buffer);
    return ares;
}

astarte_result_t astarte_storage_mqtt_find_alloc(astarte_storage_data_t *handle,
    enum astarte_storage_mqtt_message_direction direction, uint16_t identifier,
    astarte_storage_mqtt_message_t *message)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    uint8_t *buffer = NULL;
    char *topic = NULL;
    void *data = NULL;

    if (!handle || !message) {
        ASTARTE_LOG_ERR("NULL parameters provided");
        ares = ASTARTE_RESULT_INVALID_PARAM;
        goto error;
    }

    // First, query the required buffer size by passing NULL
    size_t value_size = 0;
    ares = astarte_key_value_direct_find(
        &handle->zms_fs, direction_to_alternate(direction), identifier, NULL, &value_size);
    if (ares == ASTARTE_RESULT_NOT_FOUND) {
        goto error;
    } else if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed in finding the size of the MQTT message from storage");
        goto error;
    }

    buffer = astarte_calloc(value_size, sizeof(uint8_t));
    if (!buffer) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto error;
    }

    // Read the actual serialized data
    ares = astarte_key_value_direct_find(
        &handle->zms_fs, direction_to_alternate(direction), identifier, buffer, &value_size);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed in extracting the MQTT message from storage");
        goto error;
    }

    size_t offset = 0;
    size_t topic_len = 0;

    // Deserialize core fields
    memcpy(&message->type, buffer + offset, sizeof(message->type));
    offset += sizeof(message->type);

    memcpy(&message->qos, buffer + offset, sizeof(message->qos));
    offset += sizeof(message->qos);

    memcpy(&topic_len, buffer + offset, sizeof(topic_len));
    offset += sizeof(topic_len);

    memcpy(&message->data_size, buffer + offset, sizeof(message->data_size));
    offset += sizeof(message->data_size);

    // Deserialize strings/payloads
    if (topic_len > 0) {
        topic = astarte_calloc(topic_len + 1, sizeof(char));
        if (!topic) {
            ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
            ares = ASTARTE_RESULT_OUT_OF_MEMORY;
            goto error;
        }
        memcpy(topic, buffer + offset, topic_len);
        topic[topic_len] = '\0';
        message->topic = topic;
        offset += topic_len;
    } else {
        message->topic = NULL;
    }

    if (message->data_size > 0) {
        data = astarte_calloc(message->data_size, sizeof(uint8_t));
        if (!data) {
            ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
            ares = ASTARTE_RESULT_OUT_OF_MEMORY;
            goto error;
        }
        memcpy(data, buffer + offset, message->data_size);
        message->data = data;
    } else {
        message->data = NULL;
    }

    astarte_free(buffer);
    return ASTARTE_RESULT_OK;

error:
    astarte_free(buffer);
    astarte_free(topic);
    astarte_free(data);
    return ares;
}

void astarte_storage_mqtt_find_free(astarte_storage_mqtt_message_t *message)
{
    astarte_free(message->topic);
    message->topic = NULL;
    astarte_free(message->data);
    message->data = NULL;
}

astarte_result_t astarte_storage_mqtt_delete(astarte_storage_data_t *handle,
    enum astarte_storage_mqtt_message_direction direction, uint16_t identifier)
{
    if (!handle) {
        ASTARTE_LOG_ERR("NULL parameters provided");
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    return astarte_key_value_direct_delete(
        &handle->zms_fs, direction_to_alternate(direction), identifier);
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static bool direction_to_alternate(enum astarte_storage_mqtt_message_direction direction)
{
    return direction == STORAGE_MQTT_MSG_INCOMING;
}
