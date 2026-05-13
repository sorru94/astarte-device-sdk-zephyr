/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "key_value/core.h"

#include <stdlib.h>
#include <string.h>

#include <zephyr/drivers/flash.h>
#include <zephyr/kernel.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/mutex.h>
#include <zephyr/version.h>

#if (KERNEL_VERSION_MAJOR >= 4) && (KERNEL_VERSION_MINOR >= 4)
#include <zephyr/kvss/nvs.h>
#else
#include <zephyr/fs/nvs.h>
#endif

#include "key_value/entry.h"
#include "key_value/entry_delete.h"
#include "key_value/entry_intent.h"
#include "log.h"

ASTARTE_LOG_MODULE_REGISTER(astarte_key_value, CONFIG_ASTARTE_DEVICE_SDK_KEY_VALUE_LOG_LEVEL);

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

// This mutex will be shared by all instances of this driver.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static SYS_MUTEX_DEFINE(astarte_key_value_mutex);

/************************************************
 *         Static functions declaration         *
 ***********************************************/

static astarte_result_t find_next_matching_id(
    astarte_key_value_iter_t *iter, uint16_t *next_id, bool *has_next);

/************************************************
 *         Global functions definitions         *
 ***********************************************/
astarte_result_t astarte_key_value_open(astarte_key_value_cfg_t config, struct nvs_fs *nvs_fs)
{
    struct flash_pages_info fp_info = { 0 };

    if (!device_is_ready(config.flash_device)) {
        ASTARTE_LOG_ERR("Flash device %s not ready.", config.flash_device->name);
        return ASTARTE_RESULT_DEVICE_NOT_READY;
    }

    int flash_rc = flash_get_page_info_by_offs(config.flash_device, config.flash_offset, &fp_info);
    if (flash_rc) {
        ASTARTE_LOG_ERR("Unable to get page info: %d.", flash_rc);
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }

    uint16_t flash_sector_count = (uint16_t) (config.flash_partition_size / fp_info.size);

    memset(nvs_fs, 0, sizeof(struct nvs_fs));
    nvs_fs->flash_device = config.flash_device;
    nvs_fs->offset = config.flash_offset;
    nvs_fs->sector_size = fp_info.size;
    nvs_fs->sector_count = flash_sector_count;

    int nvs_rc = nvs_mount(nvs_fs);
    if (nvs_rc) {
        ASTARTE_LOG_ERR("NVS mount error: %s (%d).", strerror(-nvs_rc), nvs_rc);
        return ASTARTE_RESULT_NVS_ERROR;
    }

    astarte_key_value_version_t current_version = { .major = ASTARTE_KEY_VALUE_FORMAT_VERSION_MAJOR,
        .minor = ASTARTE_KEY_VALUE_FORMAT_VERSION_MINOR,
        .patch = ASTARTE_KEY_VALUE_FORMAT_VERSION_PATCH };
    astarte_key_value_version_t stored_version = { 0 };

    ssize_t ver_rc = nvs_read(
        nvs_fs, ASTARTE_KEY_VALUE_ENTRY_VERSION_ID, &stored_version, sizeof(stored_version));

    if (ver_rc == -ENOENT) {
        // Version not found, assuming empty NVS. Write the current format version.
        ver_rc = nvs_write(
            nvs_fs, ASTARTE_KEY_VALUE_ENTRY_VERSION_ID, &current_version, sizeof(current_version));
        if (ver_rc < 0) {
            ASTARTE_LOG_ERR("Failed to initialize NVS format version: %d", (int) ver_rc);
            return ASTARTE_RESULT_NVS_ERROR;
        }
        ASTARTE_LOG_INF("Initialized NVS with format version %d.%d.%d", current_version.major,
            current_version.minor, current_version.patch);
    } else if (ver_rc < 0) {
        ASTARTE_LOG_ERR("Failed to read NVS format version: %d", (int) ver_rc);
        return ASTARTE_RESULT_NVS_ERROR;
    } else if (stored_version.major != current_version.major
        || stored_version.minor != current_version.minor
        || stored_version.patch != current_version.patch) {

        // A version mismatch indicates the partition was written by an older/newer driver
        ASTARTE_LOG_ERR("NVS format version mismatch! Expected %d.%d.%d, found %d.%d.%d. Migration "
                        "or wipe required.",
            current_version.major, current_version.minor, current_version.patch,
            stored_version.major, stored_version.minor, stored_version.patch);
        return ASTARTE_RESULT_KEY_VALUE_INCOMPATIBLE_VERSION;
    }

    astarte_result_t ares = astarte_key_value_entry_intent_resolve(nvs_fs);
    // Explicitly handle the fatal corruption scenario
    if (ares == ASTARTE_RESULT_KEY_VALUE_RECOVERY_FAILED) {
        ASTARTE_LOG_ERR("Unrecoverable NVS corruption detected during mount check.");
        return ares;
    }
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed to resolve leftover intents during initialization: %s",
            astarte_result_to_name(ares));
        return ares;
    }

    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_key_value_new(
    struct nvs_fs *nvs_fs, const char *namespace, astarte_key_value_t *kv_storage)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    char *namespace_cpy = NULL;
    size_t namespace_cpy_size = strlen(namespace) + 1;

    namespace_cpy = calloc(namespace_cpy_size, sizeof(char));
    if (!namespace_cpy) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto error;
    }
    strncpy(namespace_cpy, namespace, namespace_cpy_size);

    kv_storage->namespace = namespace_cpy;
    kv_storage->nvs_fs = nvs_fs;

    return ASTARTE_RESULT_OK;

