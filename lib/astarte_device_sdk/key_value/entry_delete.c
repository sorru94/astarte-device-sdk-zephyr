/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "key_value/entry_delete.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "key_value/entry.h"
#include "key_value/entry_hash.h"
#include "key_value/entry_header.h"
#include "key_value/entry_intent.h"
#include "key_value/entry_list.h"
#include "log.h"

ASTARTE_LOG_MODULE_DECLARE(astarte_key_value, CONFIG_ASTARTE_DEVICE_SDK_KEY_VALUE_LOG_LEVEL);

// TODO: this deletion system could be improved, consider adding a collision next ID field to the
// entry header and storing it in the colliding entry. This increases the occupied space but
// reduces the worst case O(N) shift penality

/************************************************
 *         Static functions declaration         *
 ***********************************************/

static astarte_result_t shift_back_single_entry(
    struct zms_fs *zms_fs, uint32_t idx, uint32_t hole_id, bool *shift_performed);
static astarte_result_t update_shifted_entry_neighbors(
    struct zms_fs *zms_fs, const struct astarte_key_value_entry_header *header, uint32_t hole_id);
static astarte_result_t delete_and_unlink_single_entry(struct zms_fs *zms_fs, uint32_t idx);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_key_value_entry_delete(struct zms_fs *zms_fs, uint32_t idx)
{
    astarte_result_t ares = delete_and_unlink_single_entry(zms_fs, idx);
    if (ares == ASTARTE_RESULT_NOT_FOUND) {
        return ASTARTE_RESULT_OK;
    }
    if (ares != ASTARTE_RESULT_OK) {
        return ares;
    }

    // The entry is safely unlinked and deleted. We now transition to SHIFTING to fill the hole.
    ares = astarte_key_value_entry_intent_write(zms_fs, ASTARTE_KEY_VALUE_ENTRY_INTENT_SHIFTING,
        idx, ASTARTE_KEY_VALUE_ENTRY_NULL_ID, ASTARTE_KEY_VALUE_ENTRY_NULL_ID);
    if (ares != ASTARTE_RESULT_OK) {
        return ares;
    }

    ares = astarte_key_value_entry_delete_resume_shift(zms_fs, idx);
    if (ares != ASTARTE_RESULT_OK) {
        return ares;
    }

    // Clear intent block once the shift loop has fully repaired the hash map
    return astarte_key_value_entry_intent_clear(zms_fs);
}

