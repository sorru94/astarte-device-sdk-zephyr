/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "astarte_device_sdk/data.h"

#include "log.h"
ASTARTE_LOG_MODULE_REGISTER(data, CONFIG_ASTARTE_DEVICE_SDK_DATA_LOG_LEVEL);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

// clang-format off
#define MAKE_FUNCTION_DATA_FROM(NAME, ENUM, TYPE, PARAM)                                           \
    astarte_data_t astarte_data_from_##NAME(TYPE PARAM)                                            \
    {                                                                                              \
        return (astarte_data_t) {                                                                  \
            .data = {                                                                              \
                .PARAM = (PARAM),                                                                  \
            },                                                                                     \
            .tag = (ENUM),                                                                         \
            .is_owned = false,                                                                     \
        };                                                                                         \
    }

#define MAKE_FUNCTION_DATA_FROM_ARRAY(NAME, ENUM, TYPE, PARAM)                                     \
    astarte_data_t astarte_data_from_##NAME(TYPE PARAM, size_t len)                                \
    {                                                                                              \
        return (astarte_data_t) {                                                                  \
            .data = {                                                                              \
                .PARAM = {                                                                         \
                    .buf = (PARAM),                                                                \
                    .len = len,                                                                    \
                },                                                                                 \
            },                                                                                     \
            .tag = (ENUM),                                                                         \
            .is_owned = false,                                                                     \
        };                                                                                         \
    }
// clang-format on

MAKE_FUNCTION_DATA_FROM_ARRAY(binaryblob, ASTARTE_MAPPING_TYPE_BINARYBLOB, const void *, binaryblob)
MAKE_FUNCTION_DATA_FROM(boolean, ASTARTE_MAPPING_TYPE_BOOLEAN, bool, boolean)
MAKE_FUNCTION_DATA_FROM(datetime, ASTARTE_MAPPING_TYPE_DATETIME, int64_t, datetime)
MAKE_FUNCTION_DATA_FROM(double, ASTARTE_MAPPING_TYPE_DOUBLE, double, dbl)
MAKE_FUNCTION_DATA_FROM(integer, ASTARTE_MAPPING_TYPE_INTEGER, int32_t, integer)
MAKE_FUNCTION_DATA_FROM(longinteger, ASTARTE_MAPPING_TYPE_LONGINTEGER, int64_t, longinteger)
MAKE_FUNCTION_DATA_FROM(string, ASTARTE_MAPPING_TYPE_STRING, const char *, string)

astarte_data_t astarte_data_from_binaryblob_array(const void **blobs, size_t *sizes, size_t count)
{
    return (astarte_data_t) {
        .data = {
            .binaryblob_array = {
                .blobs = blobs,
                .sizes = sizes,
                .count = count,
            },
        },
        .tag = ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY,
        .is_owned = false,
    };
}

MAKE_FUNCTION_DATA_FROM_ARRAY(
    boolean_array, ASTARTE_MAPPING_TYPE_BOOLEANARRAY, const bool *, boolean_array)
MAKE_FUNCTION_DATA_FROM_ARRAY(
    datetime_array, ASTARTE_MAPPING_TYPE_DATETIMEARRAY, const int64_t *, datetime_array)
MAKE_FUNCTION_DATA_FROM_ARRAY(
    double_array, ASTARTE_MAPPING_TYPE_DOUBLEARRAY, const double *, double_array)
MAKE_FUNCTION_DATA_FROM_ARRAY(
    integer_array, ASTARTE_MAPPING_TYPE_INTEGERARRAY, const int32_t *, integer_array)
MAKE_FUNCTION_DATA_FROM_ARRAY(
    longinteger_array, ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY, const int64_t *, longinteger_array)
MAKE_FUNCTION_DATA_FROM_ARRAY(
    string_array, ASTARTE_MAPPING_TYPE_STRINGARRAY, const char **, string_array)

astarte_mapping_type_t astarte_data_get_type(astarte_data_t data)
{
    return data.tag;
}

