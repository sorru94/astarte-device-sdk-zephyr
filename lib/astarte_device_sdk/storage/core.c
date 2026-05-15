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

#if (KERNEL_VERSION_MAJOR >= 4) && (KERNEL_VERSION_MINOR >= 4)
#include <zephyr/kvss/nvs.h>
#else
#include <zephyr/fs/nvs.h>
#endif

#include "log.h"
ASTARTE_LOG_MODULE_REGISTER(device_storage, CONFIG_ASTARTE_DEVICE_SDK_DEVICE_STORAGE_LOG_LEVEL);

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

#define NVS_PARTITION astarte_partition
#if !FIXED_PARTITION_EXISTS(NVS_PARTITION)
#error "Permanent storage is enabled but 'astarte_partition' flash partition is missing."
#endif // FIXED_PARTITION_EXISTS(NVS_PARTITION)

#if (KERNEL_VERSION_MAJOR >= 4) && (KERNEL_VERSION_MINOR >= 4)
#define NVS_PARTITION_DEVICE PARTITION_DEVICE(NVS_PARTITION)
#define NVS_PARTITION_OFFSET PARTITION_OFFSET(NVS_PARTITION)
#define NVS_PARTITION_SIZE PARTITION_SIZE(NVS_PARTITION)
#else
#define NVS_PARTITION_DEVICE FIXED_PARTITION_DEVICE(NVS_PARTITION)
#define NVS_PARTITION_OFFSET FIXED_PARTITION_OFFSET(NVS_PARTITION)
#define NVS_PARTITION_SIZE FIXED_PARTITION_SIZE(NVS_PARTITION)
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
 * @brief Checks if the stored version matches the current SDK version.
 * Erases the flash partition if the major or minor versions differ and saves the new one.
 *
 * @param[inout] handle Pointer to the storage handle data.
 * @param[in] major Major version number.
 * @param[in] minor Minor version number.
 * @param[in] patch Patch version number.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
static astarte_result_t check_and_update_version(
    astarte_storage_data_t *handle, uint8_t major, uint8_t minor, uint8_t patch);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_storage_init(
    astarte_storage_data_t *handle, uint8_t major, uint8_t minor, uint8_t patch)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    if (!handle) {
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    // Zero out the memory to ensure clean state
    memset(handle, 0, sizeof(astarte_storage_data_t));

    // Open the key value storage flash partition
    ares = astarte_storage_key_value_open(&handle->nvs_fs);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Error opening cache: %s.", astarte_result_to_name(ares));
        return ares;
    }

    // Check & update storage version
    ares = check_and_update_version(handle, major, minor, patch);
    if (ares != ASTARTE_RESULT_OK) {
        return ares;
    }

    // Init Synchronization Storage
    ares = astarte_storage_key_value_new(
        &handle->nvs_fs, SYNCHRONIZATION_NAMESPACE, &handle->sync_storage);
    if (ares != ASTARTE_RESULT_OK) {
        return ares;
    }

    // Init Introspection Storage
    ares = astarte_storage_key_value_new(
        &handle->nvs_fs, INTROSPECTION_NAMESPACE, &handle->intro_storage);
    if (ares != ASTARTE_RESULT_OK) {
        astarte_storage_key_value_destroy(&handle->sync_storage); // Rollback
        return ares;
    }

    // Init Properties Storage
    ares = astarte_storage_key_value_new(
        &handle->nvs_fs, PROPERTIES_NAMESPACE, &handle->prop_storage);
    if (ares != ASTARTE_RESULT_OK) {
        astarte_storage_key_value_destroy(&handle->sync_storage);
        astarte_storage_key_value_destroy(&handle->intro_storage);
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
    astarte_storage_key_value_destroy(&handle->sync_storage);
    astarte_storage_key_value_destroy(&handle->intro_storage);
    astarte_storage_key_value_destroy(&handle->prop_storage);

    handle->initialized = false;
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static astarte_result_t check_and_update_version(
    astarte_storage_data_t *handle, uint8_t major, uint8_t minor, uint8_t patch)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    astarte_storage_key_value_t version_storage = { 0 };

    ares = astarte_storage_key_value_new(&handle->nvs_fs, VERSION_NAMESPACE, &version_storage);
    if (ares != ASTARTE_RESULT_OK) {
        goto exit;
    }

    uint8_t stored_version[3] = { 0 };
    size_t version_size = sizeof(stored_version);
    ares = astarte_storage_key_value_find(
        &version_storage, VERSION_KEY, stored_version, &version_size);

    bool erase_needed = false;
    bool write_version = false;

    if (ares == ASTARTE_RESULT_NOT_FOUND) {
        // First boot or newly provisioned NVS
        ares = ASTARTE_RESULT_OK;
        write_version = true;
    } else if (ares == ASTARTE_RESULT_OK) {
        if (stored_version[0] != major || stored_version[1] != minor) {
            ASTARTE_LOG_INF("Version changed from %d.%d.%d to %d.%d.%d. Erasing partition...",
                stored_version[0], stored_version[1], stored_version[2], major, minor, patch);
            erase_needed = true;
            write_version = true;
        } else if (stored_version[2] != patch) {
            // Only a patch version change - no erase needed
            write_version = true;
        }
    } else {
        ASTARTE_LOG_ERR("Failed to read version from storage: %s", astarte_result_to_name(ares));
        goto exit;
    }

    if (erase_needed) {
        // Completely wipe the partition if the format might be incompatible
        int flash_rc = flash_erase(NVS_PARTITION_DEVICE, NVS_PARTITION_OFFSET, NVS_PARTITION_SIZE);
        if (flash_rc != 0) {
            ASTARTE_LOG_ERR("Flash erase failed: %d", flash_rc);
            ares = ASTARTE_RESULT_INTERNAL_ERROR;
            goto exit;
        }

        // Remount NVS after wiping the physical flash partition
        ares = astarte_storage_key_value_open(&handle->nvs_fs);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Error reopening cache after erase: %s.", astarte_result_to_name(ares));
            goto exit;
        }
    }

    if (write_version) {
        uint8_t new_version[3] = { major, minor, patch };
        ares = astarte_storage_key_value_insert(
            &version_storage, VERSION_KEY, new_version, sizeof(new_version));
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Failed to store new version: %s.", astarte_result_to_name(ares));
            goto exit;
        }
    }

exit:
    astarte_storage_key_value_destroy(&version_storage);
    return ares;
}
