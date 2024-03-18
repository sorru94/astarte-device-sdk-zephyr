/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "astarte_device_sdk/value.h"
#include "value_private.h"

#include <stdlib.h>

#include "bson_types.h"
#include "log.h"

ASTARTE_LOG_MODULE_REGISTER(astarte_value, CONFIG_ASTARTE_DEVICE_SDK_VALUE_LOG_LEVEL);

/************************************************
 *         Static functions declaration         *
 ***********************************************/

/**
 * @brief Deserialize a scalar bson element.
 *
 * @param[in] bson_elem BSON element to deserialize.
 * @param[out] value The Astarte value where to store the deserialized data.
 * @return ASTARTE_RESULT_OK upon success, ASTARTE_RESULT_BSON_DESERIALIZER_ERROR if BSON is
 * not a scalar or contains unsupported data.
 */
static astarte_result_t astarte_value_deserialize_scalar(
    astarte_bson_element_t bson_elem, astarte_value_t *value);

/**
 * @brief Deserialize a bson element containing an array.
 *
 * @param[in] bson_elem BSON element to deserialize.
 * @param[out] value The Astarte value where to store the deserialized data.
 * @return ASTARTE_RESULT_OK upon success, an error code otherwise.
 */
static astarte_result_t astarte_value_deserialize_array(
    astarte_bson_element_t bson_elem, astarte_value_t *value);

/**
 * @brief Deserialize a bson element containing an array of doubles.
 *
 * @param[in] bson_doc BSON document containing the array to deserialize.
 * @param[out] value The Astarte value where to store the deserialized data.
 * @param[in] array_length The number of elements of the BSON array.
 * @return ASTARTE_RESULT_OK upon success, an error code otherwise.
 */
static astarte_result_t astarte_value_deserialize_array_double(
    astarte_bson_document_t bson_doc, astarte_value_t *value, size_t array_length);

/**
 * @brief Deserialize a bson element containing an array of strings.
 *
 * @param[in] bson_doc BSON document containing the array to deserialize.
 * @param[out] value The Astarte value where to store the deserialized data.
 * @param[in] array_length The number of elements of the BSON array.
 * @return ASTARTE_RESULT_OK upon success, an error code otherwise.
 */
static astarte_result_t astarte_value_deserialize_array_string(
    astarte_bson_document_t bson_doc, astarte_value_t *value, size_t array_length);

/**
 * @brief Deserialize a bson element containing an array of booleans.
 *
 * @param[in] bson_doc BSON document containing the array to deserialize.
 * @param[out] value The Astarte value where to store the deserialized data.
 * @param[in] array_length The number of elements of the BSON array.
 * @return ASTARTE_RESULT_OK upon success, an error code otherwise.
 */
static astarte_result_t astarte_value_deserialize_array_bool(
    astarte_bson_document_t bson_doc, astarte_value_t *value, size_t array_length);

/**
 * @brief Deserialize a bson element containing an array of datetimes.
 *
 * @param[in] bson_doc BSON document containing the array to deserialize.
 * @param[out] value The Astarte value where to store the deserialized data.
 * @param[in] array_length The number of elements of the BSON array.
 * @return ASTARTE_RESULT_OK upon success, an error code otherwise.
 */
static astarte_result_t astarte_value_deserialize_array_datetime(
    astarte_bson_document_t bson_doc, astarte_value_t *value, size_t array_length);

/**
 * @brief Deserialize a bson element containing an array of integers.
 *
 * @param[in] bson_doc BSON document containing the array to deserialize.
 * @param[out] value The Astarte value where to store the deserialized data.
 * @param[in] array_length The number of elements of the BSON array.
 * @return ASTARTE_RESULT_OK upon success, an error code otherwise.
 */
static astarte_result_t astarte_value_deserialize_array_int32(
    astarte_bson_document_t bson_doc, astarte_value_t *value, size_t array_length);

/**
 * @brief Deserialize a bson element containing an array of long integers.
 *
 * @param[in] bson_doc BSON document containing the array to deserialize.
 * @param[out] value The Astarte value where to store the deserialized data.
 * @param[in] array_length The number of elements of the BSON array.
 * @return ASTARTE_RESULT_OK upon success, an error code otherwise.
 */