// clang-format off
// NOLINTBEGIN(bugprone-macro-parentheses)
#define MAKE_FUNCTION_DATA_TO(NAME, ENUM, TYPE, PARAM)                                             \
    astarte_result_t astarte_data_to_##NAME(astarte_data_t data, TYPE *PARAM)                      \
    {                                                                                              \
        if (!(PARAM) || (data.tag != (ENUM))) {                                                    \
            ASTARTE_LOG_ERR("Conversion from Astarte data to %s error.", #NAME);                   \
            return ASTARTE_RESULT_INVALID_PARAM;                                                   \
        }                                                                                          \
        *PARAM = data.data.PARAM;                                                                  \
        return ASTARTE_RESULT_OK;                                                                  \
    }

#define MAKE_FUNCTION_DATA_TO_ARRAY(NAME, ENUM, TYPE, PARAM)                                       \
    astarte_result_t astarte_data_to_##NAME(                                                       \
        astarte_data_t data, TYPE *PARAM, size_t *len)                                             \
    {                                                                                              \
        if (!(PARAM) || !len || (data.tag != (ENUM))) {                                            \
            ASTARTE_LOG_ERR("Conversion from Astarte data to %s error.", #NAME);                   \
            return ASTARTE_RESULT_INVALID_PARAM;                                                   \
        }                                                                                          \
        *PARAM = data.data.PARAM.buf;                                                              \
        *len = data.data.PARAM.len;                                                                \
        return ASTARTE_RESULT_OK;                                                                  \
    }
// NOLINTEND(bugprone-macro-parentheses)
// clang-format on

MAKE_FUNCTION_DATA_TO_ARRAY(binaryblob, ASTARTE_MAPPING_TYPE_BINARYBLOB, const void *, binaryblob)
MAKE_FUNCTION_DATA_TO(boolean, ASTARTE_MAPPING_TYPE_BOOLEAN, bool, boolean)
MAKE_FUNCTION_DATA_TO(datetime, ASTARTE_MAPPING_TYPE_DATETIME, int64_t, datetime)
MAKE_FUNCTION_DATA_TO(double, ASTARTE_MAPPING_TYPE_DOUBLE, double, dbl)
MAKE_FUNCTION_DATA_TO(integer, ASTARTE_MAPPING_TYPE_INTEGER, int32_t, integer)
MAKE_FUNCTION_DATA_TO(longinteger, ASTARTE_MAPPING_TYPE_LONGINTEGER, int64_t, longinteger)
MAKE_FUNCTION_DATA_TO(string, ASTARTE_MAPPING_TYPE_STRING, const char *, string)

astarte_result_t astarte_data_to_binaryblob_array(
    astarte_data_t data, const void ***blobs, size_t **sizes, size_t *count)
{
    if (!blobs || !sizes || !count || (data.tag != ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY)) {
        ASTARTE_LOG_ERR("Conversion from Astarte data to binaryblob_array error.");
        return ASTARTE_RESULT_INVALID_PARAM;
    }
    *blobs = data.data.binaryblob_array.blobs;
    *sizes = data.data.binaryblob_array.sizes;
    *count = data.data.binaryblob_array.count;
    return ASTARTE_RESULT_OK;
}

MAKE_FUNCTION_DATA_TO_ARRAY(
    boolean_array, ASTARTE_MAPPING_TYPE_BOOLEANARRAY, const bool *, boolean_array)
MAKE_FUNCTION_DATA_TO_ARRAY(
    datetime_array, ASTARTE_MAPPING_TYPE_DATETIMEARRAY, const int64_t *, datetime_array)
MAKE_FUNCTION_DATA_TO_ARRAY(
    double_array, ASTARTE_MAPPING_TYPE_DOUBLEARRAY, const double *, double_array)
MAKE_FUNCTION_DATA_TO_ARRAY(
    integer_array, ASTARTE_MAPPING_TYPE_INTEGERARRAY, const int32_t *, integer_array)
MAKE_FUNCTION_DATA_TO_ARRAY(
    longinteger_array, ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY, const int64_t *, longinteger_array)
MAKE_FUNCTION_DATA_TO_ARRAY(
    string_array, ASTARTE_MAPPING_TYPE_STRINGARRAY, const char **, string_array)
