/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "key_value/entry.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/sys/crc.h>

#include "alloc.h"
#include "key_value/entry_hash.h"
#include "key_value/entry_header.h"
#include "key_value/entry_intent.h"
#include "key_value/entry_list.h"
#include "log.h"

ASTARTE_LOG_MODULE_DECLARE(astarte_key_value, CONFIG_ASTARTE_DEVICE_SDK_KEY_VALUE_LOG_LEVEL);

/************************************************
 *         Static functions declaration         *
 ***********************************************/

static astarte_result_t update_list_tail(struct zms_fs *zms_fs, uint32_t new_tail_id);
static uint8_t *serialize_entry(struct astarte_key_value_entry_header header, const void *value,
    size_t value_size, size_t *serialized_entry_size);
static astarte_result_t check_entry_match(
    struct zms_fs *zms_fs, uint32_t idx, const char *namespace, const char *key);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_key_value_entry_find_or_alloc(
    struct zms_fs *zms_fs, const char *namespace, const char *key, uint32_t *idx, bool allocate)
{
    uint32_t start_id = astarte_key_value_entry_hash_generate(namespace, key);
    uint32_t curr_id = start_id;

    do {
        // Check if the namespace and key match the stored values
        astarte_result_t ares = check_entry_match(zms_fs, curr_id, namespace, key);
        if (ares == ASTARTE_RESULT_NOT_FOUND) {
            if (!allocate) {
                ASTARTE_LOG_DBG("Key not found at ID %d", curr_id);
                return ares;
            }
            ASTARTE_LOG_DBG("Found empty slot at ID %d", curr_id);
            *idx = curr_id;
            return ASTARTE_RESULT_OK;
        }
        if (ares == ASTARTE_RESULT_OK) {
            *idx = curr_id;
            ASTARTE_LOG_DBG("Found matching entry at ID %d", curr_id);
            return ASTARTE_RESULT_OK;
        }
        if (ares != ASTARTE_RESULT_MISMATCH) {
            ASTARTE_LOG_ERR("Error while matching namespace and key at ID: %d", curr_id);
            return ares;
        }
        // If ASTARTE_RESULT_MISMATCH is returned, it means a collision occurred.
        // The loop will continue to the next ID.
        curr_id++;
        if (curr_id > ASTARTE_KEY_VALUE_ENTRY_MAX_USABLE_ID) {
            curr_id = ASTARTE_KEY_VALUE_ENTRY_MIN_USABLE_ID;
        }
    } while (curr_id != start_id);

    ASTARTE_LOG_ERR("Key-value storage is full");
    return ASTARTE_RESULT_KEY_VALUE_FULL;
}

astarte_result_t astarte_key_value_entry_write(struct zms_fs *zms_fs, uint32_t idx,
    const char *namespace, const char *key, const void *value, size_t value_size)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    uint8_t *raw_entry = NULL;
    size_t raw_entry_size = 0;

    bool is_end_of_list = false;
    uint32_t prev_id = ASTARTE_KEY_VALUE_ENTRY_NULL_ID;
    uint32_t next_id = ASTARTE_KEY_VALUE_ENTRY_NULL_ID;
    ares = astarte_key_value_entry_list_compute_next_and_prev_ids(zms_fs, idx, &next_id, &prev_id);
    if (ares == ASTARTE_RESULT_NOT_FOUND) {
        is_end_of_list = true;
    } else if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Error computing next and previous IDs for entry at ID %d: %s", idx,
            astarte_result_to_name(ares));
        goto exit;
    }

    // Serialize the entry with the computed next and previous IDs
    struct astarte_key_value_entry_header header = {
        .fixed_header = {
            .namespace_len = strlen(namespace),
            .key_len = strlen(key),
            .next_id = next_id,
            .prev_id = prev_id,
        },
        .namespace = (char *) namespace,
        .key = (char *) key,
        .dynamically_allocated = false,
    };
    raw_entry = serialize_entry(header, value, value_size, &raw_entry_size);
    if (!raw_entry) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto exit;
    }

    // Log intent before any physical write occurs
    astarte_key_value_entry_intent_state_t intent_state = is_end_of_list
        ? ASTARTE_KEY_VALUE_ENTRY_INTENT_INSERTING
        : ASTARTE_KEY_VALUE_ENTRY_INTENT_UPDATING;
    ares = astarte_key_value_entry_intent_write(zms_fs, intent_state, idx, prev_id, next_id);
    if (ares != ASTARTE_RESULT_OK) {
        goto exit;
    }

    // Write the serialized entry to ZMS
    ssize_t ret = zms_write(zms_fs, idx, raw_entry, raw_entry_size);
    if (ret < 0) {
        ASTARTE_LOG_ERR("Error writing to ZMS at ID %d, error: %d", idx, ret);
        ares = ASTARTE_RESULT_ZMS_ERROR;
        goto exit;
    }

    // Update the previous tail to point to the new entry and update the head/tail IDs if needed
    if (is_end_of_list) {
        ares = update_list_tail(zms_fs, idx);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Error updating tail %d, error: %s", idx, astarte_result_to_name(ares));
            goto exit;
        }
    }

    // Clear intent block to signal successful multi-step transaction
    ares = astarte_key_value_entry_intent_clear(zms_fs);

