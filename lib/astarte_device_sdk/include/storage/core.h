/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef STORAGE_CORE_H
#define STORAGE_CORE_H

/**
 * @file storage/core.h
 * @brief Core Astarte device storage utilities.
 */

#include "astarte_device_sdk/astarte.h"
#include "astarte_device_sdk/result.h"

#include "storage/key_value.h"

#include <zephyr/version.h>
#if (KERNEL_VERSION_MAJOR >= 4) && (KERNEL_VERSION_MINOR >= 4)
#include <zephyr/kvss/nvs.h>
#else
#include <zephyr/fs/nvs.h>
#endif

/**
 * @brief Handle containing the persistent state for device storage.
 * @details This struct holds the context for the three NVS namespaces used by the storage.
 */
typedef struct
{
    /** @brief NVS file system handle shared between all storages */
    struct nvs_fs nvs_fs;
    /** @brief Key value storage handle for synchronization state */
    astarte_storage_key_value_t sync_storage;
    /** @brief Key value storage handle for introspection data */
    astarte_storage_key_value_t intro_storage;
    /** @brief Key value storage handle for device properties */
    astarte_storage_key_value_t prop_storage;
    /** @brief Flag to ensure we don't double-init or use uninitialized handles */
    bool initialized;
} astarte_storage_data_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the device storage and open all required NVS namespaces.
 * @param[in,out] handle Pointer to the handle structure to initialize.
 * @param[in] major Major version number.
 * @param[in] minor Minor version number.
 * @param[in] patch Patch version number.
 * @return ASTARTE_RESULT_OK if successful.
 */
astarte_result_t astarte_storage_init(
    astarte_storage_data_t *handle, uint8_t major, uint8_t minor, uint8_t patch);

/**
 * @brief Close and clean up the device storage.
 * * @param[in] handle Pointer to the handle structure to destroy.
 */
void astarte_storage_destroy(astarte_storage_data_t *handle);

#ifdef __cplusplus
}
#endif

#endif // STORAGE_CORE_H
