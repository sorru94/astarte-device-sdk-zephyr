/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "storage/introsp.h"

#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>

#include "alloc.h"
#include "log.h"
ASTARTE_LOG_MODULE_DECLARE(astarte_storage, CONFIG_ASTARTE_DEVICE_SDK_STORAGE_LOG_LEVEL);

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

#define INTROSPECTION_KEY "introspection_string"

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_storage_introspection_store(
    astarte_storage_data_t *handle, const char *intr, size_t intr_size)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    if (!handle || !handle->initialized) {
        ASTARTE_LOG_ERR("Device caching handle is uninitialized or NULL.");
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    ASTARTE_LOG_DBG("Storing introspection in key-value storage: '%s' (%d).", intr, intr_size);

    ASTARTE_LOG_DBG("Inserting pair in storage. Key: %s", INTROSPECTION_KEY);
    ares = astarte_key_value_insert(&handle->intro_storage, INTROSPECTION_KEY, intr, intr_size);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Error caching introspection: %s.", astarte_result_to_name(ares));
    }

    return ares;
}

astarte_result_t astarte_storage_introspection_check(
    astarte_storage_data_t *handle, const char *intr, size_t intr_size)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    size_t read_intr_size = 0;

    if (!handle || !handle->initialized) {
        ASTARTE_LOG_ERR("Device caching handle is uninitialized or NULL.");
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    ASTARTE_LOG_DBG("Checking stored introspection against new one: '%s' (%d).", intr, intr_size);

    ASTARTE_LOG_DBG("Searching for pair in storage. Key: '%s'", INTROSPECTION_KEY);
    ares = astarte_key_value_find(&handle->intro_storage, INTROSPECTION_KEY, NULL, &read_intr_size);
    if (ares == ASTARTE_RESULT_NOT_FOUND) {
        return ASTARTE_RESULT_DEVICE_CACHING_OUTDATED_INTROSPECTION;
    }
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Fetch error for cached introspection: %s.", astarte_result_to_name(ares));
        return ares;
    }

    if (read_intr_size != intr_size) {
        return ASTARTE_RESULT_DEVICE_CACHING_OUTDATED_INTROSPECTION;
    }

    scope_var(scoped_char, read_intr)(read_intr_size);
    if (!read_intr) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        return ASTARTE_RESULT_OUT_OF_MEMORY;
    }

    ASTARTE_LOG_DBG("Searching for pair in storage. Key: '%s'", INTROSPECTION_KEY);
    ares = astarte_key_value_find(
        &handle->intro_storage, INTROSPECTION_KEY, read_intr, &read_intr_size);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Fetch error for cached introspection: %s.", astarte_result_to_name(ares));
        return ares;
    }

    if (memcmp(intr, read_intr, MIN(read_intr_size, intr_size)) != 0) {
        ASTARTE_LOG_INF("Found outdated introspection: '%s' (%d).", read_intr, read_intr_size);
        return ASTARTE_RESULT_DEVICE_CACHING_OUTDATED_INTROSPECTION;
    }

    return ASTARTE_RESULT_OK;
}