static astarte_result_t astarte_value_deserialize_array_int64(
    astarte_bson_document_t bson_doc, astarte_value_t *value, size_t array_length);

/**
 * @brief Deserialize a bson element containing an array of binary blobs.
 *
 * @param[in] bson_doc BSON document containing the array to deserialize.
 * @param[out] value The Astarte value where to store the deserialized data.
 * @param[in] array_length The number of elements of the BSON array.
 * @return ASTARTE_RESULT_OK upon success, an error code otherwise.
 */
static astarte_result_t astarte_value_deserialize_array_binblob(
    astarte_bson_document_t bson_doc, astarte_value_t *value, size_t array_length);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_value_serialize(
    astarte_bson_serializer_handle_t bson, const char *key, astarte_value_t value)
{
    astarte_result_t res = ASTARTE_RESULT_OK;

    switch (value.tag) {
        case ASTARTE_MAPPING_TYPE_INTEGER:
            astarte_bson_serializer_append_int32(bson, key, value.data.integer);
            break;
        case ASTARTE_MAPPING_TYPE_LONGINTEGER:
            astarte_bson_serializer_append_int64(bson, key, value.data.longinteger);
            break;
        case ASTARTE_MAPPING_TYPE_DOUBLE:
            astarte_bson_serializer_append_double(bson, key, value.data.dbl);
            break;
        case ASTARTE_MAPPING_TYPE_STRING:
            astarte_bson_serializer_append_string(bson, key, value.data.string);
            break;
        case ASTARTE_MAPPING_TYPE_BINARYBLOB: {
            astarte_value_binaryblob_t binaryblob = value.data.binaryblob;
            astarte_bson_serializer_append_binary(bson, key, binaryblob.buf, binaryblob.len);
            break;
        }
        case ASTARTE_MAPPING_TYPE_BOOLEAN:
            astarte_bson_serializer_append_boolean(bson, key, value.data.boolean);
            break;
        case ASTARTE_MAPPING_TYPE_DATETIME:
            astarte_bson_serializer_append_datetime(bson, key, value.data.datetime);
            break;
        case ASTARTE_MAPPING_TYPE_INTEGERARRAY: {
            astarte_value_integerarray_t int32_array = value.data.integer_array;
            res = astarte_bson_serializer_append_int32_array(
                bson, key, int32_array.buf, (int) int32_array.len);
            break;
        }
        case ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY: {
            astarte_value_longintegerarray_t int64_array = value.data.longinteger_array;
            res = astarte_bson_serializer_append_int64_array(
                bson, key, int64_array.buf, (int) int64_array.len);
            break;
        }
        case ASTARTE_MAPPING_TYPE_DOUBLEARRAY: {
            astarte_value_doublearray_t double_array = value.data.double_array;
            res = astarte_bson_serializer_append_double_array(
                bson, key, double_array.buf, (int) double_array.len);
            break;
        }
        case ASTARTE_MAPPING_TYPE_STRINGARRAY: {
            astarte_value_stringarray_t string_array = value.data.string_array;
            res = astarte_bson_serializer_append_string_array(
                bson, key, string_array.buf, (int) string_array.len);
            break;
        }
        case ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY: {
            astarte_value_binaryblobarray_t binary_arrays = value.data.binaryblob_array;
            res = astarte_bson_serializer_append_binary_array(
                bson, key, binary_arrays.blobs, binary_arrays.sizes, (int) binary_arrays.count);
            break;
        }
        case ASTARTE_MAPPING_TYPE_BOOLEANARRAY: {
            astarte_value_booleanarray_t bool_array = value.data.boolean_array;
            res = astarte_bson_serializer_append_boolean_array(
                bson, key, bool_array.buf, (int) bool_array.len);
            break;
        }
        case ASTARTE_MAPPING_TYPE_DATETIMEARRAY: {
            astarte_value_longintegerarray_t dt_array = value.data.datetime_array;
            res = astarte_bson_serializer_append_datetime_array(
                bson, key, dt_array.buf, (int) dt_array.len);
            break;
        }
        default:
            res = ASTARTE_RESULT_INVALID_PARAM;
            break;
    }

    return res;
}

