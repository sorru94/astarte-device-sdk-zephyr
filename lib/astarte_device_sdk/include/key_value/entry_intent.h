/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KEY_VALUE_ENTRY_INTENT_H
#define KEY_VALUE_ENTRY_INTENT_H

/**
 * @file key_value/entry_intent.h
 * @brief Intent block (write-ahead log) definitions to prevent corruption during power loss.
 */

#include "astarte_device_sdk/result.h"
#include <stdint.h>

#include <zephyr/version.h>

#if (KERNEL_VERSION_MAJOR >= 4) && (KERNEL_VERSION_MINOR >= 4)
#include <zephyr/kvss/nvs.h>
#else
#include <zephyr/fs/nvs.h>
#endif

/** @brief Enum defining the current in-flight multi-step operation. */
typedef enum
{
    /** @brief No intent stored, no previous operation failed and was not recovered. */
    ASTARTE_KEY_VALUE_ENTRY_INTENT_NONE = 0,
    /** @brief Insert operation failed. */
    ASTARTE_KEY_VALUE_ENTRY_INTENT_INSERTING = 1,
    /** @brief Update operation failed. */
    ASTARTE_KEY_VALUE_ENTRY_INTENT_UPDATING = 2,
    /** @brief Delete operation failed. */
    ASTARTE_KEY_VALUE_ENTRY_INTENT_DELETING = 3,
    /** @brief Shift operation failed. */
    ASTARTE_KEY_VALUE_ENTRY_INTENT_SHIFTING = 4
} astarte_key_value_entry_intent_state_t;

/** @brief Data structure representing an in-flight NVS operation. */
struct astarte_key_value_entry_intent
{
    /** @brief The type of operation in progress. */
    uint8_t state;
    /** @brief The primary NVS ID being modified (e.g., the new entry ID). */
    uint16_t target_id;
    /** @brief Auxiliary ID 1 (e.g., prev_id). */
    uint16_t affected_id_1;
    /** @brief Auxiliary ID 2 (e.g., next_id). */
    uint16_t affected_id_2;
};

/**
 * @brief Writes an intent block to NVS before beginning a multi-step operation.
 *
 * @param[inout] nvs_fs NVS file system.
 * @param[in] state The operation state.
 * @param[in] target_id Primary ID involved in the operation.
 * @param[in] affected_id_1 First auxiliary ID.
 * @param[in] affected_id_2 Second auxiliary ID.
 * @return ASTARTE_RESULT_OK or error code.
 */
astarte_result_t astarte_key_value_entry_intent_write(struct nvs_fs *nvs_fs,
    astarte_key_value_entry_intent_state_t state, uint16_t target_id, uint16_t affected_id_1,
    uint16_t affected_id_2);

/**
 * @brief Clears the current intent block, marking the operation as successful.
 *
 * @param[inout] nvs_fs NVS file system.
 * @return ASTARTE_RESULT_OK or error code.
 */
astarte_result_t astarte_key_value_entry_intent_clear(struct nvs_fs *nvs_fs);

/**
 * @brief Checks for and resolves any pending intents from a previous interrupted operation.
 *
 * @param[inout] nvs_fs NVS file system.
 * @return ASTARTE_RESULT_OK or error code.
 */
astarte_result_t astarte_key_value_entry_intent_resolve(struct nvs_fs *nvs_fs);

#endif // KEY_VALUE_ENTRY_INTENT_H
