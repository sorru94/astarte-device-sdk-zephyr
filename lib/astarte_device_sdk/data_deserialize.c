/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "data_deserialize.h"

#include <stdlib.h>
#include <string.h>

#include "bson_deserializer.h"
#include "bson_types.h"
#include "interface_private.h"
#include "mapping_private.h"

#include "log.h"
ASTARTE_LOG_MODULE_REGISTER(data_deserialize, CONFIG_ASTARTE_DEVICE_SDK_DATA_DESERIALIZE_LOG_LEVEL);

// NOLINENUMBERLINT

/************************************************
 *         Static functions declaration         *
 ***********************************************/

/**
 * @brief Fill an empty array Astarte data of the required type.
 *
 * @param[in] type Mapping type to use for the Astarte data.
 * @param[out] data The Astarte data to fill.
 * @return An Astarte result that may take the following values:
 * @retval ASTARTE_RESULT_OK upon success
 * @retval ASTARTE_RESULT_INTERNAL_ERROR if the input mapping type is not an array.
 */
static astarte_result_t initialize_empty_array(astarte_mapping_type_t type, astarte_data_t *data);

/**
 * @brief Deserialize a scalar bson element.
 *
 * @param[in] bson_elem BSON element to deserialize.
 * @param[in] type The expected type for the Astarte data.
 * @param[out] data The Astarte data where to store the deserialized data.
 * @return ASTARTE_RESULT_OK upon success, an error code otherwise.
 */
static astarte_result_t deserialize_scalar(
    astarte_bson_element_t bson_elem, astarte_mapping_type_t type, astarte_data_t *data);

/**
 * @brief Deserialize a bson element containing a binaryblob.
 *
 * @note This function will perform dynamic allocation.
 *
 * @param[in] bson_elem BSON element to deserialize.
 * @param[out] data The Astarte data where to store the deserialized data.
 * @return ASTARTE_RESULT_OK upon success, an error code otherwise.
 */
static astarte_result_t deserialize_binaryblob(
    astarte_bson_element_t bson_elem, astarte_data_t *data);

/**
 * @brief Deserialize a bson element containing a string.
 *
 * @note This function will perform dynamic allocation.
 *
 * @param[in] bson_elem BSON element to deserialize.
 * @param[out] data The Astarte data where to store the deserialized data.
 * @return ASTARTE_RESULT_OK upon success, an error code otherwise.
 */
static astarte_result_t deserialize_string(astarte_bson_element_t bson_elem, astarte_data_t *data);

/**
 * @brief Deserialize a bson element containing an array.
 *
 * @param[in] bson_elem BSON element to deserialize.
 * @param[in] type The expected type for the Astarte data.
 * @param[out] data The Astarte data where to store the deserialized data.
 * @return ASTARTE_RESULT_OK upon success, an error code otherwise.
 */
static astarte_result_t deserialize_array(
    astarte_bson_element_t bson_elem, astarte_mapping_type_t type, astarte_data_t *data);

/**
 * @brief Deserialize a bson element containing an array of doubles.
 *
 * @param[in] bson_doc BSON document containing the array to deserialize.
 * @param[out] data The Astarte data where to store the deserialized data.
 * @return ASTARTE_RESULT_OK upon success, an error code otherwise.
 */
static astarte_result_t deserialize_array_double(
    astarte_bson_document_t bson_doc, astarte_data_t *data);

/**
 * @brief Deserialize a bson element containing an array of strings.
 *
 * @param[in] bson_doc BSON document containing the array to deserialize.
 * @param[out] data The Astarte data where to store the deserialized data.
 * @return ASTARTE_RESULT_OK upon success, an error code otherwise.
 */
static astarte_result_t deserialize_array_string(
    astarte_bson_document_t bson_doc, astarte_data_t *data);

/**
 * @brief Deserialize a bson element containing an array of booleans.
 *
 * @param[in] bson_doc BSON document containing the array to deserialize.
 * @param[out] data The Astarte data where to store the deserialized data.
 * @return ASTARTE_RESULT_OK upon success, an error code otherwise.
 */