astarte_result_t astarte_value_pair_serialize(
    astarte_bson_serializer_handle_t bson, astarte_value_pair_t *values, size_t values_length)
{
    astarte_result_t res = ASTARTE_RESULT_OK;
    for (size_t i = 0; i < values_length; i++) {
        res = astarte_value_serialize(bson, values[i].endpoint, values[i].value);
        if (res != ASTARTE_RESULT_OK) {
            break;
        }
    }

    return res;
}

astarte_result_t astarte_value_deserialize(astarte_bson_element_t bson_elem, astarte_value_t *value)
{
    astarte_result_t res = ASTARTE_RESULT_OK;
    switch (bson_elem.type) {
        case ASTARTE_BSON_TYPE_DOUBLE:
        case ASTARTE_BSON_TYPE_STRING:
        case ASTARTE_BSON_TYPE_BINARY:
        case ASTARTE_BSON_TYPE_BOOLEAN:
        case ASTARTE_BSON_TYPE_DATETIME:
        case ASTARTE_BSON_TYPE_INT32:
        case ASTARTE_BSON_TYPE_INT64:
            res = astarte_value_deserialize_scalar(bson_elem, value);
            break;
        case ASTARTE_BSON_TYPE_ARRAY:
            res = astarte_value_deserialize_array(bson_elem, value);
            break;
        default:
            ASTARTE_LOG_ERR("Unsupported BSON type.");
            res = ASTARTE_RESULT_BSON_DESERIALIZER_ERROR;
            break;
    }

    return res;
}

void astarte_value_destroy_deserialized(astarte_value_t value)
{
    switch (value.tag) {
        case ASTARTE_MAPPING_TYPE_INTEGERARRAY:
            free(value.data.integer_array.buf);
            break;
        case ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY:
            free(value.data.longinteger_array.buf);
            break;
        case ASTARTE_MAPPING_TYPE_DOUBLEARRAY:
            free(value.data.double_array.buf);
            break;
        case ASTARTE_MAPPING_TYPE_STRINGARRAY:
            free(value.data.string_array.buf);
            break;
        case ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY:
            free(value.data.binaryblob_array.sizes);
            free(value.data.binaryblob_array.blobs);
            break;
        case ASTARTE_MAPPING_TYPE_BOOLEANARRAY:
            free(value.data.boolean_array.buf);
            break;
        case ASTARTE_MAPPING_TYPE_DATETIMEARRAY:
            free(value.data.datetime_array.buf);
            break;
        default:
            break;
    }
}

astarte_result_t astarte_value_pair_deserialize(
    astarte_bson_element_t bson_elem, astarte_value_pair_t **values, size_t *values_length)
{
    astarte_value_pair_t *pairs = NULL;
    astarte_result_t res = ASTARTE_RESULT_OK;

    // Step 1: extract the document from the BSON and calculate its length
    if (bson_elem.type != ASTARTE_BSON_TYPE_DOCUMENT) {
        ASTARTE_LOG_ERR("Received BSON element that is not a document.");
        res = ASTARTE_RESULT_BSON_DESERIALIZER_ERROR;
        goto failure;
    }
    astarte_bson_document_t bson_doc = astarte_bson_deserializer_element_to_document(bson_elem);

    size_t bson_doc_length = 0;
    res = astarte_bson_deserializer_doc_count_elements(bson_doc, &bson_doc_length);
    if (res != ASTARTE_RESULT_OK) {
        goto failure;
    }
    if (bson_doc_length == 0) {
        ASTARTE_LOG_ERR("BSON document can't be empty.");
        res = ASTARTE_RESULT_BSON_EMPTY_DOCUMENT_ERROR;
        goto failure;
    }

    // Step 2: Allocate sufficient memory for all the astarte value pairs
    pairs = calloc(bson_doc_length, sizeof(astarte_value_pair_t));
    if (!pairs) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        res = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto failure;
    }

    // Step 3: Fill the allocated memory
    astarte_bson_element_t inner_element;
    res = astarte_bson_deserializer_first_element(bson_doc, &inner_element);
    if (res != ASTARTE_RESULT_OK) {
        goto failure;
    }
    pairs[0].endpoint = inner_element.name;
    res = astarte_value_deserialize(inner_element, &(pairs[0].value));
    if (res != ASTARTE_RESULT_OK) {
        goto failure;
    }

    for (size_t i = 1; i < bson_doc_length; i++) {
        res = astarte_bson_deserializer_next_element(bson_doc, inner_element, &inner_element);
        if (res != ASTARTE_RESULT_OK) {
            for (size_t j = 0; j < i; j++) {
                astarte_value_destroy_deserialized(pairs[j].value);
            }
            goto failure;
        }

        pairs[i].endpoint = inner_element.name;
        res = astarte_value_deserialize(inner_element, &(pairs[i].value));
        if (res != ASTARTE_RESULT_OK) {
            for (size_t j = 0; j < i; j++) {
                astarte_value_destroy_deserialized(pairs[j].value);
            }
            goto failure;
        }
    }

    // Step 4: fill in the output variables
    *values_length = bson_doc_length;
    *values = pairs;

    return ASTARTE_RESULT_OK;

