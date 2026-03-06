/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "data_serialize.h"

#include "bson_serializer.h"

#include "log.h"
ASTARTE_LOG_MODULE_REGISTER(data_serialize, CONFIG_ASTARTE_DEVICE_SDK_DATA_SERIALIZE_LOG_LEVEL);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t data_serialize(
    astarte_bson_serializer_t *bson, const char *key, astarte_data_t data)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    switch (data.tag) {
        case ASTARTE_MAPPING_TYPE_INTEGER:
            ares = astarte_bson_serializer_append_int32(bson, key, data.data.integer);
            break;
        case ASTARTE_MAPPING_TYPE_LONGINTEGER:
            ares = astarte_bson_serializer_append_int64(bson, key, data.data.longinteger);
            break;
        case ASTARTE_MAPPING_TYPE_DOUBLE:
            ares = astarte_bson_serializer_append_double(bson, key, data.data.dbl);
            break;
        case ASTARTE_MAPPING_TYPE_STRING:
            ares = astarte_bson_serializer_append_string(bson, key, data.data.string);
            break;
        case ASTARTE_MAPPING_TYPE_BINARYBLOB: {
            astarte_data_binaryblob_t binaryblob = data.data.binaryblob;
            ares = astarte_bson_serializer_append_binary(bson, key, binaryblob.buf, binaryblob.len);
            break;
        }
        case ASTARTE_MAPPING_TYPE_BOOLEAN:
            ares = astarte_bson_serializer_append_boolean(bson, key, data.data.boolean);
            break;
        case ASTARTE_MAPPING_TYPE_DATETIME:
            ares = astarte_bson_serializer_append_datetime(bson, key, data.data.datetime);
            break;
        case ASTARTE_MAPPING_TYPE_INTEGERARRAY: {
            astarte_data_integerarray_t int32_array = data.data.integer_array;
            ares = astarte_bson_serializer_append_int32_array(
                bson, key, int32_array.buf, (int) int32_array.len);
            break;
        }
        case ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY: {
            astarte_data_longintegerarray_t int64_array = data.data.longinteger_array;
            ares = astarte_bson_serializer_append_int64_array(
                bson, key, int64_array.buf, (int) int64_array.len);
            break;
        }
        case ASTARTE_MAPPING_TYPE_DOUBLEARRAY: {
            astarte_data_doublearray_t double_array = data.data.double_array;
            ares = astarte_bson_serializer_append_double_array(
                bson, key, double_array.buf, (int) double_array.len);
            break;
        }
        case ASTARTE_MAPPING_TYPE_STRINGARRAY: {
            astarte_data_stringarray_t string_array = data.data.string_array;
            ares = astarte_bson_serializer_append_string_array(
                bson, key, string_array.buf, (int) string_array.len);
            break;
        }
        case ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY: {
            astarte_data_binaryblobarray_t binary_arrays = data.data.binaryblob_array;
            ares = astarte_bson_serializer_append_binary_array(
                bson, key, binary_arrays.blobs, binary_arrays.sizes, (int) binary_arrays.count);
            break;
        }
        case ASTARTE_MAPPING_TYPE_BOOLEANARRAY: {
            astarte_data_booleanarray_t bool_array = data.data.boolean_array;
            ares = astarte_bson_serializer_append_boolean_array(
                bson, key, bool_array.buf, (int) bool_array.len);
            break;
        }
        case ASTARTE_MAPPING_TYPE_DATETIMEARRAY: {
            astarte_data_longintegerarray_t dt_array = data.data.datetime_array;
            ares = astarte_bson_serializer_append_datetime_array(
                bson, key, dt_array.buf, (int) dt_array.len);
            break;
        }
        default:
            ares = ASTARTE_RESULT_INVALID_PARAM;
            break;
    }

    return ares;
}