static astarte_result_t deserialize_array_bool(
    astarte_bson_document_t bson_doc, astarte_data_t *data);

/**
 * @brief Deserialize a bson element containing an array of datetimes.
 *
 * @param[in] bson_doc BSON document containing the array to deserialize.
 * @param[out] data The Astarte data where to store the deserialized data.
 * @return ASTARTE_RESULT_OK upon success, an error code otherwise.
 */
static astarte_result_t deserialize_array_datetime(
    astarte_bson_document_t bson_doc, astarte_data_t *data);

/**
 * @brief Deserialize a bson element containing an array of integers.
 *
 * @param[in] bson_doc BSON document containing the array to deserialize.
 * @param[out] data The Astarte data where to store the deserialized data.
 * @return ASTARTE_RESULT_OK upon success, an error code otherwise.
 */
static astarte_result_t deserialize_array_int32(
    astarte_bson_document_t bson_doc, astarte_data_t *data);

/**
 * @brief Deserialize a bson element containing an array of long integers.
 *
 * @param[in] bson_doc BSON document containing the array to deserialize.
 * @param[out] data The Astarte data where to store the deserialized data.
 * @return ASTARTE_RESULT_OK upon success, an error code otherwise.
 */
static astarte_result_t deserialize_array_int64(
    astarte_bson_document_t bson_doc, astarte_data_t *data);
/**
 * @brief Helper function to resize the parallel arrays used for binary blob arrays.
 *
 * @param[in,out] blobs Pointer to the array of uint8_t pointers.
 * @param[in,out] sizes Pointer to the array of sizes.
 * @param[in,out] capacity Pointer to the current capacity.
 * @return ASTARTE_RESULT_OK upon success, an error code otherwise.
 */
static astarte_result_t resize_binblob_arrays(uint8_t ***blobs, size_t **sizes, size_t *capacity);
/**
 * @brief Deserialize a bson element containing an array of binary blobs.
 *
 * @param[in] bson_doc BSON document containing the array to deserialize.
 * @param[out] data The Astarte data where to store the deserialized data.
 * @return ASTARTE_RESULT_OK upon success, an error code otherwise.
 */
static astarte_result_t deserialize_array_binblob(
    astarte_bson_document_t bson_doc, astarte_data_t *data);

/**
 * @brief Check if a BSON type is compatible with a mapping type.
 *
 * @param[in] mapping_type Mapping type to evaluate
 * @param[in] bson_type BSON type to evaluate.
 * @return true if the BSON type and mapping type are compatible, false otherwise.
 */
static bool check_if_bson_type_is_mapping_type(
    astarte_mapping_type_t mapping_type, uint8_t bson_type);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t data_deserialize(
    astarte_bson_element_t bson_elem, astarte_mapping_type_t type, astarte_data_t *data)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    switch (type) {
        case ASTARTE_MAPPING_TYPE_BINARYBLOB:
        case ASTARTE_MAPPING_TYPE_BOOLEAN:
        case ASTARTE_MAPPING_TYPE_DATETIME:
        case ASTARTE_MAPPING_TYPE_DOUBLE:
        case ASTARTE_MAPPING_TYPE_INTEGER:
        case ASTARTE_MAPPING_TYPE_LONGINTEGER:
        case ASTARTE_MAPPING_TYPE_STRING:
            ares = deserialize_scalar(bson_elem, type, data);
            break;
        case ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY:
        case ASTARTE_MAPPING_TYPE_BOOLEANARRAY:
        case ASTARTE_MAPPING_TYPE_DATETIMEARRAY:
        case ASTARTE_MAPPING_TYPE_DOUBLEARRAY:
        case ASTARTE_MAPPING_TYPE_INTEGERARRAY:
        case ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY:
        case ASTARTE_MAPPING_TYPE_STRINGARRAY:
            ares = deserialize_array(bson_elem, type, data);
            break;
        default:
            ASTARTE_LOG_ERR("Unsupported mapping type.");
            ares = ASTARTE_RESULT_INTERNAL_ERROR;
            break;
    }

    if (ares == ASTARTE_RESULT_OK) {
        data->is_owned = true;
    }

    return ares;
}