failure:
    free(pairs);

    return res;
}

void astarte_value_pair_destroy_deserialized(astarte_value_pair_t *values, size_t values_length)
{
    for (size_t i = 0; i < values_length; i++) {
        astarte_value_destroy_deserialized(values[i].value);
    }
    free(values);
}

// clang-format off
#define DEFINE_ASTARTE_VALUE_MAKE_FN(NAME, ENUM, TYPE, PARAM)                                      \
    astarte_value_t astarte_value_from_##NAME(TYPE PARAM)                                          \
    {                                                                                              \
        return (astarte_value_t) {                                                                 \
            .data = {                                                                             \
                .PARAM = (PARAM),                                                                  \
            },                                                                                     \
            .tag = (ENUM),                                                                         \
        };                                                                                         \
    }

#define DEFINE_ASTARTE_ARRAY_VALUE_MAKE_FN(NAME, ENUM, TYPE, PARAM)                                \
    astarte_value_t astarte_value_from_##NAME(TYPE PARAM, size_t len)                              \
    {                                                                                              \
        return (astarte_value_t) {                                                                 \
            .data = {                                                                             \
                .PARAM = {                                                                         \
                    .buf = (PARAM),                                                                \
                    .len = len,                                                                    \
                },                                                                                 \
            },                                                                                     \
            .tag = (ENUM),                                                                         \
        };                                                                                         \
    }
// clang-format on

DEFINE_ASTARTE_VALUE_MAKE_FN(integer, ASTARTE_MAPPING_TYPE_INTEGER, int32_t, integer)
DEFINE_ASTARTE_VALUE_MAKE_FN(longinteger, ASTARTE_MAPPING_TYPE_LONGINTEGER, int64_t, longinteger)
DEFINE_ASTARTE_VALUE_MAKE_FN(double, ASTARTE_MAPPING_TYPE_DOUBLE, double, dbl)
DEFINE_ASTARTE_VALUE_MAKE_FN(string, ASTARTE_MAPPING_TYPE_STRING, const char *, string)
DEFINE_ASTARTE_ARRAY_VALUE_MAKE_FN(binaryblob, ASTARTE_MAPPING_TYPE_BINARYBLOB, void *, binaryblob)
DEFINE_ASTARTE_VALUE_MAKE_FN(boolean, ASTARTE_MAPPING_TYPE_BOOLEAN, bool, boolean)
DEFINE_ASTARTE_VALUE_MAKE_FN(datetime, ASTARTE_MAPPING_TYPE_DATETIME, int64_t, datetime)

DEFINE_ASTARTE_ARRAY_VALUE_MAKE_FN(
    integer_array, ASTARTE_MAPPING_TYPE_INTEGERARRAY, int32_t *, integer_array)
DEFINE_ASTARTE_ARRAY_VALUE_MAKE_FN(
    longinteger_array, ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY, int64_t *, longinteger_array)
DEFINE_ASTARTE_ARRAY_VALUE_MAKE_FN(
    double_array, ASTARTE_MAPPING_TYPE_DOUBLEARRAY, double *, double_array)
DEFINE_ASTARTE_ARRAY_VALUE_MAKE_FN(
    string_array, ASTARTE_MAPPING_TYPE_STRINGARRAY, const char **, string_array)
