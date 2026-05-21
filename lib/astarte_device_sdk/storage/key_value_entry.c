/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "storage/key_value_entry.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/sys/crc.h>

#include "log.h"

ASTARTE_LOG_MODULE_DECLARE(astarte_kv_storage, CONFIG_ASTARTE_DEVICE_SDK_KV_STORAGE_LOG_LEVEL);

// TODO: This driver is weak to power losses in between writes. Consider introducing a tombstone
// method to be more resilient over power losses.

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

#define HEADER_NAMESPACE_LEN_BYTES 2
#define HEADER_KEY_LEN_BYTES 2
#define HEADER_NEXT_ID_BYTES 2
#define HEADER_PREV_ID_BYTES 2
#define FIXED_HEADER_BYTES                                                                         \
    (HEADER_NAMESPACE_LEN_BYTES + HEADER_KEY_LEN_BYTES + HEADER_NEXT_ID_BYTES                      \
        + HEADER_PREV_ID_BYTES)

#define HEAD_AND_TAIL_ID_POSITION (UINT16_MAX - 1)
#define MAX_USABLE_ID (HEAD_AND_TAIL_ID_POSITION - 1)

struct fixed_header
{
    uint16_t namespace_len;
    uint16_t key_len;
    uint16_t next_id;
    uint16_t prev_id;
};

struct header
{
    struct fixed_header fixed_header;
    char *namespace;
    char *key;
    bool dynamically_allocated;
};

/************************************************
 *         Static functions declaration         *
 ***********************************************/

static astarte_result_t shift_back_single_entry(
    struct nvs_fs *nvs_fs, uint16_t idx, uint16_t hole_id, bool *shift_performed);
static astarte_result_t update_shifted_entry_neighbors(
    struct nvs_fs *nvs_fs, const struct header *header, uint16_t hole_id);
static astarte_result_t delete_and_unlink_single_entry(struct nvs_fs *nvs_fs, uint16_t idx);
// If the entry already exists takes the next and previous IDs already stored.
// If it doesn't exist, these will be set to link the new entry at the end of the list.
static astarte_result_t compute_entry_next_and_prev_ids(
    struct nvs_fs *nvs_fs, uint16_t idx, uint16_t *next_id, uint16_t *prev_id);
static uint8_t *serialize_entry(
    struct header header, const void *value, size_t value_size, size_t *serialized_entry_size);
static astarte_result_t check_entry_match(
    struct nvs_fs *nvs_fs, uint16_t idx, const char *namespace, const char *key);
static astarte_result_t read_header(
    struct nvs_fs *nvs_fs, uint16_t idx, struct header *header, size_t *raw_size);
static void free_header(struct header *header);
static astarte_result_t read_fixed_header(
    struct nvs_fs *nvs_fs, uint16_t idx, struct fixed_header *fixed_header, size_t *raw_size);
static uint16_t generate_hash(const char *namespace, const char *key);
static astarte_result_t read_head_and_tail_ids(
    struct nvs_fs *nvs_fs, uint16_t *head_id, uint16_t *tail_id);
static astarte_result_t write_head_and_tail_ids(
    struct nvs_fs *nvs_fs, uint16_t head_id, uint16_t tail_id);
static astarte_result_t update_entry_next_id(
    struct nvs_fs *nvs_fs, uint16_t idx, uint16_t new_next);
static astarte_result_t update_entry_prev_id(
    struct nvs_fs *nvs_fs, uint16_t idx, uint16_t new_prev);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_storage_key_value_entry_find_or_alloc(
    struct nvs_fs *nvs_fs, const char *namespace, const char *key, uint16_t *idx, bool allocate)
{
    uint16_t start_id = generate_hash(namespace, key);
    uint16_t curr_id = start_id;

