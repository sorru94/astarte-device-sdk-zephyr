/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "device/transmission_queue.h"

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
static astarte_result_t volatile_msg_init(
    struct astarte_device_transmission_queue_volatile_msg *volatile_msg,
    const struct astarte_device_transmission_queue_msg *queue_msg, uint64_t timestamp,
    uint64_t sequence_number);
static astarte_result_t queue_msg_from_volatile_msg_deep_cpy(
    struct astarte_device_transmission_queue_msg *queue_msg,
    const struct astarte_device_transmission_queue_volatile_msg *volatile_msg);
void volatile_msg_cleanup(struct astarte_device_transmission_queue_volatile_msg *msg);
static uint64_t get_system_timestamp(struct astarte_device_transmission_queue *handle);
static astarte_result_t insert_volatile_msg(struct k_msgq *msgq,
    const struct astarte_device_transmission_queue_msg *msg, uint64_t timestamp_ms,
    uint64_t sequence_number);
#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
static astarte_result_t insert_stored_msg(struct astarte_device_transmission_queue *handle,
    const struct astarte_device_transmission_queue_msg *msg, uint64_t timestamp_ms,
    uint64_t sequence_number);
#endif

ASTARTE_SCOPE_DEFER_DEFINE(
    volatile_msg_cleanup, struct astarte_device_transmission_queue_volatile_msg *);
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
        volatile_msg_cleanup(&msg);
    }
    while (k_msgq_get(&handle->volatile_msgq, &msg, K_NO_WAIT) == 0) {
        volatile_msg_cleanup(&msg);
    }
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
        volatile_msg_cleanup(&msg);
    }
    ASTARTE_LOG_DBG("Transmission queue purged of discard messages.");
}

astarte_result_t astarte_transmission_queue_insert(struct astarte_device_transmission_queue *handle,
    const struct astarte_device_transmission_queue_msg *msg)
{
    if (!handle || !msg || !msg->interface_name || !msg->path) {
        ASTARTE_LOG_ERR("Received NULL reference for transmission queue or parameters");
        return ASTARTE_RESULT_INVALID_PARAM;
    }
    if (msg->payload_len < 0 || ((msg->payload_len > 0) && !msg->payload)) {
        ASTARTE_LOG_ERR("Invalid data length");
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    ASTARTE_LOG_DBG("Insert into transmission queue");

    uint64_t timestamp_ms = get_system_timestamp(handle);
    uint64_t seq_num = handle->next_sequence_number++;
    astarte_result_t ares = ASTARTE_RESULT_OK;

    if (msg->retention == ASTARTE_MAPPING_RETENTION_DISCARD) {
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

    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Message not queued for %s%s", msg->interface_name, msg->path);
        return ares;
    }

    ASTARTE_LOG_DBG("Message queued for %s%s", msg->interface_name, msg->path);
    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_transmission_queue_peek(struct astarte_device_transmission_queue *handle,
    struct astarte_device_transmission_queue_msg *msg)
{
    if (!handle || !msg) {
        ASTARTE_LOG_ERR("Received NULL reference for transmission queue or message");
        return ASTARTE_RESULT_INVALID_PARAM;
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
#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
    uint64_t storage_seq
        = (storage_ares == ASTARTE_RESULT_OK) ? storage_msg.sequence_number : UINT64_MAX;
#else
    uint64_t storage_seq = UINT64_MAX;
#endif

    if (volatile_seq == UINT64_MAX && discard_seq == UINT64_MAX && storage_seq == UINT64_MAX) {
        return ASTARTE_RESULT_NOT_FOUND;
    }

    // Determine the oldest message
    if (volatile_seq <= discard_seq && volatile_seq <= storage_seq) {
        // Volatile is the oldest.
#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
        if (storage_ares == ASTARTE_RESULT_OK) {
            astarte_storage_transmission_msg_cleanup(&storage_msg);
        }
#endif
        return queue_msg_from_volatile_msg_deep_cpy(msg, &volatile_msg);
    }
    if (discard_seq <= volatile_seq && discard_seq <= storage_seq) {
        // Discard is the oldest.
#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
        if (storage_ares == ASTARTE_RESULT_OK) {
            astarte_storage_transmission_msg_cleanup(&storage_msg);
        }
#endif
        astarte_result_t ares = queue_msg_from_volatile_msg_deep_cpy(msg, &discard_msg);
        if (ares == ASTARTE_RESULT_OK) {
            // Override the retention since the deep copy helper hardcodes it to volatile
            msg->retention = ASTARTE_MAPPING_RETENTION_DISCARD;
        }
        return ares;
    }
#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
    // Storage is the oldest.
    msg->interface_name = storage_msg.interface_name;
    msg->path = storage_msg.path;
    msg->payload = storage_msg.payload;
    msg->payload_len = storage_msg.payload_len;
    msg->qos = storage_msg.qos;
    msg->retention = ASTARTE_MAPPING_RETENTION_STORED;
    return ASTARTE_RESULT_OK;
#else
    // Unreachable
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
            volatile_msg_cleanup(&volatile_msg);
            return ASTARTE_RESULT_OK;
        }
        return ASTARTE_RESULT_NOT_FOUND;
    }

    if (retention == ASTARTE_MAPPING_RETENTION_DISCARD) {
        if (k_msgq_get(&handle->discard_msgq, &volatile_msg, K_NO_WAIT) == 0) {
            volatile_msg_cleanup(&volatile_msg);
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

static astarte_result_t volatile_msg_init(
    struct astarte_device_transmission_queue_volatile_msg *volatile_msg,
    const struct astarte_device_transmission_queue_msg *queue_msg, uint64_t timestamp,
    uint64_t sequence_number)
{
    if (!volatile_msg || !queue_msg) {
        ASTARTE_LOG_ERR("NULL parameters provided");
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    struct astarte_device_transmission_queue_volatile_msg local_volatile_msg = { 0 };
    scope_defer(volatile_msg_cleanup)(&local_volatile_msg);

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

static astarte_result_t queue_msg_from_volatile_msg_deep_cpy(
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

void volatile_msg_cleanup(struct astarte_device_transmission_queue_volatile_msg *msg)
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
    astarte_result_t ares = volatile_msg_init(&volatile_msg, msg, timestamp_ms, sequence_number);
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
            volatile_msg_cleanup(&oldest_volatile_msg);
            ret = k_msgq_put(msgq, &volatile_msg, K_NO_WAIT);
        }

        if (ret != 0) {
            ASTARTE_LOG_ERR(
                "Failed to insert message after eviction attempt. Dropping message on %s%s",
                msg->interface_name, msg->path);
            volatile_msg_cleanup(&volatile_msg);
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