DEFINE_ASTARTE_ARRAY_VALUE_MAKE_FN(
    boolean_array, ASTARTE_MAPPING_TYPE_BOOLEANARRAY, bool *, boolean_array)
DEFINE_ASTARTE_ARRAY_VALUE_MAKE_FN(
    datetime_array, ASTARTE_MAPPING_TYPE_DATETIMEARRAY, int64_t *, datetime_array)

astarte_value_t astarte_value_from_binaryblob_array(const void **buf, size_t *sizes, size_t count)
{
    return (astarte_value_t) {
        .data = {
            .binaryblob_array = {
                .blobs = buf,
                .sizes = sizes,
                .count = count,
            },
        },
        .tag = ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY,
    };
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static astarte_result_t astarte_value_deserialize_scalar(
    astarte_bson_element_t bson_elem, astarte_value_t *value)
{
    astarte_result_t res = ASTARTE_RESULT_OK;
    switch (bson_elem.type) {
        case ASTARTE_BSON_TYPE_DOUBLE:
            ASTARTE_LOG_DBG("Deserializing double value.");
            double double_tmp = astarte_bson_deserializer_element_to_double(bson_elem);
            *value = astarte_value_from_double(double_tmp);
            break;
        case ASTARTE_BSON_TYPE_STRING:
            ASTARTE_LOG_DBG("Deserializing string value.");
            const char *string_tmp = astarte_bson_deserializer_element_to_string(bson_elem, NULL);
            *value = astarte_value_from_string(string_tmp);
            break;
        case ASTARTE_BSON_TYPE_BINARY:
            ASTARTE_LOG_DBG("Deserializing binary blob value.");
            uint32_t len = 0;
            const uint8_t *bin_tmp = astarte_bson_deserializer_element_to_binary(bson_elem, &len);
            *value = astarte_value_from_binaryblob((void *) bin_tmp, len);
            break;
        case ASTARTE_BSON_TYPE_BOOLEAN:
            ASTARTE_LOG_DBG("Deserializing boolean value.");
            bool bool_tmp = astarte_bson_deserializer_element_to_bool(bson_elem);
            *value = astarte_value_from_boolean(bool_tmp);
            break;
        case ASTARTE_BSON_TYPE_DATETIME:
            ASTARTE_LOG_DBG("Deserializing datetime value.");
            int64_t datetime_tmp = astarte_bson_deserializer_element_to_datetime(bson_elem);
            *value = astarte_value_from_datetime(datetime_tmp);
            break;
        case ASTARTE_BSON_TYPE_INT32:
            ASTARTE_LOG_DBG("Deserializing integer value.");
            int32_t int32_tmp = astarte_bson_deserializer_element_to_int32(bson_elem);
            *value = astarte_value_from_integer(int32_tmp);
            break;
        case ASTARTE_BSON_TYPE_INT64:
            ASTARTE_LOG_DBG("Deserializing long integer value.");
            int64_t int64_tmp = astarte_bson_deserializer_element_to_int64(bson_elem);
            *value = astarte_value_from_longinteger(int64_tmp);
            break;
        default:
            ASTARTE_LOG_ERR("Unsupported BSON type.");
            res = ASTARTE_RESULT_BSON_DESERIALIZER_ERROR;
            break;
    }
    return res;
}

static astarte_result_t astarte_value_deserialize_array(
    astarte_bson_element_t bson_elem, astarte_value_t *value)
{
    astarte_result_t res = ASTARTE_RESULT_OK;
    astarte_bson_document_t bson_doc = astarte_bson_deserializer_element_to_array(bson_elem);

    // Step 1: figure out the size and type of the array
    astarte_bson_element_t inner_element;
    res = astarte_bson_deserializer_first_element(bson_doc, &inner_element);
    if (res != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Deserialization of empty arrays is not yet a supported feature.");
        return ASTARTE_RESULT_BSON_EMPTY_ARRAY_ERROR;
    }
    uint8_t array_type = inner_element.type;
    size_t array_length = 0U;

    do {
        array_length++;
        if (inner_element.type != array_type) {
            ASTARTE_LOG_ERR("BSON array contains elements of different types.");
            return ASTARTE_RESULT_BSON_ARRAY_TYPES_ERROR;
        }
        res = astarte_bson_deserializer_next_element(bson_doc, inner_element, &inner_element);
    } while (res == ASTARTE_RESULT_OK);

    if (res != ASTARTE_RESULT_NOT_FOUND) {
        return res;
    }

    // Step 2: depending on the array type call the appropriate function
    switch (array_type) {
        case ASTARTE_BSON_TYPE_DOUBLE:
            ASTARTE_LOG_DBG("Deserializing array of doubles.");
            res = astarte_value_deserialize_array_double(bson_doc, value, array_length);
            break;
        case ASTARTE_BSON_TYPE_STRING:
            ASTARTE_LOG_DBG("Deserializing array of strings.");
            res = astarte_value_deserialize_array_string(bson_doc, value, array_length);
            break;
        case ASTARTE_BSON_TYPE_BINARY:
            ASTARTE_LOG_DBG("Deserializing array of binary blobs.");
            res = astarte_value_deserialize_array_binblob(bson_doc, value, array_length);
            break;
        case ASTARTE_BSON_TYPE_BOOLEAN:
            ASTARTE_LOG_DBG("Deserializing array of booleans.");
            res = astarte_value_deserialize_array_bool(bson_doc, value, array_length);
            break;
        case ASTARTE_BSON_TYPE_DATETIME:
            ASTARTE_LOG_DBG("Deserializing array of datetimes.");
            res = astarte_value_deserialize_array_datetime(bson_doc, value, array_length);
            break;
        case ASTARTE_BSON_TYPE_INT32:
            ASTARTE_LOG_DBG("Deserializing array of integers.");
            res = astarte_value_deserialize_array_int32(bson_doc, value, array_length);
            break;
        case ASTARTE_BSON_TYPE_INT64:
            ASTARTE_LOG_DBG("Deserializing array of long integers.");
            res = astarte_value_deserialize_array_int64(bson_doc, value, array_length);
            break;
        default:
            ASTARTE_LOG_ERR("Unsupported BSON type.");
            res = ASTARTE_RESULT_BSON_DESERIALIZER_ERROR;
            break;
    }

    return res;
}

// clang-format off
// NOLINTBEGIN(bugprone-macro-parentheses) Some can't be wrapped in parenthesis
#define ASTARTE_VALUE_DESERIALIZE_ARRAY_MAKE_FN(NAME, TYPE, TAG, UNION)                            \
static astarte_result_t astarte_value_deserialize_array_##NAME(                                    \
    astarte_bson_document_t bson_doc, astarte_value_t *value, size_t array_length)                 \
{                                                                                                  \
    astarte_result_t res = ASTARTE_RESULT_OK;                                                      \
    TYPE *array = calloc(array_length, sizeof(TYPE));                                              \
    if (!array) {                                                                                  \
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);                               \
        res = ASTARTE_RESULT_OUT_OF_MEMORY;                                                        \
        goto failure;                                                                              \
    }                                                                                              \
                                                                                                   \
    astarte_bson_element_t inner_element;                                                          \
    res = astarte_bson_deserializer_first_element(bson_doc, &inner_element);                       \
    if (res != ASTARTE_RESULT_OK) {                                                                \
        goto failure;                                                                              \
    }                                                                                              \
    array[0] = astarte_bson_deserializer_element_to_##NAME(inner_element);                         \
                                                                                                   \
    for (size_t i = 1; i < array_length; i++) {                                                    \
        res = astarte_bson_deserializer_next_element(bson_doc, inner_element, &inner_element);     \
        if (res != ASTARTE_RESULT_OK) {                                                            \
            goto failure;                                                                          \
        }                                                                                          \
        array[i] = astarte_bson_deserializer_element_to_##NAME(inner_element);                     \
    }                                                                                              \
                                                                                                   \
    value->tag = (TAG);                                                                            \
    value->data.UNION.len = array_length;                                                          \
    value->data.UNION.buf = array;                                                                 \
    return ASTARTE_RESULT_OK;                                                                      \
                                                                                                   \
