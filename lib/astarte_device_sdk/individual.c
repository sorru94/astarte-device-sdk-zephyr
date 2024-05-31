/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "astarte_device_sdk/individual.h"
#include "individual_private.h"

#include <stdlib.h>

#include "bson_types.h"
#include "interface_private.h"
#include "log.h"
#include "mapping_private.h"

ASTARTE_LOG_MODULE_REGISTER(astarte_individual, CONFIG_ASTARTE_DEVICE_SDK_INDIVIDUAL_LOG_LEVEL);

/************************************************
 *         Static functions declaration         *
 ***********************************************/

/**
 * @brief Fill an empty array Astarte individual of the required type.
 *
 * @param[in] type Mapping type to use for the Astarte individual.
 * @param[out] individual The Astarte individual to fill.
 * @return An Astarte result that may take the following values:
 * @retval ASTARTE_RESULT_OK upon success
 * @retval ASTARTE_RESULT_INTERNAL_ERROR if the input mapping type is not an array.
 */
static astarte_result_t initialize_empty_array(
    astarte_mapping_type_t type, astarte_individual_t *individual);

/**
 * @brief Deserialize a scalar bson element.
 *
 * @param[in] bson_elem BSON element to deserialize.
 * @param[in] type The expected type for the Astarte individual.
 * @param[out] individual The Astarte individual where to store the deserialized data.
 * @return An Astarte result that may take the following values:
 * @retval ASTARTE_RESULT_OK upon success
 * @retval ASTARTE_RESULT_BSON_DESERIALIZER_ERROR if BSON is not a scalar or contains bad data.
 */
static astarte_result_t deserialize_scalar(astarte_bson_element_t bson_elem,
    astarte_mapping_type_t type, astarte_individual_t *individual);

/**
 * @brief Deserialize a bson element containing an array.
 *
 * @param[in] bson_elem BSON element to deserialize.
 * @param[in] type The expected type for the Astarte individual.
 * @param[out] individual The Astarte individual where to store the deserialized data.
 * @return ASTARTE_RESULT_OK upon success, an error code otherwise.
 */
static astarte_result_t deserialize_array(astarte_bson_element_t bson_elem,
    astarte_mapping_type_t type, astarte_individual_t *individual);

/**
 * @brief Deserialize a bson element containing an array of doubles.
 *
 * @param[in] bson_doc BSON document containing the array to deserialize.
 * @param[out] individual The Astarte individual where to store the deserialized data.
 * @param[in] array_length The number of elements of the BSON array.
 * @return ASTARTE_RESULT_OK upon success, an error code otherwise.
 */
static astarte_result_t deserialize_array_double(
    astarte_bson_document_t bson_doc, astarte_individual_t *individual, size_t array_length);

/**
 * @brief Deserialize a bson element containing an array of strings.
 *
 * @param[in] bson_doc BSON document containing the array to deserialize.
 * @param[out] individual The Astarte individual where to store the deserialized data.
 * @param[in] array_length The number of elements of the BSON array.
 * @return ASTARTE_RESULT_OK upon success, an error code otherwise.
 */
static astarte_result_t deserialize_array_string(
    astarte_bson_document_t bson_doc, astarte_individual_t *individual, size_t array_length);

/**
 * @brief Deserialize a bson element containing an array of booleans.
 *
 * @param[in] bson_doc BSON document containing the array to deserialize.
 * @param[out] individual The Astarte individual where to store the deserialized data.
 * @param[in] array_length The number of elements of the BSON array.
 * @return ASTARTE_RESULT_OK upon success, an error code otherwise.
 */
static astarte_result_t deserialize_array_bool(
    astarte_bson_document_t bson_doc, astarte_individual_t *individual, size_t array_length);

/**
 * @brief Deserialize a bson element containing an array of datetimes.
 *
 * @param[in] bson_doc BSON document containing the array to deserialize.
 * @param[out] individual The Astarte individual where to store the deserialized data.
 * @param[in] array_length The number of elements of the BSON array.
 * @return ASTARTE_RESULT_OK upon success, an error code otherwise.
 */
static astarte_result_t deserialize_array_datetime(
    astarte_bson_document_t bson_doc, astarte_individual_t *individual, size_t array_length);

/**
 * @brief Deserialize a bson element containing an array of integers.
 *
 * @param[in] bson_doc BSON document containing the array to deserialize.
 * @param[out] individual The Astarte individual where to store the deserialized data.
 * @param[in] array_length The number of elements of the BSON array.
 * @return ASTARTE_RESULT_OK upon success, an error code otherwise.
 */
