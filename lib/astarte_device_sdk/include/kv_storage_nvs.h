/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KV_STORAGE_NVS_H
#define KV_STORAGE_NVS_H

#include "astarte_device_sdk/result.h"
#include <zephyr/fs/nvs.h>

/** @brief Offsets for the NVS IDs of components of a key-value pair */
#define NVS_ID_OFFSET_NAMESPACE 0U
#define NVS_ID_OFFSET_KEY 1U
#define NVS_ID_OFFSET_VALUE 2U

/**
 * @brief Calculate the base NVS ID for a given pair index.
 *
 * @param[in] pair_index The 0-based index of the pair (0 to stored_pairs - 1).
 * @return The NVS ID for the start of that pair's record.
 */
uint16_t kv_storage_nvs_get_pair_base_id(uint16_t pair_index);
/**
 * @brief Get number of stored pairs.
 *
 * @param[inout] nvs_fs NVS file system from which to fetch the number.
 * @param[out] count Number of pairs stored in NVS.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t kv_storage_nvs_get_pairs_number(struct nvs_fs *nvs_fs, uint16_t *count);
/**
 * @brief Update the number of stored pairs.
 *
 * @param[inout] nvs_fs NVS file system in which to store the entry.
 * @param[in] count Updated number of pairs stored in NVS.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t kv_storage_nvs_set_pairs_number(struct nvs_fs *nvs_fs, uint16_t count);
/**
 * @brief Get data for the NVS entry at the provided ID.
 *
 * @param[inout] nvs_fs NVS file system from which to fetch the entry.
 * @param[in] entry_id NVS ID for the entry to read.
 * @param[out] data Buffer where to store the NVS entry data, can be NULL.
 * @param[inout] data_size When @p data is non NULL should correspond the the size of @p data.
 * Upon success it will be set to the required size to store the data.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t kv_storage_nvs_read_entry(
    struct nvs_fs *nvs_fs, uint16_t entry_id, void *data, size_t *data_size);
/**
 * @brief Get value for the NVS entry at the provided ID. Dynamically allocates the data struct.
 *
 * @param[inout] nvs_fs NVS file system from which to fetch the entry.
 * @param[in] entry_id NVS ID for the entry to read.
 * @param[out] data On success will point to a dynamically allocated buffer containing the data.
 * @param[inout] data_size The size of the allocated @p data buffer.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t kv_storage_nvs_read_entry_alloc(
    struct nvs_fs *nvs_fs, uint16_t entry_id, void **data, size_t *data_size);
/**
 * @brief Write a key-value pair using an NVS base ID.
 *
 * @param[inout] nvs_fs NVS file system to use.
 * @param[in] base_id Base NVS ID where to store the key-value pair.
 * @param[in] namespace Namespace for the key-value pair.
 * @param[in] key Key for the key-value pair.
 * @param[in] value Value for the key-value pair.
 * @param[in] value_size Size of the value array for the key-value pair.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t kv_storage_nvs_write_pair(struct nvs_fs *nvs_fs, uint16_t base_id,
    const char *namespace, const char *key, const void *value, size_t value_size);
/**
 * @brief Relocate a single key-value pair.
 *
 * @param[inout] nvs_fs NVS file system to use.
 * @param[in] dst_base_id Destination NVS base ID where the pair should be relocated.
 * @param[in] src_base_id Source NVS base ID where the pair should be relocated.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t kv_storage_nvs_relocate_pair(
    struct nvs_fs *nvs_fs, uint16_t dst_base_id, uint16_t src_base_id);
/**
 * @brief Scans storage for duplicate keys caused by interrupted delete operations.
 *
 * @param[in,out] nvs_fs NVS file system to use.
 */
astarte_result_t kv_storage_nvs_remove_duplicates(struct nvs_fs *nvs_fs);

#endif // KV_STORAGE_NVS_H
