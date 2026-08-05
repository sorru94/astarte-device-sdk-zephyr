/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "key_value/core.h"

#include <stdlib.h>
#include <string.h>

#include <zephyr/version.h>

#if KERNEL_VERSION_NUMBER >= ZEPHYR_VERSION(4, 4, 0)
#include <zephyr/kvss/zms.h>
#else
#include <zephyr/fs/zms.h>
#endif

#include "key_value/mutex.h"
#include "log.h"

ASTARTE_LOG_MODULE_DECLARE(astarte_key_value, CONFIG_ASTARTE_DEVICE_SDK_KEY_VALUE_LOG_LEVEL);

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

#define FULL_KEY(alternate, key) ((alternate) ? ((1U << 16U) + (uint32_t) (key)) : (uint32_t) (key))

ASTARTE_SCOPE_DEFER_DEFINE(astarte_key_value_mutex_unlock);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_key_value_direct_insert(
    struct zms_fs *zms_fs, bool alternate, uint16_t key, const void *value, size_t value_size)
{
    if (!value || value_size == 0) {
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    astarte_result_t ares = astarte_key_value_mutex_lock();
    if (ares != ASTARTE_RESULT_OK) {
        return ares;
    }
    scope_defer(astarte_key_value_mutex_unlock)();

    uint32_t full_key = FULL_KEY(alternate, key);

    ssize_t ret = zms_write(zms_fs, full_key, value, value_size);
    if (ret < 0) {
        ASTARTE_LOG_ERR("Failed to insert direct key %d: %d", full_key, (int) ret);
        return ASTARTE_RESULT_ZMS_ERROR;
    }

    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_key_value_direct_find(
    struct zms_fs *zms_fs, bool alternate, uint16_t key, void *value, size_t *value_size)
{
    if (!value_size) {
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    astarte_result_t ares = astarte_key_value_mutex_lock();
    if (ares != ASTARTE_RESULT_OK) {
        return ares;
    }
    scope_defer(astarte_key_value_mutex_unlock)();

    uint32_t full_key = FULL_KEY(alternate, key);

    ssize_t data_len = zms_get_data_length(zms_fs, full_key);

    if (data_len == -ENOENT) {
        return ASTARTE_RESULT_NOT_FOUND;
    }
    if (data_len < 0) {
        ASTARTE_LOG_ERR("Failed to get data length for key %d: %d", key, (int) data_len);
        return ASTARTE_RESULT_ZMS_ERROR;
    }

    if (!value) {
        *value_size = data_len;
        return ASTARTE_RESULT_OK;
    }

    if (*value_size < (size_t) data_len) {
        ASTARTE_LOG_ERR(
            "Buffer too small for key %d. Need %d, got %zu", key, (int) data_len, *value_size);
        *value_size = data_len;
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    ssize_t ret = zms_read(zms_fs, full_key, value, *value_size);
    if (ret != data_len) {
        ASTARTE_LOG_ERR("Failed to read direct key %d: %d", key, (int) ret);
        return ASTARTE_RESULT_ZMS_ERROR;
    }

    *value_size = ret;

    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_key_value_direct_delete(
    struct zms_fs *zms_fs, bool alternate, uint16_t key)
{
    astarte_result_t ares = astarte_key_value_mutex_lock();
    if (ares != ASTARTE_RESULT_OK) {
        return ares;
    }
    scope_defer(astarte_key_value_mutex_unlock)();

    uint32_t full_key = FULL_KEY(alternate, key);

    ssize_t ret = zms_delete(zms_fs, full_key);

    // -ENOENT means it was already deleted, which is a successful state
    if (ret < 0 && ret != -ENOENT) {
        ASTARTE_LOG_ERR("Failed to delete direct key %d: %d", key, (int) ret);
        return ASTARTE_RESULT_ZMS_ERROR;
    }

    return ASTARTE_RESULT_OK;
}
