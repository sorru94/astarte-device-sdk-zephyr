/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "storage/trans.h"

#include <stdlib.h>
#include <string.h>

#include "alloc.h"

#include "log.h"
ASTARTE_LOG_MODULE_DECLARE(astarte_storage, CONFIG_ASTARTE_DEVICE_SDK_STORAGE_LOG_LEVEL);

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

/** @brief Max size required to store a uint32_t string (10 digits + null terminator) */
#define MAX_UINT32_STR_LEN 11

/************************************************
 *         Static functions declaration         *
 ***********************************************/

static astarte_result_t extract_string(
    const uint8_t *buffer, size_t buffer_size, size_t *offset, size_t str_len, char **out_str);
static astarte_result_t extract_payload(
    const uint8_t *buffer, size_t buffer_size, size_t *offset, int payload_len, void **out_payload);

ASTARTE_SCOPE_DEFER_DEFINE(
    astarte_storage_transmission_msg_cleanup, struct astarte_storage_transmission_msg *);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_storage_transmission_get_indexes(
    astarte_storage_data_t *handle, astarte_storage_transmission_indexes_t *indexes)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    astarte_key_value_iter_t iter = { 0 };
    uint32_t min_id = UINT32_MAX;
    uint32_t max_id = 0;
    uint64_t sum_id = 0;
    uint32_t element_count = 0;
    bool found_any = false;

    if (!handle || !handle->initialized) {
        ASTARTE_LOG_ERR("Device caching handle is uninitialized or NULL.");
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    if (!indexes) {
        ASTARTE_LOG_ERR("Indexes pointer is NULL.");
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    // Initialize iterator for the transmission namespace
    ares = astarte_key_value_iterator_init(&handle->trans_storage, &iter);
    if ((ares != ASTARTE_RESULT_OK) && (ares != ASTARTE_RESULT_NOT_FOUND)) {
        ASTARTE_LOG_ERR("Transmission iterator init failed: %s", astarte_result_to_name(ares));
        return ares;
    }

    // Iterate over all the key-value pairs in this namespace
    while (ares != ASTARTE_RESULT_NOT_FOUND) {
        char key[MAX_UINT32_STR_LEN] = { 0 };
        size_t key_size = MAX_UINT32_STR_LEN;

        // Fetch key directly into stack buffer
        ares = astarte_key_value_iterator_get(&iter, key, &key_size);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Transmission iterator get error: %s", astarte_result_to_name(ares));
            return ares;
        }

        // Keys are numbers in string representation
        const int base_ten = 10;
        uint32_t current_id = (uint32_t) strtoul(key, NULL, base_ten);

        if (current_id < min_id) {
            min_id = current_id;
        }
        if (current_id > max_id) {
            max_id = current_id;
        }
        sum_id += current_id;
        element_count++;
        found_any = true;

        // Advance to the next element
        ares = astarte_key_value_iterator_next(&iter);
        if ((ares != ASTARTE_RESULT_OK) && (ares != ASTARTE_RESULT_NOT_FOUND)) {
            ASTARTE_LOG_ERR("Iterator next error: %s", astarte_result_to_name(ares));
            return ares;
        }
    }

    if (found_any) {
        if ((uint64_t) max_id - (uint64_t) min_id + 1 == (uint64_t) element_count) {
            // No wrap around
            indexes->head = min_id;
            indexes->tail = max_id;
        } else {
            // Wrap around case:
            // Since the IDs wrapped around UINT32_MAX, the valid elements are split into
            // two blocks. This means the *missing* elements form a single contiguous
            // arithmetic sequence between tail and head: from (tail + 1) to (head - 1).

            // Calculate the sum of all possible IDs from 0 to UINT32_MAX.
            uint64_t sum_total = ((uint64_t) UINT32_MAX * ((uint64_t) UINT32_MAX + 1)) / 2;

            // The sum of the missing block is the total sum minus the sum of present IDs.
            uint64_t sum_missing = sum_total - sum_id;

            // The number of missing elements (let's call this m).
            uint64_t missing = (uint64_t) UINT32_MAX + 1 - (uint64_t) element_count;

            // Sum of an arithmetic sequence: Sum = m * (first_val + last_val) / 2
            // Here, first_val = tail + 1 and last_val = head - 1.
            // So, (first_val + last_val) = (tail + 1) + (head - 1) = head + tail.
            // Isolating (head + tail):
            uint64_t sum_ht = (2 * sum_missing) / missing;

            // The number of missing elements m is also: m = last_val - first_val + 1
            // k = (head - 1) - (tail + 1) + 1 = head - tail - 1
            // Isolating (head - tail):
            uint64_t diff_ht = missing + 1;

            // We now have a system of two linear equations:
            // sum_ht  = head + tail
            // diff_ht = head - tail
            // Solving this system by adding/subtracting the equations yields head and tail:
            indexes->head = (uint32_t) ((sum_ht + diff_ht) / 2);
            indexes->tail = (uint32_t) ((sum_ht - diff_ht) / 2);
        }
    } else {
        indexes->head = 1;
        indexes->tail = 0;
    }

    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_storage_transmission_get_last_sequence_number(
    astarte_storage_data_t *handle, const astarte_storage_transmission_indexes_t *indexes,
    uint64_t *sequence_number)
{
    if (!handle || !handle->initialized || !indexes || !sequence_number) {
        ASTARTE_LOG_ERR("Some input parameters are NULL.");
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    // Detect if the storage is completely empty
    if (indexes->head == (uint32_t) (indexes->tail + 1)) {
        return ASTARTE_RESULT_NOT_FOUND;
    }

    char key[MAX_UINT32_STR_LEN] = { 0 };
    // The newest message is always located at `tail`
    int snprintf_rc = snprintf(key, sizeof(key), "%010u", indexes->tail);
    if (snprintf_rc != MAX_UINT32_STR_LEN - 1) {
        ASTARTE_LOG_ERR("Error encoding key into string");
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }

    size_t value_size = 0;
    astarte_result_t ares = astarte_key_value_find(&handle->trans_storage, key, NULL, &value_size);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Error finding entry size: %s", astarte_result_to_name(ares));
        return ares;
    }

    scope_var(scoped_uint8, buffer)(value_size);
    if (!buffer) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        return ASTARTE_RESULT_OUT_OF_MEMORY;
    }

    ares = astarte_key_value_find(&handle->trans_storage, key, buffer, &value_size);
    if (ares == ASTARTE_RESULT_OK) {
        // Calculate exact offset based on the serialization logic in `push`
        size_t offset = sizeof(int) /* qos */ + sizeof(uint64_t) /* timestamp */;

        if (offset + sizeof(uint64_t) <= value_size) {
            memcpy(sequence_number, buffer + offset, sizeof(uint64_t));
        } else {
            ASTARTE_LOG_ERR("Corrupted storage: buffer too small for sequence number");
            ares = ASTARTE_RESULT_INTERNAL_ERROR;
        }
    } else {
        ASTARTE_LOG_ERR("Error finding entry: %s", astarte_result_to_name(ares));
    }

    return ares;
}

