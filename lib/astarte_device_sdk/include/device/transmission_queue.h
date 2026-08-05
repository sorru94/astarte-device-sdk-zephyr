/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DEVICE_TRANSMISSION_QUEUE_H
#define DEVICE_TRANSMISSION_QUEUE_H

/**
 * @file device/transmission_queue.h
 * @brief Device transmission queue definitions and state.
 */

#include "astarte_device_sdk/astarte.h"
#include "astarte_device_sdk/interface.h"
#include "astarte_device_sdk/result.h"

#include <zephyr/kernel.h>

#include "storage/core.h"
#include "storage/trans.h"

/** @brief Message structure for the transmission storage. */
struct astarte_device_transmission_queue_msg
{
    /** @brief Name of the Astarte interface. */
    char *interface_name;
    /** @brief Path associated with the message. */
    char *path;
    /** @brief Pointer to the payload data. */
    void *payload;
    /** @brief Length of the payload data in bytes. */
    int payload_len;
    /** @brief Quality of Service (QoS) level for the message. */
    int qos;
    /** @brief Retention policy for the message. */
    astarte_mapping_retention_t retention;
};

/** @brief Message structure for the transmission storage. */
struct astarte_device_transmission_queue_volatile_msg
{
    /** @brief Name of the Astarte interface. */
    char *interface_name;
    /** @brief Path associated with the message. */
    char *path;
    /** @brief Pointer to the payload data. */
    void *payload;
    /** @brief Length of the payload data in bytes. */
    int payload_len;
    /** @brief Quality of Service (QoS) level for the message. */
    int qos;
    /** @brief Timestamp of the message. */
    uint64_t timestamp;
    /** @brief Sequence number to maintain ordering. */
    uint64_t sequence_number;
};

/** @brief Transmission queue structure for managing outbound messages. */
struct astarte_device_transmission_queue
{
    /** @brief Message queue for discard messages. */
    struct k_msgq discard_msgq;
    /** @brief Buffer backing the discard message queue. */
    char discard_msgq_buffer[CONFIG_ASTARTE_DEVICE_SDK_TRANSMISSION_QUEUE_SIZE
        * sizeof(struct astarte_device_transmission_queue_volatile_msg)];
    /** @brief Message queue for volatile messages. */
    struct k_msgq volatile_msgq;
    /** @brief Buffer backing the volatile message queue. */
    char volatile_msgq_buffer[CONFIG_ASTARTE_DEVICE_SDK_TRANSMISSION_QUEUE_SIZE
        * sizeof(struct astarte_device_transmission_queue_volatile_msg)];
    /** @brief Flag indicating if the system time is valid. */
    bool system_time_valid;
#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
    /** @brief Handle to the persistent storage driver */
    astarte_storage_data_t *storage;
    /** @brief Indexes tracking the transmission storage position. */
    astarte_storage_transmission_indexes_t storage_indexes;
#endif
    /** @brief Next absolute sequence number, required to keep the queue ordered. */
    uint64_t next_sequence_number;
};

#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
/**
 * @brief Initializes a new transmission queue.
 *
 * @param[in] handle Pointer to the transmission queue to initialize.
 * @param[in] storage Pointer to the persistent storage driver handle.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_transmission_queue_init(
    struct astarte_device_transmission_queue *handle, astarte_storage_data_t *storage);
#else
/**
 * @brief Initializes a new transmission queue.
 *
 * @param[in] handle Pointer to the transmission queue to initialize.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_transmission_queue_init(struct astarte_device_transmission_queue *handle);
#endif

/**
 * @brief Destroys an existing transmission queue.
 *
 * @param[in] handle Pointer to the transmission queue to destroy.
 */
void astarte_transmission_queue_clear(struct astarte_device_transmission_queue *handle);

/**
 * @brief Purge all messages with retention discard in an existing transmission queue.
 *
 * @param[in] handle Pointer to the transmission queue to purge.
 */
void astarte_transmission_queue_purge_discard(struct astarte_device_transmission_queue *handle);

/**
 * @brief Inserts a new message into the transmission queue.
 *
 * @param[in] handle Pointer to the transmission queue.
 * @param[in] msg Pointer to the message structure containing data to insert.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_transmission_queue_insert(struct astarte_device_transmission_queue *handle,
    const struct astarte_device_transmission_queue_msg *msg);

/**
 * @brief Peeks at the next message in the transmission queue without removing it.
 *
 * @param[in] handle Pointer to the transmission queue.
 * @param[out] msg Pointer to a message structure to populate.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_transmission_queue_peek(struct astarte_device_transmission_queue *handle,
    struct astarte_device_transmission_queue_msg *msg);

/**
 * @brief Discards messages from the transmission queue based on their retention policy.
 *
 * @param[in] handle Pointer to the transmission queue.
 * @param[in] retention Retention policy matching the messages to be discarded.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_transmission_queue_discard_by_retention(
    struct astarte_device_transmission_queue *handle, astarte_mapping_retention_t retention);

/**
 * @brief Frees the memory allocated for a transmission queue message payload and paths.
 *
 * @param[in] msg Pointer to the message structure to clean up.
 */
void astarte_transmission_queue_msg_cleanup(struct astarte_device_transmission_queue_msg *msg);

#endif // DEVICE_TRANSMISSION_QUEUE_H