astarte_result_t astarte_key_value_entry_delete_resume_shift(
    struct zms_fs *zms_fs, uint32_t hole_id)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    uint32_t curr_id = hole_id + 1;

    if (curr_id > ASTARTE_KEY_VALUE_ENTRY_MAX_USABLE_ID) {
        curr_id = ASTARTE_KEY_VALUE_ENTRY_MIN_USABLE_ID;
    }

    while (curr_id != hole_id) {
        bool shift_performed = false;
        ares = shift_back_single_entry(zms_fs, curr_id, hole_id, &shift_performed);
        if (ares == ASTARTE_RESULT_NOT_FOUND) {
            // No more entries to shift back
            break;
        }
        if (ares != ASTARTE_RESULT_OK) {
            return ares;
        }

        // The new hole is the physical position we just vacated
        if (shift_performed) {
            hole_id = curr_id;

            // Update the intent block so recovery knows the hole has moved
            ares = astarte_key_value_entry_intent_write(zms_fs,
                ASTARTE_KEY_VALUE_ENTRY_INTENT_SHIFTING, hole_id, ASTARTE_KEY_VALUE_ENTRY_NULL_ID,
                ASTARTE_KEY_VALUE_ENTRY_NULL_ID);
            if (ares != ASTARTE_RESULT_OK) {
                ASTARTE_LOG_ERR("Failed to update shift intent for new hole %d", hole_id);
                return ares;
            }
        }

        curr_id++;
        if (curr_id > ASTARTE_KEY_VALUE_ENTRY_MAX_USABLE_ID) {
            curr_id = ASTARTE_KEY_VALUE_ENTRY_MIN_USABLE_ID;
        }
    }

    return ASTARTE_RESULT_OK;
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static astarte_result_t shift_back_single_entry(
    struct zms_fs *zms_fs, uint32_t idx, uint32_t hole_id, bool *shift_performed)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    uint8_t *raw_entry = NULL;
    ssize_t raw_entry_size = 0;
    struct astarte_key_value_entry_header header = { 0 };

    if (idx <= 1) {
        ASTARTE_LOG_ERR("Attempting to shift back entry with ID %d", idx);
        ares = ASTARTE_RESULT_INTERNAL_ERROR;
        goto exit;
    }

    uint32_t source_id = idx;

    // Read the source entry to be shifted back
    ares = astarte_key_value_entry_header_read(zms_fs, source_id, &header, &raw_entry_size);
    if (ares != ASTARTE_RESULT_OK) {
        if (ares != ASTARTE_RESULT_NOT_FOUND) {
            ASTARTE_LOG_ERR("Failed in reading header for entry with ID %d", idx);
        }
        goto exit;
    }

    raw_entry = astarte_calloc(raw_entry_size, sizeof(uint8_t));
    if (!raw_entry) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        ares = ASTARTE_RESULT_INTERNAL_ERROR;
        goto exit;
    }

    ssize_t ret = zms_read(zms_fs, source_id, raw_entry, raw_entry_size);
    if (ret != raw_entry_size) {
        ASTARTE_LOG_ERR("Failed in reading entry with ID %d", idx);
        ares = ASTARTE_RESULT_ZMS_ERROR;
        goto exit;
    }

    // Calculate the natural hash for the entry to be shifted back
    // And the cyclic absolute distances from the natural_hash
    uint32_t natural_hash = astarte_key_value_entry_hash_generate(header.namespace, header.key);
    uint32_t range
        = ASTARTE_KEY_VALUE_ENTRY_MAX_USABLE_ID - ASTARTE_KEY_VALUE_ENTRY_MIN_USABLE_ID + 1;
    uint32_t d_curr = (source_id >= natural_hash) ? (source_id - natural_hash)
                                                  : (range - (natural_hash - source_id));
    uint32_t d_hole
        = (hole_id >= natural_hash) ? (hole_id - natural_hash) : (range - (natural_hash - hole_id));

    // If the hole is closer to the natural hash than the current element's distance
    if (d_hole < d_curr) {
        // Shift the current entry to the hole's position
        ret = zms_write(zms_fs, hole_id, raw_entry, raw_entry_size);
        if (ret < 0) {
            ASTARTE_LOG_ERR("Failed in writing entry with ID %d", idx);
            ares = ASTARTE_RESULT_ZMS_ERROR;
            goto exit;
        }

        // Update linked list pointers of the physically moved entry's neighbors
        ares = update_shifted_entry_neighbors(zms_fs, &header, hole_id);
        if (ares != ASTARTE_RESULT_OK) {
            goto exit;
        }

        // Clean up the entry from its old position
        ret = zms_delete(zms_fs, source_id);
        if (ret < 0) {
            ares = ASTARTE_RESULT_ZMS_ERROR;
            goto exit;
        }

        *shift_performed = true;
    } else {
        *shift_performed = false;
    }

exit:
    astarte_free(raw_entry);
    astarte_key_value_entry_header_free(&header);
    return ares;
}