    do {
        // Check if the namespace and key match the stored values
        astarte_result_t ares = check_entry_match(nvs_fs, curr_id, namespace, key);
        if (ares == ASTARTE_RESULT_NOT_FOUND) {
            if (!allocate) {
                ASTARTE_LOG_DBG("Key not found at ID %d", curr_id);
                return ares;
            }
            ASTARTE_LOG_DBG("Found empty slot at ID %d", curr_id);
            *idx = curr_id;
            return ASTARTE_RESULT_OK;
        }
        if (ares == ASTARTE_RESULT_OK) {
            *idx = curr_id;
            ASTARTE_LOG_DBG("Found matching entry at ID %d", curr_id);
            return ASTARTE_RESULT_OK;
        }
        if (ares != ASTARTE_RESULT_MISMATCH) {
            ASTARTE_LOG_ERR("Error while matching namespace and key at ID: %d", curr_id);
            return ares;
        }
        // If ASTARTE_RESULT_MISMATCH is returned, it means a collision occurred.
        // The loop will continue to the next ID.
        curr_id++;
        if (curr_id == HEAD_AND_TAIL_ID_POSITION) {
            curr_id = 1;
        }
    } while (curr_id != start_id);

    ASTARTE_LOG_ERR("Key-value storage is full");
    return ASTARTE_RESULT_KV_STORAGE_FULL;
}

astarte_result_t astarte_storage_key_value_entry_write(struct nvs_fs *nvs_fs, uint16_t idx,
    const char *namespace, const char *key, const void *value, size_t value_size)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    uint8_t *raw_entry = NULL;
    size_t raw_entry_size = 0;

    // Find the next and previous IDs if the entry already exists to maintain list integrity.
    // Otherwise, these will be set to link the new entry at the end of the list.
    bool is_end_of_list = false;
    uint16_t prev_id = 0;
    uint16_t next_id = 0;
    ares = compute_entry_next_and_prev_ids(nvs_fs, idx, &next_id, &prev_id);
    if (ares == ASTARTE_RESULT_NOT_FOUND) {
        is_end_of_list = true;
        ares = ASTARTE_RESULT_OK;
    } else if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Error computing next and previous IDs for entry at ID %d: %s", idx,
            astarte_result_to_name(ares));
        return ares;
    }

    // Serialize the entry with the computed next and previous IDs
    struct header header = {
        .fixed_header = {
            .namespace_len = strlen(namespace),
            .key_len = strlen(key),
            .next_id = next_id,
            .prev_id = prev_id,
        },
        .namespace = (char *) namespace,
        .key = (char *) key,
        .dynamically_allocated = false,
    };
    raw_entry = serialize_entry(header, value, value_size, &raw_entry_size);
    if (!raw_entry) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto exit;
    }
    // Write the serialized entry to NVS
    ssize_t ret = nvs_write(nvs_fs, idx, raw_entry, raw_entry_size);
    if (ret < 0) {
        ASTARTE_LOG_ERR("Error writing to NVS at ID %d, error: %d", idx, ret);
        ares = ASTARTE_RESULT_NVS_ERROR;
        goto exit;
    }

    // Update the previous tail to point to the new entry and update the head/tail IDs if needed
    if (is_end_of_list) {
        uint16_t head_id = 0;
        uint16_t tail_id = 0;
        ares = read_head_and_tail_ids(nvs_fs, &head_id, &tail_id);
        if (ares != ASTARTE_RESULT_OK) {
            goto exit;
        }

        if (tail_id != 0) {
            // NOLINTNEXTLINE(readability-suspicious-call-argument)
            update_entry_next_id(nvs_fs, tail_id, idx);
        } else {
            head_id = idx;
        }
        tail_id = idx;
        ares = write_head_and_tail_ids(nvs_fs, head_id, tail_id);
        if (ares != ASTARTE_RESULT_OK) {
            goto exit;
        }
    }

exit:
    free(raw_entry);
    return ares;
}

