/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "device/transmission_queue.h"
#include "device/transmission_queue_msg.h"

#include <time.h>

#include <zephyr/sys/clock.h>

#include "alloc.h"

#include "log.h"
ASTARTE_LOG_MODULE_DECLARE(astarte_device, CONFIG_ASTARTE_DEVICE_SDK_DEVICE_LOG_LEVEL);

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

// January first 2026, simple reference time to validate system time.
#define MIN_VALID_TIME_SEC 1767225600

/************************************************
 *         Static functions declaration         *
 ***********************************************/

static bool is_system_time_valid();
static uint64_t get_system_timestamp(struct astarte_device_transmission_queue *handle);
static astarte_result_t insert_volatile_msg(struct k_msgq *msgq,
    const struct astarte_device_transmission_queue_msg *msg, uint64_t timestamp_ms,
    uint64_t sequence_number);
static astarte_result_t insert_interface_msg(struct k_msgq *msgq,
    const struct astarte_device_transmission_queue_msg *msg, uint64_t timestamp_ms,
    uint64_t sequence_number);
#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
static astarte_result_t insert_stored_msg(struct astarte_device_transmission_queue *handle,
    const struct astarte_device_transmission_queue_msg *msg, uint64_t timestamp_ms,
    uint64_t sequence_number);
#endif

ASTARTE_SCOPE_DEFER_DEFINE(astarte_transmission_queue_volatile_msg_cleanup,
    struct astarte_device_transmission_queue_volatile_msg *);
ASTARTE_SCOPE_DEFER_DEFINE(
    astarte_transmission_queue_msg_cleanup, struct astarte_device_transmission_queue_msg *);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