static astarte_result_t deserialize_array_int32(
    astarte_bson_document_t bson_doc, astarte_individual_t *individual, size_t array_length);

/**
 * @brief Deserialize a bson element containing an array of long integers.
 *
 * @param[in] bson_doc BSON document containing the array to deserialize.
 * @param[out] individual The Astarte individual where to store the deserialized data.
 * @param[in] array_length The number of elements of the BSON array.
 * @return ASTARTE_RESULT_OK upon success, an error code otherwise.
 */
static astarte_result_t deserialize_array_int64(
    astarte_bson_document_t bson_doc, astarte_individual_t *individual, size_t array_length);

/**
 * @brief Deserialize a bson element containing an array of binary blobs.
 *
 * @param[in] bson_doc BSON document containing the array to deserialize.
 * @param[out] individual The Astarte individual where to store the deserialized data.
 * @param[in] array_length The number of elements of the BSON array.
 * @return ASTARTE_RESULT_OK upon success, an error code otherwise.
 */
static astarte_result_t deserialize_array_binblob(
    astarte_bson_document_t bson_doc, astarte_individual_t *individual, size_t array_length);

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
 *     Global public functions definitions      *
 ***********************************************/

// clang-format off
#define MAKE_INDIVIDUAL_FROM_FUNC(NAME, ENUM, TYPE, PARAM)                                         \
    astarte_individual_t astarte_individual_from_##NAME(TYPE PARAM)                                \
    {                                                                                              \
        return (astarte_individual_t) {                                                            \
            .data = {                                                                              \
                .PARAM = (PARAM),                                                                  \
            },                                                                                     \
            .tag = (ENUM),                                                                         \
        };                                                                                         \
    }

#define MAKE_INDIVIDUAL_FROM_ARRAY_FUNC(NAME, ENUM, TYPE, PARAM)                                   \
    astarte_individual_t astarte_individual_from_##NAME(TYPE PARAM, size_t len)                    \
    {                                                                                              \
        return (astarte_individual_t) {                                                            \
            .data = {                                                                              \
                .PARAM = {                                                                         \
                    .buf = (PARAM),                                                                \
                    .len = len,                                                                    \
                },                                                                                 \
            },                                                                                     \
            .tag = (ENUM),                                                                         \
        };                                                                                         \
    }
// clang-format on

MAKE_INDIVIDUAL_FROM_ARRAY_FUNC(binaryblob, ASTARTE_MAPPING_TYPE_BINARYBLOB, void *, binaryblob)
MAKE_INDIVIDUAL_FROM_FUNC(boolean, ASTARTE_MAPPING_TYPE_BOOLEAN, bool, boolean)
MAKE_INDIVIDUAL_FROM_FUNC(datetime, ASTARTE_MAPPING_TYPE_DATETIME, int64_t, datetime)
MAKE_INDIVIDUAL_FROM_FUNC(double, ASTARTE_MAPPING_TYPE_DOUBLE, double, dbl)
MAKE_INDIVIDUAL_FROM_FUNC(integer, ASTARTE_MAPPING_TYPE_INTEGER, int32_t, integer)
MAKE_INDIVIDUAL_FROM_FUNC(longinteger, ASTARTE_MAPPING_TYPE_LONGINTEGER, int64_t, longinteger)
MAKE_INDIVIDUAL_FROM_FUNC(string, ASTARTE_MAPPING_TYPE_STRING, const char *, string)

astarte_individual_t astarte_individual_from_binaryblob_array(
    const void **blobs, size_t *sizes, size_t count)
{
    return (astarte_individual_t) {
        .data = {
            .binaryblob_array = {
                .blobs = blobs,
                .sizes = sizes,
                .count = count,
            },
        },
        .tag = ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY,
    };
}

MAKE_INDIVIDUAL_FROM_ARRAY_FUNC(
    boolean_array, ASTARTE_MAPPING_TYPE_BOOLEANARRAY, bool *, boolean_array)
MAKE_INDIVIDUAL_FROM_ARRAY_FUNC(
    datetime_array, ASTARTE_MAPPING_TYPE_DATETIMEARRAY, int64_t *, datetime_array)
MAKE_INDIVIDUAL_FROM_ARRAY_FUNC(
    double_array, ASTARTE_MAPPING_TYPE_DOUBLEARRAY, double *, double_array)
