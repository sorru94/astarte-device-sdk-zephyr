/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "device/transmission_queue_msg.h"

#include <string.h>

#include "alloc.h"

#include "log.h"
ASTARTE_LOG_MODULE_DECLARE(astarte_device, CONFIG_ASTARTE_DEVICE_SDK_DEVICE_LOG_LEVEL);

/************************************************
 *         Static functions declaration         *
 ***********************************************/

ASTARTE_SCOPE_DEFER_DEFINE(astarte_transmission_queue_volatile_msg_cleanup,
    struct astarte_device_transmission_queue_volatile_msg *);
ASTARTE_SCOPE_DEFER_DEFINE(
    astarte_transmission_queue_msg_cleanup, struct astarte_device_transmission_queue_msg *);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_transmission_queue_volatile_msg_init(
    struct astarte_device_transmission_queue_volatile_msg *volatile_msg,
    const struct astarte_device_transmission_queue_msg *queue_msg, uint64_t timestamp,
    uint64_t sequence_number)
{
    if (!volatile_msg || !queue_msg) {
        ASTARTE_LOG_ERR("NULL parameters provided");
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    struct astarte_device_transmission_queue_volatile_msg local_volatile_msg = { 0 };
    scope_defer(astarte_transmission_queue_volatile_msg_cleanup)(&local_volatile_msg);

    local_volatile_msg.qos = queue_msg->qos;
    local_volatile_msg.timestamp = timestamp;
    local_volatile_msg.sequence_number = sequence_number;
    local_volatile_msg.payload_len = queue_msg->payload_len;

    size_t interface_name_len = strlen(queue_msg->interface_name);
    local_volatile_msg.interface_name = astarte_calloc(interface_name_len + 1, sizeof(char));
    if (!local_volatile_msg.interface_name) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        return ASTARTE_RESULT_OUT_OF_MEMORY;
    }
    memcpy(local_volatile_msg.interface_name, queue_msg->interface_name, interface_name_len);

    size_t path_len = strlen(queue_msg->path);
    local_volatile_msg.path = astarte_calloc(path_len + 1, sizeof(char));
    if (!local_volatile_msg.path) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        return ASTARTE_RESULT_OUT_OF_MEMORY;
    }
    memcpy(local_volatile_msg.path, queue_msg->path, path_len);

    if (queue_msg->payload_len > 0 && queue_msg->payload) {
        local_volatile_msg.payload = astarte_malloc(queue_msg->payload_len);
        if (!local_volatile_msg.payload) {
            ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
            return ASTARTE_RESULT_OUT_OF_MEMORY;
        }
        memcpy(local_volatile_msg.payload, queue_msg->payload, queue_msg->payload_len);
    }

    *volatile_msg = local_volatile_msg;

    // Disarm the local variable
    local_volatile_msg.interface_name = NULL;
    local_volatile_msg.path = NULL;
    local_volatile_msg.payload = NULL;

    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_transmission_queue_msg_from_volatile_msg_deep_cpy(
    struct astarte_device_transmission_queue_msg *queue_msg,
    const struct astarte_device_transmission_queue_volatile_msg *volatile_msg)
{
    if (!queue_msg || !volatile_msg) {
        ASTARTE_LOG_ERR("NULL parameters provided");
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    struct astarte_device_transmission_queue_msg local_queue_msg = { 0 };
    scope_defer(astarte_transmission_queue_msg_cleanup)(&local_queue_msg);

    local_queue_msg.qos = volatile_msg->qos;
    local_queue_msg.payload_len = volatile_msg->payload_len;
    local_queue_msg.retention = ASTARTE_MAPPING_RETENTION_VOLATILE;

    size_t interface_name_len = strlen(volatile_msg->interface_name);
    local_queue_msg.interface_name = astarte_calloc(interface_name_len + 1, sizeof(char));
    if (!local_queue_msg.interface_name) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        return ASTARTE_RESULT_OUT_OF_MEMORY;
    }
    memcpy(local_queue_msg.interface_name, volatile_msg->interface_name, interface_name_len);

    size_t path_len = strlen(volatile_msg->path);
    local_queue_msg.path = astarte_calloc(path_len + 1, sizeof(char));
    if (!local_queue_msg.path) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        return ASTARTE_RESULT_OUT_OF_MEMORY;
    }
    memcpy(local_queue_msg.path, volatile_msg->path, path_len);

    if (volatile_msg->payload_len > 0 && volatile_msg->payload) {
        local_queue_msg.payload = astarte_malloc(volatile_msg->payload_len);
        if (!local_queue_msg.payload) {
            ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
            return ASTARTE_RESULT_OUT_OF_MEMORY;
        }
        memcpy(local_queue_msg.payload, volatile_msg->payload, volatile_msg->payload_len);
    }

    *queue_msg = local_queue_msg;

    // Disarm the local variable
    local_queue_msg.interface_name = NULL;
    local_queue_msg.path = NULL;
    local_queue_msg.payload = NULL;

    return ASTARTE_RESULT_OK;
}

void astarte_transmission_queue_volatile_msg_cleanup(
    struct astarte_device_transmission_queue_volatile_msg *msg)
{
    if (!msg) {
        return;
    }
    astarte_free(msg->interface_name);
    msg->interface_name = NULL;
    astarte_free(msg->path);
    msg->path = NULL;
    astarte_free(msg->payload);
    msg->payload = NULL;
}
