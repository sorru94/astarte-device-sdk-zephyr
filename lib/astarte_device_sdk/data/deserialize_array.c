/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "data/deserialize_array.h"

#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "bson/types.h"
#include "mapping_private.h"

#include "log.h"
ASTARTE_LOG_MODULE_DECLARE(data_deserialize, CONFIG_ASTARTE_DEVICE_SDK_DATA_LOG_LEVEL);

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

// NOLINTBEGIN(bugprone-macro-parentheses)
#define DECLARE_PRIMITIVE_DESERIALIZER(FUNC_NAME)                                                  \
    static astarte_result_t FUNC_NAME(astarte_bson_document_t bson_doc, astarte_data_t *data);
#define DEFINE_PRIMITIVE_DESERIALIZER(FUNC_NAME, TYPE, MAP_TYPE, DATA_FIELD, EXTRACTOR)            \
    static astarte_result_t FUNC_NAME(astarte_bson_document_t bson_doc, astarte_data_t *data)      \
    {                                                                                              \
        astarte_result_t ares = ASTARTE_RESULT_OK;                                                 \
        size_t capacity = 4;                                                                       \
        size_t count = 0;                                                                          \
        TYPE *array = astarte_calloc(capacity, sizeof(TYPE));                                      \
        if (!array) {                                                                              \
            ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);                           \
            return ASTARTE_RESULT_OUT_OF_MEMORY;                                                   \
        }                                                                                          \
        astarte_bson_element_t elem = { 0 };                                                       \
        ares = astarte_bson_deserializer_first_element(bson_doc, &elem);                           \
        while (ares == ASTARTE_RESULT_OK) {                                                        \
            if (!astarte_mapping_bson_is_compatible(MAP_TYPE, elem.type)) {                        \
                ares = ASTARTE_RESULT_BSON_DESERIALIZER_TYPES_ERROR;                               \
                break;                                                                             \
            }                                                                                      \
            if (count >= capacity) {                                                               \
                capacity *= 2;                                                                     \
                TYPE *new_array = astarte_realloc(array, capacity * sizeof(TYPE));                 \
                if (!new_array) {                                                                  \
                    ares = ASTARTE_RESULT_OUT_OF_MEMORY;                                           \
                    break;                                                                         \
                }                                                                                  \
                array = new_array;                                                                 \
            }                                                                                      \
            array[count++] = EXTRACTOR(elem);                                                      \
            ares = astarte_bson_deserializer_next_element(bson_doc, elem, &elem);                  \
        }                                                                                          \
        if (ares == ASTARTE_RESULT_NOT_FOUND) {                                                    \
            if (count == 0) {                                                                      \
                astarte_free(array);                                                               \
                return initialize_empty_array(MAP_TYPE##ARRAY, data);                              \
            }                                                                                      \
            data->tag = MAP_TYPE##ARRAY;                                                           \
            data->data.DATA_FIELD.len = count;                                                     \
            data->data.DATA_FIELD.buf = array;                                                     \
            return ASTARTE_RESULT_OK;                                                              \
        }                                                                                          \
        astarte_free(array);                                                                       \
        return ares;                                                                               \
    }
// NOLINTEND(bugprone-macro-parentheses)

static inline int64_t extract_int64(astarte_bson_element_t elem)
{
    return (elem.type == ASTARTE_BSON_TYPE_INT32)
        ? (int64_t) astarte_bson_deserializer_element_to_int32(elem)
        : astarte_bson_deserializer_element_to_int64(elem);
}

/** @brief Context to hold both arrays and the current item count for binary blobs cleanup. */
typedef struct
{
    /** @cond INTERNAL_HIDDEN */
    uint8_t **array;
    size_t *sizes;
    size_t count;
    /** @endcond */
} binblob_array_cleanup_ctx_t;

/** @brief Context to hold the string array and the item count for strings cleanup. */
typedef struct
{
    /** @cond INTERNAL_HIDDEN */
    char **array;
    size_t count;
    /** @endcond */
} string_array_cleanup_ctx_t;

// Binary blob array cleanup function
static void cleanup_binblob_array(binblob_array_cleanup_ctx_t *ctx)
{
    if (ctx) {
        if (ctx->array) {
            for (size_t i = 0; i < ctx->count; i++) {
                astarte_free(ctx->array[i]);
                ctx->array[i] = NULL;
            }
            astarte_free((void *) ctx->array);
            ctx->array = NULL;
        }
        if (ctx->sizes) {
            astarte_free((void *) ctx->sizes);
            ctx->sizes = NULL;
        }
    }
}

// String array cleanup function
static void cleanup_string_array(string_array_cleanup_ctx_t *ctx)
{
    if (ctx && ctx->array) {
        for (size_t i = 0; i < ctx->count; i++) {
            astarte_free(ctx->array[i]);
            ctx->array[i] = NULL;
        }
        astarte_free((void *) ctx->array);
        ctx->array = NULL;
    }
}

