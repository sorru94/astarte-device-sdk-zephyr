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
#include <zephyr/sys/util.h>
#include <zephyr/version.h>

#if KERNEL_VERSION_NUMBER >= ZEPHYR_VERSION(4, 4, 0)
#include <zephyr/kvss/zms.h>
#else
#include <zephyr/fs/zms.h>
#endif

#include "alloc.h"
#include "key_value/entry.h"
#include "key_value/entry_delete.h"
#include "key_value/entry_intent.h"
#include "key_value/mutex.h"
#include "log.h"

ASTARTE_LOG_MODULE_REGISTER(astarte_key_value, CONFIG_ASTARTE_DEVICE_SDK_KEY_VALUE_LOG_LEVEL);

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

#define FULL_KEY(alternate, key) ((alternate) ? ((1U << 16U) + (uint32_t) (key)) : (uint32_t) (key))

ASTARTE_SCOPE_DEFER_DEFINE(astarte_key_value_mutex_unlock);

static inline void free_char_ptr(char **ptr)
{
    if (ptr && *ptr) {
        astarte_free(*ptr);
        *ptr = NULL;
    }
}

ASTARTE_SCOPE_DEFER_DEFINE(free_char_ptr, char **);

/************************************************
 *         Static functions declaration         *
 ***********************************************/

static astarte_result_t find_next_matching_id(
    astarte_key_value_iter_t *iter, uint32_t *next_id, bool *has_next);
static astarte_result_t read_next_key(
    astarte_key_value_iter_t *iter, uint32_t next_id, char **next_key);
