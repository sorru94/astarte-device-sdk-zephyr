/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "kv_storage_nvs.h"

#include <stdlib.h>
#include <string.h>

#include "log.h"
ASTARTE_LOG_MODULE_DECLARE(astarte_kv_storage, CONFIG_ASTARTE_DEVICE_SDK_KV_STORAGE_LOG_LEVEL);

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

/** @brief NVS ID for the number of stored key-value pairs */
#define STORED_PAIRS_NVS_ID 0U
/** @brief Number of NVS entries required to hold a key-value pair */
#define NVS_ENTRIES_FOR_PAIR 3U

/************************************************
 *         Global functions definitions         *
 ***********************************************/

uint16_t kv_storage_nvs_get_pair_base_id(uint16_t pair_index)
{
    // 1 is the offset for the global counter (STORED_PAIRS_NVS_ID is 0)
    return 1U + (pair_index * NVS_ENTRIES_FOR_PAIR);
}

astarte_result_t kv_storage_nvs_get_pairs_number(struct nvs_fs *nvs_fs, uint16_t *count)
{
    uint16_t data = 0;
    int nvs_rc = nvs_read(nvs_fs, STORED_PAIRS_NVS_ID, &data, sizeof(data));
    if ((nvs_rc < 0) && (nvs_rc != -ENOENT)) {
        ASTARTE_LOG_ERR("NVS read error: %s (%d).", strerror(-nvs_rc), nvs_rc);
        return ASTARTE_RESULT_NVS_ERROR;
    }
    // If ENOENT (first run), count is 0
    *count = (nvs_rc == -ENOENT) ? 0 : data;
    return ASTARTE_RESULT_OK;
}

astarte_result_t kv_storage_nvs_set_pairs_number(struct nvs_fs *nvs_fs, uint16_t count)
{
    int nvs_rc = nvs_write(nvs_fs, STORED_PAIRS_NVS_ID, &count, sizeof(count));
    if (nvs_rc < 0) {
        ASTARTE_LOG_ERR("NVS write error: %s (%d).", strerror(-nvs_rc), nvs_rc);
        return ASTARTE_RESULT_NVS_ERROR;
    }
    return ASTARTE_RESULT_OK;
}

astarte_result_t kv_storage_nvs_read_entry(
    struct nvs_fs *nvs_fs, uint16_t entry_id, void *data, size_t *data_size)
{
    int nvs_rc = 0;
    if (!data) {
        nvs_rc = nvs_read(nvs_fs, entry_id, NULL, 0);
    } else {
        nvs_rc = nvs_read(nvs_fs, entry_id, data, *data_size);
    }

    if (nvs_rc < 0) {
        ASTARTE_LOG_ERR("NVS read error: %s (%d).", strerror(-nvs_rc), nvs_rc);
        return ASTARTE_RESULT_NVS_ERROR;
    }

    if (data && (nvs_rc > *data_size)) {
        ASTARTE_LOG_ERR("Stored data is too large for provided buffer.");
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    *data_size = nvs_rc;
    return ASTARTE_RESULT_OK;
}

astarte_result_t kv_storage_nvs_read_entry_alloc(
    struct nvs_fs *nvs_fs, uint16_t entry_id, void **data, size_t *data_size)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    size_t required_size = 0;

    // First call to get size
    ares = kv_storage_nvs_read_entry(nvs_fs, entry_id, NULL, &required_size);
    if (ares != ASTARTE_RESULT_OK) {
        return ares;
    }

    void *buff = calloc(required_size, sizeof(uint8_t));
    if (!buff) {
        return ASTARTE_RESULT_OUT_OF_MEMORY;
    }

    // Second call to get data
    ares = kv_storage_nvs_read_entry(nvs_fs, entry_id, buff, &required_size);
    if (ares != ASTARTE_RESULT_OK) {
        free(buff);
        return ares;
    }

    *data = buff;
    *data_size = required_size;
    return ASTARTE_RESULT_OK;
}

astarte_result_t kv_storage_nvs_write_pair(struct nvs_fs *nvs_fs, uint16_t base_id,
    const char *namespace, const char *key, const void *value, size_t value_size)
{
    ssize_t nvs_rc = 0;
    uint16_t id_nsp = base_id + NVS_ID_OFFSET_NAMESPACE;
    uint16_t id_key = base_id + NVS_ID_OFFSET_KEY;
    uint16_t id_val = base_id + NVS_ID_OFFSET_VALUE;

    nvs_rc = nvs_write(nvs_fs, id_nsp, namespace, strlen(namespace) + 1);
    if (nvs_rc < 0) {
        ASTARTE_LOG_ERR("NVS write error: %s (%d).", strerror(-nvs_rc), nvs_rc);
        return ASTARTE_RESULT_NVS_ERROR;
    }
    nvs_rc = nvs_write(nvs_fs, id_key, key, strlen(key) + 1);
    if (nvs_rc < 0) {
        ASTARTE_LOG_ERR("NVS write error: %s (%d).", strerror(-nvs_rc), nvs_rc);
        return ASTARTE_RESULT_NVS_ERROR;
    }
    nvs_rc = nvs_write(nvs_fs, id_val, value, value_size);
    if (nvs_rc < 0) {
        ASTARTE_LOG_ERR("NVS write error: %s (%d).", strerror(-nvs_rc), nvs_rc);
        return ASTARTE_RESULT_NVS_ERROR;
    }

    return ASTARTE_RESULT_OK;
}

