/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KV_STORAGE_H
#define KV_STORAGE_H

/**
 * @file kv_storage.h
 * @brief Key-value persistent storage implementation, with namespacing. Uses NVS as backend.
 *
 * @details
 * Each namespaced key-value pair is stored as three separate NVS entries:
 * 1. An entry containing the namespace
 * 2. An entry containing the key
 * 3. An entry containing the value
 *
 * The NVS storage is used in an array like manner, with the following organization:
 * <pre>
 * +---------+------------------------------+
 * | NVS ID  | NVS VALUE                    |
 * +=========+==============================+
 * | 0       | Total number of stored pairs |
 * +---------+------------------------------+
 * | 1       | Namespace for first pair     |
 * +---------+------------------------------+
 * | 2       | Key for first pair           |
 * +---------+------------------------------+
 * | 3       | Value for first pair         |
 * +---------+------------------------------+
 * | ...     | ...                          |
 * +---------+------------------------------+
 * | n*3 + 1 | Namespace for nth pair       |
 * +---------+------------------------------+
 * | n*3 + 2 | Key for nth pair             |
 * +---------+------------------------------+
 * | n*3 + 3 | Value for nth pair           |
 * +---------+------------------------------+
 * | ...     | ...                          |
 * +---------+------------------------------+
 * </pre>
 *
 * The first NVS entry will hold the total number of namespaced pairs, independently from their
 * namespace.
 *
 * This driver implements the following functionalities:
 * - Inserting a key-value pair.
 * - Fetching a value from a known key.
 * - Removing a key-value pair.
 * - Iterating through all the stored key-value pairs.
 */

#include <zephyr/fs/nvs.h>

#include "astarte_device_sdk/astarte.h"
#include "astarte_device_sdk/result.h"

/** @brief Configuration struct for a key-value storage instance. */
typedef struct
{
    /** @brief Flash device runtime structure */
    const struct device *flash_device;
    /** @brief Flash partition offset */
    off_t flash_offset;
    /** @brief Flash page sector size, each sector must be multiple of erase-block-size */
    uint16_t flash_sector_size;
    /** @brief Flash page sector count */
    uint16_t flash_sector_count;
} astarte_kv_storage_cfg_t;

/** @brief Data struct for an instance of the key-value storage driver. */
typedef struct
{
    /** @brief Flash device runtime structure */
    const struct device *flash_device;
    /** @brief Flash partition offset */
    off_t flash_offset;
    /** @brief Flash page sector size, each sector must be multiple of erase-block-size */
    uint16_t flash_sector_size;
    /** @brief Flash page sector count */
    uint16_t flash_sector_count;
    /** @brief Namespace used for this key-value storage instance. */
    char *namespace;
} astarte_kv_storage_t;

/** @brief Iterator struct for the key-value pair storage. */
typedef struct
{
    /** @brief Reference to the storage instance used by the iterator. */
    astarte_kv_storage_t *kv_storage;
    /** @brief Current key-value pair pointed by the iterator. */
    uint16_t current_pair;
} astarte_kv_storage_iter_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a new instance of the key-value pairs storage driver.
 *
 * @note After being used the key-value storage instance should be destroyed with
 * #astarte_kv_storage_destroy.
 *
 * @param[in] config Configuration struct for the storage instance.
 * @param[in] namespace The namespace to be used for this storage instance.
 * @param[out] kv_storage Data struct for the key-value storage instance to initialize.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_kv_storage_new(
    astarte_kv_storage_cfg_t config, const char *namespace, astarte_kv_storage_t *kv_storage);

/**
 * @brief Destroy an instance of the key-value pairs storage driver.
 *
 * @param[inout] kv_storage The storage instance to destroy.
 */
void astarte_kv_storage_destroy(astarte_kv_storage_t kv_storage);

/**
 * @brief Insert or update a new key-value pair into storage.
 *
 * @param[inout] kv_storage Data struct for the instance of the driver.
 * @param[in] key Key to the value to store.
 * @param[in] value Value to store.
 * @param[in] value_size Size of the value to store.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_kv_storage_insert(
    astarte_kv_storage_t *kv_storage, const char *key, const void *value, size_t value_size);

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
astarte_result_t astarte_kv_storage_find(
    astarte_kv_storage_t *kv_storage, const char *key, void *value, size_t *value_size);

/**
 * @brief Delete an existing key-value pair from storage.
 *
 * @param[inout] kv_storage Data struct for the instance of the driver.
 * @param[in] key Key of the key-value pair to delete.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_kv_storage_delete(astarte_kv_storage_t *kv_storage, const char *key);

/**
 * @brief Initialize a new iterator to be used to iterate over the keys present in storage.
 *
 * @param[in] kv_storage Data struct for the instance of the driver.
 * @param[out] iter Iterator instance to initialize.
 * @return ASTARTE_RESULT_OK if successful, ASTARTE_RESULT_NOT_FOUND if not found, otherwise an
 * error code.
 */
astarte_result_t astarte_kv_storage_iterator_init(
    astarte_kv_storage_t *kv_storage, astarte_kv_storage_iter_t *iter);

/**
 * @brief Advance the iterator of one position.
 *
 * @param[inout] iter Iterator instance.
 * @return ASTARTE_RESULT_OK if successful, ASTARTE_RESULT_NOT_FOUND if not found, otherwise an
 * error code.
 */
astarte_result_t astarte_kv_storage_iterator_next(astarte_kv_storage_iter_t *iter);

/**
 * @brief Get the key for the key-valie pair pointed to by the iterator.
 *
 * @param[in] iter Iterator instance.
 * @param[out] key Buffer where to store the key for the pair, can be set to NULL.
 * @param[inout] key_size When @p key is not NULL it should correspond the the size the @p key
 * buffer. Upon success it will be set to the size of the read data or to the required buffer size.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_kv_storage_iterator_get(
    astarte_kv_storage_iter_t *iter, void *key, size_t *key_size);

#ifdef __cplusplus
}
#endif

#endif // KV_STORAGE_H
