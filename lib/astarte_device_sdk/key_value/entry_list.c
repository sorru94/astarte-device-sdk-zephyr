/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "key_value/entry_list.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "key_value/entry.h"
#include "key_value/entry_header.h"
#include "log.h"

ASTARTE_LOG_MODULE_DECLARE(astarte_key_value, CONFIG_ASTARTE_DEVICE_SDK_KEY_VALUE_LOG_LEVEL);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_key_value_entry_list_compute_next_and_prev_ids(
    struct nvs_fs *nvs_fs, uint16_t idx, uint16_t *next_id, uint16_t *prev_id)
{
    struct astarte_key_value_entry_header_fixed fixed_header = { 0 };
    size_t raw_size = 0;
    astarte_result_t ares
        = astarte_key_value_entry_header_read_fixed(nvs_fs, idx, &fixed_header, &raw_size);
    if (ares == ASTARTE_RESULT_OK) {
        *next_id = fixed_header.next_id;
        *prev_id = fixed_header.prev_id;
    }
    if (ares == ASTARTE_RESULT_NOT_FOUND) {
        uint16_t head_id = ASTARTE_KEY_VALUE_ENTRY_NULL_ID;
        uint16_t tail_id = ASTARTE_KEY_VALUE_ENTRY_NULL_ID;
        astarte_result_t internal_ares
            = astarte_key_value_entry_list_read_head_and_tail_ids(nvs_fs, &head_id, &tail_id);
        if (internal_ares != ASTARTE_RESULT_OK) {
            return internal_ares;
        }
        *prev_id = tail_id;
        *next_id = ASTARTE_KEY_VALUE_ENTRY_NULL_ID;
    }
    return ares;
}

astarte_result_t astarte_key_value_entry_list_read_head_and_tail_ids(
    struct nvs_fs *nvs_fs, uint16_t *head_id, uint16_t *tail_id)
{
    uint16_t ids[2] = { 0 };
    ssize_t ret = nvs_read(nvs_fs, ASTARTE_KEY_VALUE_ENTRY_HEAD_AND_TAIL_ID, ids, sizeof(ids));

    if (ret == -ENOENT) {
        *head_id = ASTARTE_KEY_VALUE_ENTRY_NULL_ID;
        *tail_id = ASTARTE_KEY_VALUE_ENTRY_NULL_ID;
        return ASTARTE_RESULT_OK;
    }
    if (ret < 0) {
        ASTARTE_LOG_ERR("Error reading head and tail IDs: %d", (int) ret);
        return ASTARTE_RESULT_NVS_ERROR;
    }

    *head_id = ids[0];
    *tail_id = ids[1];
    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_key_value_entry_list_write_head_and_tail_ids(
    struct nvs_fs *nvs_fs, uint16_t head_id, uint16_t tail_id)
{
    uint16_t ids[2] = { head_id, tail_id };
    ssize_t ret = nvs_write(nvs_fs, ASTARTE_KEY_VALUE_ENTRY_HEAD_AND_TAIL_ID, ids, sizeof(ids));
    if (ret < 0) {
        ASTARTE_LOG_ERR("Error writing head and tail IDs to NVS, error: %d", (int) ret);
        return ASTARTE_RESULT_NVS_ERROR;
    }
    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_key_value_entry_list_update_next_id(
    struct nvs_fs *nvs_fs, uint16_t idx, uint16_t new_next)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    uint8_t *raw_entry = NULL;
    ssize_t raw_entry_size = nvs_read(nvs_fs, idx, NULL, 0);
    if (raw_entry_size <= 0) {
        ares = ASTARTE_RESULT_NVS_ERROR;
        goto exit;
    }

    raw_entry = calloc(raw_entry_size, 1);
    if (!raw_entry) {
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto exit;
    }

    ssize_t ret = nvs_read(nvs_fs, idx, raw_entry, raw_entry_size);
    if (ret < 0) {
        ares = ASTARTE_RESULT_NVS_ERROR;
        goto exit;
    }

    size_t next_id_offset = ASTARTE_KEY_VALUE_ENTRY_HEADER_NAMESPACE_LEN_BYTES
        + ASTARTE_KEY_VALUE_ENTRY_HEADER_KEY_LEN_BYTES;
    memcpy(&raw_entry[next_id_offset], &new_next, sizeof(new_next));

    ret = nvs_write(nvs_fs, idx, raw_entry, raw_entry_size);
    if (ret < 0) {
        ares = ASTARTE_RESULT_NVS_ERROR;
    }

exit:
    free(raw_entry);
    return ares;
}

astarte_result_t astarte_key_value_entry_list_update_prev_id(
    struct nvs_fs *nvs_fs, uint16_t idx, uint16_t new_prev)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    uint8_t *raw_entry = NULL;
    ssize_t raw_entry_size = nvs_read(nvs_fs, idx, NULL, 0);
    if (raw_entry_size <= 0) {
        ares = ASTARTE_RESULT_NVS_ERROR;
        goto exit;
    }

    raw_entry = calloc(raw_entry_size, 1);
    if (!raw_entry) {
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto exit;
    }

    ssize_t ret = nvs_read(nvs_fs, idx, raw_entry, raw_entry_size);
    if (ret < 0) {
        ares = ASTARTE_RESULT_NVS_ERROR;
        goto exit;
    }

    size_t prev_id_offset = ASTARTE_KEY_VALUE_ENTRY_HEADER_NAMESPACE_LEN_BYTES
        + ASTARTE_KEY_VALUE_ENTRY_HEADER_KEY_LEN_BYTES
        + ASTARTE_KEY_VALUE_ENTRY_HEADER_NEXT_ID_BYTES;
    memcpy(&raw_entry[prev_id_offset], &new_prev, sizeof(new_prev));

    ret = nvs_write(nvs_fs, idx, raw_entry, raw_entry_size);
    if (ret < 0) {
        ares = ASTARTE_RESULT_NVS_ERROR;
        goto exit;
    }

exit:
    free(raw_entry);
    return ares;
}