exit:
    astarte_free(raw_entry);
    return ares;
}

astarte_result_t astarte_key_value_entry_read_value(
    struct zms_fs *zms_fs, uint32_t idx, void *value, size_t *value_size)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    uint8_t *raw_entry = NULL;
    size_t raw_entry_size = 0;

    // Read the fixed header
    struct astarte_key_value_entry_header_fixed fixed_header = { 0 };
    ares = astarte_key_value_entry_header_read_fixed(zms_fs, idx, &fixed_header, &raw_entry_size);
    if (ares != ASTARTE_RESULT_OK) {
        goto exit;
    }

    // Calculate the full header size
    size_t header_size = ASTARTE_KEY_VALUE_ENTRY_HEADER_FIXED_HEADER_BYTES
        + fixed_header.namespace_len + fixed_header.key_len;
    if (header_size > raw_entry_size) {
        ASTARTE_LOG_ERR("Error: Incomplete header at ID %d", idx);
        ares = ASTARTE_RESULT_INTERNAL_ERROR;
        goto exit;
    }

    // Compute the value size and return it immediately if there is no output buffer
    size_t read_value_size = raw_entry_size - header_size;
    if (!value) {
        *value_size = read_value_size;
        goto exit;
    }
    if (*value_size < read_value_size) {
        ASTARTE_LOG_ERR("Error: Value buffer too small at ID %d", idx);
        ares = ASTARTE_RESULT_INVALID_PARAM;
        goto exit;
    }

    // Read the full entry
    raw_entry = astarte_calloc(raw_entry_size, sizeof(uint8_t));
    if (!raw_entry) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto exit;
    }
    ssize_t ret = zms_read(zms_fs, idx, raw_entry, raw_entry_size);
    if (ret != raw_entry_size) {
        ASTARTE_LOG_ERR("Error reading full payload from ZMS at ID %d, error: %d", idx, ret);
        ares = ASTARTE_RESULT_ZMS_ERROR;
        goto exit;
    }

    if (read_value_size > 0) {
        memcpy(value, raw_entry + header_size, read_value_size);
    }

    *value_size = read_value_size;

exit:
    astarte_free(raw_entry);
    return ares;
}

astarte_result_t astarte_key_value_entry_read_key(
    struct zms_fs *zms_fs, uint32_t idx, char *key, size_t *key_size)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    struct astarte_key_value_entry_header header = { 0 };
    size_t raw_entry_size = 0;

    ares = astarte_key_value_entry_header_read(zms_fs, idx, &header, &raw_entry_size);
    if (ares != ASTARTE_RESULT_OK) {
        goto exit;
    }

    if (!key) {
        // +1 includes room for termination
        *key_size = header.fixed_header.key_len + 1;
        goto exit;
    }
    if (*key_size < header.fixed_header.key_len + 1) {
        ASTARTE_LOG_ERR("Error: Key buffer too small at ID %d", idx);
        ares = ASTARTE_RESULT_INVALID_PARAM;
        goto exit;
    }

    memcpy(key, header.key, header.fixed_header.key_len);
    key[header.fixed_header.key_len] = '\0';
    *key_size = header.fixed_header.key_len + 1;

