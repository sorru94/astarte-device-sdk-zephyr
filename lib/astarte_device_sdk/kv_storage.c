/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "kv_storage.h"
#include "kv_storage_nvs.h"

#include <stdlib.h>
#include <string.h>

#include <zephyr/sys/mutex.h>

#include "log.h"

ASTARTE_LOG_MODULE_REGISTER(astarte_kv_storage, CONFIG_ASTARTE_DEVICE_SDK_KV_STORAGE_LOG_LEVEL);

/************************************************
 * Defines, constants and typedef        *
 ***********************************************/

// This mutex will be shared by all instances of this driver.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static SYS_MUTEX_DEFINE(astarte_kv_storage_mutex);

/************************************************
 *         Static functions declaration         *
 ***********************************************/

/**
 * @brief Find the NVS base ID for the a key-value pair.
 *
 * @param[inout] nvs_fs NVS file system to use.
 * @param[in] namespace Namespace for the key-value pair.
 * @param[in] stored_pairs Number of total stored key-value pairs.
 * @param[in] key Key to use for the search.
 * @param[out] base_id Found base ID for the key-value pair.
 * @return ASTARTE_RESULT_OK if found, ASTARTE_RESULT_NOT_FOUND if not, or error code.
 */
static astarte_result_t find_pair_base_id(struct nvs_fs *nvs_fs, const char *namespace,
    uint16_t stored_pairs, const char *key, uint16_t *base_id);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_kv_storage_new(
    astarte_kv_storage_cfg_t config, const char *namespace, astarte_kv_storage_t *kv_storage)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    char *namespace_cpy = NULL;
    size_t namespace_cpy_size = 0U;

    namespace_cpy_size = strlen(namespace) + 1;
    namespace_cpy = calloc(namespace_cpy_size, sizeof(char));
    if (!namespace_cpy) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto error;
    }
    strncpy(namespace_cpy, namespace, namespace_cpy_size);

    kv_storage->flash_device = config.flash_device;
    kv_storage->flash_offset = config.flash_offset;
    kv_storage->flash_sector_size = config.flash_sector_size;
    kv_storage->flash_sector_count = config.flash_sector_count;
    kv_storage->namespace = namespace_cpy;

    memset(&kv_storage->nvs_fs, 0, sizeof(kv_storage->nvs_fs));
    kv_storage->nvs_fs.flash_device = kv_storage->flash_device;
    kv_storage->nvs_fs.offset = kv_storage->flash_offset;
    kv_storage->nvs_fs.sector_size = kv_storage->flash_sector_size;
    kv_storage->nvs_fs.sector_count = kv_storage->flash_sector_count;

    int nvs_rc = nvs_mount(&kv_storage->nvs_fs);
    if (nvs_rc) {
        ASTARTE_LOG_ERR("NVS mount error: %s (%d).", strerror(-nvs_rc), nvs_rc);
        ares = ASTARTE_RESULT_NVS_ERROR;
        goto error;
    }

    ASTARTE_LOG_DBG("Checking for interrupted delete operations...");
    ares = kv_storage_nvs_remove_duplicates(&kv_storage->nvs_fs);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_WRN("Recovery check failed: %s. Storage might contain duplicates.",
            astarte_result_to_name(ares));
    }

    return ASTARTE_RESULT_OK;

error:
    free(namespace_cpy);
    kv_storage->namespace = NULL;

    return ares;
}

void astarte_kv_storage_destroy(astarte_kv_storage_t kv_storage)
{
    free(kv_storage.namespace);
}

astarte_result_t astarte_kv_storage_insert(
    astarte_kv_storage_t *kv_storage, const char *key, const void *value, size_t value_size)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    uint16_t stored_pairs = 0;
    uint16_t base_id = 0U;
    bool append_at_end = false;

    int mutex_rc = sys_mutex_lock(&astarte_kv_storage_mutex, K_FOREVER);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex lock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    ares = kv_storage_nvs_get_pairs_number(&kv_storage->nvs_fs, &stored_pairs);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Get total stored pairs failed %s.", astarte_result_to_name(ares));
        goto exit;
    }

    // Check if key is already present in storage
    ares = find_pair_base_id(
        &kv_storage->nvs_fs, kv_storage->namespace, stored_pairs, key, &base_id);
    if (ares == ASTARTE_RESULT_NOT_FOUND) {
        append_at_end = true;
        // Calculate new ID for the end of the list
        size_t unbound_base_id = kv_storage_nvs_get_pair_base_id(stored_pairs);
        if (unbound_base_id + NVS_ID_OFFSET_VALUE >= UINT16_MAX) {
            ares = ASTARTE_RESULT_KV_STORAGE_FULL;
            goto exit;
        }
        base_id = (uint16_t) unbound_base_id;
    } else if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Check for old values failed %s.", astarte_result_to_name(ares));
        goto exit;
    }

    ares = kv_storage_nvs_write_pair(
        &kv_storage->nvs_fs, base_id, kv_storage->namespace, key, value, value_size);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Insert failed %s.", astarte_result_to_name(ares));
        goto exit;
    }

    if (append_at_end) {
        stored_pairs++;
        ares = kv_storage_nvs_set_pairs_number(&kv_storage->nvs_fs, stored_pairs);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Update total stored pairs failed %s.", astarte_result_to_name(ares));
            goto exit;
        }
    }