static astarte_result_t heal_iterator_post_delete(
    astarte_key_value_iter_t *iter, const char *next_key);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_key_value_open(astarte_key_value_cfg_t config, struct zms_fs *zms_fs)
{
    struct flash_pages_info fp_info = { 0 };

    if (!device_is_ready(config.flash_device)) {
        ASTARTE_LOG_ERR("Flash device %s not ready.", config.flash_device->name);
        return ASTARTE_RESULT_DEVICE_NOT_READY;
    }

    int flash_rc = flash_get_page_info_by_offs(config.flash_device, config.flash_offset, &fp_info);
    if (flash_rc) {
        ASTARTE_LOG_ERR("Unable to get flash page info: %d.", flash_rc);
        return ASTARTE_RESULT_INVALID_CONFIGURATION;
    }

    uint16_t flash_sector_count = (uint16_t) (config.flash_partition_size / fp_info.size);

    memset(zms_fs, 0, sizeof(struct zms_fs));
    zms_fs->flash_device = config.flash_device;
    zms_fs->offset = config.flash_offset;
    zms_fs->sector_size = fp_info.size;
    zms_fs->sector_count = flash_sector_count;

#if KERNEL_VERSION_NUMBER >= ZEPHYR_VERSION(4, 4, 0)
    int zms_rc = zms_mount_force(zms_fs);
#else
    int zms_rc = zms_mount(zms_fs);
#endif
    if (zms_rc != 0) {
        ASTARTE_LOG_ERR("ZMS mount error: %s (%d).", strerror(-zms_rc), zms_rc);
        return ASTARTE_RESULT_ZMS_ERROR;
    }

    astarte_key_value_version_t current_version = { .major = ASTARTE_KEY_VALUE_FORMAT_VERSION_MAJOR,
        .minor = ASTARTE_KEY_VALUE_FORMAT_VERSION_MINOR,
        .patch = ASTARTE_KEY_VALUE_FORMAT_VERSION_PATCH };
    astarte_key_value_version_t stored_version = { 0 };

    ssize_t ver_rc = zms_read(
        zms_fs, ASTARTE_KEY_VALUE_ENTRY_VERSION_ID, &stored_version, sizeof(stored_version));

    if (ver_rc == -ENOENT) {
        // Version not found, assuming empty ZMS. Write the current format version.
        ver_rc = zms_write(
            zms_fs, ASTARTE_KEY_VALUE_ENTRY_VERSION_ID, &current_version, sizeof(current_version));
        if (ver_rc < 0) {
            ASTARTE_LOG_ERR("Failed to initialize ZMS format version: %d", (int) ver_rc);
            return ASTARTE_RESULT_ZMS_ERROR;
        }
        ASTARTE_LOG_INF("Initialized ZMS with format version %d.%d.%d", current_version.major,
            current_version.minor, current_version.patch);
    } else if (ver_rc != sizeof(current_version)) {
        ASTARTE_LOG_ERR("Failed to read ZMS format version: %d", (int) ver_rc);
        return ASTARTE_RESULT_ZMS_ERROR;
    } else if (stored_version.major != current_version.major
        || stored_version.minor != current_version.minor
        || stored_version.patch != current_version.patch) {

        // A version mismatch indicates the partition was written by an older/newer driver
        ASTARTE_LOG_ERR("ZMS format version mismatch! Expected %d.%d.%d, found %d.%d.%d. Migration "
                        "or wipe required.",
            current_version.major, current_version.minor, current_version.patch,
            stored_version.major, stored_version.minor, stored_version.patch);
        return ASTARTE_RESULT_KEY_VALUE_INCOMPATIBLE_VERSION;
    }

    astarte_result_t ares = astarte_key_value_entry_intent_resolve(zms_fs);
    // Explicitly handle the fatal corruption scenario
    if (ares == ASTARTE_RESULT_KEY_VALUE_RECOVERY_FAILED) {
        ASTARTE_LOG_ERR("Unrecoverable ZMS corruption detected during mount check.");
        return ares;
    }
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed to resolve leftover intents during initialization: %s",
            astarte_result_to_name(ares));
        return ares;
    }

    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_key_value_new(struct zms_fs *zms_fs, const char *namespace,
    uint8_t max_quota_pct, astarte_key_value_t *kv_storage)
{
    size_t namespace_cpy_size = strlen(namespace) + 1;

    scope_var(scoped_char, namespace_cpy)(namespace_cpy_size);
    if (!namespace_cpy) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        return ASTARTE_RESULT_OUT_OF_MEMORY;
    }
    strncpy(namespace_cpy, namespace, namespace_cpy_size);

    kv_storage->namespace = namespace_cpy;
    kv_storage->zms_fs = zms_fs;

    size_t total_partition_bytes = zms_fs->sector_size * zms_fs->sector_count;
    const size_t one_hundred_prc = 100;
    size_t calculated_max_bytes = (total_partition_bytes * max_quota_pct) / one_hundred_prc;
    kv_storage->max_quota_bytes = calculated_max_bytes;
    kv_storage->current_usage_bytes = 0;

    // If a quota is set, calculate current usage immediately
    if (calculated_max_bytes > 0) {
        astarte_result_t ares = astarte_key_value_mutex_lock();
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Failed to lock mutex");
            return ares;
        }
        scope_defer(astarte_key_value_mutex_unlock)();

        astarte_key_value_iter_t iter;
        astarte_result_t iter_res = astarte_key_value_iterator_init(kv_storage, &iter);
        while (iter_res == ASTARTE_RESULT_OK) {
            size_t entry_size = 0;
            astarte_result_t read_res = astarte_key_value_entry_read_value(
                kv_storage->zms_fs, iter.current_id, NULL, &entry_size);
            if (read_res == ASTARTE_RESULT_OK) {
                kv_storage->current_usage_bytes += entry_size;
            } else {
                ASTARTE_LOG_WRN("Failed to read size for ID %d during quota init", iter.current_id);
            }

            iter_res = astarte_key_value_iterator_next(&iter);
        }
    }

    // Leave the memory intact for kv_storage
    namespace_cpy = NULL;

    return ASTARTE_RESULT_OK;
}

// Used as scope-based exit function
void astarte_key_value_destroy(astarte_key_value_t *kv_storage)
{
    if (kv_storage) {
        astarte_free(kv_storage->namespace);
        kv_storage->namespace = NULL;
    }
}