error:
    free(namespace_cpy);
    kv_storage->namespace = NULL;
    return ares;
}

void astarte_key_value_destroy(astarte_key_value_t *kv_storage)
{
    free(kv_storage->namespace);
    kv_storage->namespace = NULL;
}

astarte_result_t astarte_key_value_insert(
    astarte_key_value_t *kv_storage, const char *key, const void *value, size_t value_size)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    uint16_t entry_id = 0;

    int mutex_rc = sys_mutex_lock(&astarte_key_value_mutex, K_FOREVER);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex lock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    ares = astarte_key_value_entry_intent_resolve(kv_storage->nvs_fs);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Pre-operation integrity check failed: %s", astarte_result_to_name(ares));
        goto exit;
    }

    ares = astarte_key_value_entry_find_or_alloc(
        kv_storage->nvs_fs, kv_storage->namespace, key, &entry_id, true);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Key finding/allocation failed %s.", astarte_result_to_name(ares));
        goto exit;
    }

    ares = astarte_key_value_entry_write(
        kv_storage->nvs_fs, entry_id, kv_storage->namespace, key, value, value_size);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Insert failed %s.", astarte_result_to_name(ares));
    }

exit:
    mutex_rc = sys_mutex_unlock(&astarte_key_value_mutex);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex unlock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    return ares;
}

astarte_result_t astarte_key_value_find(
    astarte_key_value_t *kv_storage, const char *key, void *value, size_t *value_size)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    uint16_t entry_id = 0;

    int mutex_rc = sys_mutex_lock(&astarte_key_value_mutex, K_FOREVER);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex lock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    ares = astarte_key_value_entry_intent_resolve(kv_storage->nvs_fs);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Pre-operation integrity check failed: %s", astarte_result_to_name(ares));
        goto exit;
    }

    ares = astarte_key_value_entry_find_or_alloc(
        kv_storage->nvs_fs, kv_storage->namespace, key, &entry_id, false);
    if (ares != ASTARTE_RESULT_OK) {
        // No error logs as this could be a not found case, which is not necessarily an error
        goto exit;
    }

    ares = astarte_key_value_entry_read_value(kv_storage->nvs_fs, entry_id, value, value_size);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Get value of key-value storage failed %s.", astarte_result_to_name(ares));
    }

