/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "device_caching.h"

#include "log.h"
ASTARTE_LOG_MODULE_DECLARE(device_caching, CONFIG_ASTARTE_DEVICE_SDK_DEVICE_CACHING_LOG_LEVEL);

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

#define SYNCHRONIZATION_KEY "synchronization_status"

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_device_caching_synchronization_get(
    astarte_device_caching_t *handle, bool *sync)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    if (!handle || !handle->initialized) {
        ASTARTE_LOG_ERR("Device caching handle is uninitialized or NULL.");
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    bool read_sync = false;
    size_t read_sync_size = sizeof(read_sync);

    ASTARTE_LOG_DBG("Searching for pair in storage. Key: '%s'", SYNCHRONIZATION_KEY);
    ares = astarte_kv_storage_find(
        &handle->sync_storage, SYNCHRONIZATION_KEY, &read_sync, &read_sync_size);
    if (ares == ASTARTE_RESULT_NOT_FOUND) {
        ASTARTE_LOG_INF("No previous synchronization with Astarte present.");
        return ares;
    }
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR(
            "Fetch error for cached synchronization: %s.", astarte_result_to_name(ares));
        return ares;
    }

    if (!read_sync) {
        ASTARTE_LOG_INF("No previous synchronization with Astarte present.");
    }
    *sync = read_sync;

    return ares;
}

astarte_result_t astarte_device_caching_synchronization_set(
    astarte_device_caching_t *handle, bool sync)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    if (!handle || !handle->initialized) {
        ASTARTE_LOG_ERR("Device caching handle is uninitialized or NULL.");
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    ASTARTE_LOG_DBG("Storing synchronization: %s", (sync) ? "synchronized" : "not synchronized");

    ASTARTE_LOG_DBG("Inserting pair in storage. Key: %s", SYNCHRONIZATION_KEY);
    ares = astarte_kv_storage_insert(
        &handle->sync_storage, SYNCHRONIZATION_KEY, &sync, sizeof(sync));
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Error caching synchronization: %s.", astarte_result_to_name(ares));
    }
    return ares;
}
