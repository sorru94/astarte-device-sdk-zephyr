/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KEY_VALUE_ENTRY_DELETE_H
#define KEY_VALUE_ENTRY_DELETE_H

/**
 * @file key_value/entry_delete.h
 * @brief Deletion logic for the key-value persistent storage implementation.
 */

#include <stdint.h>

#include "astarte_device_sdk/result.h"

#include <zephyr/version.h>

#if KERNEL_VERSION_NUMBER >= ZEPHYR_VERSION(4, 4, 0)
#include <zephyr/kvss/zms.h>
#else
#include <zephyr/fs/zms.h>
#endif

/**
 * @brief Deletes a specific key-value entry and repairs the linked list integrity.
 *
 * @param[inout] zms_fs ZMS file system.
 * @param[in] idx Valid ZMS ID of the entry to delete.
 * @return ASTARTE_RESULT_OK or error code.
 */
astarte_result_t astarte_key_value_entry_delete(struct zms_fs *zms_fs, uint32_t idx);

/**
 * @brief Resumes a shift operation starting from a known hole ID.
 *
 * @param[inout] zms_fs ZMS file system.
 * @param[in] hole_id The ZMS ID of the newly deleted entry (the hole).
 * @return ASTARTE_RESULT_OK or error code.
 */
astarte_result_t astarte_key_value_entry_delete_resume_shift(
    struct zms_fs *zms_fs, uint32_t hole_id);

#endif // KEY_VALUE_ENTRY_DELETE_H