astarte_result_t kv_storage_nvs_relocate_pair(
    struct nvs_fs *nvs_fs, uint16_t dst_base_id, uint16_t src_base_id)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    void *nsp = NULL;
    void *key = NULL;
    void *val = NULL;

    // Fetch all components from source
    size_t nsp_len = 0;
    ares = kv_storage_nvs_read_entry_alloc(
        nvs_fs, src_base_id + NVS_ID_OFFSET_NAMESPACE, &nsp, &nsp_len);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed fetching entry namespace %s.", astarte_result_to_name(ares));
        goto exit;
    }
    size_t key_len = 0;
    ares = kv_storage_nvs_read_entry_alloc(nvs_fs, src_base_id + NVS_ID_OFFSET_KEY, &key, &key_len);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed fetching entry key %s.", astarte_result_to_name(ares));
        goto exit;
    }
    size_t val_len = 0;
    ares = kv_storage_nvs_read_entry_alloc(
        nvs_fs, src_base_id + NVS_ID_OFFSET_VALUE, &val, &val_len);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed fetching entry value %s.", astarte_result_to_name(ares));
        goto exit;
    }

    // Write to destination
    ares = kv_storage_nvs_write_pair(nvs_fs, dst_base_id, (char *) nsp, (char *) key, val, val_len);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed relocating entry %s.", astarte_result_to_name(ares));
    }

exit:
    free(nsp);
    free(key);
    free(val);
    return ares;
}

// This function is still understandable
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
astarte_result_t kv_storage_nvs_remove_duplicates(struct nvs_fs *nvs_fs)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    uint16_t pairs_number = 0;

    ares = kv_storage_nvs_get_pairs_number(nvs_fs, &pairs_number);
    if ((ares != ASTARTE_RESULT_OK) || pairs_number < 2) {
        // Error, or if we have 0 or 1 items no chance of duplication
        return ares;
    }

    uint16_t last_idx = pairs_number - 1;
    uint16_t last_base_id = kv_storage_nvs_get_pair_base_id(last_idx);
    uint16_t last_key_id = last_base_id + NVS_ID_OFFSET_KEY;
    uint16_t last_nsp_id = last_base_id + NVS_ID_OFFSET_NAMESPACE;

    char *last_key = NULL;
    char *last_nsp = NULL;
    char *curr_key = NULL;
    char *curr_nsp = NULL;
    size_t last_key_len = 0;
    size_t last_nsp_len = 0;

    // Fetch the last pair (the potential duplicate)
    ares = kv_storage_nvs_read_entry_alloc(nvs_fs, last_key_id, (void **) &last_key, &last_key_len);
    if (ares != ASTARTE_RESULT_OK) {
        goto exit;
    }
    ares = kv_storage_nvs_read_entry_alloc(nvs_fs, last_nsp_id, (void **) &last_nsp, &last_nsp_len);
    if (ares != ASTARTE_RESULT_OK) {
        goto exit;
    }

    // Pre allocate memory where to store the key and namespace of the other items
    curr_key = calloc(last_key_len, sizeof(char));
    curr_nsp = calloc(last_nsp_len, sizeof(char));
    if (!curr_key || !curr_nsp) {
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto exit;
    }

    // Scan previous items
    for (uint16_t curr_idx = 0; curr_idx < last_idx; curr_idx++) {
        uint16_t curr_base_id = kv_storage_nvs_get_pair_base_id(curr_idx);
        uint16_t curr_key_id = curr_base_id + NVS_ID_OFFSET_KEY;
        uint16_t curr_nsp_id = curr_base_id + NVS_ID_OFFSET_NAMESPACE;
        size_t size = 0;

        // Check if the current pair key matches first the length then the content of the last
        ares = kv_storage_nvs_read_entry(nvs_fs, curr_key_id, NULL, &size);
        if (ares != ASTARTE_RESULT_OK) {
            goto exit;
        }

        if (size != last_key_len) {
            continue;
        }

        ares = kv_storage_nvs_read_entry(nvs_fs, curr_key_id, curr_key, &size);
        if (ares != ASTARTE_RESULT_OK) {
            goto exit;
        }

        if (strcmp(last_key, curr_key) != 0) {
            continue;
        }

        // Check if the current pair namespace matches first the length then the content of the last
        ares = kv_storage_nvs_read_entry(nvs_fs, curr_nsp_id, NULL, &size);
        if (ares != ASTARTE_RESULT_OK) {
            goto exit;
        }

        if (size != last_nsp_len) {
            continue;
        }

        ares = kv_storage_nvs_read_entry(nvs_fs, curr_nsp_id, curr_nsp, &size);
        if (ares != ASTARTE_RESULT_OK) {
            goto exit;
        }

        if (strcmp(last_nsp, curr_nsp) == 0) {
            ASTARTE_LOG_WRN("Recovering duplicate pair IDs %d and %d.", curr_base_id, last_base_id);
            pairs_number--;
            ares = kv_storage_nvs_set_pairs_number(nvs_fs, pairs_number);
            goto exit;
        }
    }

exit:
    free(last_key);
    free(last_nsp);
    free(curr_key);
    free(curr_nsp);
    return ares;
}
