/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef STORAGE_TRANSMISSION_H
#define STORAGE_TRANSMISSION_H

/**
 * @file storage/trans.h
 * @brief Storage functions for the Astarte device transmission.
 */

#include "astarte_device_sdk/astarte.h"
#include "astarte_device_sdk/interface.h"
#include "astarte_device_sdk/result.h"

#include "storage/core.h"

/** @brief Message structure for the transmission storage. */
struct astarte_storage_transmission_msg
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
    /** @brief Global sequence identifier for cross-queue ordering. */
    uint64_t sequence_number;
};

/** @brief Indexes tracking the head and tail of the transmission storage queue. */
typedef struct
{
    /** @brief Head index of the transmission queue. */
    uint32_t head;
    /** @brief Tail index of the transmission queue. */
    uint32_t tail;

} astarte_storage_transmission_indexes_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Retrieves the current head and tail indexes of the transmission storage.
 *
 * @param[in] handle Pointer to the storage handle.
 * @param[out] indexes Pointer to the indexes structure to populate.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_storage_transmission_get_indexes(
    astarte_storage_data_t *handle, astarte_storage_transmission_indexes_t *indexes);

/**
 * @brief Retrieves the sequence number of the most recently stored message.
 *
 * @param[in] handle Pointer to the storage handle.
 * @param[in] indexes Pointer to the current transmission indexes.
 * @param[out] sequence_number Pointer to populate with the highest sequence number.
 * @return ASTARTE_RESULT_OK if successful, ASTARTE_RESULT_NOT_FOUND if empty, otherwise an error.
 */
astarte_result_t astarte_storage_transmission_get_last_sequence_number(
    astarte_storage_data_t *handle, const astarte_storage_transmission_indexes_t *indexes,
    uint64_t *sequence_number);

/**
 * @brief Pushes a new message into the transmission storage.
 *
 * @param[in] handle Pointer to the storage handle.
 * @param[in,out] indexes Pointer to the transmission indexes, updated upon successful push.
 * @param[in] msg Pointer to the message to be stored.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_storage_transmission_push(astarte_storage_data_t *handle,
    astarte_storage_transmission_indexes_t *indexes,
    const struct astarte_storage_transmission_msg *msg);

/**
 * @brief Peeks at the next message in the transmission storage without discarding it.
 *
 * @param[in] handle Pointer to the storage handle.
 * @param[in] indexes Pointer to the current transmission indexes.
 * @param[out] msg Pointer to a message structure to populate.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_storage_transmission_peek(astarte_storage_data_t *handle,
    const astarte_storage_transmission_indexes_t *indexes,
    struct astarte_storage_transmission_msg *msg);

/**
 * @brief Gets and processes the next message from the transmission storage.
 *
 * @param[in] handle Pointer to the storage handle.
 * @param[in,out] indexes Pointer to the transmission indexes, updated upon successful retrieval.
 * @param[out] msg Pointer to a message structure to populate.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_storage_transmission_get(astarte_storage_data_t *handle,
    astarte_storage_transmission_indexes_t *indexes, struct astarte_storage_transmission_msg *msg);

/**
 * @brief Discards the oldest message from the transmission storage.
 *
 * @param[in] handle Pointer to the storage handle.
 * @param[in,out] indexes Pointer to the transmission indexes, updated upon successful discard.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_storage_transmission_discard(
    astarte_storage_data_t *handle, astarte_storage_transmission_indexes_t *indexes);

/**
 * @brief Frees the memory allocated for a transmission storage message payload and paths.
 *
 * Call this function to free the memory allocated by:
 * - #astarte_storage_transmission_peek
 * - #astarte_storage_transmission_get
 *
 * @param[in] msg Pointer to the message structure to clean up.
 */
void astarte_storage_transmission_msg_cleanup(struct astarte_storage_transmission_msg *msg);

#ifdef __cplusplus
}
#endif

#endif // STORAGE_TRANSMISSION_H