exit:
    mutex_rc = sys_mutex_unlock(&astarte_kv_storage_mutex);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex unlock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    return ares;
}

astarte_result_t astarte_kv_storage_find(
    astarte_kv_storage_t *kv_storage, const char *key, void *value, size_t *value_size)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    uint16_t stored_pairs = 0;
    uint16_t base_id = 0U;

    int mutex_rc = sys_mutex_lock(&astarte_kv_storage_mutex, K_FOREVER);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex lock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    ares = kv_storage_nvs_get_pairs_number(&kv_storage->nvs_fs, &stored_pairs);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Get total stored pairs failed %s.", astarte_result_to_name(ares));
        goto exit;
    }

    ares = find_pair_base_id(
        &kv_storage->nvs_fs, kv_storage->namespace, stored_pairs, key, &base_id);
    if (ares != ASTARTE_RESULT_OK) {
        goto exit;
    }

    ares = kv_storage_nvs_read_entry(
        &kv_storage->nvs_fs, base_id + NVS_ID_OFFSET_VALUE, value, value_size);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Get value of key-value storage failed %s.", astarte_result_to_name(ares));
    }

exit:
    mutex_rc = sys_mutex_unlock(&astarte_kv_storage_mutex);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex unlock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    return ares;
}

astarte_result_t astarte_kv_storage_delete(astarte_kv_storage_t *kv_storage, const char *key)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    uint16_t stored_pairs = 0U;
    uint16_t base_id = 0U;

    int mutex_rc = sys_mutex_lock(&astarte_kv_storage_mutex, K_FOREVER);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex lock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    ares = kv_storage_nvs_get_pairs_number(&kv_storage->nvs_fs, &stored_pairs);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Get total stored pairs failed %s.", astarte_result_to_name(ares));
        goto exit;
    }

    ares = find_pair_base_id(
        &kv_storage->nvs_fs, kv_storage->namespace, stored_pairs, key, &base_id);
    if (ares != ASTARTE_RESULT_OK) {
        goto exit;
    }

    // Determine the ID of the last element in the array
    uint16_t last_base_id = kv_storage_nvs_get_pair_base_id(stored_pairs - 1);

    // If the item to delete is NOT the last one, swap the last one into this slot
    if (base_id != last_base_id) {
        ares = kv_storage_nvs_relocate_pair(&kv_storage->nvs_fs, base_id, last_base_id);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Relocation (swap) failed %s.", astarte_result_to_name(ares));
            goto exit;
        }
    }

    stored_pairs--;
    ares = kv_storage_nvs_set_pairs_number(&kv_storage->nvs_fs, stored_pairs);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Update total stored pairs failed %s.", astarte_result_to_name(ares));
        goto exit;
    }

exit:
    mutex_rc = sys_mutex_unlock(&astarte_kv_storage_mutex);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex unlock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    return ares;
}

astarte_result_t astarte_kv_storage_iterator_init(
    astarte_kv_storage_t *kv_storage, astarte_kv_storage_iter_t *iter)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    uint16_t stored_pairs = 0;
    void *namespace = NULL;
    size_t namespace_size = 0;

    int mutex_rc = sys_mutex_lock(&astarte_kv_storage_mutex, K_FOREVER);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex lock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    ares = kv_storage_nvs_get_pairs_number(&kv_storage->nvs_fs, &stored_pairs);
    if (ares != ASTARTE_RESULT_OK) {
        goto exit;
    }

    // Iterate backwards to find the first match
    for (int32_t pair_number = stored_pairs - 1; pair_number >= 0; pair_number--) {
        uint16_t base_id = kv_storage_nvs_get_pair_base_id(pair_number);
        uint16_t namespace_id = base_id + NVS_ID_OFFSET_NAMESPACE;

        ares = kv_storage_nvs_read_entry_alloc(
            &kv_storage->nvs_fs, namespace_id, &namespace, &namespace_size);
        if (ares != ASTARTE_RESULT_OK) {
            goto exit;
        }

        if (strcmp(kv_storage->namespace, (char *) namespace) == 0) {
            iter->kv_storage = kv_storage;
            iter->current_pair = pair_number;
            ares = ASTARTE_RESULT_OK;
            goto exit;
        }

        free(namespace);
        namespace = NULL;
    }

    ares = ASTARTE_RESULT_NOT_FOUND;