MAKE_INDIVIDUAL_FROM_ARRAY_FUNC(
    integer_array, ASTARTE_MAPPING_TYPE_INTEGERARRAY, int32_t *, integer_array)
MAKE_INDIVIDUAL_FROM_ARRAY_FUNC(
    longinteger_array, ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY, int64_t *, longinteger_array)
MAKE_INDIVIDUAL_FROM_ARRAY_FUNC(
    string_array, ASTARTE_MAPPING_TYPE_STRINGARRAY, const char **, string_array)

astarte_mapping_type_t astarte_individual_get_type(astarte_individual_t individual)
{
    return individual.tag;
}

// clang-format off
// NOLINTBEGIN(bugprone-macro-parentheses)
#define MAKE_INDIVIDUAL_TO_FUNC(NAME, ENUM, TYPE, PARAM)                                           \
    astarte_result_t astarte_individual_to_##NAME(astarte_individual_t individual, TYPE *PARAM)    \
    {                                                                                              \
        if (!(PARAM) || (individual.tag != (ENUM))) {                                              \
            ASTARTE_LOG_ERR("Conversion from Astarte individual to %s error.", #NAME);             \
            return ASTARTE_RESULT_INVALID_PARAM;                                                   \
        }                                                                                          \
        *PARAM = individual.data.PARAM;                                                            \
        return ASTARTE_RESULT_OK;                                                                  \
    }

#define MAKE_INDIVIDUAL_TO_ARRAY_FUNC(NAME, ENUM, TYPE, PARAM)                                     \
    astarte_result_t astarte_individual_to_##NAME(                                                 \
        astarte_individual_t individual, TYPE *PARAM, size_t *len)                                 \
    {                                                                                              \
        if (!(PARAM) || !len || (individual.tag != (ENUM))) {                                      \
            ASTARTE_LOG_ERR("Conversion from Astarte individual to %s error.", #NAME);             \
            return ASTARTE_RESULT_INVALID_PARAM;                                                   \
        }                                                                                          \
        *PARAM = individual.data.PARAM.buf;                                                        \
        *len = individual.data.PARAM.len;                                                          \
        return ASTARTE_RESULT_OK;                                                                  \
    }
// NOLINTEND(bugprone-macro-parentheses)
// clang-format on

MAKE_INDIVIDUAL_TO_ARRAY_FUNC(binaryblob, ASTARTE_MAPPING_TYPE_BINARYBLOB, void *, binaryblob)
MAKE_INDIVIDUAL_TO_FUNC(boolean, ASTARTE_MAPPING_TYPE_BOOLEAN, bool, boolean)
MAKE_INDIVIDUAL_TO_FUNC(datetime, ASTARTE_MAPPING_TYPE_DATETIME, int64_t, datetime)
MAKE_INDIVIDUAL_TO_FUNC(double, ASTARTE_MAPPING_TYPE_DOUBLE, double, dbl)
MAKE_INDIVIDUAL_TO_FUNC(integer, ASTARTE_MAPPING_TYPE_INTEGER, int32_t, integer)
MAKE_INDIVIDUAL_TO_FUNC(longinteger, ASTARTE_MAPPING_TYPE_LONGINTEGER, int64_t, longinteger)
MAKE_INDIVIDUAL_TO_FUNC(string, ASTARTE_MAPPING_TYPE_STRING, const char *, string)

astarte_result_t astarte_individual_to_binaryblob_array(
    astarte_individual_t individual, const void ***blobs, size_t **sizes, size_t *count)
{
    if (!blobs || !sizes || !count || (individual.tag != ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY)) {
        ASTARTE_LOG_ERR("Conversion from Astarte individual to binaryblob_array error.");
        return ASTARTE_RESULT_INVALID_PARAM;
    }
    *blobs = individual.data.binaryblob_array.blobs;
    *sizes = individual.data.binaryblob_array.sizes;
    *count = individual.data.binaryblob_array.count;
    return ASTARTE_RESULT_OK;
}

MAKE_INDIVIDUAL_TO_ARRAY_FUNC(
    boolean_array, ASTARTE_MAPPING_TYPE_BOOLEANARRAY, bool *, boolean_array)
MAKE_INDIVIDUAL_TO_ARRAY_FUNC(
    datetime_array, ASTARTE_MAPPING_TYPE_DATETIMEARRAY, int64_t *, datetime_array)
MAKE_INDIVIDUAL_TO_ARRAY_FUNC(
    double_array, ASTARTE_MAPPING_TYPE_DOUBLEARRAY, double *, double_array)
MAKE_INDIVIDUAL_TO_ARRAY_FUNC(
    integer_array, ASTARTE_MAPPING_TYPE_INTEGERARRAY, int32_t *, integer_array)
MAKE_INDIVIDUAL_TO_ARRAY_FUNC(
    longinteger_array, ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY, int64_t *, longinteger_array)
MAKE_INDIVIDUAL_TO_ARRAY_FUNC(
    string_array, ASTARTE_MAPPING_TYPE_STRINGARRAY, const char **, string_array)

/************************************************
 *     Global private functions definitions     *
 ***********************************************/

astarte_result_t astarte_individual_serialize(
    astarte_bson_serializer_t *bson, const char *key, astarte_individual_t individual)
{
    astarte_result_t res = ASTARTE_RESULT_OK;

    switch (individual.tag) {
        case ASTARTE_MAPPING_TYPE_INTEGER:
            astarte_bson_serializer_append_int32(bson, key, individual.data.integer);
            break;
        case ASTARTE_MAPPING_TYPE_LONGINTEGER:
            astarte_bson_serializer_append_int64(bson, key, individual.data.longinteger);
            break;
        case ASTARTE_MAPPING_TYPE_DOUBLE:
            astarte_bson_serializer_append_double(bson, key, individual.data.dbl);
            break;
        case ASTARTE_MAPPING_TYPE_STRING:
            astarte_bson_serializer_append_string(bson, key, individual.data.string);
            break;
        case ASTARTE_MAPPING_TYPE_BINARYBLOB: {
            astarte_individual_binaryblob_t binaryblob = individual.data.binaryblob;
            astarte_bson_serializer_append_binary(bson, key, binaryblob.buf, binaryblob.len);
            break;
        }
        case ASTARTE_MAPPING_TYPE_BOOLEAN:
            astarte_bson_serializer_append_boolean(bson, key, individual.data.boolean);
            break;
        case ASTARTE_MAPPING_TYPE_DATETIME:
            astarte_bson_serializer_append_datetime(bson, key, individual.data.datetime);
            break;
        case ASTARTE_MAPPING_TYPE_INTEGERARRAY: {
            astarte_individual_integerarray_t int32_array = individual.data.integer_array;
            res = astarte_bson_serializer_append_int32_array(
                bson, key, int32_array.buf, (int) int32_array.len);
            break;
        }
        case ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY: {
            astarte_individual_longintegerarray_t int64_array = individual.data.longinteger_array;
            res = astarte_bson_serializer_append_int64_array(
                bson, key, int64_array.buf, (int) int64_array.len);
            break;
        }
        case ASTARTE_MAPPING_TYPE_DOUBLEARRAY: {
            astarte_individual_doublearray_t double_array = individual.data.double_array;
            res = astarte_bson_serializer_append_double_array(
                bson, key, double_array.buf, (int) double_array.len);
            break;
        }
        case ASTARTE_MAPPING_TYPE_STRINGARRAY: {
            astarte_individual_stringarray_t string_array = individual.data.string_array;
            res = astarte_bson_serializer_append_string_array(
                bson, key, string_array.buf, (int) string_array.len);
            break;
        }
        case ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY: {
            astarte_individual_binaryblobarray_t binary_arrays = individual.data.binaryblob_array;
            res = astarte_bson_serializer_append_binary_array(
                bson, key, binary_arrays.blobs, binary_arrays.sizes, (int) binary_arrays.count);
            break;
        }
        case ASTARTE_MAPPING_TYPE_BOOLEANARRAY: {
            astarte_individual_booleanarray_t bool_array = individual.data.boolean_array;
            res = astarte_bson_serializer_append_boolean_array(
                bson, key, bool_array.buf, (int) bool_array.len);
            break;
        }
        case ASTARTE_MAPPING_TYPE_DATETIMEARRAY: {
            astarte_individual_longintegerarray_t dt_array = individual.data.datetime_array;
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

astarte_result_t astarte_individual_deserialize(
    astarte_bson_element_t bson_elem, astarte_mapping_type_t type, astarte_individual_t *individual)
{
    astarte_result_t res = ASTARTE_RESULT_OK;

    switch (type) {
        case ASTARTE_MAPPING_TYPE_BINARYBLOB:
        case ASTARTE_MAPPING_TYPE_BOOLEAN:
        case ASTARTE_MAPPING_TYPE_DATETIME:
        case ASTARTE_MAPPING_TYPE_DOUBLE:
        case ASTARTE_MAPPING_TYPE_INTEGER:
        case ASTARTE_MAPPING_TYPE_LONGINTEGER:
        case ASTARTE_MAPPING_TYPE_STRING:
            res = deserialize_scalar(bson_elem, type, individual);
            break;
        case ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY:
        case ASTARTE_MAPPING_TYPE_BOOLEANARRAY:
        case ASTARTE_MAPPING_TYPE_DATETIMEARRAY:
        case ASTARTE_MAPPING_TYPE_DOUBLEARRAY:
        case ASTARTE_MAPPING_TYPE_INTEGERARRAY:
        case ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY:
        case ASTARTE_MAPPING_TYPE_STRINGARRAY:
            res = deserialize_array(bson_elem, type, individual);
            break;
        default:
            ASTARTE_LOG_ERR("Unsupported mapping type.");
            res = ASTARTE_RESULT_INTERNAL_ERROR;
            break;
    }
    return res;
}

void astarte_individual_destroy_deserialized(astarte_individual_t individual)
{
    switch (individual.tag) {
        case ASTARTE_MAPPING_TYPE_INTEGERARRAY:
            free(individual.data.integer_array.buf);
            break;
        case ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY:
            free(individual.data.longinteger_array.buf);
            break;
        case ASTARTE_MAPPING_TYPE_DOUBLEARRAY:
            free(individual.data.double_array.buf);
            break;
        case ASTARTE_MAPPING_TYPE_STRINGARRAY:
            free((void *) individual.data.string_array.buf);
            break;
        case ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY:
            free(individual.data.binaryblob_array.sizes);
            free((void *) individual.data.binaryblob_array.blobs);
            break;
        case ASTARTE_MAPPING_TYPE_BOOLEANARRAY:
            free(individual.data.boolean_array.buf);
            break;
        case ASTARTE_MAPPING_TYPE_DATETIMEARRAY:
            free(individual.data.datetime_array.buf);
            break;
        default:
            break;
    }
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static astarte_result_t initialize_empty_array(
    astarte_mapping_type_t type, astarte_individual_t *individual)
{
    astarte_result_t res = ASTARTE_RESULT_OK;
    switch (type) {
        case ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY:
            individual->tag = type;
            individual->data.binaryblob_array.count = 0;
            individual->data.binaryblob_array.blobs = NULL;
            individual->data.binaryblob_array.sizes = NULL;
            break;
        case ASTARTE_MAPPING_TYPE_BOOLEANARRAY:
            individual->tag = type;
            individual->data.boolean_array.len = 0;
            individual->data.boolean_array.buf = NULL;
            break;
        case ASTARTE_MAPPING_TYPE_DATETIMEARRAY:
            individual->tag = type;
            individual->data.datetime_array.len = 0;
            individual->data.datetime_array.buf = NULL;
            break;
        case ASTARTE_MAPPING_TYPE_DOUBLEARRAY:
            individual->tag = type;
            individual->data.double_array.len = 0;
            individual->data.double_array.buf = NULL;
            break;
        case ASTARTE_MAPPING_TYPE_INTEGERARRAY:
            individual->tag = type;
            individual->data.integer_array.len = 0;
            individual->data.integer_array.buf = NULL;
            break;
        case ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY:
            individual->tag = type;
            individual->data.longinteger_array.len = 0;
            individual->data.longinteger_array.buf = NULL;
            break;
        case ASTARTE_MAPPING_TYPE_STRINGARRAY:
            individual->tag = type;
            individual->data.string_array.len = 0;
            individual->data.string_array.buf = NULL;
            break;
        default:
            ASTARTE_LOG_ERR("Creating empty array Astarte individual for scalar mapping type.");
            res = ASTARTE_RESULT_INTERNAL_ERROR;
            break;
    }
    return res;
}

static astarte_result_t deserialize_scalar(
    astarte_bson_element_t bson_elem, astarte_mapping_type_t type, astarte_individual_t *individual)
{
    astarte_result_t res = ASTARTE_RESULT_OK;

    if (!check_if_bson_type_is_mapping_type(type, bson_elem.type)) {
        ASTARTE_LOG_ERR("BSON element is not of the expected type.");
        return ASTARTE_RESULT_BSON_DESERIALIZER_TYPES_ERROR;
    }

    switch (type) {
        case ASTARTE_MAPPING_TYPE_BINARYBLOB:
            ASTARTE_LOG_DBG("Deserializing binary blob individual.");
            uint32_t len = 0;
            const uint8_t *bin_tmp = astarte_bson_deserializer_element_to_binary(bson_elem, &len);
            *individual = astarte_individual_from_binaryblob((void *) bin_tmp, len);
            break;
        case ASTARTE_MAPPING_TYPE_BOOLEAN:
            ASTARTE_LOG_DBG("Deserializing boolean individual.");
            bool bool_tmp = astarte_bson_deserializer_element_to_bool(bson_elem);
            *individual = astarte_individual_from_boolean(bool_tmp);
            break;
        case ASTARTE_MAPPING_TYPE_DATETIME:
            ASTARTE_LOG_DBG("Deserializing datetime individual.");
            int64_t datetime_tmp = astarte_bson_deserializer_element_to_datetime(bson_elem);
            *individual = astarte_individual_from_datetime(datetime_tmp);
            break;
        case ASTARTE_MAPPING_TYPE_DOUBLE:
            ASTARTE_LOG_DBG("Deserializing double individual.");
            double double_tmp = astarte_bson_deserializer_element_to_double(bson_elem);
            *individual = astarte_individual_from_double(double_tmp);
            break;
        case ASTARTE_MAPPING_TYPE_INTEGER:
            ASTARTE_LOG_DBG("Deserializing integer individual.");
            int32_t int32_tmp = astarte_bson_deserializer_element_to_int32(bson_elem);
            *individual = astarte_individual_from_integer(int32_tmp);
            break;
        case ASTARTE_MAPPING_TYPE_LONGINTEGER:
            ASTARTE_LOG_DBG("Deserializing long integer individual.");
            int64_t int64_tmp = 0U;
            if (bson_elem.type == ASTARTE_BSON_TYPE_INT32) {
                int64_tmp = (int64_t) astarte_bson_deserializer_element_to_int32(bson_elem);
            } else {
                int64_tmp = astarte_bson_deserializer_element_to_int64(bson_elem);
            }
            *individual = astarte_individual_from_longinteger(int64_tmp);
            break;
        case ASTARTE_MAPPING_TYPE_STRING:
            ASTARTE_LOG_DBG("Deserializing string individual.");
            const char *string_tmp = astarte_bson_deserializer_element_to_string(bson_elem, NULL);
            *individual = astarte_individual_from_string(string_tmp);
            break;
        default:
            ASTARTE_LOG_ERR("Unsupported mapping type.");
            res = ASTARTE_RESULT_INTERNAL_ERROR;
            break;
    }
    return res;
}

static astarte_result_t deserialize_array(
    astarte_bson_element_t bson_elem, astarte_mapping_type_t type, astarte_individual_t *individual)
{
    astarte_result_t res = ASTARTE_RESULT_OK;

    if (bson_elem.type != ASTARTE_BSON_TYPE_ARRAY) {
        ASTARTE_LOG_ERR("Expected an array but BSON element type is %d.", bson_elem.type);
        return ASTARTE_RESULT_BSON_DESERIALIZER_TYPES_ERROR;
    }

    astarte_bson_document_t bson_doc = astarte_bson_deserializer_element_to_array(bson_elem);

    // Step 1: figure out the size and type of the array
    astarte_bson_element_t inner_elem = { 0 };
    res = astarte_bson_deserializer_first_element(bson_doc, &inner_elem);
    if (res != ASTARTE_RESULT_OK) {
        return initialize_empty_array(type, individual);
    }
    size_t array_length = 0U;

    astarte_mapping_type_t scalar_type = ASTARTE_MAPPING_TYPE_BINARYBLOB;
    res = astarte_mapping_array_to_scalar_type(type, &scalar_type);
    if (res != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Non array type passed to deserialize_array.");
        return res;
    }

    do {
        array_length++;
        if (!check_if_bson_type_is_mapping_type(scalar_type, inner_elem.type)) {
            ASTARTE_LOG_ERR("BSON array element is not of the expected type.");
            return ASTARTE_RESULT_BSON_DESERIALIZER_TYPES_ERROR;
        }
        res = astarte_bson_deserializer_next_element(bson_doc, inner_elem, &inner_elem);
    } while (res == ASTARTE_RESULT_OK);

    if (res != ASTARTE_RESULT_NOT_FOUND) {
        return res;
    }

    // Step 2: depending on the array type call the appropriate function
    switch (scalar_type) {
        case ASTARTE_MAPPING_TYPE_BINARYBLOB:
            ASTARTE_LOG_DBG("Deserializing array of binary blobs.");
            res = deserialize_array_binblob(bson_doc, individual, array_length);
            break;
        case ASTARTE_MAPPING_TYPE_BOOLEAN:
            ASTARTE_LOG_DBG("Deserializing array of booleans.");
            res = deserialize_array_bool(bson_doc, individual, array_length);
            break;
        case ASTARTE_MAPPING_TYPE_DATETIME:
            ASTARTE_LOG_DBG("Deserializing array of datetimes.");
            res = deserialize_array_datetime(bson_doc, individual, array_length);
            break;
        case ASTARTE_MAPPING_TYPE_DOUBLE:
            ASTARTE_LOG_DBG("Deserializing array of doubles.");
            res = deserialize_array_double(bson_doc, individual, array_length);
            break;
        case ASTARTE_MAPPING_TYPE_INTEGER:
            ASTARTE_LOG_DBG("Deserializing array of integers.");
            res = deserialize_array_int32(bson_doc, individual, array_length);
            break;
        case ASTARTE_MAPPING_TYPE_LONGINTEGER:
            ASTARTE_LOG_DBG("Deserializing array of long integers.");
            res = deserialize_array_int64(bson_doc, individual, array_length);
            break;
        case ASTARTE_MAPPING_TYPE_STRING:
            ASTARTE_LOG_DBG("Deserializing array of strings.");
            res = deserialize_array_string(bson_doc, individual, array_length);
            break;
        default:
            ASTARTE_LOG_ERR("Unsupported mapping type.");
            res = ASTARTE_RESULT_INTERNAL_ERROR;
            break;
    }

    return res;
}

// clang-format off
// NOLINTBEGIN(bugprone-macro-parentheses) Some can't be wrapped in parenthesis
#define DESERIALIZE_ARRAY_FUNC(NAME, TYPE, TAG, UNION)                                             \
static astarte_result_t deserialize_array_##NAME(                                                  \
    astarte_bson_document_t bson_doc, astarte_individual_t *individual, size_t array_length)       \
{                                                                                                  \
    astarte_result_t res = ASTARTE_RESULT_OK;                                                      \
    TYPE *array = calloc(array_length, sizeof(TYPE));                                              \
    if (!array) {                                                                                  \
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);                               \
        res = ASTARTE_RESULT_OUT_OF_MEMORY;                                                        \
        goto failure;                                                                              \
    }                                                                                              \
                                                                                                   \
    astarte_bson_element_t inner_elem = {0};                                                       \
    res = astarte_bson_deserializer_first_element(bson_doc, &inner_elem);                          \
    if (res != ASTARTE_RESULT_OK) {                                                                \
        goto failure;                                                                              \
    }                                                                                              \
    array[0] = astarte_bson_deserializer_element_to_##NAME(inner_elem);                            \
                                                                                                   \
    for (size_t i = 1; i < array_length; i++) {                                                    \
        res = astarte_bson_deserializer_next_element(bson_doc, inner_elem, &inner_elem);           \
        if (res != ASTARTE_RESULT_OK) {                                                            \
            goto failure;                                                                          \
        }                                                                                          \
        array[i] = astarte_bson_deserializer_element_to_##NAME(inner_elem);                        \
    }                                                                                              \
                                                                                                   \
    individual->tag = (TAG);                                                                       \
    individual->data.UNION.len = array_length;                                                     \
    individual->data.UNION.buf = array;                                                            \
    return ASTARTE_RESULT_OK;                                                                      \
                                                                                                   \
failure:                                                                                           \
    free(array);                                                                                   \
    return res;                                                                                    \
}
// NOLINTEND(bugprone-macro-parentheses)
// clang-format on

DESERIALIZE_ARRAY_FUNC(double, double, ASTARTE_MAPPING_TYPE_DOUBLEARRAY, double_array)
DESERIALIZE_ARRAY_FUNC(bool, bool, ASTARTE_MAPPING_TYPE_BOOLEANARRAY, boolean_array)
DESERIALIZE_ARRAY_FUNC(datetime, int64_t, ASTARTE_MAPPING_TYPE_DATETIMEARRAY, datetime_array)
DESERIALIZE_ARRAY_FUNC(int32, int32_t, ASTARTE_MAPPING_TYPE_INTEGERARRAY, integer_array)

static astarte_result_t deserialize_array_int64(
    astarte_bson_document_t bson_doc, astarte_individual_t *individual, size_t array_length)
{
    astarte_result_t res = ASTARTE_RESULT_OK;
    int64_t *array = calloc(array_length, sizeof(int64_t));
    if (!array) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        res = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto failure;
    }

    astarte_bson_element_t inner_elem = { 0 };
    res = astarte_bson_deserializer_first_element(bson_doc, &inner_elem);
    if (res != ASTARTE_RESULT_OK) {
        goto failure;
    }

    if (inner_elem.type == ASTARTE_BSON_TYPE_INT32) {
        array[0] = (int64_t) astarte_bson_deserializer_element_to_int32(inner_elem);
    } else {
        array[0] = astarte_bson_deserializer_element_to_int64(inner_elem);
    }

    for (size_t i = 1; i < array_length; i++) {
        res = astarte_bson_deserializer_next_element(bson_doc, inner_elem, &inner_elem);
        if (res != ASTARTE_RESULT_OK) {
            goto failure;
        }

        if (inner_elem.type == ASTARTE_BSON_TYPE_INT32) {
            array[i] = (int64_t) astarte_bson_deserializer_element_to_int32(inner_elem);
        } else {
            array[i] = astarte_bson_deserializer_element_to_int64(inner_elem);
        }
    }

    individual->tag = ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY;
    individual->data.longinteger_array.len = array_length;
    individual->data.longinteger_array.buf = array;
    return ASTARTE_RESULT_OK;

failure:
    free(array);
    return res;
}

static astarte_result_t deserialize_array_string(
    astarte_bson_document_t bson_doc, astarte_individual_t *individual, size_t array_length)
{
    astarte_result_t res = ASTARTE_RESULT_OK;
    // Step 1: allocate enough memory to contain the array from the BSON file
    const char **array = (const char **) calloc(array_length, sizeof(char *));
    if (!array) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        res = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto failure;
    }

    // Step 2: fill in the array with data
    astarte_bson_element_t inner_elem = { 0 };
    res = astarte_bson_deserializer_first_element(bson_doc, &inner_elem);
    if (res != ASTARTE_RESULT_OK) {
        goto failure;
    }
    array[0] = astarte_bson_deserializer_element_to_string(inner_elem, NULL);

    for (size_t i = 1; i < array_length; i++) {
        res = astarte_bson_deserializer_next_element(bson_doc, inner_elem, &inner_elem);
        if (res != ASTARTE_RESULT_OK) {
            goto failure;
        }
        array[i] = astarte_bson_deserializer_element_to_string(inner_elem, NULL);
    }

    // Step 3: Place the generated array in the output struct
    individual->tag = ASTARTE_MAPPING_TYPE_STRINGARRAY;
    individual->data.string_array.len = array_length;
    individual->data.string_array.buf = array;

    return ASTARTE_RESULT_OK;

failure:
    free((void *) array);
    return res;
}

static astarte_result_t deserialize_array_binblob(
    astarte_bson_document_t bson_doc, astarte_individual_t *individual, size_t array_length)
{
    astarte_result_t res = ASTARTE_RESULT_OK;
    const uint8_t **array = NULL;
    size_t *array_sizes = NULL;
    // Step 1: allocate enough memory to contain the array from the BSON file
    array = (const uint8_t **) calloc(array_length, sizeof(uint8_t *));
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
    astarte_bson_element_t inner_elem = { 0 };
    res = astarte_bson_deserializer_first_element(bson_doc, &inner_elem);
    if (res != ASTARTE_RESULT_OK) {
        goto failure;
    }
    size_t array_entry_size = 0;
    array[0] = astarte_bson_deserializer_element_to_binary(inner_elem, &array_entry_size);
    array_sizes[0] = array_entry_size;

    for (size_t i = 1; i < array_length; i++) {
        res = astarte_bson_deserializer_next_element(bson_doc, inner_elem, &inner_elem);
        if (res != ASTARTE_RESULT_OK) {
            goto failure;
        }
        array[i] = astarte_bson_deserializer_element_to_binary(inner_elem, &array_entry_size);
        array_sizes[i] = array_entry_size;
    }

    // Step 3: Place the generated array in the output struct
    individual->tag = ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY;
    individual->data.binaryblob_array.count = array_length;
    individual->data.binaryblob_array.sizes = array_sizes;
    individual->data.binaryblob_array.blobs = (const void **) array;

    return ASTARTE_RESULT_OK;

failure:
    free((void *) array);
    free(array_sizes);
    return res;
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