failure:                                                                                           \
    free(array);                                                                                   \
    return res;                                                                                    \
}
// NOLINTEND(bugprone-macro-parentheses)
// clang-format on

ASTARTE_VALUE_DESERIALIZE_ARRAY_MAKE_FN(
    double, double, ASTARTE_MAPPING_TYPE_DOUBLEARRAY, double_array)
ASTARTE_VALUE_DESERIALIZE_ARRAY_MAKE_FN(
    bool, bool, ASTARTE_MAPPING_TYPE_BOOLEANARRAY, boolean_array)
ASTARTE_VALUE_DESERIALIZE_ARRAY_MAKE_FN(
    datetime, int64_t, ASTARTE_MAPPING_TYPE_DATETIMEARRAY, datetime_array)
ASTARTE_VALUE_DESERIALIZE_ARRAY_MAKE_FN(
    int32, int32_t, ASTARTE_MAPPING_TYPE_INTEGERARRAY, integer_array)
ASTARTE_VALUE_DESERIALIZE_ARRAY_MAKE_FN(
    int64, int64_t, ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY, longinteger_array)

static astarte_result_t astarte_value_deserialize_array_string(
    astarte_bson_document_t bson_doc, astarte_value_t *value, size_t array_length)
{
    astarte_result_t res = ASTARTE_RESULT_OK;
    // Step 1: allocate enough memory to contain the array from the BSON file
    const char **array = calloc(array_length, sizeof(char *));
    if (!array) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        res = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto failure;
    }

    // Step 2: fill in the array with data
    astarte_bson_element_t inner_element;
    res = astarte_bson_deserializer_first_element(bson_doc, &inner_element);
    if (res != ASTARTE_RESULT_OK) {
        goto failure;
    }
    array[0] = astarte_bson_deserializer_element_to_string(inner_element, NULL);

    for (size_t i = 1; i < array_length; i++) {
        res = astarte_bson_deserializer_next_element(bson_doc, inner_element, &inner_element);
        if (res != ASTARTE_RESULT_OK) {
            goto failure;
        }
        array[i] = astarte_bson_deserializer_element_to_string(inner_element, NULL);
    }

    // Step 3: Place the generated array in the astarte_value struct
    value->tag = ASTARTE_MAPPING_TYPE_STRINGARRAY;
    value->data.string_array.len = array_length;
    value->data.string_array.buf = array;

    return ASTARTE_RESULT_OK;

