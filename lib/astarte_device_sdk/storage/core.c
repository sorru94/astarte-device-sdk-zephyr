/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "storage/core.h"

#include <string.h>

#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/version.h>

#if KERNEL_VERSION_NUMBER >= ZEPHYR_VERSION(4, 4, 0)
#include <zephyr/kvss/zms.h>
#else
#include <zephyr/fs/zms.h>
#endif

#include "log.h"
ASTARTE_LOG_MODULE_REGISTER(astarte_storage, CONFIG_ASTARTE_DEVICE_SDK_STORAGE_LOG_LEVEL);

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

#define ZMS_PARTITION astarte_partition
#if !FIXED_PARTITION_EXISTS(ZMS_PARTITION)
#error "Permanent storage is enabled but 'astarte_partition' flash partition is missing."
#endif // FIXED_PARTITION_EXISTS(ZMS_PARTITION)

#if KERNEL_VERSION_NUMBER >= ZEPHYR_VERSION(4, 4, 0)
#define ZMS_PARTITION_DEVICE PARTITION_DEVICE(ZMS_PARTITION)
#define ZMS_PARTITION_OFFSET PARTITION_OFFSET(ZMS_PARTITION)
#define ZMS_PARTITION_SIZE PARTITION_SIZE(ZMS_PARTITION)
#else
#define ZMS_PARTITION_DEVICE FIXED_PARTITION_DEVICE(ZMS_PARTITION)
#define ZMS_PARTITION_OFFSET FIXED_PARTITION_OFFSET(ZMS_PARTITION)
#define ZMS_PARTITION_SIZE FIXED_PARTITION_SIZE(ZMS_PARTITION)
#endif

#define VERSION_NAMESPACE "version_namespace"
#define SYNCHRONIZATION_NAMESPACE "synchronization_namespace"
#define INTROSPECTION_NAMESPACE "introspection_namespace"
#define PROPERTIES_NAMESPACE "properties_namespace"

#define VERSION_KEY "sdk_version"

/************************************************
 *         Static functions declaration         *
 ***********************************************/

/**
 * @brief Erases the flash partition.
 *
 * @param[inout] handle Pointer to the storage handle data.
 * @param[in] kv_cfg Configuration struct to remount ZMS.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
static astarte_result_t erase_storage(
    astarte_storage_data_t *handle, const astarte_key_value_cfg_t *kv_cfg);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_storage_init(astarte_storage_data_t *handle)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    if (!handle) {
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    // Zero out the memory to ensure clean state
    memset(handle, 0, sizeof(astarte_storage_data_t));

    // Open the key value storage flash partition
    astarte_key_value_cfg_t kv_astarte_storage_cfg = {
        .flash_device = ZMS_PARTITION_DEVICE,
        .flash_offset = ZMS_PARTITION_OFFSET,
        .flash_partition_size = ZMS_PARTITION_SIZE,
    };
    ares = astarte_key_value_open(kv_astarte_storage_cfg, &handle->zms_fs);
    if ((ares == ASTARTE_RESULT_KEY_VALUE_INCOMPATIBLE_VERSION)
        || (ares == ASTARTE_RESULT_KEY_VALUE_RECOVERY_FAILED)) {
        ASTARTE_LOG_ERR("key-value is corrupted or of incompatible version.");
        ares = erase_storage(handle, &kv_astarte_storage_cfg);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Storage erase error: %s.", astarte_result_to_name(ares));
            return ares;
        }
    } else if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Error opening cache: %s.", astarte_result_to_name(ares));
        return ares;
    }

    // Init Synchronization Storage
    ares = astarte_key_value_new(&handle->zms_fs, SYNCHRONIZATION_NAMESPACE, &handle->sync_storage);
    if (ares != ASTARTE_RESULT_OK) {
        return ares;
    }

    // Init Introspection Storage
    ares = astarte_key_value_new(&handle->zms_fs, INTROSPECTION_NAMESPACE, &handle->intro_storage);
    if (ares != ASTARTE_RESULT_OK) {
        astarte_key_value_destroy(&handle->sync_storage); // Rollback
        return ares;
    }

    // Init Properties Storage
    ares = astarte_key_value_new(&handle->zms_fs, PROPERTIES_NAMESPACE, &handle->prop_storage);
    if (ares != ASTARTE_RESULT_OK) {
        astarte_key_value_destroy(&handle->sync_storage);
        astarte_key_value_destroy(&handle->intro_storage);
        return ares;
    }

    handle->initialized = true;
    return ASTARTE_RESULT_OK;
}

void astarte_storage_destroy(astarte_storage_data_t *handle)
{
    if (!handle || !handle->initialized) {
        return;
    }

    // Destroy individual storage instances
    astarte_key_value_destroy(&handle->sync_storage);
    astarte_key_value_destroy(&handle->intro_storage);
    astarte_key_value_destroy(&handle->prop_storage);

    handle->initialized = false;
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static astarte_result_t erase_storage(
    astarte_storage_data_t *handle, const astarte_key_value_cfg_t *kv_cfg)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    // Completely wipe the partition if the format might be incompatible
    int flash_rc = flash_erase(ZMS_PARTITION_DEVICE, ZMS_PARTITION_OFFSET, ZMS_PARTITION_SIZE);
    if (flash_rc != 0) {
        ASTARTE_LOG_ERR("Flash erase failed: %d", flash_rc);
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }

    // Remount ZMS after wiping the physical flash partition
    ares = astarte_key_value_open(*kv_cfg, &handle->zms_fs);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Error reopening cache after erase: %s.", astarte_result_to_name(ares));
    }

    return ares;
}
