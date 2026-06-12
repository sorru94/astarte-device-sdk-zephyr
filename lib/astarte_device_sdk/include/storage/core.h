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

#include "key_value/core.h"

/**
 * @brief Handle containing the persistent state for device storage.
 * @details This struct holds the context for the three ZMS namespaces used by the storage.
 */
typedef struct
{
    /** @brief ZMS file system handle shared between all storages */
    struct zms_fs zms_fs;
    /** @brief Key value storage handle for synchronization state */
    astarte_key_value_t sync_storage;
    /** @brief Key value storage handle for introspection data */
    astarte_key_value_t intro_storage;
    /** @brief Key value storage handle for device properties */
    astarte_key_value_t prop_storage;
    /** @brief Flag to ensure we don't double-init or use uninitialized handles */
    bool initialized;
} astarte_storage_data_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the device storage and open all required ZMS namespaces.
 * @param[in,out] handle Pointer to the handle structure to initialize.
 * @return ASTARTE_RESULT_OK if successful.
 */
astarte_result_t astarte_storage_init(astarte_storage_data_t *handle);

/**
 * @brief Close and clean up the device storage.
 * * @param[in] handle Pointer to the handle structure to destroy.
 */
void astarte_storage_destroy(astarte_storage_data_t *handle);

#ifdef __cplusplus
}
#endif

#endif // STORAGE_CORE_H
