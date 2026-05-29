/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "key_value/entry_intent.h"

#include "key_value/entry.h"
#include "key_value/entry_delete.h"
#include "key_value/entry_header.h"
#include "key_value/entry_list.h"
#include "log.h"

ASTARTE_LOG_MODULE_DECLARE(astarte_key_value, CONFIG_ASTARTE_DEVICE_SDK_KEY_VALUE_LOG_LEVEL);

/************************************************
 *         Static functions declaration         *
 ***********************************************/

static astarte_result_t resolve_insert_intent(
    struct zms_fs *zms_fs, struct astarte_key_value_entry_intent *intent);
static astarte_result_t resolve_delete_intent(
    struct zms_fs *zms_fs, struct astarte_key_value_entry_intent *intent);
static astarte_result_t resolve_shift_intent(
    struct zms_fs *zms_fs, struct astarte_key_value_entry_intent *intent);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_key_value_entry_intent_write(struct zms_fs *zms_fs,
    astarte_key_value_entry_intent_state_t state, uint32_t target_id, uint32_t affected_id_1,
    uint32_t affected_id_2)
{
    struct astarte_key_value_entry_intent intent = { .state = (uint8_t) state,
        .target_id = target_id,
        .affected_id_1 = affected_id_1,
        .affected_id_2 = affected_id_2 };

    ssize_t ret = zms_write(zms_fs, ASTARTE_KEY_VALUE_ENTRY_INTENT_ID, &intent, sizeof(intent));
    if (ret < 0) {
        ASTARTE_LOG_ERR("Failed to write Intent Block: %d", (int) ret);
        return ASTARTE_RESULT_ZMS_ERROR;
    }

    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_key_value_entry_intent_clear(struct zms_fs *zms_fs)
{
    // Writing an empty block with ASTARTE_KEY_VALUE_ENTRY_INTENT_NONE effectively clears it
    struct astarte_key_value_entry_intent intent = { 0 };
    intent.state = ASTARTE_KEY_VALUE_ENTRY_INTENT_NONE;

    ssize_t ret = zms_write(zms_fs, ASTARTE_KEY_VALUE_ENTRY_INTENT_ID, &intent, sizeof(intent));
    if (ret < 0) {
        ASTARTE_LOG_ERR("Failed to clear Intent Block: %d", (int) ret);
        return ASTARTE_RESULT_ZMS_ERROR;
    }

    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_key_value_entry_intent_resolve(struct zms_fs *zms_fs)
{
    struct astarte_key_value_entry_intent intent = { 0 };
    astarte_result_t ares = ASTARTE_RESULT_OK;
    ssize_t ret = zms_read(zms_fs, ASTARTE_KEY_VALUE_ENTRY_INTENT_ID, &intent, sizeof(intent));

    // If the block is empty or explicitly clear, the system is healthy.
    if (ret == -ENOENT || intent.state == ASTARTE_KEY_VALUE_ENTRY_INTENT_NONE) {
        return ASTARTE_RESULT_OK;
    }
    if (ret != sizeof(intent)) {
        ASTARTE_LOG_ERR("Failed to read Intent Block: %d", (int) ret);
        return ASTARTE_RESULT_ZMS_ERROR;
    }

    ASTARTE_LOG_WRN("Found interrupted operation (state %d). Initiating recovery...", intent.state);

    if (intent.state == ASTARTE_KEY_VALUE_ENTRY_INTENT_INSERTING) {
        ares = resolve_insert_intent(zms_fs, &intent);
    } else if (intent.state == ASTARTE_KEY_VALUE_ENTRY_INTENT_UPDATING) {
        ASTARTE_LOG_INF("Found ghost update intent for ID %d.", intent.target_id);
    } else if (intent.state == ASTARTE_KEY_VALUE_ENTRY_INTENT_DELETING) {
        ares = resolve_delete_intent(zms_fs, &intent);
    }

    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Fatal error during intent resolution. Storage is corrupted.");
        return ASTARTE_RESULT_KEY_VALUE_RECOVERY_FAILED;
    }

    // This block is divided from the rest as a deleting intent could immediately transition to a
    // shifting one
    if (intent.state == ASTARTE_KEY_VALUE_ENTRY_INTENT_SHIFTING) {
        ares = resolve_shift_intent(zms_fs, &intent);
    }

    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Fatal error during intent resolution. Storage is corrupted.");
        return ASTARTE_RESULT_KEY_VALUE_RECOVERY_FAILED;
    }

    // Clear the intent block now that we have cleaned up the ZMS state
    return astarte_key_value_entry_intent_clear(zms_fs);
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static astarte_result_t resolve_insert_intent(
    struct zms_fs *zms_fs, struct astarte_key_value_entry_intent *intent)
{
    /*
     * ZMS Flash Write Operations Breakdown
     *
     * 1. Updating an existing entry (1 writes)
     * When updating an existing key-value pair, the driver does not need to modify the linked
     * list pointers.
     * - Write 1: zms_write writes the actual updated serialized payload to the target ID.
     *
     * 2. Inserting the first entry (empty list) (2 writes)
     * When inserting the very first entry into the storage, the driver must update the global
     * pointers but doesn't have a previous neighbor to update.
     * - Write 1: zms_write writes the serialized payload.
     * - Write 2: astarte_key_value_entry_list_write_head_and_tail_ids initializes the global
     * head and tail ZMS ID.
     *
     * 3. Inserting a new entry (populated list) (3 writes)
     * When appending a new entry to an existing list, the driver must link the previous tail to
     * the new entry.
     * - Write 1: zms_write writes the serialized payload.
     * - Write 2: astarte_key_value_entry_list_update_next_id overwrites the previous tail's
     * payload to update its next_id pointer.
     * - Write 3: astarte_key_value_entry_list_write_head_and_tail_ids updates the global head
     * and tail ZMS ID.
     */

    astarte_result_t ares = ASTARTE_RESULT_OK;
    uint32_t head_id = ASTARTE_KEY_VALUE_ENTRY_NULL_ID;
    uint32_t tail_id = ASTARTE_KEY_VALUE_ENTRY_NULL_ID;
    ares = astarte_key_value_entry_list_read_head_and_tail_ids(zms_fs, &head_id, &tail_id);
    if (ares != ASTARTE_RESULT_OK) {
        return ares;
    }
    if (tail_id == intent->target_id) {
        ASTARTE_LOG_INF(
            "Insertion %d was already completed. Clearing ghost intent.", intent->target_id);
        return ares;
    }

    ASTARTE_LOG_WRN("Rolling back incomplete insertion for ID %d", intent->target_id);

    uint32_t prev_id = intent->affected_id_1;

    // Revert the dangling next_id pointer of the previous tail
    if (prev_id != ASTARTE_KEY_VALUE_ENTRY_NULL_ID) {
        ares = astarte_key_value_entry_list_update_next_id(
            zms_fs, prev_id, ASTARTE_KEY_VALUE_ENTRY_NULL_ID);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Failed to repair previous tail pointer %d", prev_id);
            return ares;
        }
    }

    // Safely delete the orphaned payload
    ssize_t del_ret = zms_delete(zms_fs, intent->target_id);
    if (del_ret < 0) {
        ASTARTE_LOG_ERR("Failed to clean up orphaned entry %d", intent->target_id);
        return ares;
    }

    // Clear the intent
    return ares;
}
static astarte_result_t resolve_delete_intent(
    struct zms_fs *zms_fs, struct astarte_key_value_entry_intent *intent)
{
    /*
     * ZMS Flash Write Operations Breakdown
     *
     * 1. Deleting from a list with a single element (2 writes)
     * When the entry is both the head and the tail, no neighbor pointers need updating.
     * - Write 1: astarte_key_value_entry_list_write_head_and_tail_ids clears the global head
     * and tail.
     * - Write 2: zms_delete removes the serialized payload.
     *
     * 2. Deleting a head of a list with more than one element (3 writes)
     * The driver must update the global head and clear the next node's previous pointer.
     * - Write 1: astarte_key_value_entry_list_update_prev_id overwrites the next node to set
     * its prev_id to NULL.
     * - Write 2: astarte_key_value_entry_list_write_head_and_tail_ids updates the head ID.
     * - Write 3: zms_delete removes the serialized payload.
     *
     * 3. Deleting a tail of a list with more than one element (3 writes)
     * The driver must update the global tail and clear the previous node's next pointer.
     * - Write 1: astarte_key_value_entry_list_update_next_id overwrites the previous node to
     * set its next_id to NULL.
     * - Write 2: astarte_key_value_entry_list_write_head_and_tail_ids updates the tail ID.
     * - Write 3: zms_delete removes the serialized payload.
     *
     * 4. Deleting a middle entry for a list with more than 2 elements (3 writes)
     * The driver must link the previous node and next node together.
     * - Write 1: astarte_key_value_entry_list_update_next_id overwrites the previous node to
     * point to the next node.
     * - Write 2: astarte_key_value_entry_list_update_prev_id overwrites the next node to point
     * to the previous node.
     * - Write 3: zms_delete removes the serialized payload.
     */