astarte_result_t astarte_storage_transmission_push(astarte_storage_data_t *handle,
    astarte_storage_transmission_indexes_t *indexes,
    const struct astarte_storage_transmission_msg *msg)
{
    if (!handle || !handle->initialized || !indexes || !msg->interface_name || !msg->path) {
        ASTARTE_LOG_ERR("NULL parameters provided");
        return ASTARTE_RESULT_INVALID_PARAM;
    }
    if ((msg->payload_len < 0) || (msg->payload_len > 0 && msg->payload == NULL)) {
        ASTARTE_LOG_ERR("Improper payload provided");
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    astarte_result_t ares = ASTARTE_RESULT_OK;

    size_t interface_name_len = strlen(msg->interface_name);
    size_t path_len = strlen(msg->path);

    // Calculate total buffer size required for serialization
    size_t buffer_size = sizeof(msg->qos) + sizeof(msg->timestamp) + sizeof(msg->sequence_number)
        + sizeof(interface_name_len) + interface_name_len + sizeof(path_len) + path_len
        + sizeof(msg->payload_len) + msg->payload_len;

    scope_var(scoped_uint8, buffer)(buffer_size);
    if (!buffer) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        return ASTARTE_RESULT_OUT_OF_MEMORY;
    }

    // Serialize static fields
    size_t offset = 0;
    memcpy(buffer + offset, &msg->qos, sizeof(msg->qos));
    offset += sizeof(msg->qos);

    memcpy(buffer + offset, &msg->timestamp, sizeof(msg->timestamp));
    offset += sizeof(msg->timestamp);

    memcpy(buffer + offset, &msg->sequence_number, sizeof(msg->sequence_number));
    offset += sizeof(msg->sequence_number);

    memcpy(buffer + offset, &interface_name_len, sizeof(interface_name_len));
    offset += sizeof(interface_name_len);

    memcpy(buffer + offset, &path_len, sizeof(path_len));
    offset += sizeof(path_len);

    memcpy(buffer + offset, &msg->payload_len, sizeof(msg->payload_len));
    offset += sizeof(msg->payload_len);

    // Serialize dynamic payload fields
    if (interface_name_len > 0) {
        memcpy(buffer + offset, msg->interface_name, interface_name_len);
        offset += interface_name_len;
    }

    if (path_len > 0) {
        memcpy(buffer + offset, msg->path, path_len);
        offset += path_len;
    }

    if (msg->payload_len > 0 && msg->payload != NULL) {
        memcpy(buffer + offset, msg->payload, msg->payload_len);
    }

    // Keys are stored as zero-padded 10-digit strings
    char key[MAX_UINT32_STR_LEN];
    int snprintf_rc = snprintf(key, sizeof(key), "%010u", indexes->tail + 1);
    if (snprintf_rc != MAX_UINT32_STR_LEN - 1) {
        ASTARTE_LOG_ERR("Error encoding key into string");
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }

    // Insert to ZMS
    ares = astarte_key_value_insert(&handle->trans_storage, key, buffer, buffer_size);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Error inserting entry: %s", astarte_result_to_name(ares));
        return ares;
    }

    indexes->tail++;
    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_storage_transmission_peek(astarte_storage_data_t *handle,
    const astarte_storage_transmission_indexes_t *indexes,
    struct astarte_storage_transmission_msg *msg)
{
    if (!handle || !handle->initialized || !indexes || !msg) {
        ASTARTE_LOG_ERR("NULL parameters provided");
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    astarte_result_t ares = ASTARTE_RESULT_OK;

    char key[MAX_UINT32_STR_LEN] = { 0 };
    int snprintf_rc = snprintf(key, sizeof(key), "%010u", indexes->head);
    if (snprintf_rc != MAX_UINT32_STR_LEN - 1) {
        ASTARTE_LOG_ERR("Error encoding key into string");
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }

    // Query the size of the stored message
    size_t buffer_size = 0;
    ares = astarte_key_value_find(&handle->trans_storage, key, NULL, &buffer_size);
    if (ares != ASTARTE_RESULT_OK) {
        // Could be ASTARTE_RESULT_NOT_FOUND
        return ares;
    }

    scope_var(scoped_uint8, buffer)(buffer_size);
    if (!buffer) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        return ASTARTE_RESULT_OUT_OF_MEMORY;
    }