exit:
    mutex_rc = sys_mutex_unlock(&astarte_kv_storage_mutex);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex unlock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    free(namespace);
    return ares;
}

astarte_result_t astarte_kv_storage_iterator_next(astarte_kv_storage_iter_t *iter)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    void *namespace = NULL;
    size_t namespace_size = 0;

    int mutex_rc = sys_mutex_lock(&astarte_kv_storage_mutex, K_FOREVER);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex lock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    // Go backwards from current_pair -1 looking for pairs in the same namespace
    for (int32_t pair_number = iter->current_pair - 1; pair_number >= 0; pair_number--) {
        uint16_t base_id = kv_storage_nvs_get_pair_base_id(pair_number);
        uint16_t namespace_id = base_id + NVS_ID_OFFSET_NAMESPACE;

        ares = kv_storage_nvs_read_entry_alloc(
            &iter->kv_storage->nvs_fs, namespace_id, &namespace, &namespace_size);
        if (ares != ASTARTE_RESULT_OK) {
            goto exit;
        }

        if (strcmp(iter->kv_storage->namespace, (char *) namespace) == 0) {
            iter->current_pair = pair_number;
            ares = ASTARTE_RESULT_OK;
            goto exit;
        }

        free(namespace);
        namespace = NULL;
    }

    ares = ASTARTE_RESULT_NOT_FOUND;

exit:
    mutex_rc = sys_mutex_unlock(&astarte_kv_storage_mutex);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex unlock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    free(namespace);
    return ares;
}

astarte_result_t astarte_kv_storage_iterator_get(
    astarte_kv_storage_iter_t *iter, void *key, size_t *key_size)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    int mutex_rc = sys_mutex_lock(&astarte_kv_storage_mutex, K_FOREVER);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex lock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    uint16_t key_id = kv_storage_nvs_get_pair_base_id(iter->current_pair) + NVS_ID_OFFSET_KEY;

    ares = kv_storage_nvs_read_entry(&iter->kv_storage->nvs_fs, key_id, key, key_size);

    mutex_rc = sys_mutex_unlock(&astarte_kv_storage_mutex);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex unlock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    return ares;
}

/************************************************
 * Static functions definitions         *
 ***********************************************/

static astarte_result_t find_pair_base_id(struct nvs_fs *nvs_fs, const char *namespace,
    uint16_t stored_pairs, const char *key, uint16_t *base_id)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    void *tmp_namespace = NULL;
    void *tmp_key = NULL;
    size_t size = 0;
    bool found = false;
    uint16_t pair_number = 0U;

    for (; pair_number < stored_pairs; pair_number++) {
        uint16_t current_base = kv_storage_nvs_get_pair_base_id(pair_number);

        // Check namespace
        ares = kv_storage_nvs_read_entry_alloc(
            nvs_fs, current_base + NVS_ID_OFFSET_NAMESPACE, &tmp_namespace, &size);
        if (ares != ASTARTE_RESULT_OK) {
            goto exit;
        }

        if (strcmp((char *) tmp_namespace, namespace) == 0) {

            ares = kv_storage_nvs_read_entry_alloc(
                nvs_fs, current_base + NVS_ID_OFFSET_KEY, &tmp_key, &size);
            if (ares != ASTARTE_RESULT_OK) {
                goto exit;
            }

            if (strcmp((char *) tmp_key, key) == 0) {
                found = true;
                break;
            }
        }

        free(tmp_namespace);
        tmp_namespace = NULL;
        free(tmp_key);
        tmp_key = NULL;
    }

    if (found) {
        *base_id = kv_storage_nvs_get_pair_base_id(pair_number);
    }

exit:

    free(tmp_namespace);
    tmp_namespace = NULL;
    free(tmp_key);
    tmp_key = NULL;

    return (ares != ASTARTE_RESULT_OK) ? ares : ASTARTE_RESULT_NOT_FOUND;
}