astarte_result_t astarte_storage_key_value_entry_read_value(
    struct nvs_fs *nvs_fs, uint16_t idx, void *value, size_t *value_size)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    uint8_t *raw_entry = NULL;
    size_t raw_entry_size = 0;

    // Read the fixed header
    struct fixed_header fixed_header = { 0 };
    ares = read_fixed_header(nvs_fs, idx, &fixed_header, &raw_entry_size);
    if (ares != ASTARTE_RESULT_OK) {
        goto exit;
    }

    // Calculate the full header size
    size_t header_size = FIXED_HEADER_BYTES + fixed_header.namespace_len + fixed_header.key_len;
    if (header_size > raw_entry_size) {
        ASTARTE_LOG_ERR("Error: Incomplete header at ID %d", idx);
        ares = ASTARTE_RESULT_INTERNAL_ERROR;
        goto exit;
    }

    // Compute the value size and return it immediately if there is no output buffer
    size_t read_value_size = raw_entry_size - header_size;
    if (!value) {
        *value_size = read_value_size;
        goto exit;
    }
    if (*value_size < read_value_size) {
        ASTARTE_LOG_ERR("Error: Value buffer too small at ID %d", idx);
        ares = ASTARTE_RESULT_INVALID_PARAM;
        goto exit;
    }

    // Read the full entry
    raw_entry = calloc(raw_entry_size, sizeof(uint8_t));
    if (!raw_entry) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto exit;
    }
    ssize_t ret = nvs_read(nvs_fs, idx, raw_entry, raw_entry_size);
    if (ret < 0) {
        ASTARTE_LOG_ERR("Error reading full payload from NVS at ID %d, error: %d", idx, ret);
        ares = ASTARTE_RESULT_NVS_ERROR;
        goto exit;
    }

    if (read_value_size > 0) {
        memcpy(value, raw_entry + header_size, read_value_size);
    }

    *value_size = read_value_size;

exit:
    free(raw_entry);
    return ares;
}

astarte_result_t astarte_storage_key_value_entry_read_key(
    struct nvs_fs *nvs_fs, uint16_t idx, char *key, size_t *key_size)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    struct header header = { 0 };
    size_t raw_entry_size = 0;

    ares = read_header(nvs_fs, idx, &header, &raw_entry_size);
    if (ares != ASTARTE_RESULT_OK) {
        goto exit;
    }

    if (!key) {
        // +1 includes room for termination
        *key_size = header.fixed_header.key_len + 1;
        goto exit;
    }
    if (*key_size < header.fixed_header.key_len + 1) {
        ASTARTE_LOG_ERR("Error: Key buffer too small at ID %d", idx);
        ares = ASTARTE_RESULT_INVALID_PARAM;
        goto exit;
    }

    memcpy(key, header.key, header.fixed_header.key_len);
    key[header.fixed_header.key_len] = '\0';
    *key_size = header.fixed_header.key_len + 1;

exit:
    free_header(&header);
    return ares;
}

astarte_result_t astarte_storage_key_value_entry_check_namespace(
    struct nvs_fs *nvs_fs, uint16_t idx, const char *namespace, bool *matches)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    struct header header = { 0 };
    size_t raw_entry_size = 0;

    if (!namespace || !matches) {
        ASTARTE_LOG_ERR("Trying to check namespace at ID %d passing null pointers", idx);
        ares = ASTARTE_RESULT_INVALID_PARAM;
        goto exit;
    }

    ares = read_header(nvs_fs, idx, &header, &raw_entry_size);
    if (ares == ASTARTE_RESULT_NOT_FOUND) {
        ares = ASTARTE_RESULT_OK;
        *matches = false;
        goto exit;
    }
    if (ares != ASTARTE_RESULT_OK) {
        goto exit;
    }

    if (header.fixed_header.namespace_len != strlen(namespace)) {
        *matches = false;
        goto exit;
    }

    if (strncmp(header.namespace, namespace, header.fixed_header.namespace_len) == 0) {
        *matches = true;
    } else {
        *matches = false;
    }

exit:
    free_header(&header);
    return ares;
}