void data_destroy_deserialized(astarte_data_t data)
{
    if (!data.is_owned) {
        return;
    }

    switch (data.tag) {
        case ASTARTE_MAPPING_TYPE_BINARYBLOB:
            free((void *) data.data.binaryblob.buf);
            break;
        case ASTARTE_MAPPING_TYPE_STRING:
            free((void *) data.data.string);
            break;
        case ASTARTE_MAPPING_TYPE_INTEGERARRAY:
            free((void *) data.data.integer_array.buf);
            break;
        case ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY:
            free((void *) data.data.longinteger_array.buf);
            break;
        case ASTARTE_MAPPING_TYPE_DOUBLEARRAY:
            free((void *) data.data.double_array.buf);
            break;
        case ASTARTE_MAPPING_TYPE_STRINGARRAY:
            for (size_t i = 0; i < data.data.string_array.len; i++) {
                free((void *) data.data.string_array.buf[i]);
            }
            free((void *) data.data.string_array.buf);
            break;
        case ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY:
            for (size_t i = 0; i < data.data.binaryblob_array.count; i++) {
                free((void *) data.data.binaryblob_array.blobs[i]);
            }
            free(data.data.binaryblob_array.sizes);
            free((void *) data.data.binaryblob_array.blobs);
            break;
        case ASTARTE_MAPPING_TYPE_BOOLEANARRAY:
            free((void *) data.data.boolean_array.buf);
            break;
        case ASTARTE_MAPPING_TYPE_DATETIMEARRAY:
            free((void *) data.data.datetime_array.buf);
            break;
        default:
            break;
    }
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static astarte_result_t initialize_empty_array(astarte_mapping_type_t type, astarte_data_t *data)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    switch (type) {
        case ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY:
            data->tag = type;
            data->data.binaryblob_array.count = 0;
            data->data.binaryblob_array.blobs = NULL;
            data->data.binaryblob_array.sizes = NULL;
            break;
        case ASTARTE_MAPPING_TYPE_BOOLEANARRAY:
            data->tag = type;
            data->data.boolean_array.len = 0;
            data->data.boolean_array.buf = NULL;
            break;
        case ASTARTE_MAPPING_TYPE_DATETIMEARRAY:
            data->tag = type;
            data->data.datetime_array.len = 0;
            data->data.datetime_array.buf = NULL;
            break;
        case ASTARTE_MAPPING_TYPE_DOUBLEARRAY:
            data->tag = type;
            data->data.double_array.len = 0;
            data->data.double_array.buf = NULL;
            break;
        case ASTARTE_MAPPING_TYPE_INTEGERARRAY:
            data->tag = type;
            data->data.integer_array.len = 0;
            data->data.integer_array.buf = NULL;
            break;
        case ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY:
            data->tag = type;
            data->data.longinteger_array.len = 0;
            data->data.longinteger_array.buf = NULL;
            break;
        case ASTARTE_MAPPING_TYPE_STRINGARRAY:
            data->tag = type;
            data->data.string_array.len = 0;
            data->data.string_array.buf = NULL;
            break;
        default:
            ASTARTE_LOG_ERR("Creating empty array Astarte data for scalar mapping type.");
            ares = ASTARTE_RESULT_INTERNAL_ERROR;
            break;
    }
    return ares;
}

