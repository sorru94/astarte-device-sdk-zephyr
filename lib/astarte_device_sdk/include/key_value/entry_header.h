/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KEY_VALUE_ENTRY_HEADER_H
#define KEY_VALUE_ENTRY_HEADER_H

/**
 * @file key_value/entry_header.h
 * @brief Entry header function helpers.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "astarte_device_sdk/result.h"

#include <zephyr/version.h>

#if KERNEL_VERSION_NUMBER >= ZEPHYR_VERSION(4, 4, 0)
#include <zephyr/kvss/zms.h>
#else
#include <zephyr/fs/zms.h>
#endif

/** @brief Size in bytes of the namespace string length field. */
#define ASTARTE_KEY_VALUE_ENTRY_HEADER_NAMESPACE_LEN_BYTES 2

/** @brief Size in bytes of the key string length field. */
#define ASTARTE_KEY_VALUE_ENTRY_HEADER_KEY_LEN_BYTES 2

/** @brief Size in bytes of the next entry ID field. */
#define ASTARTE_KEY_VALUE_ENTRY_HEADER_NEXT_ID_BYTES 4

/** @brief Size in bytes of the previous entry ID field. */
#define ASTARTE_KEY_VALUE_ENTRY_HEADER_PREV_ID_BYTES 4

/** @brief Total size in bytes of the fixed portion of the entry header. */
#define ASTARTE_KEY_VALUE_ENTRY_HEADER_FIXED_HEADER_BYTES                                          \
    (ASTARTE_KEY_VALUE_ENTRY_HEADER_NAMESPACE_LEN_BYTES                                            \
        + ASTARTE_KEY_VALUE_ENTRY_HEADER_KEY_LEN_BYTES                                             \
        + ASTARTE_KEY_VALUE_ENTRY_HEADER_NEXT_ID_BYTES                                             \
        + ASTARTE_KEY_VALUE_ENTRY_HEADER_PREV_ID_BYTES)

/**
 * @brief Represents the fixed-size structure at the start of a ZMS entry payload.
 */
struct astarte_key_value_entry_header_fixed
{
    /** @brief Namespace string length (excluding null terminator). */
    uint16_t namespace_len;
    /** @brief Key string length (excluding null terminator). */
    uint16_t key_len;
    /** @brief Next entry ID in the linked list. */
    uint32_t next_id;
    /** @brief Previous entry ID in the linked list. */
    uint32_t prev_id;
};

/**
 * @brief Represents a fully parsed ZMS entry header, including dynamic data.
 */
struct astarte_key_value_entry_header
{
    /** @brief The fixed-size header attributes. */
    struct astarte_key_value_entry_header_fixed fixed_header;
    /** @brief Null-terminated namespace string. */
    char *namespace;
    /** @brief Null-terminated key string. */
    char *key;
    /** @brief Flag indicating if the namespace and key strings were dynamically allocated. */
    bool dynamically_allocated;
};

/**
 * @brief Reads and parses a complete entry header from ZMS.
 *
 * @param[inout] zms_fs ZMS file system.
 * @param[in] idx Valid ZMS ID to read from.
 * @param[out] header Struct to populate with the header data.
 * @param[out] raw_size Evaluated size of the read payload block.
 * @retval ASTARTE_RESULT_OK The entry header was read and parsed successfully.
 * @retval ASTARTE_RESULT_OUT_OF_MEMORY Dynamic allocation failure due to a lack of memory.
 * @retval ASTARTE_RESULT_NOT_FOUND The specified index was not found in ZMS.
 * @retval ASTARTE_RESULT_ZMS_ERROR An underlying ZMS file system error occurred.
 */
astarte_result_t astarte_key_value_entry_header_read(struct zms_fs *zms_fs, uint32_t idx,
    struct astarte_key_value_entry_header *header, size_t *raw_size);

/**
 * @brief Frees dynamically allocated strings within an entry header.
 *
 * @param[inout] header The entry header struct to free strings from.
 */
void astarte_key_value_entry_header_free(struct astarte_key_value_entry_header *header);

/**
 * @brief Reads only the fixed portion of an entry header from ZMS.
 *
 * @param[inout] zms_fs ZMS file system.
 * @param[in] idx Valid ZMS ID to read from.
 * @param[out] fixed_header Struct to populate with the fixed header data.
 * @param[out] raw_size Evaluated size of the read payload block.
 * @retval ASTARTE_RESULT_OK The entry header was read and parsed successfully.
 * @retval ASTARTE_RESULT_NOT_FOUND The specified index was not found in ZMS.
 * @retval ASTARTE_RESULT_ZMS_ERROR An underlying ZMS file system error occurred.
 */
astarte_result_t astarte_key_value_entry_header_read_fixed(struct zms_fs *zms_fs, uint32_t idx,
    struct astarte_key_value_entry_header_fixed *fixed_header, size_t *raw_size);

#endif // KEY_VALUE_ENTRY_HEADER_H
