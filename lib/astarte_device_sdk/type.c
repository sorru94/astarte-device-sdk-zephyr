/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "astarte_device_sdk/value.h"

#include "astarte_device_sdk/bson_serializer.h"
#include "astarte_device_sdk/result.h"

astarte_result_t astarte_value_serialize(
    astarte_bson_serializer_handle_t bson, char *key, astarte_value_t value)
{
    astarte_result_t res = ASTARTE_RESULT_OK;

    switch (value.tag) {
        case ASTARTE_MAPPING_TYPE_INTEGER:
            astarte_bson_serializer_append_int32(bson, key, value.value.integer);
            break;
        case ASTARTE_MAPPING_TYPE_LONGINTEGER:
            astarte_bson_serializer_append_int64(bson, key, value.value.longinteger);
            break;
        case ASTARTE_MAPPING_TYPE_DOUBLE:
            astarte_bson_serializer_append_double(bson, key, value.value.dbl);
            break;
        case ASTARTE_MAPPING_TYPE_STRING:
            astarte_bson_serializer_append_string(bson, key, value.value.string);
            break;
        case ASTARTE_MAPPING_TYPE_BINARYBLOB: {
            astarte_value_binaryblob_t binaryblob = value.value.binaryblob;
            astarte_bson_serializer_append_binary(bson, key, binaryblob.buf, binaryblob.len);
            break;
        }
        case ASTARTE_MAPPING_TYPE_BOOLEAN:
            astarte_bson_serializer_append_boolean(bson, key, value.value.boolean);
            break;
        case ASTARTE_MAPPING_TYPE_DATETIME:
            astarte_bson_serializer_append_datetime(bson, key, value.value.datetime);
            break;
        case ASTARTE_MAPPING_TYPE_INTEGERARRAY: {
            astarte_value_integerarray_t int32_array = value.value.integer_array;
            res = astarte_bson_serializer_append_int32_array(
                bson, key, int32_array.buf, (int) int32_array.len);
            break;
        }
        case ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY: {
            astarte_value_longintegerarray_t int64_array = value.value.longinteger_array;
            res = astarte_bson_serializer_append_int64_array(
                bson, key, int64_array.buf, (int) int64_array.len);
            break;
        }
        case ASTARTE_MAPPING_TYPE_DOUBLEARRAY: {
            astarte_value_doublearray_t double_array = value.value.double_array;
            res = astarte_bson_serializer_append_double_array(
                bson, key, double_array.buf, (int) double_array.len);
            break;
        }
        case ASTARTE_MAPPING_TYPE_STRINGARRAY: {
            astarte_value_stringarray_t string_array = value.value.string_array;
            res = astarte_bson_serializer_append_string_array(
                bson, key, string_array.buf, (int) string_array.len);
            break;
        }
        case ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY: {
            astarte_value_binaryblobarray_t binary_arrays = value.value.binaryblob_array;
            res = astarte_bson_serializer_append_binary_array(
                bson, key, binary_arrays.blobs, binary_arrays.sizes, (int) binary_arrays.count);
            break;
        }
        case ASTARTE_MAPPING_TYPE_BOOLEANARRAY: {
            astarte_value_booleanarray_t bool_array = value.value.boolean_array;
            res = astarte_bson_serializer_append_boolean_array(
                bson, key, bool_array.buf, (int) bool_array.len);
            break;
        }
        case ASTARTE_MAPPING_TYPE_DATETIMEARRAY: {
            astarte_value_longintegerarray_t dt_array = value.value.datetime_array;
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

// clang-format off
#define DEFINE_ASTARTE_TYPE_MAKE_FN(NAME, ENUM, TYPE, PARAM)                                       \
    astarte_value_t astarte_value_from_##NAME(TYPE PARAM)                                          \
    {                                                                                              \
        return (astarte_value_t) {                                                                 \
            .value = {                                                                             \
                .PARAM = (PARAM),                                                                  \
            },                                                                                     \
            .tag = (ENUM),                                                                         \
        };                                                                                         \
    }

#define DEFINE_ASTARTE_ARRAY_TYPE_MAKE_FN(NAME, ENUM, TYPE, PARAM)                                 \
    astarte_value_t astarte_value_from_##NAME(TYPE PARAM, size_t len)                              \
    {                                                                                              \
        return (astarte_value_t) {                                                                 \
            .value = {                                                                             \
                .PARAM = {                                                                         \
                    .buf = (PARAM),                                                                \
                    .len = len,                                                                    \
                },                                                                                 \
            },                                                                                     \
            .tag = (ENUM),                                                                         \
        };                                                                                         \
    }
// clang-format on

DEFINE_ASTARTE_TYPE_MAKE_FN(integer, ASTARTE_MAPPING_TYPE_INTEGER, int32_t, integer)
DEFINE_ASTARTE_TYPE_MAKE_FN(longinteger, ASTARTE_MAPPING_TYPE_LONGINTEGER, int64_t, longinteger)
DEFINE_ASTARTE_TYPE_MAKE_FN(double, ASTARTE_MAPPING_TYPE_DOUBLE, double, dbl)
DEFINE_ASTARTE_TYPE_MAKE_FN(string, ASTARTE_MAPPING_TYPE_STRING, const char *, string)
DEFINE_ASTARTE_ARRAY_TYPE_MAKE_FN(binaryblob, ASTARTE_MAPPING_TYPE_BINARYBLOB, void *, binaryblob)
DEFINE_ASTARTE_TYPE_MAKE_FN(boolean, ASTARTE_MAPPING_TYPE_BOOLEAN, bool, boolean)
DEFINE_ASTARTE_TYPE_MAKE_FN(datetime, ASTARTE_MAPPING_TYPE_DATETIME, int64_t, datetime)

DEFINE_ASTARTE_ARRAY_TYPE_MAKE_FN(
    integer_array, ASTARTE_MAPPING_TYPE_INTEGERARRAY, int32_t *, integer_array)
DEFINE_ASTARTE_ARRAY_TYPE_MAKE_FN(
    longinteger_array, ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY, int64_t *, longinteger_array)
DEFINE_ASTARTE_ARRAY_TYPE_MAKE_FN(
    double_array, ASTARTE_MAPPING_TYPE_DOUBLEARRAY, double *, double_array)
DEFINE_ASTARTE_ARRAY_TYPE_MAKE_FN(
    string_array, ASTARTE_MAPPING_TYPE_STRINGARRAY, const char *const *, string_array)
DEFINE_ASTARTE_ARRAY_TYPE_MAKE_FN(
    boolean_array, ASTARTE_MAPPING_TYPE_BOOLEANARRAY, bool *, boolean_array)
DEFINE_ASTARTE_ARRAY_TYPE_MAKE_FN(
    datetime_array, ASTARTE_MAPPING_TYPE_DATETIMEARRAY, int64_t *, datetime_array)

astarte_value_t astarte_value_from_binaryblob_array(
    const void *const *buf, const int *sizes, size_t count)
{
    return (astarte_value_t) {
        .value = {
            .binaryblob_array = {
                .blobs = buf,
                .sizes = sizes,
                .count = count,
            },
        },
        .tag = ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY,
    };
}