static astarte_result_t deserialize_scalar(
    astarte_bson_element_t bson_elem, astarte_mapping_type_t type, astarte_data_t *data)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    if (!check_if_bson_type_is_mapping_type(type, bson_elem.type)) {
        ASTARTE_LOG_ERR("BSON element is not of the expected type.");
        return ASTARTE_RESULT_BSON_DESERIALIZER_TYPES_ERROR;
    }

    switch (type) {
        case ASTARTE_MAPPING_TYPE_BINARYBLOB:
            ASTARTE_LOG_DBG("Deserializing binary blob data.");
            ares = deserialize_binaryblob(bson_elem, data);
            break;
        case ASTARTE_MAPPING_TYPE_BOOLEAN:
            ASTARTE_LOG_DBG("Deserializing boolean data.");
            bool bool_tmp = astarte_bson_deserializer_element_to_bool(bson_elem);
            *data = astarte_data_from_boolean(bool_tmp);
            break;
        case ASTARTE_MAPPING_TYPE_DATETIME:
            ASTARTE_LOG_DBG("Deserializing datetime data.");
            int64_t datetime_tmp = astarte_bson_deserializer_element_to_datetime(bson_elem);
            *data = astarte_data_from_datetime(datetime_tmp);
            break;
        case ASTARTE_MAPPING_TYPE_DOUBLE:
            ASTARTE_LOG_DBG("Deserializing double data.");
            double double_tmp = astarte_bson_deserializer_element_to_double(bson_elem);
            *data = astarte_data_from_double(double_tmp);
            break;
        case ASTARTE_MAPPING_TYPE_INTEGER:
            ASTARTE_LOG_DBG("Deserializing integer data.");
            int32_t int32_tmp = astarte_bson_deserializer_element_to_int32(bson_elem);
            *data = astarte_data_from_integer(int32_tmp);
            break;
        case ASTARTE_MAPPING_TYPE_LONGINTEGER:
            ASTARTE_LOG_DBG("Deserializing long integer data.");
            int64_t int64_tmp = 0U;
            if (bson_elem.type == ASTARTE_BSON_TYPE_INT32) {
                int64_tmp = (int64_t) astarte_bson_deserializer_element_to_int32(bson_elem);
            } else {
                int64_tmp = astarte_bson_deserializer_element_to_int64(bson_elem);
            }
            *data = astarte_data_from_longinteger(int64_tmp);
            break;
        case ASTARTE_MAPPING_TYPE_STRING:
            ASTARTE_LOG_DBG("Deserializing string data.");
            ares = deserialize_string(bson_elem, data);
            break;
        default:
            ASTARTE_LOG_ERR("Unsupported mapping type.");
            ares = ASTARTE_RESULT_INTERNAL_ERROR;
            break;
    }
    return ares;
}

static astarte_result_t deserialize_binaryblob(
    astarte_bson_element_t bson_elem, astarte_data_t *data)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    uint8_t *dyn_deserialized = NULL;

    uint32_t deserialized_len = 0;
    const uint8_t *deserialized
        = astarte_bson_deserializer_element_to_binary(bson_elem, &deserialized_len);

    dyn_deserialized = calloc(deserialized_len, sizeof(uint8_t));
    if (!dyn_deserialized) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto failure;
    }

    memcpy(dyn_deserialized, deserialized, deserialized_len);
    // NOLINTNEXTLINE(clang-analyzer-unix.Malloc)
    *data = astarte_data_from_binaryblob((const void *) dyn_deserialized, deserialized_len);
    return ares;

failure:
    free(dyn_deserialized);
    return ares;
}

static astarte_result_t deserialize_string(astarte_bson_element_t bson_elem, astarte_data_t *data)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    char *dyn_deserialized = NULL;

    uint32_t deserialized_len = 0;
    const char *deserialized
        = astarte_bson_deserializer_element_to_string(bson_elem, &deserialized_len);

    dyn_deserialized = calloc(deserialized_len + 1, sizeof(char));
    if (!dyn_deserialized) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto failure;
    }

    strncpy(dyn_deserialized, deserialized, deserialized_len);
    // NOLINTNEXTLINE(clang-analyzer-unix.Malloc)
    *data = astarte_data_from_string(dyn_deserialized);
    return ares;

failure:
    free(dyn_deserialized);
    return ares;
}