exit:
    mutex_rc = sys_mutex_unlock(&astarte_key_value_mutex);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex unlock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    return ares;
}

astarte_result_t astarte_key_value_delete(astarte_key_value_t *kv_storage, const char *key)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    uint16_t entry_id = 0;

    int mutex_rc = sys_mutex_lock(&astarte_key_value_mutex, K_FOREVER);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex lock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    ares = astarte_key_value_entry_intent_resolve(kv_storage->nvs_fs);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Pre-operation integrity check failed: %s", astarte_result_to_name(ares));
        goto exit;
    }

    ares = astarte_key_value_entry_find_or_alloc(
        kv_storage->nvs_fs, kv_storage->namespace, key, &entry_id, false);
    if (ares != ASTARTE_RESULT_OK) {
        // No error logs as this could be a not found case, which is not necessarily an error
        goto exit;
    }

    ares = astarte_key_value_entry_delete(kv_storage->nvs_fs, entry_id);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("NVS Delete Error: %s.", astarte_result_to_name(ares));
    }

exit:
    mutex_rc = sys_mutex_unlock(&astarte_key_value_mutex);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex unlock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    return ares;
}

astarte_result_t astarte_key_value_iterator_init(
    astarte_key_value_t *kv_storage, astarte_key_value_iter_t *iter)
{
    // The maximum ID is a reserved starting point for the linked list iterator
    iter->kv_storage = kv_storage;
    iter->current_id = UINT16_MAX;
    iter->prev_id = UINT16_MAX;
    return astarte_key_value_iterator_next(iter);
}

astarte_result_t astarte_key_value_iterator_next(astarte_key_value_iter_t *iter)
{
    astarte_result_t ares = ASTARTE_RESULT_NOT_FOUND;
    bool matches = false;

    int mutex_rc = sys_mutex_lock(&astarte_key_value_mutex, K_FOREVER);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex lock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    ares = astarte_key_value_entry_intent_resolve(iter->kv_storage->nvs_fs);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Pre-operation integrity check failed: %s", astarte_result_to_name(ares));
        goto exit;
    }

    uint16_t curr_id = iter->current_id;

    while (true) {
        uint16_t next_id = UINT16_MAX;
        ares = astarte_key_value_entry_get_next_id(iter->kv_storage->nvs_fs, curr_id, &next_id);

        if (ares != ASTARTE_RESULT_OK || next_id == UINT16_MAX) {
            ASTARTE_LOG_DBG("Iterator reached the end.");
            ares = ASTARTE_RESULT_NOT_FOUND;
            break;
        }

        ares = astarte_key_value_entry_check_namespace(
            iter->kv_storage->nvs_fs, next_id, iter->kv_storage->namespace, &matches);

        if (ares == ASTARTE_RESULT_OK && matches) {
            // Cache the previous entry ID for deletion operations
            iter->prev_id = curr_id;
            iter->current_id = next_id;
            goto exit;
        }

        curr_id = next_id;
    }

exit:
    mutex_rc = sys_mutex_unlock(&astarte_key_value_mutex);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex unlock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    return ares;
}

astarte_result_t astarte_key_value_iterator_get(
    astarte_key_value_iter_t *iter, void *key, size_t *key_size)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    int mutex_rc = sys_mutex_lock(&astarte_key_value_mutex, K_FOREVER);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex lock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    ares = astarte_key_value_entry_intent_resolve(iter->kv_storage->nvs_fs);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Pre-operation integrity check failed: %s", astarte_result_to_name(ares));
        goto exit;
    }

    ares = astarte_key_value_entry_read_key(
        iter->kv_storage->nvs_fs, iter->current_id, (char *) key, key_size);

exit:

    mutex_rc = sys_mutex_unlock(&astarte_key_value_mutex);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex unlock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    return ares;
}

