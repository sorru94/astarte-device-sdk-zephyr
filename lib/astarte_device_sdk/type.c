/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "astarte_device_sdk/type.h"

#include "astarte_device_sdk/bson_serializer.h"
#include "astarte_device_sdk/error.h"

astarte_err_t astarte_value_serialize(
    bson_serializer_handle_t bson, char *key, astarte_value_t value)
{
    astarte_err_t ret = ASTARTE_OK;

    switch (value.tag) {
        case TYPE_INTEGER:
            bson_serializer_append_int32(bson, key, value.value.integer);
            break;
        case TYPE_LONGINTEGER:
            bson_serializer_append_int64(bson, key, value.value.longinteger);
            break;
        case TYPE_DOUBLE:
            bson_serializer_append_double(bson, key, value.value.dbl);
            break;
        case TYPE_STRING:
            bson_serializer_append_string(bson, key, value.value.string);
            break;
        case TYPE_BINARYBLOB: {
            astarte_binaryblob_t binaryblob = value.value.binaryblob;
            bson_serializer_append_binary(bson, key, binaryblob.buf, binaryblob.len);
            break;
        }
        case TYPE_BOOLEAN:
            bson_serializer_append_boolean(bson, key, value.value.boolean);
            break;
        case TYPE_DATETIME:
            bson_serializer_append_datetime(bson, key, value.value.datetime);
            break;
        case TYPE_INTEGERARRAY: {
            astarte_int32_array_t int32_array = value.value.integer_array;
            ret = bson_serializer_append_int32_array(
                bson, key, int32_array.buf, (int) int32_array.len);
            break;
        }
        case TYPE_LONGINTEGERARRAY: {
            astarte_int64_array_t int64_array = value.value.longinteger_array;
            ret = bson_serializer_append_int64_array(
                bson, key, int64_array.buf, (int) int64_array.len);
            break;
        }
        case TYPE_DOUBLEARRAY: {
            astarte_double_array_t double_array = value.value.double_array;
            ret = bson_serializer_append_double_array(
                bson, key, double_array.buf, (int) double_array.len);
            break;
        }
        case TYPE_STRINGARRAY: {
            astarte_string_array_t string_array = value.value.string_array;
            ret = bson_serializer_append_string_array(
                bson, key, string_array.buf, (int) string_array.len);
            break;
        }
        case TYPE_BINARYBLOBARRAY: {
            astarte_binary_arrays_t binary_arrays = value.value.binaryblob_array;
            ret = bson_serializer_append_binary_array(
                bson, key, binary_arrays.blobs, binary_arrays.sizes, (int) binary_arrays.count);
            break;
        }
        case TYPE_BOOLEANARRAY: {
            astarte_bool_array_t bool_array = value.value.boolean_array;
            ret = bson_serializer_append_boolean_array(
                bson, key, bool_array.buf, (int) bool_array.len);
            break;
        }
        case TYPE_DATETIMEARRAY: {
            astarte_int64_array_t dt_array = value.value.datetime_array;
            ret = bson_serializer_append_datetime_array(
                bson, key, dt_array.buf, (int) dt_array.len);
            break;
        }
        default:
            ret = ASTARTE_ERR_INVALID_PARAM;
            break;
    }

    return ret;
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

DEFINE_ASTARTE_TYPE_MAKE_FN(integer, TYPE_INTEGER, int32_t, integer)
DEFINE_ASTARTE_TYPE_MAKE_FN(longinteger, TYPE_LONGINTEGER, int64_t, longinteger)
DEFINE_ASTARTE_TYPE_MAKE_FN(double, TYPE_DOUBLE, double, dbl)
DEFINE_ASTARTE_TYPE_MAKE_FN(string, TYPE_STRING, const char *, string)
DEFINE_ASTARTE_ARRAY_TYPE_MAKE_FN(binaryblob, TYPE_BINARYBLOB, void *, binaryblob)
DEFINE_ASTARTE_TYPE_MAKE_FN(boolean, TYPE_BOOLEAN, bool, boolean)
DEFINE_ASTARTE_TYPE_MAKE_FN(datetime, TYPE_DATETIME, int64_t, datetime)

DEFINE_ASTARTE_ARRAY_TYPE_MAKE_FN(integer_array, TYPE_INTEGERARRAY, int32_t *, integer_array)
DEFINE_ASTARTE_ARRAY_TYPE_MAKE_FN(
    longinteger_array, TYPE_LONGINTEGERARRAY, int64_t *, longinteger_array)
DEFINE_ASTARTE_ARRAY_TYPE_MAKE_FN(double_array, TYPE_DOUBLEARRAY, double *, double_array)
DEFINE_ASTARTE_ARRAY_TYPE_MAKE_FN(string_array, TYPE_STRINGARRAY, const char *const *, string_array)
DEFINE_ASTARTE_ARRAY_TYPE_MAKE_FN(boolean_array, TYPE_BOOLEANARRAY, bool *, boolean_array)
DEFINE_ASTARTE_ARRAY_TYPE_MAKE_FN(datetime_array, TYPE_DATETIMEARRAY, int64_t *, datetime_array)

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
        .tag = TYPE_BINARYBLOBARRAY,
    };
}