static astarte_result_t deserialize_array(
    astarte_bson_element_t bson_elem, astarte_mapping_type_t type, astarte_data_t *data)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    if (bson_elem.type != ASTARTE_BSON_TYPE_ARRAY) {
        ASTARTE_LOG_ERR("Expected an array but BSON element type is %d.", bson_elem.type);
        return ASTARTE_RESULT_BSON_DESERIALIZER_TYPES_ERROR;
    }

    astarte_bson_document_t bson_doc = astarte_bson_deserializer_element_to_array(bson_elem);

    astarte_mapping_type_t scalar_type = ASTARTE_MAPPING_TYPE_BINARYBLOB;
    ares = astarte_mapping_array_to_scalar_type(type, &scalar_type);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Non array type passed to deserialize_array.");
        return ares;
    }

    switch (scalar_type) {
        case ASTARTE_MAPPING_TYPE_BINARYBLOB:
            ASTARTE_LOG_DBG("Deserializing array of binary blobs.");
            ares = deserialize_array_binblob(bson_doc, data);
            break;
        case ASTARTE_MAPPING_TYPE_BOOLEAN:
            ASTARTE_LOG_DBG("Deserializing array of booleans.");
            ares = deserialize_array_bool(bson_doc, data);
            break;
        case ASTARTE_MAPPING_TYPE_DATETIME:
            ASTARTE_LOG_DBG("Deserializing array of datetimes.");
            ares = deserialize_array_datetime(bson_doc, data);
            break;
        case ASTARTE_MAPPING_TYPE_DOUBLE:
            ASTARTE_LOG_DBG("Deserializing array of doubles.");
            ares = deserialize_array_double(bson_doc, data);
            break;
        case ASTARTE_MAPPING_TYPE_INTEGER:
            ASTARTE_LOG_DBG("Deserializing array of integers.");
            ares = deserialize_array_int32(bson_doc, data);
            break;
        case ASTARTE_MAPPING_TYPE_LONGINTEGER:
            ASTARTE_LOG_DBG("Deserializing array of long integers.");
            ares = deserialize_array_int64(bson_doc, data);
            break;
        case ASTARTE_MAPPING_TYPE_STRING:
            ASTARTE_LOG_DBG("Deserializing array of strings.");
            ares = deserialize_array_string(bson_doc, data);
            break;
        default:
            ASTARTE_LOG_ERR("Unsupported mapping type.");
            ares = ASTARTE_RESULT_INTERNAL_ERROR;
            break;
    }

    return ares;
}

// clang-format off
// NOLINTBEGIN(bugprone-macro-parentheses) Some can't be wrapped in parenthesis
#define MAKE_FUNCTION_DESERIALIZE_ARRAY(NAME, TYPE, TAG, SCALAR_TAG, UNION)                        \
static astarte_result_t deserialize_array_##NAME(                                                  \
    astarte_bson_document_t bson_doc, astarte_data_t *data)                                        \
{                                                                                                  \
    astarte_result_t ares = ASTARTE_RESULT_OK;                                                     \
    size_t capacity = 4;                                                                           \
    size_t count = 0;                                                                              \
    TYPE *array = calloc(capacity, sizeof(TYPE));                                                  \
    if (!array) {                                                                                  \
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);                               \
        return ASTARTE_RESULT_OUT_OF_MEMORY;                                                       \
    }                                                                                              \
    astarte_bson_element_t inner_elem = {0};                                                       \
    ares = astarte_bson_deserializer_first_element(bson_doc, &inner_elem);                         \
    while (ares == ASTARTE_RESULT_OK) {                                                            \
        if (!check_if_bson_type_is_mapping_type((SCALAR_TAG), inner_elem.type)) {                  \
            free(array);                                                                           \
            return ASTARTE_RESULT_BSON_DESERIALIZER_TYPES_ERROR;                                   \
        }                                                                                          \
        if (count >= capacity) {                                                                   \
            capacity *= 2;                                                                         \
            TYPE *new_array = realloc(array, capacity * sizeof(TYPE));                             \
            if (!new_array) {                                                                      \
                free(array);                                                                       \
                return ASTARTE_RESULT_OUT_OF_MEMORY;                                               \
            }                                                                                      \
            array = new_array;                                                                     \
        }                                                                                          \
        array[count++] = astarte_bson_deserializer_element_to_##NAME(inner_elem);                  \
        ares = astarte_bson_deserializer_next_element(bson_doc, inner_elem, &inner_elem);          \
    }                                                                                              \
    if (ares != ASTARTE_RESULT_NOT_FOUND) {                                                        \
        free(array);                                                                               \
        return ares;                                                                               \
    }                                                                                              \
    if (count == 0) {                                                                              \
        free(array);                                                                               \
        return initialize_empty_array((TAG), data);                                                \
    }                                                                                              \
    data->tag = (TAG);                                                                             \
    data->data.UNION.len = count;                                                                  \
    data->data.UNION.buf = array;                                                                  \
    return ASTARTE_RESULT_OK;                                                                      \
}
// NOLINTEND(bugprone-macro-parentheses)
// clang-format on