astarte_result_t astarte_transmission_queue_init(
    struct astarte_device_transmission_queue *handle, astarte_storage_data_t *storage)
{
    if (!handle || !storage) {
#else
astarte_result_t astarte_transmission_queue_init(struct astarte_device_transmission_queue *handle)
{
    if (!handle) {
#endif
        ASTARTE_LOG_ERR("NULL parameters provided");
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    ASTARTE_LOG_DBG("Initializing transmission queue");

    memset(handle, 0, sizeof(struct astarte_device_transmission_queue));

    k_msgq_init(&handle->discard_msgq, handle->discard_msgq_buffer,
        sizeof(struct astarte_device_transmission_queue_volatile_msg),
        CONFIG_ASTARTE_DEVICE_SDK_TRANSMISSION_QUEUE_SIZE);

    k_msgq_init(&handle->interface_msgq, handle->interface_msgq_buffer,
        sizeof(struct astarte_device_transmission_queue_interface_msg),
        CONFIG_ASTARTE_DEVICE_SDK_TRANSMISSION_QUEUE_SIZE);

    k_msgq_init(&handle->volatile_msgq, handle->volatile_msgq_buffer,
        sizeof(struct astarte_device_transmission_queue_volatile_msg),
        CONFIG_ASTARTE_DEVICE_SDK_TRANSMISSION_QUEUE_SIZE);

    handle->system_time_valid = is_system_time_valid();
    if (!handle->system_time_valid) {
        ASTARTE_LOG_WRN(
            "System time is not valid. Messages with retention storage will be kept indefinitely.");
    }

#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
    astarte_storage_transmission_indexes_t indexes = { 0 };
    astarte_result_t ares = astarte_storage_transmission_get_indexes(storage, &indexes);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_WRN("Failed in finding the head and tail from the transmission queue storage.");
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }

    handle->storage = storage;
    handle->storage_indexes = indexes;

    uint64_t last_seq = 0;
    ares = astarte_storage_transmission_get_last_sequence_number(storage, &indexes, &last_seq);
    if (ares == ASTARTE_RESULT_OK) {
        // Resume counting from the next available number
        handle->next_sequence_number = last_seq + 1;
        ASTARTE_LOG_DBG("Recovered highest sequence number from storage: %llu", last_seq);
    } else {
        // Storage is empty or unreadable, safe to start at 0
        handle->next_sequence_number = 0;
    }
#else
    handle->next_sequence_number = 0;
#endif

    return ASTARTE_RESULT_OK;
}

void astarte_transmission_queue_clear(struct astarte_device_transmission_queue *handle)
{
    if (!handle) {
        return;
    }
    struct astarte_device_transmission_queue_volatile_msg msg;
    while (k_msgq_get(&handle->discard_msgq, &msg, K_NO_WAIT) == 0) {
        astarte_transmission_queue_volatile_msg_cleanup(&msg);
    }
    while (k_msgq_get(&handle->volatile_msgq, &msg, K_NO_WAIT) == 0) {
        astarte_transmission_queue_volatile_msg_cleanup(&msg);
    }
    k_msgq_purge(&handle->interface_msgq);

#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
    astarte_result_t ares = ASTARTE_RESULT_OK;
    do {
        ares = astarte_storage_transmission_discard(handle->storage, &handle->storage_indexes);
    } while (ares == ASTARTE_RESULT_OK);
#endif
    ASTARTE_LOG_DBG("Transmission queue cleared.");
}

void astarte_transmission_queue_purge_discard(struct astarte_device_transmission_queue *handle)
{
    if (!handle) {
        return;
    }
    struct astarte_device_transmission_queue_volatile_msg msg;
    while (k_msgq_get(&handle->discard_msgq, &msg, K_NO_WAIT) == 0) {
        astarte_transmission_queue_volatile_msg_cleanup(&msg);
    }
    ASTARTE_LOG_DBG("Transmission queue purged of discard messages.");
}

astarte_result_t astarte_transmission_queue_insert(struct astarte_device_transmission_queue *handle,
    const struct astarte_device_transmission_queue_msg *msg)
{
    if (!handle || !msg) {
        ASTARTE_LOG_ERR("Received NULL reference for transmission queue or message");
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    // Validate parameters based on the operation type
    if (msg->operation == ASTARTE_TRANSMISSION_OP_ADD_INTERFACE
        || msg->operation == ASTARTE_TRANSMISSION_OP_REMOVE_INTERFACE) {
        if (!msg->interface) {
            ASTARTE_LOG_ERR("Received NULL reference for interface struct");
            return ASTARTE_RESULT_INVALID_PARAM;
        }
    } else {
        if (!msg->interface_name || !msg->path) {
            ASTARTE_LOG_ERR("Received NULL reference for interface name or path");
            return ASTARTE_RESULT_INVALID_PARAM;
        }
        if (msg->payload_len < 0 || ((msg->payload_len > 0) && !msg->payload)) {
            ASTARTE_LOG_ERR("Invalid data length");
            return ASTARTE_RESULT_INVALID_PARAM;
        }
    }

    ASTARTE_LOG_DBG("Insert into transmission queue");

    uint64_t timestamp_ms = get_system_timestamp(handle);
    uint64_t seq_num = handle->next_sequence_number++;
    astarte_result_t ares = ASTARTE_RESULT_OK;

    if (msg->operation == ASTARTE_TRANSMISSION_OP_ADD_INTERFACE
        || msg->operation == ASTARTE_TRANSMISSION_OP_REMOVE_INTERFACE) {
        ares = insert_interface_msg(&handle->interface_msgq, msg, timestamp_ms, seq_num);
    } else if (msg->retention == ASTARTE_MAPPING_RETENTION_DISCARD) {
        ares = insert_volatile_msg(&handle->discard_msgq, msg, timestamp_ms, seq_num);
    } else if (msg->retention == ASTARTE_MAPPING_RETENTION_VOLATILE) {
        ares = insert_volatile_msg(&handle->volatile_msgq, msg, timestamp_ms, seq_num);
#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
    } else if (msg->retention == ASTARTE_MAPPING_RETENTION_STORED) {
        ares = insert_stored_msg(handle, msg, timestamp_ms, seq_num);
#endif
    } else {
        ASTARTE_LOG_ERR("Message with invalid retention");
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    if (ares == ASTARTE_RESULT_OK) {
        if (msg->operation == ASTARTE_TRANSMISSION_OP_DATA) {
            ASTARTE_LOG_DBG("Message queued for %s%s", msg->interface_name, msg->path);
        } else {
            ASTARTE_LOG_DBG("Interface message queued");
        }
    } else {
        ASTARTE_LOG_ERR("Failed to queue message for %s%s", msg->interface_name, msg->path);
    }

    return ares;
}

astarte_result_t astarte_transmission_queue_peek(struct astarte_device_transmission_queue *handle,
    struct astarte_device_transmission_queue_msg *msg)
{
    if (!handle || !msg) {
        ASTARTE_LOG_ERR("Received NULL reference for transmission queue or message");
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    astarte_result_t interface_ares = ASTARTE_RESULT_OK;
    struct astarte_device_transmission_queue_interface_msg interface_msg = { 0 };
    if (k_msgq_peek(&handle->interface_msgq, &interface_msg) != 0) {
        interface_ares = ASTARTE_RESULT_NOT_FOUND;
    }

    astarte_result_t discard_ares = ASTARTE_RESULT_OK;
    struct astarte_device_transmission_queue_volatile_msg discard_msg = { 0 };
    if (k_msgq_peek(&handle->discard_msgq, &discard_msg) != 0) {
        discard_ares = ASTARTE_RESULT_NOT_FOUND;
    }

    astarte_result_t volatile_ares = ASTARTE_RESULT_OK;
    struct astarte_device_transmission_queue_volatile_msg volatile_msg = { 0 };
    if (k_msgq_peek(&handle->volatile_msgq, &volatile_msg) != 0) {
        volatile_ares = ASTARTE_RESULT_NOT_FOUND;
    }

#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
    astarte_result_t storage_ares = ASTARTE_RESULT_OK;
    struct astarte_storage_transmission_msg storage_msg = { 0 };
    storage_ares = astarte_storage_transmission_peek(
        handle->storage, &handle->storage_indexes, &storage_msg);
    if ((storage_ares != ASTARTE_RESULT_OK) && (storage_ares != ASTARTE_RESULT_NOT_FOUND)) {
        ASTARTE_LOG_ERR("Failed peeking message from storage.");
        return storage_ares;
    }
#endif

    uint64_t volatile_seq
        = (volatile_ares == ASTARTE_RESULT_OK) ? volatile_msg.sequence_number : UINT64_MAX;
    uint64_t discard_seq
        = (discard_ares == ASTARTE_RESULT_OK) ? discard_msg.sequence_number : UINT64_MAX;
    uint64_t interface_seq
        = (interface_ares == ASTARTE_RESULT_OK) ? interface_msg.sequence_number : UINT64_MAX;
#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
    uint64_t storage_seq
        = (storage_ares == ASTARTE_RESULT_OK) ? storage_msg.sequence_number : UINT64_MAX;
#else
    uint64_t storage_seq = UINT64_MAX;
#endif

    // Calculate the minimum sequence directly to avoid complex boolean chains
    uint64_t min_seq = interface_seq;
    if (volatile_seq < min_seq) {
        min_seq = volatile_seq;
    }
    if (discard_seq < min_seq) {
        min_seq = discard_seq;
    }
    if (storage_seq < min_seq) {
        min_seq = storage_seq;
    }

    if (min_seq == UINT64_MAX) {
        return ASTARTE_RESULT_NOT_FOUND;
    }

#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
    // Evaluate cleanup once.
    if (min_seq != storage_seq && storage_ares == ASTARTE_RESULT_OK) {
        astarte_storage_transmission_msg_cleanup(&storage_msg);
    }
#endif

    // Interface is the oldest
    if (min_seq == interface_seq) {
        memset(msg, 0, sizeof(struct astarte_device_transmission_queue_msg));
        msg->operation = interface_msg.operation;
        msg->interface = interface_msg.interface;
        return ASTARTE_RESULT_OK;
    }

    // Volatile is the oldest.
    if (min_seq == volatile_seq) {
        return astarte_transmission_queue_msg_from_volatile_msg_deep_cpy(msg, &volatile_msg);
    }

    // Discard is the oldest.
    if (min_seq == discard_seq) {
        astarte_result_t ares
            = astarte_transmission_queue_msg_from_volatile_msg_deep_cpy(msg, &discard_msg);
        if (ares == ASTARTE_RESULT_OK) {
            msg->retention = ASTARTE_MAPPING_RETENTION_DISCARD;
        }
        return ares;
    }

    // Storage is the oldest.
#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
    msg->interface_name = storage_msg.interface_name;
    msg->path = storage_msg.path;
    msg->payload = storage_msg.payload;
    msg->payload_len = storage_msg.payload_len;
    msg->qos = storage_msg.qos;
    msg->retention = ASTARTE_MAPPING_RETENTION_STORED;
    return ASTARTE_RESULT_OK;
#else
    return ASTARTE_RESULT_INTERNAL_ERROR;
#endif
}

astarte_result_t astarte_transmission_queue_discard_by_retention(
    struct astarte_device_transmission_queue *handle, astarte_mapping_retention_t retention)
{
    if (!handle) {
        ASTARTE_LOG_ERR("Received NULL reference for transmission queue");
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    struct astarte_device_transmission_queue_volatile_msg volatile_msg = { 0 };

    if (retention == ASTARTE_MAPPING_RETENTION_VOLATILE) {
        if (k_msgq_get(&handle->volatile_msgq, &volatile_msg, K_NO_WAIT) == 0) {
            astarte_transmission_queue_volatile_msg_cleanup(&volatile_msg);
            return ASTARTE_RESULT_OK;
        }
        return ASTARTE_RESULT_NOT_FOUND;
    }

    if (retention == ASTARTE_MAPPING_RETENTION_DISCARD) {
        if (k_msgq_get(&handle->discard_msgq, &volatile_msg, K_NO_WAIT) == 0) {
            astarte_transmission_queue_volatile_msg_cleanup(&volatile_msg);
            return ASTARTE_RESULT_OK;
        }
        return ASTARTE_RESULT_NOT_FOUND;
    }

#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
    if (retention == ASTARTE_MAPPING_RETENTION_STORED) {
        return astarte_storage_transmission_discard(handle->storage, &handle->storage_indexes);
    }
#endif

    ASTARTE_LOG_ERR("Invalid retention policy provided");
    return ASTARTE_RESULT_INVALID_PARAM;
}

astarte_result_t astarte_transmission_queue_discard_interface(
    struct astarte_device_transmission_queue *handle)
{
    if (!handle) {
        ASTARTE_LOG_ERR("Received NULL reference for transmission queue");
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    struct astarte_device_transmission_queue_interface_msg interface_msg = { 0 };

    if (k_msgq_get(&handle->interface_msgq, &interface_msg, K_NO_WAIT) == 0) {
        return ASTARTE_RESULT_OK;
    }

    return ASTARTE_RESULT_NOT_FOUND;
}

void astarte_transmission_queue_msg_cleanup(struct astarte_device_transmission_queue_msg *msg)
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

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static bool is_system_time_valid()
{
    struct timespec timespec;
    if (sys_clock_gettime(SYS_CLOCK_REALTIME, &timespec) == 0) {
        return (timespec.tv_sec > MIN_VALID_TIME_SEC);
    }
    return false;
}

static uint64_t get_system_timestamp(struct astarte_device_transmission_queue *handle)
{
    if (!handle->system_time_valid) {
        return 0;
    }

    struct timespec timespec;
    if (sys_clock_gettime(SYS_CLOCK_REALTIME, &timespec) == 0) {
        const uint64_t seconds_multiplier = 1000;
        const uint64_t nanoseconds_multiplier = 1000000;
        return ((uint64_t) timespec.tv_sec * seconds_multiplier)
            + ((uint64_t) timespec.tv_nsec / nanoseconds_multiplier);
    }

    ASTARTE_LOG_WRN("Failed to read system time. Stamping with 0.");
    return 0;
}

static astarte_result_t insert_volatile_msg(struct k_msgq *msgq,
    const struct astarte_device_transmission_queue_msg *msg, uint64_t timestamp_ms,
    uint64_t sequence_number)
{
    struct astarte_device_transmission_queue_volatile_msg volatile_msg = { 0 };
    astarte_result_t ares = astarte_transmission_queue_volatile_msg_init(
        &volatile_msg, msg, timestamp_ms, sequence_number);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed initializing a message");
        return ares;
    }

    int ret = k_msgq_put(msgq, &volatile_msg, K_NO_WAIT);
    if (ret != 0) {
        ASTARTE_LOG_WRN(
            "Transmission queue put failed (%d). Evicting oldest message to accommodate %s%s", ret,
            msg->interface_name, msg->path);

        struct astarte_device_transmission_queue_volatile_msg oldest_volatile_msg;
        if (k_msgq_get(msgq, &oldest_volatile_msg, K_NO_WAIT) == 0) {
            astarte_transmission_queue_volatile_msg_cleanup(&oldest_volatile_msg);
            ret = k_msgq_put(msgq, &volatile_msg, K_NO_WAIT);
        }

        if (ret != 0) {
            ASTARTE_LOG_ERR(
                "Failed to insert message after eviction attempt. Dropping message on %s%s",
                msg->interface_name, msg->path);
            astarte_transmission_queue_volatile_msg_cleanup(&volatile_msg);
            return ASTARTE_RESULT_MQTT_ERROR;
        }
    }
    return ASTARTE_RESULT_OK;
}

static astarte_result_t insert_interface_msg(struct k_msgq *msgq,
    const struct astarte_device_transmission_queue_msg *msg, uint64_t timestamp_ms,
    uint64_t sequence_number)
{
    struct astarte_device_transmission_queue_interface_msg iface_msg
        = { .operation = msg->operation,
              .interface = msg->interface,
              .timestamp = timestamp_ms,
              .sequence_number = sequence_number };

    int ret = k_msgq_put(msgq, &iface_msg, K_NO_WAIT);
    if (ret != 0) {
        // Evict the oldest interface message if the queue is full
        struct astarte_device_transmission_queue_interface_msg oldest;
        if (k_msgq_get(msgq, &oldest, K_NO_WAIT) == 0) {
            ret = k_msgq_put(msgq, &iface_msg, K_NO_WAIT);
        }

        if (ret != 0) {
            ASTARTE_LOG_ERR("Failed to insert interface message after eviction attempt.");
            return ASTARTE_RESULT_MQTT_ERROR;
        }
    }
    return ASTARTE_RESULT_OK;
}

#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
static astarte_result_t insert_stored_msg(struct astarte_device_transmission_queue *handle,
    const struct astarte_device_transmission_queue_msg *msg, uint64_t timestamp_ms,
    uint64_t sequence_number)
{
    struct astarte_storage_transmission_msg storage_msg = {
        .interface_name = msg->interface_name,
        .path = msg->path,
        .payload = msg->payload,
        .payload_len = msg->payload_len,
        .qos = msg->qos,
        .timestamp = timestamp_ms,
        .sequence_number = sequence_number,
    };

    astarte_result_t ares = astarte_storage_transmission_push(
        handle->storage, &handle->storage_indexes, &storage_msg);

    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_WRN(
            "Transmission storage push failed (%d). Evicting oldest message to fit %s%s", ares,
            storage_msg.interface_name, storage_msg.path);

        astarte_result_t discard_ares
            = astarte_storage_transmission_discard(handle->storage, &handle->storage_indexes);
        if (discard_ares == ASTARTE_RESULT_OK) {
            ares = astarte_storage_transmission_push(
                handle->storage, &handle->storage_indexes, &storage_msg);
        }

        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR(
                "Failed to insert message after eviction attempt. Dropping message on %s%s",
                storage_msg.interface_name, storage_msg.path);
            return ASTARTE_RESULT_MQTT_ERROR;
        }
    }
    return ASTARTE_RESULT_OK;
}
#endif