    // Check if the target entry has already been successfully deleted from ZMS
    astarte_result_t ares = ASTARTE_RESULT_OK;
    struct astarte_key_value_entry_header_fixed check_header = { 0 };
    size_t check_size = 0;
    ares = astarte_key_value_entry_header_read_fixed(
        zms_fs, intent->target_id, &check_header, &check_size);

    if (ares == ASTARTE_RESULT_NOT_FOUND) {
        ASTARTE_LOG_INF("Deletion for ID %d was already completed. Skipping to shift intent.",
            intent->target_id);
    } else {
        ASTARTE_LOG_WRN("Resuming incomplete deletion for ID %d", intent->target_id);

        uint32_t prev_id = intent->affected_id_1;
        uint32_t next_id = intent->affected_id_2;

        // Repair prev_id's next pointer (if it was no head)
        if (prev_id != ASTARTE_KEY_VALUE_ENTRY_NULL_ID) {
            ares = astarte_key_value_entry_list_update_next_id(zms_fs, prev_id, next_id);
            if (ares != ASTARTE_RESULT_OK) {
                ASTARTE_LOG_ERR("Previous node %d missing during recovery", prev_id);
                return ares;
            }
        }

        // Repair next_id's prev pointer (if it was no tail)
        if (next_id != ASTARTE_KEY_VALUE_ENTRY_NULL_ID) {
            ares = astarte_key_value_entry_list_update_prev_id(zms_fs, next_id, prev_id);
            if (ares != ASTARTE_RESULT_OK) {
                ASTARTE_LOG_ERR("Next node %d missing during recovery", prev_id);
                return ares;
            }
        }

        // Repair Head/Tail if the deleted entry was the head or tail
        uint32_t head_id = ASTARTE_KEY_VALUE_ENTRY_NULL_ID;
        uint32_t tail_id = ASTARTE_KEY_VALUE_ENTRY_NULL_ID;
        ares = astarte_key_value_entry_list_read_head_and_tail_ids(zms_fs, &head_id, &tail_id);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Head and tail read failed during recovery");
            return ares;
        }
        bool changed = false;
        if (prev_id == ASTARTE_KEY_VALUE_ENTRY_NULL_ID) {
            head_id = next_id;
            changed = true;
        }
        if (next_id == ASTARTE_KEY_VALUE_ENTRY_NULL_ID) {
            tail_id = prev_id;
            changed = true;
        }
        if (changed) {
            ares = astarte_key_value_entry_list_write_head_and_tail_ids(zms_fs, head_id, tail_id);
            if (ares != ASTARTE_RESULT_OK) {
                ASTARTE_LOG_ERR("Head and tail write failed during recovery");
                return ares;
            }
        }