MAKE_FUNCTION_DESERIALIZE_ARRAY(
    double, double, ASTARTE_MAPPING_TYPE_DOUBLEARRAY, ASTARTE_MAPPING_TYPE_DOUBLE, double_array)
MAKE_FUNCTION_DESERIALIZE_ARRAY(
    bool, bool, ASTARTE_MAPPING_TYPE_BOOLEANARRAY, ASTARTE_MAPPING_TYPE_BOOLEAN, boolean_array)
MAKE_FUNCTION_DESERIALIZE_ARRAY(datetime, int64_t, ASTARTE_MAPPING_TYPE_DATETIMEARRAY,
    ASTARTE_MAPPING_TYPE_DATETIME, datetime_array)
MAKE_FUNCTION_DESERIALIZE_ARRAY(
    int32, int32_t, ASTARTE_MAPPING_TYPE_INTEGERARRAY, ASTARTE_MAPPING_TYPE_INTEGER, integer_array)

static astarte_result_t deserialize_array_int64(
    astarte_bson_document_t bson_doc, astarte_data_t *data)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    size_t capacity = 4;
    size_t count = 0;
    int64_t *array = calloc(capacity, sizeof(int64_t));
    if (!array) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        return ASTARTE_RESULT_OUT_OF_MEMORY;
    }

    astarte_bson_element_t inner_elem = { 0 };
    ares = astarte_bson_deserializer_first_element(bson_doc, &inner_elem);
    while (ares == ASTARTE_RESULT_OK) {
        if (!check_if_bson_type_is_mapping_type(
                ASTARTE_MAPPING_TYPE_LONGINTEGER, inner_elem.type)) {
            free(array);
            return ASTARTE_RESULT_BSON_DESERIALIZER_TYPES_ERROR;
        }
        if (count >= capacity) {
            capacity *= 2;
            int64_t *new_array = realloc(array, capacity * sizeof(int64_t));
            if (!new_array) {
                free(array);
                return ASTARTE_RESULT_OUT_OF_MEMORY;
            }
            array = new_array;
        }
        if (inner_elem.type == ASTARTE_BSON_TYPE_INT32) {
            array[count++] = (int64_t) astarte_bson_deserializer_element_to_int32(inner_elem);
        } else {
            array[count++] = astarte_bson_deserializer_element_to_int64(inner_elem);
        }
        ares = astarte_bson_deserializer_next_element(bson_doc, inner_elem, &inner_elem);
    }

    if (ares != ASTARTE_RESULT_NOT_FOUND) {
        free(array);
        return ares;
    }
    if (count == 0) {
        free(array);
        return initialize_empty_array(ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY, data);
    }

    data->tag = ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY;
    data->data.longinteger_array.len = count;
    data->data.longinteger_array.buf = array;
    return ASTARTE_RESULT_OK;
}