ASTARTE_SCOPE_DEFER_DEFINE(cleanup_binblob_array, binblob_array_cleanup_ctx_t *);
ASTARTE_SCOPE_DEFER_DEFINE(cleanup_string_array, string_array_cleanup_ctx_t *);

/************************************************
 *         Static functions declaration         *
 ***********************************************/

static astarte_result_t initialize_empty_array(astarte_mapping_type_t type, astarte_data_t *data);
static astarte_result_t deserialize_array_binblob(
    astarte_bson_document_t bson_doc, astarte_data_t *data);
DECLARE_PRIMITIVE_DESERIALIZER(deserialize_array_bool)
DECLARE_PRIMITIVE_DESERIALIZER(deserialize_array_datetime)
DECLARE_PRIMITIVE_DESERIALIZER(deserialize_array_double)
DECLARE_PRIMITIVE_DESERIALIZER(deserialize_array_int32)
DECLARE_PRIMITIVE_DESERIALIZER(deserialize_array_int64)
static astarte_result_t deserialize_array_string(
    astarte_bson_document_t bson_doc, astarte_data_t *data);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_data_deserialize_array(
    astarte_bson_element_t bson_elem, astarte_mapping_type_t type, astarte_data_t *data)
{
    if (bson_elem.type != ASTARTE_BSON_TYPE_ARRAY) {
        ASTARTE_LOG_ERR("Expected an array but BSON element type is %d.", bson_elem.type);
        return ASTARTE_RESULT_BSON_DESERIALIZER_TYPES_ERROR;
    }

    astarte_bson_document_t bson_doc = astarte_bson_deserializer_element_to_array(bson_elem);
    astarte_mapping_type_t scalar_type = ASTARTE_MAPPING_TYPE_BINARYBLOB;

    astarte_result_t ares = astarte_mapping_array_to_scalar_type(type, &scalar_type);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Non array type passed to astarte_data_deserialize_array.");
        return ares;
    }

    switch (scalar_type) {
        case ASTARTE_MAPPING_TYPE_BINARYBLOB:
            return deserialize_array_binblob(bson_doc, data);
        case ASTARTE_MAPPING_TYPE_BOOLEAN:
            return deserialize_array_bool(bson_doc, data);
        case ASTARTE_MAPPING_TYPE_DATETIME:
            return deserialize_array_datetime(bson_doc, data);
        case ASTARTE_MAPPING_TYPE_DOUBLE:
            return deserialize_array_double(bson_doc, data);
        case ASTARTE_MAPPING_TYPE_INTEGER:
            return deserialize_array_int32(bson_doc, data);
        case ASTARTE_MAPPING_TYPE_LONGINTEGER:
            return deserialize_array_int64(bson_doc, data);
        case ASTARTE_MAPPING_TYPE_STRING:
            return deserialize_array_string(bson_doc, data);
        default:
            ASTARTE_LOG_ERR("Unsupported mapping type.");
            return ASTARTE_RESULT_INTERNAL_ERROR;
    }
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static astarte_result_t initialize_empty_array(astarte_mapping_type_t type, astarte_data_t *data)
{
    switch (type) {
        case ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY:
        case ASTARTE_MAPPING_TYPE_BOOLEANARRAY:
        case ASTARTE_MAPPING_TYPE_DATETIMEARRAY:
        case ASTARTE_MAPPING_TYPE_DOUBLEARRAY:
        case ASTARTE_MAPPING_TYPE_INTEGERARRAY:
        case ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY:
        case ASTARTE_MAPPING_TYPE_STRINGARRAY:
            memset(data, 0, sizeof(astarte_data_t));
            data->tag = type;
            return ASTARTE_RESULT_OK;
        default:
            ASTARTE_LOG_ERR("Creating empty array Astarte data for scalar mapping type.");
            return ASTARTE_RESULT_INTERNAL_ERROR;
    }
}