astarte_result_t astarte_storage_key_value_entry_delete(struct nvs_fs *nvs_fs, uint16_t idx)
{
    astarte_result_t ares = delete_and_unlink_single_entry(nvs_fs, idx);
    if (ares == ASTARTE_RESULT_NOT_FOUND) {
        return ASTARTE_RESULT_OK;
    }
    if (ares != ASTARTE_RESULT_OK) {
        return ares;
    }

    // Backward shift to maintain lookup integrity
    uint16_t hole_id = idx;
    uint16_t curr_id = hole_id + 1;
    if (curr_id == HEAD_AND_TAIL_ID_POSITION) {
        curr_id = 1;
    }

    while (curr_id != hole_id) {
        bool shift_performed = false;
        ares = shift_back_single_entry(nvs_fs, curr_id, hole_id, &shift_performed);
        if (ares == ASTARTE_RESULT_NOT_FOUND) {
            // No more entries to shift back, we can stop
            break;
        }
        if (ares != ASTARTE_RESULT_OK) {
            return ares;
        }

        // The new hole is the physical position we just vacated
        if (shift_performed) {
            hole_id = curr_id;
        }

        curr_id++;
        if (curr_id == HEAD_AND_TAIL_ID_POSITION) {
            curr_id = 1;
        }
    }

    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_storage_key_value_entry_get_next_id(
    struct nvs_fs *nvs_fs, uint16_t idx, uint16_t *next_id)
{
    // Calling this function with ID 0 will make it return the ID of the head
    if (idx == 0) {
        uint16_t head_id = 0;
        uint16_t tail_id = 0;
        astarte_result_t rd_res = read_head_and_tail_ids(nvs_fs, &head_id, &tail_id);
        if (rd_res != ASTARTE_RESULT_OK) {
            return rd_res;
        }
        *next_id = head_id;
        return ASTARTE_RESULT_OK;
    }

    struct fixed_header fixed_header = { 0 };
    size_t raw_entry_size = 0;
    astarte_result_t ares = read_fixed_header(nvs_fs, idx, &fixed_header, &raw_entry_size);
    if (ares != ASTARTE_RESULT_OK) {
        return ares;
    }

    *next_id = fixed_header.next_id;
    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_storage_key_value_entry_get_prev_id(
    struct nvs_fs *nvs_fs, uint16_t idx, uint16_t *prev_id)
{
    // Calling this function with ID 0 will make it return 0
    if (idx == 0) {
        *prev_id = 0;
        return ASTARTE_RESULT_OK;
    }

    struct fixed_header fixed_header = { 0 };
    size_t raw_entry_size = 0;
    astarte_result_t ares = read_fixed_header(nvs_fs, idx, &fixed_header, &raw_entry_size);
    if (ares != ASTARTE_RESULT_OK) {
        return ares;
    }

    *prev_id = fixed_header.prev_id;
    return ASTARTE_RESULT_OK;
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static astarte_result_t shift_back_single_entry(
    struct nvs_fs *nvs_fs, uint16_t idx, uint16_t hole_id, bool *shift_performed)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    uint8_t *raw_entry = NULL;
    ssize_t raw_entry_size = 0;
    struct header header = { 0 };

    if (idx <= 1) {
        ASTARTE_LOG_ERR("Attempting to shift back entry with ID %d", idx);
        ares = ASTARTE_RESULT_INTERNAL_ERROR;
        goto exit;
    }

    uint16_t source_id = idx;

    // Read the source entry to be shifted back
    ares = read_header(nvs_fs, source_id, &header, &raw_entry_size);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed in reading header for entry with ID %d", idx);
        goto exit;
    }

    raw_entry = calloc(raw_entry_size, sizeof(uint8_t));
    if (!raw_entry) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        ares = ASTARTE_RESULT_INTERNAL_ERROR;
        goto exit;
    }

    ssize_t ret = nvs_read(nvs_fs, source_id, raw_entry, raw_entry_size);
    if (ret < 0) {
        ASTARTE_LOG_ERR("Failed in reading entry with ID %d", idx);
        ares = ASTARTE_RESULT_NVS_ERROR;
        goto exit;
    }

    // Calculate the natural hash for the entry to be shifted back
    // And the cyclic absolute distances from the natural_hash
    uint16_t natural_hash = generate_hash(header.namespace, header.key);
    uint16_t d_curr = (source_id >= natural_hash) ? (source_id - natural_hash)
                                                  : (MAX_USABLE_ID - natural_hash + source_id);
    uint16_t d_hole = (hole_id >= natural_hash) ? (hole_id - natural_hash)
                                                : (MAX_USABLE_ID - natural_hash + hole_id);

    // If the hole is closer to the natural hash than the current element's distance
    if (d_hole < d_curr) {
        // Shift the current entry to the hole's position
        ret = nvs_write(nvs_fs, hole_id, raw_entry, raw_entry_size);
        if (ret < 0) {
            ASTARTE_LOG_ERR("Failed in writing entry with ID %d", idx);
            ares = ASTARTE_RESULT_NVS_ERROR;
            goto exit;
        }

        // Update linked list pointers of the physically moved entry's neighbors
        ares = update_shifted_entry_neighbors(nvs_fs, &header, hole_id);
        if (ares != ASTARTE_RESULT_OK) {
            goto exit;
        }

        // Clean up the entry from its old position
        ret = nvs_delete(nvs_fs, source_id);
        if (ret < 0) {
            ares = ASTARTE_RESULT_NVS_ERROR;
            goto exit;
        }

        *shift_performed = true;
    } else {
        *shift_performed = false;
    }

exit:
    free(raw_entry);
    free_header(&header);
    return ares;
}

