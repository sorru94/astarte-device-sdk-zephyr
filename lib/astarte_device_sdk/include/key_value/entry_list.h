/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KEY_VALUE_ENTRY_LIST_H
#define KEY_VALUE_ENTRY_LIST_H

/**
 * @file key_value/entry_list.h
 * @brief Entry list function helpers.
 */

#include <stdbool.h>
#include <stdint.h>

#include "astarte_device_sdk/result.h"

#include <zephyr/version.h>

#if KERNEL_VERSION_NUMBER >= ZEPHYR_VERSION(4, 4, 0)
#include <zephyr/kvss/zms.h>
#else
#include <zephyr/fs/zms.h>
#endif

/**
 * @brief Computes or retrieves the next and previous IDs for an entry based on list state.
 *
 * @param[inout] zms_fs ZMS file system.
 * @param[in] idx ZMS ID to compute relative connectivity for.
 * @param[out] next_id Pointer to store the evaluated next ID.
 * @param[out] prev_id Pointer to store the evaluated previous ID.
 * @retval ASTARTE_RESULT_OK The operation was performed correctly.
 * @retval ASTARTE_RESULT_NOT_FOUND The specified index was not found in ZMS.
 * @retval ASTARTE_RESULT_ZMS_ERROR An underlying ZMS file system error occurred.
 */
astarte_result_t astarte_key_value_entry_list_compute_next_and_prev_ids(
    struct zms_fs *zms_fs, uint32_t idx, uint32_t *next_id, uint32_t *prev_id);

/**
 * @brief Reads the current head and tail IDs for the linked list from storage.
 *
 * @param[inout] zms_fs ZMS file system.
 * @param[out] head_id Pointer to store the retrieved head ID.
 * @param[out] tail_id Pointer to store the retrieved tail ID.
 * @retval ASTARTE_RESULT_OK The operation was performed correctly.
 * @retval ASTARTE_RESULT_ZMS_ERROR An underlying ZMS file system error occurred.
 */
astarte_result_t astarte_key_value_entry_list_read_head_and_tail_ids(
    struct zms_fs *zms_fs, uint32_t *head_id, uint32_t *tail_id);

/**
 * @brief Writes updated head and tail IDs to storage.
 *
 * @param[inout] zms_fs ZMS file system.
 * @param[in] head_id The new head ID to store.
 * @param[in] tail_id The new tail ID to store.
 * @retval ASTARTE_RESULT_OK The operation was performed correctly.
 * @retval ASTARTE_RESULT_ZMS_ERROR An underlying ZMS file system error occurred.
 */
astarte_result_t astarte_key_value_entry_list_write_head_and_tail_ids(
    struct zms_fs *zms_fs, uint32_t head_id, uint32_t tail_id);

/**
 * @brief Updates the next ID pointer of a specific list entry.
 *
 * @param[inout] zms_fs ZMS file system.
 * @param[in] idx Target valid ZMS ID to update.
 * @param[in] new_next The new next ID for this entry.
 * @retval ASTARTE_RESULT_OK The operation was performed correctly.
 * @retval ASTARTE_RESULT_OUT_OF_MEMORY Dynamic allocation failure due to a lack of memory.
 * @retval ASTARTE_RESULT_ZMS_ERROR An underlying ZMS file system error occurred.
 */
astarte_result_t astarte_key_value_entry_list_update_next_id(
    struct zms_fs *zms_fs, uint32_t idx, uint32_t new_next);

/**
 * @brief Updates the previous ID pointer of a specific list entry.
 *
 * @param[inout] zms_fs ZMS file system.
 * @param[in] idx Target valid ZMS ID to update.
 * @param[in] new_prev The new previous ID for this entry.
 * @retval ASTARTE_RESULT_OK The operation was performed correctly.
 * @retval ASTARTE_RESULT_OUT_OF_MEMORY Dynamic allocation failure due to a lack of memory.
 * @retval ASTARTE_RESULT_ZMS_ERROR An underlying ZMS file system error occurred.
 */
astarte_result_t astarte_key_value_entry_list_update_prev_id(
    struct zms_fs *zms_fs, uint32_t idx, uint32_t new_prev);

#endif // KEY_VALUE_ENTRY_LIST_H