astarte_result_t astarte_key_value_iterator_delete(astarte_key_value_iter_t *iter)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    char *next_key = NULL;
    size_t next_key_size = 0;

    if (!iter || iter->current_id == 0) {
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    int mutex_rc = sys_mutex_lock(&astarte_key_value_mutex, K_FOREVER);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex lock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    ares = astarte_key_value_entry_intent_resolve(iter->kv_storage->nvs_fs);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Pre-operation integrity check failed: %s", astarte_result_to_name(ares));
        goto exit;
    }

    // Peek ahead to find the next matching element in the same namespace
    uint16_t next_matching_id = UINT16_MAX;
    bool has_next = false;
    ares = find_next_matching_id(iter, &next_matching_id, &has_next);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("List advancement failure: %s", astarte_result_to_name(ares));
        goto exit;
    }

    // Get the key of the next matching element so we can re-find it if it shifts
    if (has_next) {
        ares = astarte_key_value_entry_read_key(
            iter->kv_storage->nvs_fs, next_matching_id, NULL, &next_key_size);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Key size reading failure: %s", astarte_result_to_name(ares));
            goto exit;
        }
        next_key = calloc(next_key_size, sizeof(char));
        if (!next_key) {
            ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
            ares = ASTARTE_RESULT_OUT_OF_MEMORY;
            goto exit;
        }
        ares = astarte_key_value_entry_read_key(
            iter->kv_storage->nvs_fs, next_matching_id, next_key, &next_key_size);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Key reading failure: %s", astarte_result_to_name(ares));
            goto exit;
        }
    }

    // Physically delete the current entry and heal the NVS global linked-list
    ares = astarte_key_value_entry_delete(iter->kv_storage->nvs_fs, iter->current_id);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Iterator delete error: %s.", astarte_result_to_name(ares));
        goto exit;
    }

    // We just deleted the very last element in this namespace.
    if (!has_next) {
        iter->current_id = UINT16_MAX;
        goto exit;
    }

    // Find the post-shift valid NVS ID of the next matching element
    uint16_t valid_next_matching_id = UINT16_MAX;
    ares = astarte_key_value_entry_find_or_alloc(iter->kv_storage->nvs_fs,
        iter->kv_storage->namespace, next_key, &valid_next_matching_id, false);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Could not find post-shift entry: %s.", astarte_result_to_name(ares));
        goto exit;
    }
    // Find the post-shift NVS ID of the next element previous element
    uint16_t new_prev_id = UINT16_MAX;
    ares = astarte_key_value_entry_get_prev_id(
        iter->kv_storage->nvs_fs, valid_next_matching_id, &new_prev_id);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Could not find post-shift prev id: %s.", astarte_result_to_name(ares));
        goto exit;
    }
    iter->current_id = new_prev_id;

exit:

    free(next_key);

    mutex_rc = sys_mutex_unlock(&astarte_key_value_mutex);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex unlock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    return ares;
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static astarte_result_t find_next_matching_id(
    astarte_key_value_iter_t *iter, uint16_t *next_id, bool *has_next)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    uint16_t peek_curr_id = iter->current_id;
    *has_next = false;
    *next_id = UINT16_MAX;

    while (true) {
        uint16_t idx = 0;
        ares = astarte_key_value_entry_get_next_id(iter->kv_storage->nvs_fs, peek_curr_id, &idx);
        if (ares != ASTARTE_RESULT_OK || idx == UINT16_MAX) {
            break;
        }

        bool matches = false;
        ares = astarte_key_value_entry_check_namespace(
            iter->kv_storage->nvs_fs, idx, iter->kv_storage->namespace, &matches);
        if (ares != ASTARTE_RESULT_OK) {
            break;
        }
        if (matches) {
            *next_id = idx;
            *has_next = true;
            break;
        }

        peek_curr_id = idx;
    }

    return ares;
}