static astarte_result_t update_shifted_entry_neighbors(
    struct nvs_fs *nvs_fs, const struct header *header, uint16_t hole_id)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    uint16_t head_id = 0;
    uint16_t tail_id = 0;

    // Update previous neighbor or Head
    if (header->fixed_header.prev_id != 0) {
        ares = update_entry_next_id(nvs_fs, header->fixed_header.prev_id, hole_id);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Failed in updating next ID for prev entry");
            return ares;
        }
    } else {
        ares = read_head_and_tail_ids(nvs_fs, &head_id, &tail_id);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Failed in reading head and tail IDs");
            return ares;
        }
        ares = write_head_and_tail_ids(nvs_fs, hole_id, tail_id);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Failed in writing head and tail IDs");
            return ares;
        }
    }

    // Update next neighbor or Tail
    if (header->fixed_header.next_id != 0) {
        ares = update_entry_prev_id(nvs_fs, header->fixed_header.next_id, hole_id);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Failed in updating prev ID for next entry");
            return ares;
        }
    } else {
        ares = read_head_and_tail_ids(nvs_fs, &head_id, &tail_id);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Failed in reading head and tail IDs");
            return ares;
        }
        ares = write_head_and_tail_ids(nvs_fs, head_id, hole_id);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Failed in writing head and tail IDs");
            return ares;
        }
    }

    return ASTARTE_RESULT_OK;
}

static astarte_result_t delete_and_unlink_single_entry(struct nvs_fs *nvs_fs, uint16_t idx)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    // Fetch next and previous entries as well as head and tail
    struct fixed_header fixed_header = { 0 };
    size_t raw_entry_size = 0;
    ares = read_fixed_header(nvs_fs, idx, &fixed_header, &raw_entry_size);
    if (ares != ASTARTE_RESULT_OK) {
        return ares;
    }
    uint16_t next_id = fixed_header.next_id;
    uint16_t prev_id = fixed_header.prev_id;

    uint16_t head_id = 0;
    uint16_t tail_id = 0;
    ares = read_head_and_tail_ids(nvs_fs, &head_id, &tail_id);
    if (ares != ASTARTE_RESULT_OK) {
        return ares;
    }

    // Update the previous entry next id to the next one of the entry to delete
    // Or if this entry is the head set the head to the next entry
    if (prev_id != 0) {
        ares = update_entry_next_id(nvs_fs, prev_id, next_id);
        if (ares != ASTARTE_RESULT_OK) {
            return ares;
        }
    } else {
        head_id = next_id;
    }

    // Update the next entry previous id to the previous one of the entry to delete
    // Or if this node is the tail set the tail to the previous entry
    if (next_id != 0) {
        ares = update_entry_prev_id(nvs_fs, next_id, prev_id);
        if (ares != ASTARTE_RESULT_OK) {
            return ares;
        }
    } else {
        tail_id = prev_id;
    }

    // Update head and tail to the new values
    ares = write_head_and_tail_ids(nvs_fs, head_id, tail_id);
    if (ares != ASTARTE_RESULT_OK) {
        return ares;
    }

    // Delete the entry
    ssize_t ret = nvs_delete(nvs_fs, idx);
    if (ret < 0) {
        return ASTARTE_RESULT_NVS_ERROR;
    }
    return ASTARTE_RESULT_OK;
}