astarte_result_t astarte_key_value_insert(
    astarte_key_value_t *kv_storage, const char *key, const void *value, size_t value_size)
{
    uint32_t entry_id = 0;
    size_t old_value_size = 0;

    astarte_result_t ares = astarte_key_value_mutex_lock();
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed to lock mutex");
        return ares;
    }
    scope_defer(astarte_key_value_mutex_unlock)();

    ares = astarte_key_value_entry_intent_resolve(kv_storage->zms_fs);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Pre-operation integrity check failed: %s", astarte_result_to_name(ares));
        return ares;
    }

    ares = astarte_key_value_entry_find_or_alloc(
        kv_storage->zms_fs, kv_storage->namespace, key, &entry_id, true);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Key finding/allocation failed %s.", astarte_result_to_name(ares));
        return ares;
    }

    // Check if this is an update and extract the old size if a quota is active
    if (kv_storage->max_quota_bytes > 0) {
        ares = astarte_key_value_entry_read_value(
            kv_storage->zms_fs, entry_id, NULL, &old_value_size);
        if ((ares != ASTARTE_RESULT_OK) && (ares != ASTARTE_RESULT_NOT_FOUND)) {
            ASTARTE_LOG_ERR("Read failed %s.", astarte_result_to_name(ares));
            return ares;
        }

        // Check if the size has been desyncronized
        if (kv_storage->current_usage_bytes < old_value_size) {
            // At least old_value_size bytes are used
            kv_storage->current_usage_bytes = old_value_size;
        }

        size_t projected_size = (kv_storage->current_usage_bytes + value_size) - old_value_size;
        if (projected_size > kv_storage->max_quota_bytes) {
            ASTARTE_LOG_WRN("Namespace %s quota exceeded.", kv_storage->namespace);
            return ASTARTE_RESULT_OUT_OF_SPACE;
        }
    }

    ares = astarte_key_value_entry_write(
        kv_storage->zms_fs, entry_id, kv_storage->namespace, key, value, value_size);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Insert failed %s.", astarte_result_to_name(ares));
        return ares;
    }

    // Safely update the RAM counter post-write
    if (kv_storage->max_quota_bytes > 0) {
        kv_storage->current_usage_bytes
            = (kv_storage->current_usage_bytes + value_size) - old_value_size;
    }
    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_key_value_find(
    astarte_key_value_t *kv_storage, const char *key, void *value, size_t *value_size)
{
    uint32_t entry_id = 0;

    astarte_result_t ares = astarte_key_value_mutex_lock();
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed to lock mutex");
        return ares;
    }
    scope_defer(astarte_key_value_mutex_unlock)();

    ares = astarte_key_value_entry_intent_resolve(kv_storage->zms_fs);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Pre-operation integrity check failed: %s", astarte_result_to_name(ares));
        return ares;
    }

    ares = astarte_key_value_entry_find_or_alloc(
        kv_storage->zms_fs, kv_storage->namespace, key, &entry_id, false);
    if (ares != ASTARTE_RESULT_OK) {
        // No error logs as this could be a not found case, which is not necessarily an error
        return ares;
    }

    ares = astarte_key_value_entry_read_value(kv_storage->zms_fs, entry_id, value, value_size);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Get value of key-value storage failed %s.", astarte_result_to_name(ares));
        return ares;
    }

    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_key_value_delete(astarte_key_value_t *kv_storage, const char *key)
{
    uint32_t entry_id = 0;
    size_t deleted_value_size = 0;

    astarte_result_t ares = astarte_key_value_mutex_lock();
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed to lock mutex");
        return ares;
    }
    scope_defer(astarte_key_value_mutex_unlock)();

    ares = astarte_key_value_entry_intent_resolve(kv_storage->zms_fs);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Pre-operation integrity check failed: %s", astarte_result_to_name(ares));
        return ares;
    }

    ares = astarte_key_value_entry_find_or_alloc(
        kv_storage->zms_fs, kv_storage->namespace, key, &entry_id, false);
    if (ares != ASTARTE_RESULT_OK) {
        // No error logs as this could be a not found case, which is not necessarily an error
        return ares;
    }

    // If a quota is active, grab the size of the entry before wiping it
    if (kv_storage->max_quota_bytes > 0) {
        astarte_result_t size_check_res = astarte_key_value_entry_read_value(
            kv_storage->zms_fs, entry_id, NULL, &deleted_value_size);
        if (size_check_res != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_WRN("Failed to read size for ID %d prior to deletion", entry_id);
            deleted_value_size = 0;
        }
    }

    ares = astarte_key_value_entry_delete(kv_storage->zms_fs, entry_id);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("ZMS Delete Error: %s.", astarte_result_to_name(ares));
        return ares;
    }

    // Safely update the RAM counter post-deletion
    if (kv_storage->max_quota_bytes > 0 && deleted_value_size > 0) {
        if (kv_storage->current_usage_bytes >= deleted_value_size) {
            kv_storage->current_usage_bytes -= deleted_value_size;
        } else {
            kv_storage->current_usage_bytes = 0;
        }
    }

    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_key_value_iterator_init(
    astarte_key_value_t *kv_storage, astarte_key_value_iter_t *iter)
{
    // The maximum ID is a reserved starting point for the linked list iterator
    iter->kv_storage = kv_storage;
    iter->current_id = ASTARTE_KEY_VALUE_ENTRY_NULL_ID;
    return astarte_key_value_iterator_next(iter);
}