        // Safely delete the orphaned payload physically from ZMS
        ssize_t del_ret = zms_delete(zms_fs, intent->target_id);
        if (del_ret < 0) {
            return ASTARTE_RESULT_ZMS_ERROR;
        }
    }

    // Transition to shifting state to heal the linear probing gap
    intent->state = ASTARTE_KEY_VALUE_ENTRY_INTENT_SHIFTING;
    return astarte_key_value_entry_intent_write(zms_fs, intent->state, intent->target_id,
        ASTARTE_KEY_VALUE_ENTRY_NULL_ID, ASTARTE_KEY_VALUE_ENTRY_NULL_ID);
}
static astarte_result_t resolve_shift_intent(
    struct zms_fs *zms_fs, struct astarte_key_value_entry_intent *intent)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    // Resolve the true hole ID. If power was lost after a shift but before the intent was
    // updated, the target_id might no longer be a hole.
    // Because linear probing guarantees contiguous entries until the end of a cluster,
    // we scan forward from target_id to find the first empty slot, which is our true hole.
    uint32_t current_hole = intent->target_id;
    struct astarte_key_value_entry_header_fixed dummy_header = { 0 };
    size_t dummy_size = 0;

    while (true) {
        ares = astarte_key_value_entry_header_read_fixed(
            zms_fs, current_hole, &dummy_header, &dummy_size);
        if (ares == ASTARTE_RESULT_NOT_FOUND) {
            break;
        }
        if (ares != ASTARTE_RESULT_OK) {
            return ares;
        }

        current_hole++;
        if (current_hole > ASTARTE_KEY_VALUE_ENTRY_MAX_USABLE_ID) {
            current_hole = 0;
        }
        if (current_hole == intent->target_id) {
            // Failsafe: The hash map is 100% full. This should be impossible during shifting.
            ASTARTE_LOG_ERR("Shift recovery failed: Map is fully saturated without a hole.");
            return ASTARTE_RESULT_KEY_VALUE_RECOVERY_FAILED;
        }
    }

    ASTARTE_LOG_WRN("Resuming incomplete shift for hole ID %d", current_hole);
    return astarte_key_value_entry_delete_resume_shift(zms_fs, current_hole);
}
