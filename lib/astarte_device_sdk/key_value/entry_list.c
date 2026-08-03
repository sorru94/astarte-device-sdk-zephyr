/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "key_value/entry_list.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "key_value/entry.h"
#include "key_value/entry_header.h"
#include "log.h"

ASTARTE_LOG_MODULE_DECLARE(astarte_key_value, CONFIG_ASTARTE_DEVICE_SDK_KEY_VALUE_LOG_LEVEL);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_key_value_entry_list_compute_next_and_prev_ids(
    struct zms_fs *zms_fs, uint32_t idx, uint32_t *next_id, uint32_t *prev_id)
{
    struct astarte_key_value_entry_header_fixed fixed_header = { 0 };
    size_t raw_size = 0;
    astarte_result_t ares
        = astarte_key_value_entry_header_read_fixed(zms_fs, idx, &fixed_header, &raw_size);
    if (ares == ASTARTE_RESULT_OK) {
        *next_id = fixed_header.next_id;
        *prev_id = fixed_header.prev_id;
    }
    if (ares == ASTARTE_RESULT_NOT_FOUND) {
        uint32_t head_id = ASTARTE_KEY_VALUE_ENTRY_NULL_ID;
        uint32_t tail_id = ASTARTE_KEY_VALUE_ENTRY_NULL_ID;
        astarte_result_t internal_ares
            = astarte_key_value_entry_list_read_head_and_tail_ids(zms_fs, &head_id, &tail_id);
        if (internal_ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR(
                "Failed reading head and tail IDs: %s.", astarte_result_to_name(internal_ares));
            return internal_ares;
        }
        *prev_id = tail_id;
        *next_id = ASTARTE_KEY_VALUE_ENTRY_NULL_ID;
    }
    if (ares != ASTARTE_RESULT_OK && ares != ASTARTE_RESULT_NOT_FOUND) {
        ASTARTE_LOG_ERR("Failed reading fixed header: %s.", astarte_result_to_name(ares));
    }
    return ares;
}

astarte_result_t astarte_key_value_entry_list_read_head_and_tail_ids(
    struct zms_fs *zms_fs, uint32_t *head_id, uint32_t *tail_id)
{
    uint32_t ids[2] = { 0 };
    ssize_t ret = zms_read(zms_fs, ASTARTE_KEY_VALUE_ENTRY_HEAD_AND_TAIL_ID, ids, sizeof(ids));

    if (ret == -ENOENT) {
        *head_id = ASTARTE_KEY_VALUE_ENTRY_NULL_ID;
        *tail_id = ASTARTE_KEY_VALUE_ENTRY_NULL_ID;
        return ASTARTE_RESULT_OK;
    }
    if (ret != sizeof(ids)) {
        ASTARTE_LOG_ERR("Error reading head and tail IDs: %d", (int) ret);
        return ASTARTE_RESULT_ZMS_ERROR;
    }

    *head_id = ids[0];
    *tail_id = ids[1];
    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_key_value_entry_list_write_head_and_tail_ids(
    struct zms_fs *zms_fs, uint32_t head_id, uint32_t tail_id)
{
    uint32_t ids[2] = { head_id, tail_id };
    ssize_t ret = zms_write(zms_fs, ASTARTE_KEY_VALUE_ENTRY_HEAD_AND_TAIL_ID, ids, sizeof(ids));
    if (ret < 0) {
        ASTARTE_LOG_ERR("Error writing head and tail IDs to ZMS, error: %d", (int) ret);
        return ASTARTE_RESULT_ZMS_ERROR;
    }
    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_key_value_entry_list_update_next_id(
    struct zms_fs *zms_fs, uint32_t idx, uint32_t new_next)
{
    ssize_t raw_entry_size = zms_get_data_length(zms_fs, idx);
    if (raw_entry_size <= 0) {
        ASTARTE_LOG_ERR("Error getting raw entry size: %d", (int) raw_entry_size);
        return ASTARTE_RESULT_ZMS_ERROR;
    }

    scope_var(scoped_uint8, raw_entry)(raw_entry_size);
    if (!raw_entry) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        return ASTARTE_RESULT_OUT_OF_MEMORY;
    }

    ssize_t ret = zms_read(zms_fs, idx, raw_entry, raw_entry_size);
    if (ret != raw_entry_size) {
        ASTARTE_LOG_ERR("Error getting raw entry: %d", (int) ret);
        return ASTARTE_RESULT_ZMS_ERROR;
    }

    size_t next_id_offset = ASTARTE_KEY_VALUE_ENTRY_HEADER_NAMESPACE_LEN_BYTES
        + ASTARTE_KEY_VALUE_ENTRY_HEADER_KEY_LEN_BYTES;
    memcpy(&raw_entry[next_id_offset], &new_next, sizeof(new_next));

    ret = zms_write(zms_fs, idx, raw_entry, raw_entry_size);
    if (ret < 0) {
        ASTARTE_LOG_ERR("Error writing new raw entry: %d", (int) ret);
        return ASTARTE_RESULT_ZMS_ERROR;
    }

    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_key_value_entry_list_update_prev_id(
    struct zms_fs *zms_fs, uint32_t idx, uint32_t new_prev)
{
    ssize_t raw_entry_size = zms_get_data_length(zms_fs, idx);
    if (raw_entry_size <= 0) {
        ASTARTE_LOG_ERR("Error getting raw entry size: %d", (int) raw_entry_size);
        return ASTARTE_RESULT_ZMS_ERROR;
    }

    scope_var(scoped_uint8, raw_entry)(raw_entry_size);
    if (!raw_entry) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        return ASTARTE_RESULT_OUT_OF_MEMORY;
    }

    ssize_t ret = zms_read(zms_fs, idx, raw_entry, raw_entry_size);
    if (ret != raw_entry_size) {
        ASTARTE_LOG_ERR("Error getting raw entry: %d", (int) ret);
        return ASTARTE_RESULT_ZMS_ERROR;
    }

    size_t prev_id_offset = ASTARTE_KEY_VALUE_ENTRY_HEADER_NAMESPACE_LEN_BYTES
        + ASTARTE_KEY_VALUE_ENTRY_HEADER_KEY_LEN_BYTES
        + ASTARTE_KEY_VALUE_ENTRY_HEADER_NEXT_ID_BYTES;
    memcpy(&raw_entry[prev_id_offset], &new_prev, sizeof(new_prev));

    ret = zms_write(zms_fs, idx, raw_entry, raw_entry_size);
    if (ret < 0) {
        ASTARTE_LOG_ERR("Error writing new raw entry: %d", (int) ret);
        return ASTARTE_RESULT_ZMS_ERROR;
    }

    return ASTARTE_RESULT_OK;
}
