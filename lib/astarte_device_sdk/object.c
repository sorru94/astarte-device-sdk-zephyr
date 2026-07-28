/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "astarte_device_sdk/object.h"
#include "object_private.h"

#include <stdlib.h>

#include "alloc.h"
#include "bson/types.h"
#include "data/deserialize.h"
#include "data/serialize.h"
#include "interface_private.h"

#include "log.h"

ASTARTE_LOG_MODULE_REGISTER(astarte_object, CONFIG_ASTARTE_DEVICE_SDK_OBJECT_LOG_LEVEL);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_object_entry_t astarte_object_entry_new(const char *path, astarte_data_t data)
{
    return (astarte_object_entry_t){
        .path = path,
        .data = data,
    };
}

astarte_result_t astarte_object_entry_to_path_and_data(
    astarte_object_entry_t object_entry, const char **path, astarte_data_t *data)
{
    if (!path || !data) {
        ASTARTE_LOG_ERR("Conversion from Astarte object entry to path and data error.");
        return ASTARTE_RESULT_INVALID_PARAM;
    }
    *path = object_entry.path;
    *data = object_entry.data;
    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_object_entries_serialize(
    astarte_bson_serializer_t *bson, astarte_object_entry_t *entries, size_t entries_length)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    for (size_t i = 0; i < entries_length; i++) {
        ares = astarte_data_serialize(bson, entries[i].path, entries[i].data);
        if (ares != ASTARTE_RESULT_OK) {
            break;
        }
    }

    return ares;
}

astarte_result_t astarte_object_entries_deserialize(astarte_bson_element_t bson_elem,
    const astarte_interface_t *interface, const char *path, astarte_object_entry_t **entries,
    size_t *entries_length)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    // Init context and register the deferred cleanup
    astarte_object_entries_ctx_t cleanup_ctx = { .entries = NULL, .length = 0 };
    scope_defer(astarte_cleanup_object_entries)(&cleanup_ctx);

    // Step 1: extract the document from the BSON and calculate its length
    if (bson_elem.type != ASTARTE_BSON_TYPE_DOCUMENT) {
        ASTARTE_LOG_ERR("Received BSON element that is not a document.");
        return ASTARTE_RESULT_BSON_DESERIALIZER_ERROR;
    }
    astarte_bson_document_t bson_doc = astarte_bson_deserializer_element_to_document(bson_elem);

    size_t bson_doc_length = 0;
    ares = astarte_bson_deserializer_doc_count_elements(bson_doc, &bson_doc_length);
    if (ares != ASTARTE_RESULT_OK) {
        return ares;
    }
    if (bson_doc_length == 0) {
        ASTARTE_LOG_ERR("BSON document can't be empty.");
        return ASTARTE_RESULT_BSON_EMPTY_DOCUMENT_ERROR;
    }

    // Step 2: Allocate sufficient memory for all the astarte object entries
    cleanup_ctx.entries = astarte_calloc(bson_doc_length, sizeof(astarte_object_entry_t));
    if (!cleanup_ctx.entries) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        return ASTARTE_RESULT_OUT_OF_MEMORY;
    }

    // Step 3: Fill the allocated memory
    astarte_bson_element_t inner_elem = { 0 };
    ares = astarte_bson_deserializer_first_element(bson_doc, &inner_elem);
    if (ares != ASTARTE_RESULT_OK) {
        return ares;
    }

    const astarte_mapping_t *mapping = NULL;
    while ((ares != ASTARTE_RESULT_NOT_FOUND) && (cleanup_ctx.length < bson_doc_length)) {
        cleanup_ctx.entries[cleanup_ctx.length].path = inner_elem.name;
        ares = astarte_interface_get_mapping_from_paths(interface, path, inner_elem.name, &mapping);
        if (ares != ASTARTE_RESULT_OK) {
            return ares;
        }
        ares = astarte_data_deserialize(
            inner_elem, mapping->type, &(cleanup_ctx.entries[cleanup_ctx.length].data));
        if (ares != ASTARTE_RESULT_OK) {
            return ares;
        }

        // Increment count only after successful deserialization
        cleanup_ctx.length++;

        ares = astarte_bson_deserializer_next_element(bson_doc, inner_elem, &inner_elem);
        if ((ares != ASTARTE_RESULT_OK) && (ares != ASTARTE_RESULT_NOT_FOUND)) {
            return ares;
        }
    }

    // Step 4: fill in the output variables
    *entries = cleanup_ctx.entries;
    *entries_length = bson_doc_length;

    // Disable cleanup on success to prevent deallocation.
    // This is performed by a check in the cleanup function
    cleanup_ctx.entries = NULL;

    return ASTARTE_RESULT_OK;
}

void astarte_object_entries_destroy_deserialized(
    astarte_object_entry_t *entries, size_t entries_length)
{
    for (size_t i = 0; i < entries_length; i++) {
        astarte_data_destroy_deserialized(entries[i].data);
    }
    astarte_free(entries);
}
