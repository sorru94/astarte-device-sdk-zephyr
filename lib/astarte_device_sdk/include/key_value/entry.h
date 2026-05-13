/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KEY_VALUE_ENTRY_H
#define KEY_VALUE_ENTRY_H

/**
 * @file key_value/entry.h
 * @brief Entry functions for the key-value persistent storage implementation.
 */

#include <stdbool.h>
#include <stdint.h>

#include "astarte_device_sdk/result.h"

#include <zephyr/version.h>

#if (KERNEL_VERSION_MAJOR >= 4) && (KERNEL_VERSION_MINOR >= 4)
#include <zephyr/kvss/nvs.h>
#else
#include <zephyr/fs/nvs.h>
#endif

/** @brief Special identifier representing a null/invalid pointer in the linked list. */
#define ASTARTE_KEY_VALUE_ENTRY_NULL_ID UINT16_MAX

/** @brief Reserved specialized NVS ID to store the format version. */
#define ASTARTE_KEY_VALUE_ENTRY_VERSION_ID (ASTARTE_KEY_VALUE_ENTRY_NULL_ID - 1)

/** @brief Specialized NVS ID reserved to store the head and tail pointers of the linked list. */
#define ASTARTE_KEY_VALUE_ENTRY_HEAD_AND_TAIL_ID (ASTARTE_KEY_VALUE_ENTRY_VERSION_ID - 1)

/** @brief Reserved specialized NVS ID to store the Intent Block. */
#define ASTARTE_KEY_VALUE_ENTRY_INTENT_ID (ASTARTE_KEY_VALUE_ENTRY_HEAD_AND_TAIL_ID - 1)

/** @brief The maximum NVS ID allowable for normal key-value entries. */
#define ASTARTE_KEY_VALUE_ENTRY_MAX_USABLE_ID (ASTARTE_KEY_VALUE_ENTRY_INTENT_ID - 1)

/**
 * @brief Finds an existing NVS ID via hash and probing, or allocates an available one.
 *
 * @note This function may return ASTARTE_RESULT_NOT_FOUND only when @p allocate is set to false.
 *
 * @param[inout] nvs_fs NVS file system.
 * @param[in] namespace Target namespace string.
 * @param[in] key Target key string.
 * @param[out] idx Returns the matched or allocated ID.
 * @param[in] allocate True if a new ID should be returned upon not finding the key.
 * @retval ASTARTE_RESULT_OK The operation was performed correctly.
 * @retval ASTARTE_RESULT_NOT_FOUND The specified index was not found in NVS.
 * @retval ASTARTE_RESULT_OUT_OF_MEMORY Dynamic allocation failure due to a lack of memory.
 * @retval ASTARTE_RESULT_NVS_ERROR An underlying NVS file system error occurred.
 * @retval ASTARTE_RESULT_KEY_VALUE_FULL When the storage is full and no match has occurred.
 */
astarte_result_t astarte_key_value_entry_find_or_alloc(
    struct nvs_fs *nvs_fs, const char *namespace, const char *key, uint16_t *idx, bool allocate);

/**
 * @brief Writes an atomic combined payload to the specified ID.
 *
 * @param[inout] nvs_fs NVS file system.
 * @param[in] idx Base NVS ID.
 * @param[in] namespace Target namespace string.
 * @param[in] key Target key string.
 * @param[in] value Target value block.
 * @param[in] value_size Target value block size.
 * @retval ASTARTE_RESULT_OK The operation was performed correctly.
 * @retval ASTARTE_RESULT_NVS_ERROR An underlying NVS file system error occurred.
 * @retval ASTARTE_RESULT_OUT_OF_MEMORY Dynamic allocation failure due to a lack of memory.
 */
astarte_result_t astarte_key_value_entry_write(struct nvs_fs *nvs_fs, uint16_t idx,
    const char *namespace, const char *key, const void *value, size_t value_size);