exit:
    astarte_key_value_entry_header_free(&header);
    return ares;
}

astarte_result_t astarte_key_value_entry_check_namespace(
    struct zms_fs *zms_fs, uint32_t idx, const char *namespace, bool *matches)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    struct astarte_key_value_entry_header header = { 0 };
    size_t raw_entry_size = 0;

    if (!namespace || !matches) {
        ASTARTE_LOG_ERR("Trying to check namespace at ID %d passing null pointers", idx);
        ares = ASTARTE_RESULT_INVALID_PARAM;
        goto exit;
    }

    ares = astarte_key_value_entry_header_read(zms_fs, idx, &header, &raw_entry_size);
    if (ares == ASTARTE_RESULT_NOT_FOUND) {
        ares = ASTARTE_RESULT_OK;
        *matches = false;
        goto exit;
    }
    if (ares != ASTARTE_RESULT_OK) {
        goto exit;
    }

    if (header.fixed_header.namespace_len != strlen(namespace)) {
        *matches = false;
        goto exit;
    }

    *matches = strncmp(header.namespace, namespace, header.fixed_header.namespace_len) == 0;

exit:
    astarte_key_value_entry_header_free(&header);
    return ares;
}

astarte_result_t astarte_key_value_entry_get_next_id(
    struct zms_fs *zms_fs, uint32_t idx, uint32_t *next_id)
{
    // Calling this function with the NULL ID will make it return the ID of the head
    if (idx == ASTARTE_KEY_VALUE_ENTRY_NULL_ID) {
        uint32_t head_id = ASTARTE_KEY_VALUE_ENTRY_NULL_ID;
        uint32_t tail_id = ASTARTE_KEY_VALUE_ENTRY_NULL_ID;
        astarte_result_t rd_res
            = astarte_key_value_entry_list_read_head_and_tail_ids(zms_fs, &head_id, &tail_id);
        if (rd_res != ASTARTE_RESULT_OK) {
            return rd_res;
        }
        *next_id = head_id;
        return ASTARTE_RESULT_OK;
    }

    struct astarte_key_value_entry_header_fixed fixed_header = { 0 };
    size_t raw_entry_size = 0;
    astarte_result_t ares
        = astarte_key_value_entry_header_read_fixed(zms_fs, idx, &fixed_header, &raw_entry_size);
    if (ares != ASTARTE_RESULT_OK) {
        return ares;
    }

    *next_id = fixed_header.next_id;
    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_key_value_entry_get_prev_id(
    struct zms_fs *zms_fs, uint32_t idx, uint32_t *prev_id)
{
    // Calling this function with NULL ID will make it return the NULL ID
    if (idx == ASTARTE_KEY_VALUE_ENTRY_NULL_ID) {
        *prev_id = ASTARTE_KEY_VALUE_ENTRY_NULL_ID;
        return ASTARTE_RESULT_OK;
    }

    struct astarte_key_value_entry_header_fixed fixed_header = { 0 };
    size_t raw_entry_size = 0;
    astarte_result_t ares
        = astarte_key_value_entry_header_read_fixed(zms_fs, idx, &fixed_header, &raw_entry_size);
    if (ares != ASTARTE_RESULT_OK) {
        return ares;
    }