astarte_result_t astarte_key_value_iterator_next(astarte_key_value_iter_t *iter)
{
    bool matches = false;

    astarte_result_t ares = astarte_key_value_mutex_lock();
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed to lock mutex");
        return ares;
    }
    scope_defer(astarte_key_value_mutex_unlock)();

    ares = astarte_key_value_entry_intent_resolve(iter->kv_storage->zms_fs);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Pre-operation integrity check failed: %s", astarte_result_to_name(ares));
        return ares;
    }

    uint32_t curr_id = iter->current_id;

    while (true) {
        uint32_t next_id = ASTARTE_KEY_VALUE_ENTRY_NULL_ID;
        ares = astarte_key_value_entry_get_next_id(iter->kv_storage->zms_fs, curr_id, &next_id);

        if (ares != ASTARTE_RESULT_OK || next_id == ASTARTE_KEY_VALUE_ENTRY_NULL_ID) {
            ASTARTE_LOG_DBG("Iterator reached the end.");
            return ASTARTE_RESULT_NOT_FOUND;
        }

        ares = astarte_key_value_entry_check_namespace(
            iter->kv_storage->zms_fs, next_id, iter->kv_storage->namespace, &matches);

        if (ares == ASTARTE_RESULT_OK && matches) {
            iter->current_id = next_id;
            return ASTARTE_RESULT_OK;
        }

        curr_id = next_id;
    }

    return ASTARTE_RESULT_NOT_FOUND;
}

astarte_result_t astarte_key_value_iterator_get(
    astarte_key_value_iter_t *iter, void *key, size_t *key_size)
{
    astarte_result_t ares = astarte_key_value_mutex_lock();
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed to lock mutex");
        return ares;
    }
    scope_defer(astarte_key_value_mutex_unlock)();

    ares = astarte_key_value_entry_intent_resolve(iter->kv_storage->zms_fs);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Pre-operation integrity check failed: %s", astarte_result_to_name(ares));
        return ares;
    }

    ares = astarte_key_value_entry_read_key(
        iter->kv_storage->zms_fs, iter->current_id, (char *) key, key_size);

    return ares;
}