    // Retrieve the actual data
    ares = astarte_key_value_find(&handle->trans_storage, key, buffer, &buffer_size);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Error finding entry: %s", astarte_result_to_name(ares));
        return ares;
    }

    struct astarte_storage_transmission_msg local_msg = { 0 };
    scope_defer(astarte_storage_transmission_msg_cleanup)(&local_msg);
    size_t offset = 0;
    size_t interface_name_len = 0;
    size_t path_len = 0;

    size_t static_fields_size = sizeof(local_msg.qos) + sizeof(local_msg.timestamp)
        + sizeof(local_msg.sequence_number) + sizeof(interface_name_len) + sizeof(path_len)
        + sizeof(local_msg.payload_len);

    if (offset + static_fields_size > buffer_size) {
        ASTARTE_LOG_ERR("Corrupted storage: buffer too small for static fields");
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }

    // Safely extract static fields
    memcpy(&local_msg.qos, buffer + offset, sizeof(local_msg.qos));
    offset += sizeof(local_msg.qos);

    memcpy(&local_msg.timestamp, buffer + offset, sizeof(local_msg.timestamp));
    offset += sizeof(local_msg.timestamp);

    memcpy(&local_msg.sequence_number, buffer + offset, sizeof(local_msg.sequence_number));
    offset += sizeof(local_msg.sequence_number);

    memcpy(&interface_name_len, buffer + offset, sizeof(interface_name_len));
    offset += sizeof(interface_name_len);

    memcpy(&path_len, buffer + offset, sizeof(path_len));
    offset += sizeof(path_len);

    memcpy(&local_msg.payload_len, buffer + offset, sizeof(local_msg.payload_len));
    offset += sizeof(local_msg.payload_len);

    ares = extract_string(
        buffer, buffer_size, &offset, interface_name_len, &local_msg.interface_name);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Error extracting interface name: %s", astarte_result_to_name(ares));
        return ares;
    }

    ares = extract_string(buffer, buffer_size, &offset, path_len, &local_msg.path);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Error extracting path: %s", astarte_result_to_name(ares));
        return ares;
    }

    ares = extract_payload(buffer, buffer_size, &offset, local_msg.payload_len, &local_msg.payload);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Error extracting payload: %s", astarte_result_to_name(ares));
        return ares;
    }

    // Populate msg struct
    msg->interface_name = local_msg.interface_name;
    msg->path = local_msg.path;
    msg->payload = local_msg.payload;
    msg->payload_len = local_msg.payload_len;
    msg->qos = local_msg.qos;
    msg->timestamp = local_msg.timestamp;
    msg->sequence_number = local_msg.sequence_number;

    // Disarm message cleanup
    local_msg.interface_name = NULL;
    local_msg.path = NULL;
    local_msg.payload = NULL;

    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_storage_transmission_get(astarte_storage_data_t *handle,
    astarte_storage_transmission_indexes_t *indexes, struct astarte_storage_transmission_msg *msg)
{
    if (!handle || !handle->initialized || !indexes || !msg) {
        ASTARTE_LOG_ERR("NULL parameters provided");
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    struct astarte_storage_transmission_msg local_msg = { 0 };
    scope_defer(astarte_storage_transmission_msg_cleanup)(&local_msg);

    // Reuse peek logic to parse and populate the message
    astarte_result_t ares = astarte_storage_transmission_peek(handle, indexes, &local_msg);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Error peeking storage: %s", astarte_result_to_name(ares));
        return ares;
    }

    // Delete the message from the persistent store
    char key[MAX_UINT32_STR_LEN] = { 0 };
    int snprintf_rc = snprintf(key, sizeof(key), "%010u", indexes->head);
    if (snprintf_rc != MAX_UINT32_STR_LEN - 1) {
        ASTARTE_LOG_ERR("Error encoding key into string");
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }
    ares = astarte_key_value_delete(&handle->trans_storage, key);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed to delete fetched transmission storage entry");
        return ares;
    }
    indexes->head++;

    // Populate msg struct
    msg->interface_name = local_msg.interface_name;
    msg->path = local_msg.path;
    msg->payload = local_msg.payload;
    msg->payload_len = local_msg.payload_len;
    msg->qos = local_msg.qos;
    msg->timestamp = local_msg.timestamp;
    msg->sequence_number = local_msg.sequence_number;

    // Disarm message cleanup
    local_msg.interface_name = NULL;
    local_msg.path = NULL;
    local_msg.payload = NULL;

    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_storage_transmission_discard(
    astarte_storage_data_t *handle, astarte_storage_transmission_indexes_t *indexes)
{
    if (!handle || !handle->initialized || !indexes) {
        ASTARTE_LOG_ERR("NULL parameters provided");
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    astarte_result_t ares = ASTARTE_RESULT_OK;
    char key[MAX_UINT32_STR_LEN] = { 0 };

    // Keys are stored as zero-padded 10-digit strings
    int snprintf_rc = snprintf(key, sizeof(key), "%010u", indexes->head);
    if (snprintf_rc != MAX_UINT32_STR_LEN - 1) {
        ASTARTE_LOG_ERR("Error encoding key into string");
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }

    ares = astarte_key_value_delete(&handle->trans_storage, key);
    if (ares == ASTARTE_RESULT_OK) {
        indexes->head++;
    } else {
        ASTARTE_LOG_COND_ERR(ares != ASTARTE_RESULT_NOT_FOUND,
            "Failed to discard transmission storage entry: %s", astarte_result_to_name(ares));
    }

    return ares;
}

void astarte_storage_transmission_msg_cleanup(struct astarte_storage_transmission_msg *msg)
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

static astarte_result_t extract_string(
    const uint8_t *buffer, size_t buffer_size, size_t *offset, size_t str_len, char **out_str)
{
    if (*offset + str_len > buffer_size) {
        ASTARTE_LOG_ERR("Corrupted storage: string bounds exceeded");
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }

    *out_str = astarte_calloc(str_len + 1, sizeof(char));
    if (!*out_str) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        return ASTARTE_RESULT_OUT_OF_MEMORY;
    }

    if (str_len > 0) {
        memcpy(*out_str, buffer + *offset, str_len);
        *offset += str_len;
    }

    (*out_str)[str_len] = '\0';

    return ASTARTE_RESULT_OK;
}

static astarte_result_t extract_payload(
    const uint8_t *buffer, size_t buffer_size, size_t *offset, int payload_len, void **out_payload)
{
    if (payload_len < 0) {
        ASTARTE_LOG_ERR("Corrupted storage: payload length is negative");
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }

    if (payload_len == 0) {
        *out_payload = NULL;
        return ASTARTE_RESULT_OK;
    }

    if (*offset + payload_len > buffer_size) {
        ASTARTE_LOG_ERR("Corrupted storage: payload bounds exceeded");
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }

    *out_payload = astarte_malloc(payload_len);
    if (!*out_payload) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        return ASTARTE_RESULT_OUT_OF_MEMORY;
    }

    memcpy(*out_payload, buffer + *offset, payload_len);
    *offset += payload_len;

    return ASTARTE_RESULT_OK;
}
