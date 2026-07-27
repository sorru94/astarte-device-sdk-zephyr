/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "key_value/entry_header.h"

#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "log.h"

ASTARTE_LOG_MODULE_DECLARE(astarte_key_value, CONFIG_ASTARTE_DEVICE_SDK_KEY_VALUE_LOG_LEVEL);

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

#define OFFSET_NAMESPACE_LEN 0
#define OFFSET_KEY_LEN (OFFSET_NAMESPACE_LEN + ASTARTE_KEY_VALUE_ENTRY_HEADER_NAMESPACE_LEN_BYTES)
#define OFFSET_NEXT_ID (OFFSET_KEY_LEN + ASTARTE_KEY_VALUE_ENTRY_HEADER_KEY_LEN_BYTES)
#define OFFSET_PREV_ID (OFFSET_NEXT_ID + ASTARTE_KEY_VALUE_ENTRY_HEADER_NEXT_ID_BYTES)

/** @brief Context to hold the temporary variables during the key_value read operation. */
typedef struct
{
    /** @cond INTERNAL_HIDDEN */
    uint8_t *raw_header;
    char *namespace;
    char *key;
    /** @endcond */
} kv_read_cleanup_ctx_t;

/**
 * @brief Cleanup function for key_value entry.
 *
 * @param[in] ctx Pointer to the key_value read cleanup context.
 */
static void cleanup_kv_read(kv_read_cleanup_ctx_t *ctx)
{
    if (ctx) {
        astarte_free(ctx->raw_header);
        ctx->raw_header = NULL;
        astarte_free(ctx->namespace);
        ctx->namespace = NULL;
        astarte_free(ctx->key);
        ctx->key = NULL;
    }
}

/** @cond INTERNAL_HIDDEN */
ASTARTE_SCOPE_DEFER_DEFINE(cleanup_kv_read, kv_read_cleanup_ctx_t *);
/** @endcond */

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_key_value_entry_header_read(struct zms_fs *zms_fs, uint32_t idx,
    struct astarte_key_value_entry_header *header, size_t *raw_size)
{
    struct astarte_key_value_entry_header_fixed fixed_header = { 0 };
    size_t raw_entry_size = 0;

    kv_read_cleanup_ctx_t ctx = { 0 };
    scope_defer(cleanup_kv_read)(&ctx);

    astarte_result_t ares
        = astarte_key_value_entry_header_read_fixed(zms_fs, idx, &fixed_header, &raw_entry_size);
    if (ares != ASTARTE_RESULT_OK) {
        return ares;
    }

    size_t raw_header_size = ASTARTE_KEY_VALUE_ENTRY_HEADER_FIXED_HEADER_BYTES
        + fixed_header.namespace_len + fixed_header.key_len;

    ctx.raw_header = astarte_calloc(raw_header_size, sizeof(uint8_t));
    if (!ctx.raw_header) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        return ASTARTE_RESULT_OUT_OF_MEMORY;
    }

    ssize_t ret = zms_read(zms_fs, idx, ctx.raw_header, raw_header_size);
    if (ret != raw_header_size) {
        return ASTARTE_RESULT_ZMS_ERROR;
    }

    // Duplicate the namespace
    ctx.namespace = astarte_calloc(fixed_header.namespace_len + 1, sizeof(char));
    if (!ctx.namespace) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        return ASTARTE_RESULT_OUT_OF_MEMORY;
    }
    memcpy(ctx.namespace, ctx.raw_header + ASTARTE_KEY_VALUE_ENTRY_HEADER_FIXED_HEADER_BYTES,
        fixed_header.namespace_len);
    ctx.namespace[fixed_header.namespace_len] = '\0';

    // Duplicate the key
    ctx.key = astarte_calloc(fixed_header.key_len + 1, sizeof(char));
    if (!ctx.key) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        return ASTARTE_RESULT_OUT_OF_MEMORY;
    }
    memcpy(ctx.key,
        ctx.raw_header + ASTARTE_KEY_VALUE_ENTRY_HEADER_FIXED_HEADER_BYTES
            + fixed_header.namespace_len,
        fixed_header.key_len);
    ctx.key[fixed_header.key_len] = '\0';

    header->namespace = ctx.namespace;
    header->key = ctx.key;
    header->fixed_header = fixed_header;
    header->dynamically_allocated = true;
    *raw_size = raw_entry_size;

    // prevent namespace and key saved into header to be freed during the scope-based clanup
    ctx.namespace = NULL;
    ctx.key = NULL;

    return ASTARTE_RESULT_OK;
}

void astarte_key_value_entry_header_free(struct astarte_key_value_entry_header *header)
{
    if (!header->dynamically_allocated) {
        return;
    }

    astarte_free(header->namespace);
    header->namespace = NULL;
    astarte_free(header->key);
    header->key = NULL;
}

astarte_result_t astarte_key_value_entry_header_read_fixed(struct zms_fs *zms_fs, uint32_t idx,
    struct astarte_key_value_entry_header_fixed *fixed_header, size_t *raw_size)
{
    uint8_t raw_fixed_header[ASTARTE_KEY_VALUE_ENTRY_HEADER_FIXED_HEADER_BYTES] = { 0 };
    ssize_t ret = zms_read(
        zms_fs, idx, raw_fixed_header, ASTARTE_KEY_VALUE_ENTRY_HEADER_FIXED_HEADER_BYTES);
    if (ret == -ENOENT) {
        return ASTARTE_RESULT_NOT_FOUND;
    }
    if (ret != ASTARTE_KEY_VALUE_ENTRY_HEADER_FIXED_HEADER_BYTES) {
        ASTARTE_LOG_ERR("Error reading header from ZMS at ID %d, error: %d", idx, ret);
        return ASTARTE_RESULT_ZMS_ERROR;
    }

    ret = zms_get_data_length(zms_fs, idx);
    if (ret < 0) {
        ASTARTE_LOG_ERR("Error reading full entry length from ZMS at ID %d, error: %d", idx, ret);
        return ASTARTE_RESULT_ZMS_ERROR;
    }

    memcpy(&fixed_header->namespace_len, &raw_fixed_header[OFFSET_NAMESPACE_LEN],
        ASTARTE_KEY_VALUE_ENTRY_HEADER_NAMESPACE_LEN_BYTES);
    memcpy(&fixed_header->key_len, &raw_fixed_header[OFFSET_KEY_LEN],
        ASTARTE_KEY_VALUE_ENTRY_HEADER_KEY_LEN_BYTES);
    memcpy(&fixed_header->next_id, &raw_fixed_header[OFFSET_NEXT_ID],
        ASTARTE_KEY_VALUE_ENTRY_HEADER_NEXT_ID_BYTES);
    memcpy(&fixed_header->prev_id, &raw_fixed_header[OFFSET_PREV_ID],
        ASTARTE_KEY_VALUE_ENTRY_HEADER_PREV_ID_BYTES);

    *raw_size = ret;
    return ASTARTE_RESULT_OK;
}