static astarte_result_t compute_entry_next_and_prev_ids(
    struct nvs_fs *nvs_fs, uint16_t idx, uint16_t *next_id, uint16_t *prev_id)
{
    struct fixed_header fixed_header = { 0 };
    size_t raw_size = 0;
    astarte_result_t ares = read_fixed_header(nvs_fs, idx, &fixed_header, &raw_size);
    if (ares == ASTARTE_RESULT_OK) {
        *next_id = fixed_header.next_id;
        *prev_id = fixed_header.prev_id;
    }
    if (ares == ASTARTE_RESULT_NOT_FOUND) {
        uint16_t head_id = 0;
        uint16_t tail_id = 0;
        astarte_result_t internal_ares = read_head_and_tail_ids(nvs_fs, &head_id, &tail_id);
        if (internal_ares != ASTARTE_RESULT_OK) {
            return internal_ares;
        }
        *prev_id = tail_id;
        *next_id = 0;
    }
    return ares;
}

static astarte_result_t read_head_and_tail_ids(
    struct nvs_fs *nvs_fs, uint16_t *head_id, uint16_t *tail_id)
{
    uint16_t ids[2] = { 0 };
    ssize_t ret = nvs_read(nvs_fs, HEAD_AND_TAIL_ID_POSITION, ids, sizeof(ids));

    if (ret == -ENOENT) {
        *head_id = 0;
        *tail_id = 0;
        return ASTARTE_RESULT_OK;
    }
    if (ret < 0) {
        ASTARTE_LOG_ERR("Error reading head and tail IDs: %d", (int) ret);
        return ASTARTE_RESULT_NVS_ERROR;
    }

    *head_id = ids[0];
    *tail_id = ids[1];
    return ASTARTE_RESULT_OK;
}

static astarte_result_t write_head_and_tail_ids(
    struct nvs_fs *nvs_fs, uint16_t head_id, uint16_t tail_id)
{
    uint16_t ids[2] = { head_id, tail_id };
    ssize_t ret = nvs_write(nvs_fs, HEAD_AND_TAIL_ID_POSITION, ids, sizeof(ids));
    if (ret < 0) {
        ASTARTE_LOG_ERR("Error writing head and tail IDs to NVS, error: %d", (int) ret);
        return ASTARTE_RESULT_NVS_ERROR;
    }
    return ASTARTE_RESULT_OK;
}

static uint8_t *serialize_entry(
    struct header header, const void *value, size_t value_size, size_t *serialized_entry_size)
{
    // Allocate enough memory to store the full raw entry (header + namespace + key + value)
    uint16_t nsp_len = header.fixed_header.namespace_len;
    uint16_t key_len = header.fixed_header.key_len;
    size_t entry_size = FIXED_HEADER_BYTES + nsp_len + key_len + value_size;
    uint8_t *entry = calloc(entry_size, sizeof(uint8_t));
    if (!entry) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        return NULL;
    }

    // NOLINTBEGIN(bugprone-not-null-terminated-result)
    size_t write_offset = 0;
    size_t write_size = HEADER_NAMESPACE_LEN_BYTES;
    memcpy(entry + write_offset, &header.fixed_header.namespace_len, write_size);
    write_offset += write_size;
    write_size = HEADER_KEY_LEN_BYTES;
    memcpy(entry + write_offset, &header.fixed_header.key_len, write_size);
    write_offset += write_size;
    write_size = HEADER_NEXT_ID_BYTES;
    memcpy(entry + write_offset, &header.fixed_header.next_id, write_size);
    write_offset += write_size;
    write_size = HEADER_PREV_ID_BYTES;
    memcpy(entry + write_offset, &header.fixed_header.prev_id, write_size);
    write_offset += write_size;
    write_size = header.fixed_header.namespace_len;
    memcpy(entry + write_offset, header.namespace, write_size);
    write_offset += write_size;
    write_size = header.fixed_header.key_len;
    memcpy(entry + write_offset, header.key, write_size);
    if (value_size > 0 && value != NULL) {
        write_offset += write_size;
        write_size = value_size;
        memcpy(entry + write_offset, value, write_size);
    }
    // NOLINTEND(bugprone-not-null-terminated-result)

    *serialized_entry_size = entry_size;
    return entry;
}