    *prev_id = fixed_header.prev_id;
    return ASTARTE_RESULT_OK;
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static astarte_result_t update_list_tail(struct zms_fs *zms_fs, uint32_t new_tail_id)
{
    uint32_t head_id = ASTARTE_KEY_VALUE_ENTRY_NULL_ID;
    uint32_t tail_id = ASTARTE_KEY_VALUE_ENTRY_NULL_ID;
    astarte_result_t ares
        = astarte_key_value_entry_list_read_head_and_tail_ids(zms_fs, &head_id, &tail_id);
    if (ares != ASTARTE_RESULT_OK) {
        return ares;
    }

    if (tail_id != ASTARTE_KEY_VALUE_ENTRY_NULL_ID) {
        ares = astarte_key_value_entry_list_update_next_id(zms_fs, tail_id, new_tail_id);
        if (ares != ASTARTE_RESULT_OK) {
            return ares;
        }
    } else {
        head_id = new_tail_id;
    }
    tail_id = new_tail_id;
    return astarte_key_value_entry_list_write_head_and_tail_ids(zms_fs, head_id, tail_id);
}

static uint8_t *serialize_entry(struct astarte_key_value_entry_header header, const void *value,
    size_t value_size, size_t *serialized_entry_size)
{
    // Allocate enough memory to store the full raw entry (header + namespace + key + value)
    uint16_t nsp_len = header.fixed_header.namespace_len;
    uint16_t key_len = header.fixed_header.key_len;
    size_t entry_size
        = ASTARTE_KEY_VALUE_ENTRY_HEADER_FIXED_HEADER_BYTES + nsp_len + key_len + value_size;
    uint8_t *entry = astarte_calloc(entry_size, sizeof(uint8_t));
    if (!entry) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        return NULL;
    }

    // NOLINTBEGIN(bugprone-not-null-terminated-result)
    size_t write_offset = 0;
    size_t write_size = ASTARTE_KEY_VALUE_ENTRY_HEADER_NAMESPACE_LEN_BYTES;
    memcpy(entry + write_offset, &header.fixed_header.namespace_len, write_size);
    write_offset += write_size;
    write_size = ASTARTE_KEY_VALUE_ENTRY_HEADER_KEY_LEN_BYTES;
    memcpy(entry + write_offset, &header.fixed_header.key_len, write_size);
    write_offset += write_size;
    write_size = ASTARTE_KEY_VALUE_ENTRY_HEADER_NEXT_ID_BYTES;
    memcpy(entry + write_offset, &header.fixed_header.next_id, write_size);
    write_offset += write_size;
    write_size = ASTARTE_KEY_VALUE_ENTRY_HEADER_PREV_ID_BYTES;
    memcpy(entry + write_offset, &header.fixed_header.prev_id, write_size);
    write_offset += write_size;
    write_size = header.fixed_header.namespace_len;
    memcpy(entry + write_offset, header.namespace, write_size);
    write_offset += write_size;
    write_size = header.fixed_header.key_len;
    memcpy(entry + write_offset, header.key, write_size);
    if (value_size > 0 && value != NULL) {
        write_offset += write_size;
        write_size = value_size;
        memcpy(entry + write_offset, value, write_size);
    }
    // NOLINTEND(bugprone-not-null-terminated-result)

    *serialized_entry_size = entry_size;
    return entry;
}

static astarte_result_t check_entry_match(
    struct zms_fs *zms_fs, uint32_t idx, const char *namespace, const char *key)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    struct astarte_key_value_entry_header header = { 0 };
    size_t raw_size = 0;

    ares = astarte_key_value_entry_header_read(zms_fs, idx, &header, &raw_size);
    if (ares != ASTARTE_RESULT_OK) {
        goto exit;
    }

    struct astarte_key_value_entry_header_fixed fixed_header = header.fixed_header;
    if (fixed_header.namespace_len != strlen(namespace) || fixed_header.key_len != strlen(key)) {
        ares = ASTARTE_RESULT_MISMATCH;
        goto exit;
    }
    int nsp_cmp = strncmp(header.namespace, namespace, fixed_header.namespace_len);
    int key_cmp = strncmp(header.key, key, fixed_header.key_len);
    if (nsp_cmp != 0 || key_cmp != 0) {
        ares = ASTARTE_RESULT_MISMATCH;
        goto exit;
    }

exit:
    astarte_key_value_entry_header_free(&header);
    return ares;
}