astarte_result_t astarte_key_value_iterator_delete(astarte_key_value_iter_t *iter)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    size_t deleted_value_size = 0;

    if (!iter || iter->current_id == 0) {
        ASTARTE_LOG_ERR("Invalid iterator for deletion operation");
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    ares = astarte_key_value_mutex_lock();
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed to lock mutex");
        return ares;
    }
    scope_defer(astarte_key_value_mutex_unlock)();

    char *next_key = NULL;
    scope_defer(free_char_ptr)(&next_key);

    ares = astarte_key_value_entry_intent_resolve(iter->kv_storage->zms_fs);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Pre-operation integrity check failed: %s", astarte_result_to_name(ares));
        return ares;
    }

    // Peek ahead to find the next matching element in the same namespace
    uint32_t next_matching_id = ASTARTE_KEY_VALUE_ENTRY_NULL_ID;
    bool has_next = false;
    ares = find_next_matching_id(iter, &next_matching_id, &has_next);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("List advancement failure: %s", astarte_result_to_name(ares));
        return ares;
    }

    // Get the key of the next matching element so we can re-find it if it shifts
    if (has_next) {
        ares = read_next_key(iter, next_matching_id, &next_key);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Failed in reading the next key: %s", astarte_result_to_name(ares));
            return ares;
        }
    }

    // If a quota is active, grab the size of the entry before wiping it
    if (iter->kv_storage->max_quota_bytes > 0) {
        astarte_result_t size_check_res = astarte_key_value_entry_read_value(
            iter->kv_storage->zms_fs, iter->current_id, NULL, &deleted_value_size);
        if (size_check_res != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_WRN(
                "Failed to read size for ID %d prior to iterator deletion", iter->current_id);
            deleted_value_size = 0;
        }
    }

    // Physically delete the current entry and heal the ZMS global linked-list
    ares = astarte_key_value_entry_delete(iter->kv_storage->zms_fs, iter->current_id);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Iterator delete error: %s.", astarte_result_to_name(ares));
        return ares;
    }

    // Safely update the RAM counter post-deletion
    if (iter->kv_storage->max_quota_bytes > 0 && deleted_value_size > 0) {
        if (iter->kv_storage->current_usage_bytes >= deleted_value_size) {
            iter->kv_storage->current_usage_bytes -= deleted_value_size;
        } else {
            iter->kv_storage->current_usage_bytes = 0;
        }
    }

    // We just deleted the very last element in this namespace.
    if (!has_next) {
        iter->current_id = ASTARTE_KEY_VALUE_ENTRY_NULL_ID;
        return ASTARTE_RESULT_OK;
    }

    return heal_iterator_post_delete(iter, next_key);
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static astarte_result_t find_next_matching_id(
    astarte_key_value_iter_t *iter, uint32_t *next_id, bool *has_next)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    uint32_t peek_curr_id = iter->current_id;
    *has_next = false;
    *next_id = ASTARTE_KEY_VALUE_ENTRY_NULL_ID;

    while (true) {
        uint32_t idx = 0;
        ares = astarte_key_value_entry_get_next_id(iter->kv_storage->zms_fs, peek_curr_id, &idx);
        if (ares != ASTARTE_RESULT_OK || idx == ASTARTE_KEY_VALUE_ENTRY_NULL_ID) {
            break;
        }

        bool matches = false;
        ares = astarte_key_value_entry_check_namespace(
            iter->kv_storage->zms_fs, idx, iter->kv_storage->namespace, &matches);
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

static astarte_result_t read_next_key(
    astarte_key_value_iter_t *iter, uint32_t next_id, char **next_key)
{
    size_t next_key_size = 0;
    astarte_result_t ares
        = astarte_key_value_entry_read_key(iter->kv_storage->zms_fs, next_id, NULL, &next_key_size);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Key size reading failure: %s", astarte_result_to_name(ares));
        return ares;
    }

    char *local_next_key = NULL;
    scope_defer(free_char_ptr)(&local_next_key);

    local_next_key = astarte_calloc(next_key_size, sizeof(char));
    if (!local_next_key) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        return ASTARTE_RESULT_OUT_OF_MEMORY;
    }

    ares = astarte_key_value_entry_read_key(
        iter->kv_storage->zms_fs, next_id, local_next_key, &next_key_size);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Key reading failure: %s", astarte_result_to_name(ares));
        return ares;
    }

    *next_key = local_next_key;

    // Disarm the auto-cleanup
    local_next_key = NULL;

    return ASTARTE_RESULT_OK;
}

static astarte_result_t heal_iterator_post_delete(
    astarte_key_value_iter_t *iter, const char *next_key)
{
    // Find the post-shift valid ZMS ID of the next matching element
    uint32_t valid_next_matching_id = ASTARTE_KEY_VALUE_ENTRY_NULL_ID;
    astarte_result_t ares = astarte_key_value_entry_find_or_alloc(iter->kv_storage->zms_fs,
        iter->kv_storage->namespace, next_key, &valid_next_matching_id, false);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Could not find post-shift entry: %s.", astarte_result_to_name(ares));
        return ares;
    }

    // Find the post-shift ZMS ID of the next element previous element
    uint32_t new_prev_id = ASTARTE_KEY_VALUE_ENTRY_NULL_ID;
    ares = astarte_key_value_entry_get_prev_id(
        iter->kv_storage->zms_fs, valid_next_matching_id, &new_prev_id);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Could not find post-shift prev id: %s.", astarte_result_to_name(ares));
        return ares;
    }
    iter->current_id = new_prev_id;
    return ASTARTE_RESULT_OK;
}
