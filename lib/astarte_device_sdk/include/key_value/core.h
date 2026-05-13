/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KEY_VALUE_CORE_H
#define KEY_VALUE_CORE_H

/**
 * @file key_value/core.h
 * @brief Key-value persistent storage implementation, with namespacing. Uses NVS as backend.
 *
 * @details
 * Each namespaced key-value pair is stored as a single NVS entry. The entry ID is generated via a
 * CRC16 hash of the concatenated namespace and key strings, with linear probing used to handle
 * collisions.
 *
 * The payload of each NVS entry is structured as follows:
 * - 2 bytes: Namespace string length (excluding null terminator)
 * - 2 bytes: Key string length (excluding null terminator)
 * - 2 bytes: Next entry ID in the linked list (0xFFFF if tail)
 * - 2 bytes: Previous entry ID in the linked list (0xFFFF if head)
 * - N bytes: Namespace string (no null terminator)
 * - K bytes: Key string (no null terminator)
 * - V bytes: Value data
 *
 * This driver implements the following functionalities:
 * - Inserting a key-value pair.
 * - Fetching a value from a known key.
 * - Removing a key-value pair.
 * - Iterating through all the stored key-value pairs.
 */

#include <zephyr/version.h>

#if (KERNEL_VERSION_MAJOR >= 4) && (KERNEL_VERSION_MINOR >= 4)
#include <zephyr/kvss/nvs.h>
#else
#include <zephyr/fs/nvs.h>
#endif

#include "astarte_device_sdk/astarte.h"
#include "astarte_device_sdk/result.h"

/** @brief The current major version for the key-value storage. */
#define ASTARTE_KEY_VALUE_FORMAT_VERSION_MAJOR 0
/** @brief The current minor version for the key-value storage. */
#define ASTARTE_KEY_VALUE_FORMAT_VERSION_MINOR 5
/** @brief The current patch version for the key-value storage. */
#define ASTARTE_KEY_VALUE_FORMAT_VERSION_PATCH 0

/** @brief Version format structure for NVS storage. */
typedef struct
{
    /** @brief Major version for the key-value storage. */
    uint16_t major;
    /** @brief Minor version for the key-value storage. */
    uint16_t minor;
    /** @brief Patch version for the key-value storage. */
    uint16_t patch;
} astarte_key_value_version_t;

/** @brief Configuration struct for a key-value storage instance. */
typedef struct
{
    /** @brief Flash device runtime structure */
    const struct device *flash_device;
    /** @brief Flash partition offset */
    off_t flash_offset;
    /** @brief Full size of the partition */
    uint64_t flash_partition_size;
} astarte_key_value_cfg_t;

/** @brief Data struct for an instance of the key-value storage driver. */
typedef struct
{
    /** @brief Namespace used for this key-value storage instance. */
    char *namespace;
    /** @brief Persistent NVS file system context */
    struct nvs_fs *nvs_fs;
} astarte_key_value_t;

/** @brief Iterator struct for the key-value pair storage. */
typedef struct
{
    /** @brief Reference to the storage instance used by the iterator. */
    astarte_key_value_t *kv_storage;
    /** @brief Current NVS ID pointed to by the iterator. */
    uint16_t current_id;
    /** @brief Previous global NVS ID, cached to safely delete the current entry. */
    uint16_t prev_id;
} astarte_key_value_iter_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize a NVS partition for use with the key-value driver.
 *
 * @note Each NVS partition should be only initialize once. Furthermore call this function only
 * once for each NVS partition.
 *
 * @param[in] config Configuration struct for the NVS partition.
 * @param[in,out] nvs_fs The NVS file system context to open.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_key_value_open(astarte_key_value_cfg_t config, struct nvs_fs *nvs_fs);

/**
 * @brief Create a new instance of the key-value pairs storage driver for a specific namespace.
 *
 * @note After being used the key-value storage instance should be destroyed with
 * #astarte_key_value_destroy.
 *
 * @param[in,out] nvs_fs The NVS file system context to use. This should have been opened previously
 * using #astarte_key_value_open
 * @param[in] namespace The namespace to be used for this storage instance.
 * @param[out] kv_storage Data struct for the key-value storage instance to initialize.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_key_value_new(
    struct nvs_fs *nvs_fs, const char *namespace, astarte_key_value_t *kv_storage);

/**
 * @brief Destroy an instance of the key-value pairs storage driver.
 *
 * @param[inout] kv_storage The storage instance to destroy.
 */
void astarte_key_value_destroy(astarte_key_value_t *kv_storage);

/**
 * @brief Insert or update a new key-value pair into storage.
 *
 * @param[inout] kv_storage Data struct for the instance of the driver.
 * @param[in] key Key to the value to store.
 * @param[in] value Value to store.
 * @param[in] value_size Size of the value to store.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_key_value_insert(
    astarte_key_value_t *kv_storage, const char *key, const void *value, size_t value_size);

/**
 * @brief Find an key-value pair matching the @p key in storage.
 *
 * @param[inout] kv_storage Data struct for the instance of the driver.
 * @param[in] key Key to use for the search.
 * @param[out] value Buffer where to store the value for the pair, can be NULL.
 * @param[inout] value_size When @p value is not NULL it should correspond the the size the @p value
 * buffer. Upon success it will be set to the size of the read data or to the required buffer size.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_key_value_find(
    astarte_key_value_t *kv_storage, const char *key, void *value, size_t *value_size);

/**
 * @brief Delete an existing key-value pair from storage.
 *
 * @param[inout] kv_storage Data struct for the instance of the driver.
 * @param[in] key Key of the key-value pair to delete.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_key_value_delete(astarte_key_value_t *kv_storage, const char *key);

/**
 * @brief Initialize a new iterator to be used to iterate over the keys present in storage.
 *
 * @param[in] kv_storage Data struct for the instance of the driver.
 * @param[out] iter Iterator instance to initialize.
 * @return ASTARTE_RESULT_OK if successful, ASTARTE_RESULT_NOT_FOUND if not found, otherwise an
 * error code.
 */
astarte_result_t astarte_key_value_iterator_init(
    astarte_key_value_t *kv_storage, astarte_key_value_iter_t *iter);

/**
 * @brief Advance the iterator of one position.
 *
 * @param[inout] iter Iterator instance.
 * @return ASTARTE_RESULT_OK if successful, ASTARTE_RESULT_NOT_FOUND if not found, otherwise an
 * error code.
 */
astarte_result_t astarte_key_value_iterator_next(astarte_key_value_iter_t *iter);

/**
 * @brief Get the key for the key-value pair pointed to by the iterator.
 *
 * @param[in] iter Iterator instance.
 * @param[out] key Buffer where to store the key for the pair, can be set to NULL.
 * @param[inout] key_size When @p key is not NULL it should correspond the the size the @p key
 * buffer. Upon success it will be set to the size of the read data or to the required buffer size.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_key_value_iterator_get(
    astarte_key_value_iter_t *iter, void *key, size_t *key_size);

/**
 * @brief Delete the key-value pair currently pointed to by the iterator.
 *
 * @details This function removes the current entry without breaking the iteration.
 * The iterator state is automatically stepped backward so the next call to
 * `astarte_key_value_iterator_next` will advance to the subsequent valid entry.
 *
 * @param[inout] iter Iterator instance.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_key_value_iterator_delete(astarte_key_value_iter_t *iter);

#ifdef __cplusplus
}
#endif

#endif // KEY_VALUE_CORE_H