static astarte_result_t deserialize_array_binblob(
    astarte_bson_document_t bson_doc, astarte_data_t *data)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    size_t capacity = 4;

    binblob_array_cleanup_ctx_t ctx = { .array = NULL, .sizes = NULL, .count = 0 };
    scope_defer(cleanup_binblob_array)(&ctx);

    ctx.array = (uint8_t **) astarte_calloc(capacity, sizeof(uint8_t *));
    ctx.sizes = (size_t *) astarte_calloc(capacity, sizeof(size_t));

    if (!ctx.array || !ctx.sizes) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        return ASTARTE_RESULT_OUT_OF_MEMORY;
    }

    astarte_bson_element_t elem = { 0 };
    ares = astarte_bson_deserializer_first_element(bson_doc, &elem);

    while (ares == ASTARTE_RESULT_OK) {
        if (!astarte_mapping_bson_is_compatible(ASTARTE_MAPPING_TYPE_BINARYBLOB, elem.type)) {
            return ASTARTE_RESULT_BSON_DESERIALIZER_TYPES_ERROR;
        }

        // capacity expansion
        if (ctx.count >= capacity) {
            capacity *= 2;

            uint8_t **new_array
                = (uint8_t **) astarte_realloc((void *) ctx.array, capacity * sizeof(uint8_t *));
            if (new_array) {
                ctx.array = new_array;
            }
            size_t *new_sizes
                = (size_t *) astarte_realloc((void *) ctx.sizes, capacity * sizeof(size_t));
            if (new_sizes) {
                ctx.sizes = new_sizes;
            }
            if (!new_array || !new_sizes) {
                ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
                return ASTARTE_RESULT_OUT_OF_MEMORY;
            }
        }
        uint32_t len = 0;
        const uint8_t *blob = astarte_bson_deserializer_element_to_binary(elem, &len);

        ctx.array[ctx.count] = astarte_calloc(len, sizeof(uint8_t));
        if (!ctx.array[ctx.count]) {
            ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
            return ASTARTE_RESULT_OUT_OF_MEMORY;
        }

        memcpy(ctx.array[ctx.count], blob, len);
        ctx.sizes[ctx.count] = len;
        ctx.count++;

        ares = astarte_bson_deserializer_next_element(bson_doc, elem, &elem);
    }

    if (ares == ASTARTE_RESULT_NOT_FOUND) {
        if (ctx.count == 0) {
            return initialize_empty_array(ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY, data);
        }

        data->tag = ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY;
        data->data.binaryblob_array.count = ctx.count;
        data->data.binaryblob_array.sizes = ctx.sizes;
        data->data.binaryblob_array.blobs = (const void **) ctx.array;

        // Disarm the cleanup so the memory isn't destroyed
        ctx.array = NULL;
        ctx.sizes = NULL;

        return ASTARTE_RESULT_OK;
    }

    return ares;
}

DEFINE_PRIMITIVE_DESERIALIZER(deserialize_array_bool, bool, ASTARTE_MAPPING_TYPE_BOOLEAN,
    boolean_array, astarte_bson_deserializer_element_to_bool)
DEFINE_PRIMITIVE_DESERIALIZER(deserialize_array_datetime, int64_t, ASTARTE_MAPPING_TYPE_DATETIME,
    datetime_array, astarte_bson_deserializer_element_to_datetime)
DEFINE_PRIMITIVE_DESERIALIZER(deserialize_array_double, double, ASTARTE_MAPPING_TYPE_DOUBLE,
    double_array, astarte_bson_deserializer_element_to_double)
DEFINE_PRIMITIVE_DESERIALIZER(deserialize_array_int32, int32_t, ASTARTE_MAPPING_TYPE_INTEGER,
    integer_array, astarte_bson_deserializer_element_to_int32)
DEFINE_PRIMITIVE_DESERIALIZER(deserialize_array_int64, int64_t, ASTARTE_MAPPING_TYPE_LONGINTEGER,
    longinteger_array, extract_int64)

static astarte_result_t deserialize_array_string(
    astarte_bson_document_t bson_doc, astarte_data_t *data)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    size_t capacity = 4;

    string_array_cleanup_ctx_t ctx = { .array = NULL, .count = 0 };
    scope_defer(cleanup_string_array)(&ctx);

    ctx.array = (char **) astarte_calloc(capacity, sizeof(char *));
    if (!ctx.array) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        return ASTARTE_RESULT_OUT_OF_MEMORY;
    }

    astarte_bson_element_t elem = { 0 };
    ares = astarte_bson_deserializer_first_element(bson_doc, &elem);

    while (ares == ASTARTE_RESULT_OK) {
        if (!astarte_mapping_bson_is_compatible(ASTARTE_MAPPING_TYPE_STRING, elem.type)) {
            return ASTARTE_RESULT_BSON_DESERIALIZER_TYPES_ERROR;
        }

        if (ctx.count >= capacity) {
            capacity *= 2;
            char **new_array
                = (char **) astarte_realloc((void *) ctx.array, capacity * sizeof(char *));
            if (!new_array) {
                ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
                return ASTARTE_RESULT_OUT_OF_MEMORY;
            }
            ctx.array = new_array;
        }
        uint32_t len = 0;
        const char *str = astarte_bson_deserializer_element_to_string(elem, &len);

        ctx.array[ctx.count] = astarte_calloc(len + 1, sizeof(char));
        if (!ctx.array[ctx.count]) {
            ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
            return ASTARTE_RESULT_OUT_OF_MEMORY;
        }
        strncpy(ctx.array[ctx.count], str, len);
        ctx.count++;

        ares = astarte_bson_deserializer_next_element(bson_doc, elem, &elem);
    }

    if (ares == ASTARTE_RESULT_NOT_FOUND) {
        if (ctx.count == 0) {
            return initialize_empty_array(ASTARTE_MAPPING_TYPE_STRINGARRAY, data);
        }
        data->tag = ASTARTE_MAPPING_TYPE_STRINGARRAY;
        data->data.string_array.len = ctx.count;
        data->data.string_array.buf = (const char **) ctx.array;

        // Disarm the cleanup to prevent deallocation
        ctx.array = NULL;

        return ASTARTE_RESULT_OK;
    }

    return ares;
}