static astarte_result_t check_entry_match(
    struct nvs_fs *nvs_fs, uint16_t idx, const char *namespace, const char *key)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    struct header header = { 0 };
    size_t raw_size = 0;

    ares = read_header(nvs_fs, idx, &header, &raw_size);
    if (ares != ASTARTE_RESULT_OK) {
        goto exit;
    }

    struct fixed_header fixed_header = header.fixed_header;
    if (fixed_header.namespace_len != strlen(namespace) || fixed_header.key_len != strlen(key)) {
        ares = ASTARTE_RESULT_MISMATCH;
        goto exit;
    }
    int nsp_cmp = strncmp(header.namespace, namespace, fixed_header.namespace_len);
    int key_cmp = strncmp(header.key, key, fixed_header.key_len);
    if (nsp_cmp != 0 || key_cmp != 0) {
        ares = ASTARTE_RESULT_MISMATCH;
        goto exit;
    }

exit:
    free_header(&header);
    return ares;
}

static astarte_result_t read_header(
    struct nvs_fs *nvs_fs, uint16_t idx, struct header *header, size_t *raw_size)
{
    uint8_t *raw_header = NULL;
    char *namespace = NULL;
    char *key = NULL;
    struct fixed_header fixed_header = { 0 };
    size_t raw_entry_size = 0;
    astarte_result_t ares = read_fixed_header(nvs_fs, idx, &fixed_header, &raw_entry_size);
    if (ares != ASTARTE_RESULT_OK) {
        goto error;
    }

    size_t raw_header_size = FIXED_HEADER_BYTES + fixed_header.namespace_len + fixed_header.key_len;

    raw_header = calloc(raw_header_size, sizeof(uint8_t));
    if (!raw_header) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto error;
    }

    ssize_t ret = nvs_read(nvs_fs, idx, raw_header, raw_header_size);
    if (ret < 0) {
        ares = ASTARTE_RESULT_NVS_ERROR;
        goto error;
    }

    // Duplicate the namespace
    namespace = calloc(fixed_header.namespace_len + 1, sizeof(char));
    if (!namespace) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto error;
    }
    memcpy(namespace, raw_header + FIXED_HEADER_BYTES, fixed_header.namespace_len);
    namespace[fixed_header.namespace_len] = '\0';
    // Duplicate the key
    key = calloc(fixed_header.key_len + 1, sizeof(char));
    if (!key) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto error;
    }
    memcpy(key, raw_header + FIXED_HEADER_BYTES + fixed_header.namespace_len, fixed_header.key_len);
    key[fixed_header.key_len] = '\0';

    header->namespace = namespace;
    header->key = key;
    header->fixed_header = fixed_header;
    header->dynamically_allocated = true;
    *raw_size = ret;

    free(raw_header);
    return ares;

error:
    free(raw_header);
    free(namespace);
    free(key);
    return ares;
}

static void free_header(struct header *header)
{
    if (!header->dynamically_allocated) {
        return;
    }

    free(header->namespace);
    header->namespace = NULL;
    free(header->key);
    header->key = NULL;
}