failure:
    free(array);
    return res;
}

static astarte_result_t astarte_value_deserialize_array_binblob(
    astarte_bson_document_t bson_doc, astarte_value_t *value, size_t array_length)
{
    astarte_result_t res = ASTARTE_RESULT_OK;
    const uint8_t **array = NULL;
    size_t *array_sizes = NULL;
    // Step 1: allocate enough memory to contain the array from the BSON file
    array = calloc(array_length, sizeof(uint8_t *));
    if (!array) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        res = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto failure;
    }
    array_sizes = calloc(array_length, sizeof(size_t));
    if (!array_sizes) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        res = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto failure;
    }

    // Step 2: fill in the array with data
    astarte_bson_element_t inner_element;
    res = astarte_bson_deserializer_first_element(bson_doc, &inner_element);
    if (res != ASTARTE_RESULT_OK) {
        goto failure;
    }
    size_t array_entry_size = 0;
    array[0] = astarte_bson_deserializer_element_to_binary(inner_element, &array_entry_size);
    array_sizes[0] = array_entry_size;

    for (size_t i = 1; i < array_length; i++) {
        res = astarte_bson_deserializer_next_element(bson_doc, inner_element, &inner_element);
        if (res != ASTARTE_RESULT_OK) {
            goto failure;
        }
        array[i] = astarte_bson_deserializer_element_to_binary(inner_element, &array_entry_size);
        array_sizes[i] = array_entry_size;
    }

    // Step 3: Place the generated array in the astarte_value struct
    value->tag = ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY;
    value->data.binaryblob_array.count = array_length;
    value->data.binaryblob_array.sizes = array_sizes;
    value->data.binaryblob_array.blobs = (const void **) array;

    return ASTARTE_RESULT_OK;

failure:
    free(array);
    free(array_sizes);
    return res;
}