/**
 * @brief Retrieves a previously stored value from a combined record payload.
 *
 * @param[inout] nvs_fs NVS file system.
 * @param[in] idx Valid NVS ID.
 * @param[out] value Preallocated memory to store the retrieved data. Can be NULL to query size.
 * @param[inout] value_size Pass size of value block, returns data stored.
 * @retval ASTARTE_RESULT_OK The operation was performed correctly.
 * @retval ASTARTE_RESULT_INVALID_PARAM One of the provided parameters is invalid.
 * @retval ASTARTE_RESULT_NOT_FOUND The specified index was not found in NVS.
 * @retval ASTARTE_RESULT_NVS_ERROR An underlying NVS file system error occurred.
 * @retval ASTARTE_RESULT_INTERNAL_ERROR An internal error.
 * @retval ASTARTE_RESULT_OUT_OF_MEMORY Dynamic allocation failure due to a lack of memory.
 */
astarte_result_t astarte_key_value_entry_read_value(
    struct nvs_fs *nvs_fs, uint16_t idx, void *value, size_t *value_size);

/**
 * @brief Retrieves a previously stored key string from a combined record payload.
 *
 * @param[inout] nvs_fs NVS file system.
 * @param[in] idx Valid NVS ID.
 * @param[out] key Preallocated memory to store the retrieved string. Can be NULL to query size.
 * @param[inout] key_size Pass size of key block, returns length stored (+1 for null terminator).
 * @retval ASTARTE_RESULT_OK The operation was performed correctly.
 * @retval ASTARTE_RESULT_INVALID_PARAM One of the provided parameters is invalid.
 * @retval ASTARTE_RESULT_OUT_OF_MEMORY Dynamic allocation failure due to a lack of memory.
 * @retval ASTARTE_RESULT_NOT_FOUND The specified index was not found in NVS.
 * @retval ASTARTE_RESULT_NVS_ERROR An underlying NVS file system error occurred.
 */
astarte_result_t astarte_key_value_entry_read_key(
    struct nvs_fs *nvs_fs, uint16_t idx, char *key, size_t *key_size);

/**
 * @brief Evaluates whether a certain NVS ID belongs to a given namespace.
 *
 * @param[inout] nvs_fs NVS file system.
 * @param[in] idx Valid NVS ID to evaluate.
 * @param[in] namespace Namespace to match against.
 * @param[out] matches Evaluated to true if it matches, else false.
 * @retval ASTARTE_RESULT_OK The operation was performed correctly.
 * @retval ASTARTE_RESULT_INVALID_PARAM One of the provided parameters is invalid.
 * @retval ASTARTE_RESULT_OUT_OF_MEMORY Dynamic allocation failure due to a lack of memory.
 * @retval ASTARTE_RESULT_NVS_ERROR An underlying NVS file system error occurred.
 */
astarte_result_t astarte_key_value_entry_check_namespace(
    struct nvs_fs *nvs_fs, uint16_t idx, const char *namespace, bool *matches);

/**
 * @brief Retrieves the next ID in the linked list of entries.
 *
 * @param[inout] nvs_fs NVS file system.
 * @param[in] idx Valid NVS ID of the current entry, or UINT16_MAX to retrieve the head ID.
 * @param[out] next_id Pointer to store the retrieved next ID.
 * @retval ASTARTE_RESULT_OK The operation was performed correctly.
 * @retval ASTARTE_RESULT_NVS_ERROR An underlying NVS file system error occurred.
 * @retval ASTARTE_RESULT_NOT_FOUND The specified index was not found in NVS.
 */
astarte_result_t astarte_key_value_entry_get_next_id(
    struct nvs_fs *nvs_fs, uint16_t idx, uint16_t *next_id);

/**
 * @brief Retrieves the previous ID in the linked list of entries.
 *
 * @param[inout] nvs_fs NVS file system.
 * @param[in] idx Valid NVS ID of the current entry, or UINT16_MAX to retrieve the head ID.
 * @param[out] prev_id Pointer to store the retrieved previous ID.
 * @retval ASTARTE_RESULT_OK The operation was performed correctly.
 * @retval ASTARTE_RESULT_NVS_ERROR An underlying NVS file system error occurred.
 * @retval ASTARTE_RESULT_NOT_FOUND The specified index was not found in NVS.
 */
astarte_result_t astarte_key_value_entry_get_prev_id(
    struct nvs_fs *nvs_fs, uint16_t idx, uint16_t *prev_id);

#endif // KEY_VALUE_ENTRY_H