static astarte_result_t read_fixed_header(
    struct nvs_fs *nvs_fs, uint16_t idx, struct fixed_header *fixed_header, size_t *raw_size)
{
    uint16_t raw_fixed_header[FIXED_HEADER_BYTES / sizeof(uint16_t)] = { 0 };
    ssize_t ret = nvs_read(nvs_fs, idx, raw_fixed_header, FIXED_HEADER_BYTES);
    if (ret == -ENOENT) {
        return ASTARTE_RESULT_NOT_FOUND;
    }
    if ((ret < 0) || (ret < FIXED_HEADER_BYTES)) {
        ASTARTE_LOG_ERR("Error reading header from NVS at ID %d, error: %d", idx, ret);
        return ASTARTE_RESULT_NVS_ERROR;
    }

    fixed_header->namespace_len = raw_fixed_header[0];
    fixed_header->key_len = raw_fixed_header[1];
    fixed_header->next_id = raw_fixed_header[2];
    fixed_header->prev_id = raw_fixed_header[3];
    *raw_size = ret;

    return ASTARTE_RESULT_OK;
}

static astarte_result_t update_entry_next_id(struct nvs_fs *nvs_fs, uint16_t idx, uint16_t new_next)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    uint8_t *raw_entry = NULL;
    ssize_t raw_entry_size = nvs_read(nvs_fs, idx, NULL, 0);
    if (raw_entry_size <= 0) {
        ares = ASTARTE_RESULT_NVS_ERROR;
        goto exit;
    }

    raw_entry = calloc(raw_entry_size, 1);
    if (!raw_entry) {
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto exit;
    }

    ssize_t ret = nvs_read(nvs_fs, idx, raw_entry, raw_entry_size);
    if (ret < 0) {
        ares = ASTARTE_RESULT_NVS_ERROR;
        goto exit;
    }

    size_t next_id_offset = HEADER_NAMESPACE_LEN_BYTES + HEADER_KEY_LEN_BYTES;
    memcpy(&raw_entry[next_id_offset], &new_next, sizeof(new_next));

    ret = nvs_write(nvs_fs, idx, raw_entry, raw_entry_size);
    if (ret < 0) {
        ares = ASTARTE_RESULT_NVS_ERROR;
    }

exit:
    free(raw_entry);
    return ares;
}

static astarte_result_t update_entry_prev_id(struct nvs_fs *nvs_fs, uint16_t idx, uint16_t new_prev)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    uint8_t *raw_entry = NULL;
    ssize_t raw_entry_size = nvs_read(nvs_fs, idx, NULL, 0);
    if (raw_entry_size <= 0) {
        ares = ASTARTE_RESULT_NVS_ERROR;
        goto exit;
    }

    raw_entry = calloc(raw_entry_size, 1);
    if (!raw_entry) {
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto exit;
    }

    ssize_t ret = nvs_read(nvs_fs, idx, raw_entry, raw_entry_size);
    if (ret < 0) {
        ares = ASTARTE_RESULT_NVS_ERROR;
        goto exit;
    }

    size_t prev_id_offset
        = HEADER_NAMESPACE_LEN_BYTES + HEADER_KEY_LEN_BYTES + HEADER_NEXT_ID_BYTES;
    memcpy(&raw_entry[prev_id_offset], &new_prev, sizeof(new_prev));

    ret = nvs_write(nvs_fs, idx, raw_entry, raw_entry_size);
    if (ret < 0) {
        ares = ASTARTE_RESULT_NVS_ERROR;
        goto exit;
    }

exit:
    free(raw_entry);
    return ares;
}

static uint16_t generate_hash(const char *namespace, const char *key)
{
    uint16_t crc = UINT16_MAX; // Zephyr's crc16_ccitt seed
    crc = crc16_ccitt(crc, (const uint8_t *) namespace, strlen(namespace));
    crc = crc16_ccitt(crc, (const uint8_t *) key, strlen(key));

    // Fallbacks to avoid invalid Zephyr NVS IDs or our reserved list ptrs block (0xFFFE)
    if (crc == UINT16_MAX || crc == 0 || crc == HEAD_AND_TAIL_ID_POSITION) {
        crc = 1;
    }
    return crc;
}
