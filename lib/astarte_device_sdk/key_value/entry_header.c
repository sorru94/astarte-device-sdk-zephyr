/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "key_value/entry_header.h"

#include <stdlib.h>
#include <string.h>

#include "log.h"

ASTARTE_LOG_MODULE_DECLARE(astarte_key_value, CONFIG_ASTARTE_DEVICE_SDK_KEY_VALUE_LOG_LEVEL);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_key_value_entry_header_read(struct nvs_fs *nvs_fs, uint16_t idx,
    struct astarte_key_value_entry_header *header, size_t *raw_size)
{
    uint8_t *raw_header = NULL;
    char *namespace = NULL;
    char *key = NULL;
    struct astarte_key_value_entry_header_fixed fixed_header = { 0 };
    size_t raw_entry_size = 0;
    astarte_result_t ares
        = astarte_key_value_entry_header_read_fixed(nvs_fs, idx, &fixed_header, &raw_entry_size);
    if (ares != ASTARTE_RESULT_OK) {
        goto error;
    }

    size_t raw_header_size = ASTARTE_KEY_VALUE_ENTRY_HEADER_FIXED_HEADER_BYTES
        + fixed_header.namespace_len + fixed_header.key_len;

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
    memcpy(namespace, raw_header + ASTARTE_KEY_VALUE_ENTRY_HEADER_FIXED_HEADER_BYTES,
        fixed_header.namespace_len);
    namespace[fixed_header.namespace_len] = '\0';
    // Duplicate the key
    key = calloc(fixed_header.key_len + 1, sizeof(char));
    if (!key) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto error;
    }
    memcpy(key,
        raw_header + ASTARTE_KEY_VALUE_ENTRY_HEADER_FIXED_HEADER_BYTES + fixed_header.namespace_len,
        fixed_header.key_len);
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

void astarte_key_value_entry_header_free(struct astarte_key_value_entry_header *header)
{
    if (!header->dynamically_allocated) {
        return;
    }

    free(header->namespace);
    header->namespace = NULL;
    free(header->key);
    header->key = NULL;
}

astarte_result_t astarte_key_value_entry_header_read_fixed(struct nvs_fs *nvs_fs, uint16_t idx,
    struct astarte_key_value_entry_header_fixed *fixed_header, size_t *raw_size)
{
    uint16_t raw_fixed_header[ASTARTE_KEY_VALUE_ENTRY_HEADER_FIXED_HEADER_BYTES / sizeof(uint16_t)]
        = { 0 };
    ssize_t ret = nvs_read(
        nvs_fs, idx, raw_fixed_header, ASTARTE_KEY_VALUE_ENTRY_HEADER_FIXED_HEADER_BYTES);
    if (ret == -ENOENT) {
        return ASTARTE_RESULT_NOT_FOUND;
    }
    if ((ret < 0) || (ret < ASTARTE_KEY_VALUE_ENTRY_HEADER_FIXED_HEADER_BYTES)) {
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