static astarte_result_t update_shifted_entry_neighbors(
    struct zms_fs *zms_fs, const struct astarte_key_value_entry_header *header, uint32_t hole_id)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    uint32_t head_id = ASTARTE_KEY_VALUE_ENTRY_NULL_ID;
    uint32_t tail_id = ASTARTE_KEY_VALUE_ENTRY_NULL_ID;

    // Update previous neighbor or Head
    if (header->fixed_header.prev_id != ASTARTE_KEY_VALUE_ENTRY_NULL_ID) {
        ares = astarte_key_value_entry_list_update_next_id(
            zms_fs, header->fixed_header.prev_id, hole_id);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Failed in updating next ID for prev entry");
            return ares;
        }
    } else {
        ares = astarte_key_value_entry_list_read_head_and_tail_ids(zms_fs, &head_id, &tail_id);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Failed in reading head and tail IDs");
            return ares;
        }
        ares = astarte_key_value_entry_list_write_head_and_tail_ids(zms_fs, hole_id, tail_id);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Failed in writing head and tail IDs");
            return ares;
        }
    }

    // Update next neighbor or Tail
    if (header->fixed_header.next_id != ASTARTE_KEY_VALUE_ENTRY_NULL_ID) {
        ares = astarte_key_value_entry_list_update_prev_id(
            zms_fs, header->fixed_header.next_id, hole_id);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Failed in updating prev ID for next entry");
            return ares;
        }
    } else {
        ares = astarte_key_value_entry_list_read_head_and_tail_ids(zms_fs, &head_id, &tail_id);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Failed in reading head and tail IDs");
            return ares;
        }
        ares = astarte_key_value_entry_list_write_head_and_tail_ids(zms_fs, head_id, hole_id);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Failed in writing head and tail IDs");
            return ares;
        }
    }

    return ASTARTE_RESULT_OK;
}

static astarte_result_t delete_and_unlink_single_entry(struct zms_fs *zms_fs, uint32_t idx)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    // Fetch next and previous entries as well as head and tail
    struct astarte_key_value_entry_header_fixed fixed_header = { 0 };
    size_t raw_entry_size = 0;
    ares = astarte_key_value_entry_header_read_fixed(zms_fs, idx, &fixed_header, &raw_entry_size);
    if (ares != ASTARTE_RESULT_OK) {
        return ares;
    }
    uint32_t next_id = fixed_header.next_id;
    uint32_t prev_id = fixed_header.prev_id;

    // Log the intent before modifying any linked list pointers
    ares = astarte_key_value_entry_intent_write(
        zms_fs, ASTARTE_KEY_VALUE_ENTRY_INTENT_DELETING, idx, prev_id, next_id);
    if (ares != ASTARTE_RESULT_OK) {
        return ares;
    }

    bool update_head_tail_ids = false;
    uint32_t head_id = ASTARTE_KEY_VALUE_ENTRY_NULL_ID;
    uint32_t tail_id = ASTARTE_KEY_VALUE_ENTRY_NULL_ID;
    ares = astarte_key_value_entry_list_read_head_and_tail_ids(zms_fs, &head_id, &tail_id);
    if (ares != ASTARTE_RESULT_OK) {
        return ares;
    }

    // Update the previous entry next id to the next one of the entry to delete
    // Or if this entry is the head set the head to the next entry
    if (prev_id != ASTARTE_KEY_VALUE_ENTRY_NULL_ID) {
        ares = astarte_key_value_entry_list_update_next_id(zms_fs, prev_id, next_id);
        if (ares != ASTARTE_RESULT_OK) {
            return ares;
        }
    } else {
        update_head_tail_ids = true;
        head_id = next_id;
    }

    // Update the next entry previous id to the previous one of the entry to delete
    // Or if this node is the tail set the tail to the previous entry
    if (next_id != ASTARTE_KEY_VALUE_ENTRY_NULL_ID) {
        ares = astarte_key_value_entry_list_update_prev_id(zms_fs, next_id, prev_id);
        if (ares != ASTARTE_RESULT_OK) {
            return ares;
        }
    } else {
        update_head_tail_ids = true;
        tail_id = prev_id;
    }

    // Update head and tail to the new values
    if (update_head_tail_ids) {
        ares = astarte_key_value_entry_list_write_head_and_tail_ids(zms_fs, head_id, tail_id);
        if (ares != ASTARTE_RESULT_OK) {
            return ares;
        }
    }

    // Delete the entry
    ssize_t ret = zms_delete(zms_fs, idx);
    if (ret < 0) {
        return ASTARTE_RESULT_ZMS_ERROR;
    }

    return ASTARTE_RESULT_OK;
}