static astarte_result_t deserialize_array_string(
    astarte_bson_document_t bson_doc, astarte_data_t *data)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    size_t capacity = 4;
    size_t count = 0;
    char **array = (char **) calloc(capacity, sizeof(char *));
    if (!array) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        return ASTARTE_RESULT_OUT_OF_MEMORY;
    }

    astarte_bson_element_t inner_elem = { 0 };
    ares = astarte_bson_deserializer_first_element(bson_doc, &inner_elem);

    while (ares == ASTARTE_RESULT_OK) {
        if (!check_if_bson_type_is_mapping_type(ASTARTE_MAPPING_TYPE_STRING, inner_elem.type)) {
            ares = ASTARTE_RESULT_BSON_DESERIALIZER_TYPES_ERROR;
            goto failure;
        }
        if (count >= capacity) {
            capacity *= 2;
            char **new_array = (char **) realloc((void *) array, capacity * sizeof(char *));
            if (!new_array) {
                ares = ASTARTE_RESULT_OUT_OF_MEMORY;
                goto failure;
            }
            array = new_array;
        }
        uint32_t deser_len = 0;
        const char *deser = astarte_bson_deserializer_element_to_string(inner_elem, &deser_len);
        array[count] = calloc(deser_len + 1, sizeof(char));
        if (!array[count]) {
            ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
            ares = ASTARTE_RESULT_OUT_OF_MEMORY;
            goto failure;
        }
        strncpy(array[count], deser, deser_len);
        count++;
        ares = astarte_bson_deserializer_next_element(bson_doc, inner_elem, &inner_elem);
    }

    if (ares != ASTARTE_RESULT_NOT_FOUND) {
        goto failure;
    }
    if (count == 0) {
        free((void *) array);
        return initialize_empty_array(ASTARTE_MAPPING_TYPE_STRINGARRAY, data);
    }

    data->tag = ASTARTE_MAPPING_TYPE_STRINGARRAY;
    data->data.string_array.len = count;
    data->data.string_array.buf = (const char **) array;

    return ASTARTE_RESULT_OK;

failure:
    for (size_t i = 0; i < count; i++) {
        free(array[i]);
    }
    free((void *) array);
    return ares;
}

static astarte_result_t resize_binblob_arrays(uint8_t ***blobs, size_t **sizes, size_t *capacity)
{
    size_t new_capacity = *capacity * 2;
    uint8_t **new_blobs = (uint8_t **) realloc((void *) *blobs, new_capacity * sizeof(uint8_t *));
    size_t *new_sizes = (size_t *) realloc(*sizes, new_capacity * sizeof(size_t));

    // Handle partial or full allocation failures
    if (!new_blobs || !new_sizes) {
        if (new_blobs) {
            *blobs = new_blobs;
        }
        if (new_sizes) {
            *sizes = new_sizes;
        }
        return ASTARTE_RESULT_OUT_OF_MEMORY;
    }

    *blobs = new_blobs;
    *sizes = new_sizes;
    *capacity = new_capacity;
    return ASTARTE_RESULT_OK;
}

static astarte_result_t deserialize_array_binblob(
    astarte_bson_document_t bson_doc, astarte_data_t *data)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    size_t capacity = 4;
    size_t count = 0;

    // Initial allocation
    uint8_t **array = (uint8_t **) calloc(capacity, sizeof(uint8_t *));
    size_t *array_sizes = calloc(capacity, sizeof(size_t));

    if (!array || !array_sizes) {
        free((void *) array);
        free(array_sizes);
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        return ASTARTE_RESULT_OUT_OF_MEMORY;
    }

    astarte_bson_element_t inner_elem = { 0 };
    ares = astarte_bson_deserializer_first_element(bson_doc, &inner_elem);

    while (ares == ASTARTE_RESULT_OK) {
        if (!check_if_bson_type_is_mapping_type(ASTARTE_MAPPING_TYPE_BINARYBLOB, inner_elem.type)) {
            ares = ASTARTE_RESULT_BSON_DESERIALIZER_TYPES_ERROR;
            goto failure;
        }

        // Use the new helper to manage resizing
        if (count >= capacity) {
            ares = resize_binblob_arrays(&array, &array_sizes, &capacity);
            if (ares != ASTARTE_RESULT_OK) {
                ASTARTE_LOG_ERR("Out of memory resizing arrays %s: %d", __FILE__, __LINE__);
                goto failure;
            }
        }

        // Deserialize the element
        uint32_t deser_size = 0;
        const uint8_t *deser = astarte_bson_deserializer_element_to_binary(inner_elem, &deser_size);

        array[count] = calloc(deser_size, sizeof(uint8_t));
        if (!array[count]) {
            ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
            ares = ASTARTE_RESULT_OUT_OF_MEMORY;
            goto failure;
        }

        memcpy(array[count], deser, deser_size);
        array_sizes[count] = deser_size;
        count++;

        ares = astarte_bson_deserializer_next_element(bson_doc, inner_elem, &inner_elem);
    }

    if (ares != ASTARTE_RESULT_NOT_FOUND) {
        goto failure;
    }

    if (count == 0) {
        free((void *) array);
        free(array_sizes);
        return initialize_empty_array(ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY, data);
    }

    // Populate the out-parameter
    data->tag = ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY;
    data->data.binaryblob_array.count = count;
    data->data.binaryblob_array.sizes = array_sizes;
    data->data.binaryblob_array.blobs = (const void **) array;

    return ASTARTE_RESULT_OK;

failure:
    // Cleanup on failure
    for (size_t i = 0; i < count; i++) {
        free(array[i]);
    }
    free((void *) array);
    free(array_sizes);
    return ares;
}

static bool check_if_bson_type_is_mapping_type(
    astarte_mapping_type_t mapping_type, uint8_t bson_type)
{
    uint8_t expected_bson_type = '\x00';
    switch (mapping_type) {
        case ASTARTE_MAPPING_TYPE_BINARYBLOB:
            expected_bson_type = ASTARTE_BSON_TYPE_BINARY;
            break;
        case ASTARTE_MAPPING_TYPE_BOOLEAN:
            expected_bson_type = ASTARTE_BSON_TYPE_BOOLEAN;
            break;
        case ASTARTE_MAPPING_TYPE_DATETIME:
            expected_bson_type = ASTARTE_BSON_TYPE_DATETIME;
            break;
        case ASTARTE_MAPPING_TYPE_DOUBLE:
            expected_bson_type = ASTARTE_BSON_TYPE_DOUBLE;
            break;
        case ASTARTE_MAPPING_TYPE_INTEGER:
            expected_bson_type = ASTARTE_BSON_TYPE_INT32;
            break;
        case ASTARTE_MAPPING_TYPE_LONGINTEGER:
            expected_bson_type = ASTARTE_BSON_TYPE_INT64;
            break;
        case ASTARTE_MAPPING_TYPE_STRING:
            expected_bson_type = ASTARTE_BSON_TYPE_STRING;
            break;
        case ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY:
        case ASTARTE_MAPPING_TYPE_BOOLEANARRAY:
        case ASTARTE_MAPPING_TYPE_DATETIMEARRAY:
        case ASTARTE_MAPPING_TYPE_DOUBLEARRAY:
        case ASTARTE_MAPPING_TYPE_INTEGERARRAY:
        case ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY:
        case ASTARTE_MAPPING_TYPE_STRINGARRAY:
            expected_bson_type = ASTARTE_BSON_TYPE_ARRAY;
            break;
        default:
            ASTARTE_LOG_ERR("Invalid mapping type (%d).", mapping_type);
            return false;
    }

    if ((expected_bson_type == ASTARTE_BSON_TYPE_INT64) && (bson_type == ASTARTE_BSON_TYPE_INT32)) {
        return true;
    }

    if (bson_type != expected_bson_type) {
        ASTARTE_LOG_ERR(
            "Mapping type (%d) and BSON type (0x%x) do not match.", mapping_type, bson_type);
        return false;
    }

    return true;
}
