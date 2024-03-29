/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * @file astarte-device-sdk-zephyr/tests/lib/astarte_device_sdk/unit/bson/src/main.c
 *
 * @details This test suite verifies that the methods provided with the bson
 * module works correctly.
 *
 * @note This should be run with the latest version of zephyr present on master (or 3.6.0)
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <zephyr/ztest.h>

#include "astarte_device_sdk/value.h"
#include "lib/astarte_device_sdk/value.c"
#include "value_private.h"

#include "bson_deserializer.h"
#include "bson_serializer.h"

ZTEST_SUITE(astarte_device_sdk_value, NULL, NULL, NULL, NULL, NULL);

// Define a minimal_log function to resolve the `undefined reference to z_log_minimal_printk` error,
// because the log environment is missing in the unit_testing platform.
void z_log_minimal_printk(const char *fmt, ...) {}

// This function allocates dynamically the memory required to store the string.
// Since we are in a test environment we do not deallocate such memory.
static char *hex_to_str(const uint8_t *input, size_t input_len)
{
    char *dyn_buf = calloc((input_len * 6) + 1, sizeof(char));
    if (!dyn_buf) {
        return "Allocation error";
    }
    char *cursor = dyn_buf + sprintf(dyn_buf, "{");
    for (int i = 0; i < input_len; i++) {
        uint8_t *byte = input + i;
        cursor = cursor + sprintf(cursor, "0x%02x", *byte);
        if (i < input_len - 1) {
            cursor = cursor + sprintf(cursor, ", ");
        }
    }
    sprintf(cursor, "}");
    return dyn_buf;
}

const uint8_t test_data_binaryblob[] = { 0x68, 0x65, 0x6c, 0x6c, 0x6f };
const uint8_t test_data_serialized_binaryblob[] = { 0x12, 0x00, 0x00, 0x00, 0x05, 0x76, 0x00, 0x05,
    0x00, 0x00, 0x00, 0x00, 0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x00 };

const uint8_t test_data_binaryblob_array_blob_1[] = { 0x41, 0x53, 0x54, 0x41, 0x52, 0x54, 0x45 };
const uint8_t test_data_binaryblob_array_blob_2[] = { 0x49, 0x53 };
const uint8_t test_data_binaryblob_array_blob_3[] = { 0x43, 0x4F, 0x4F, 0x4C };
const void *test_data_binaryblob_array[] = { test_data_binaryblob_array_blob_1,
    test_data_binaryblob_array_blob_2, test_data_binaryblob_array_blob_3 };
const size_t test_data_binaryblob_sizes[] = {
    sizeof(test_data_binaryblob_array_blob_1) / sizeof(uint8_t),
    sizeof(test_data_binaryblob_array_blob_2) / sizeof(uint8_t),
    sizeof(test_data_binaryblob_array_blob_3) / sizeof(uint8_t),
};
const uint8_t test_data_serialized_binaryblob_array[] = { 0x32, 0x00, 0x00, 0x00, 0x04, 0x76, 0x00,
    0x2a, 0x00, 0x00, 0x00, 0x05, 0x30, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x41, 0x53, 0x54, 0x41,
    0x52, 0x54, 0x45, 0x05, 0x31, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x49, 0x53, 0x05, 0x32, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x00, 0x43, 0x4f, 0x4f, 0x4c, 0x00, 0x00 };

const bool test_data_boolean = true;
const uint8_t test_data_serialized_boolean[]
    = { 0x09, 0x00, 0x00, 0x00, 0x08, 0x76, 0x00, 0x01, 0x00 };

const bool test_data_boolean_array[] = { true, false, true, true };
const uint8_t test_data_serialized_boolean_array[]
    = { 0x1d, 0x00, 0x00, 0x00, 0x04, 0x76, 0x00, 0x15, 0x00, 0x00, 0x00, 0x08, 0x30, 0x00, 0x01,
          0x08, 0x31, 0x00, 0x00, 0x08, 0x32, 0x00, 0x01, 0x08, 0x33, 0x00, 0x01, 0x00, 0x00 };

const int64_t test_data_datetime = 1669111881000;
const uint8_t test_data_serialized_datetime[] = { 0x10, 0x00, 0x00, 0x00, 0x09, 0x76, 0x00, 0x28,
    0x1d, 0xd2, 0x9e, 0x84, 0x01, 0x00, 0x00, 0x00 };

const int64_t test_data_datetime_array[] = { 1669111881000, 1669111881000 };
const uint8_t test_data_serialized_datetime_array[] = { 0x23, 0x00, 0x00, 0x00, 0x04, 0x76, 0x00,
    0x1b, 0x00, 0x00, 0x00, 0x09, 0x30, 0x00, 0x28, 0x1d, 0xd2, 0x9e, 0x84, 0x01, 0x00, 0x00, 0x09,
    0x31, 0x00, 0x28, 0x1d, 0xd2, 0x9e, 0x84, 0x01, 0x00, 0x00, 0x00, 0x00 };

const double test_data_double = 432.4324;
const uint8_t test_data_serialized_double[] = { 0x10, 0x00, 0x00, 0x00, 0x01, 0x76, 0x00, 0xa5,
    0x2c, 0x43, 0x1c, 0xeb, 0x06, 0x7b, 0x40, 0x00 };

const double test_data_double_array[] = { 21.0, 11.5, 0.0, 44.5 };
const uint8_t test_data_serialized_double_array[]
    = { 0x39, 0x00, 0x00, 0x00, 0x04, 0x76, 0x00, 0x31, 0x00, 0x00, 0x00, 0x01, 0x30, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x35, 0x40, 0x01, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x27, 0x40, 0x01, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
          0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x46, 0x40, 0x00, 0x00 };

const int32_t test_data_integer = 42;
const uint8_t test_data_serialized_integer[]
    = { 0x0C, 0x00, 0x00, 0x00, 0x10, 0x76, 0x00, 0x2a, 0x00, 0x00, 0x00, 0x00 };

const int32_t test_data_integer_array[] = { 42, 10, 128, 9, 256 };
const uint8_t test_data_serialized_integer_array[] = { 0x30, 0x00, 0x00, 0x00, 0x04, 0x76, 0x00,
    0x28, 0x00, 0x00, 0x00, 0x10, 0x30, 0x00, 0x2a, 0x00, 0x00, 0x00, 0x10, 0x31, 0x00, 0x0a, 0x00,
    0x00, 0x00, 0x10, 0x32, 0x00, 0x80, 0x00, 0x00, 0x00, 0x10, 0x33, 0x00, 0x09, 0x00, 0x00, 0x00,
    0x10, 0x34, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00 };

const int64_t test_data_longinteger = 3147483647;
const uint8_t test_data_serialized_longinteger[] = { 0x10, 0x00, 0x00, 0x00, 0x12, 0x76, 0x00, 0xff,
    0xc9, 0x9a, 0xbb, 0x00, 0x00, 0x00, 0x00, 0x00 };

const int64_t test_data_longinteger_array[] = { 68719476736 };
const uint8_t test_data_serialized_longinteger_array[]
    = { 0x18, 0x00, 0x00, 0x00, 0x04, 0x76, 0x00, 0x10, 0x00, 0x00, 0x00, 0x12, 0x30, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00 };

const char *test_data_string = "this is a test string";
const uint8_t test_data_serialized_string[] = { 0x22, 0x00, 0x00, 0x00, 0x02, 0x76, 0x00, 0x16,
    0x00, 0x00, 0x00, 0x74, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x61, 0x20, 0x74, 0x65, 0x73,
    0x74, 0x20, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x00, 0x00 };

const char *test_data_string_array[] = { "this", "is", "a", "test", "string_array" };
const uint8_t test_data_serialized_string_array[] = { 0x4c, 0x00, 0x00, 0x00, 0x04, 0x76, 0x00,
    0x44, 0x00, 0x00, 0x00, 0x02, 0x30, 0x00, 0x05, 0x00, 0x00, 0x00, 0x74, 0x68, 0x69, 0x73, 0x00,
    0x02, 0x31, 0x00, 0x03, 0x00, 0x00, 0x00, 0x69, 0x73, 0x00, 0x02, 0x32, 0x00, 0x02, 0x00, 0x00,
    0x00, 0x61, 0x00, 0x02, 0x33, 0x00, 0x05, 0x00, 0x00, 0x00, 0x74, 0x65, 0x73, 0x74, 0x00, 0x02,
    0x34, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x5f, 0x61, 0x72, 0x72,
    0x61, 0x79, 0x00, 0x00, 0x00 };

const uint8_t test_data_serialized_empty_array[]
    = { 0x0d, 0x00, 0x00, 0x00, 0x04, 0x76, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00 };

const uint8_t test_data_serialized_mismatched_array_initial[] = { 0x32, 0x00, 0x00, 0x00, 0x04,
    0x76, 0x00, 0x2a, 0x00, 0x00, 0x00, 0x01, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x35,
    0x40, 0x02, 0x31, 0x00, 0x06, 0x00, 0x00, 0x00, 0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x00, 0x02, 0x32,
    0x00, 0x06, 0x00, 0x00, 0x00, 0x77, 0x6f, 0x72, 0x6c, 0x64, 0x00, 0x00, 0x00 };

const uint8_t test_data_serialized_mismatched_array_final[] = { 0x2e, 0x00, 0x00, 0x00, 0x04, 0x76,
    0x00, 0x26, 0x00, 0x00, 0x00, 0x02, 0x30, 0x00, 0x06, 0x00, 0x00, 0x00, 0x68, 0x65, 0x6c, 0x6c,
    0x6f, 0x00, 0x02, 0x31, 0x00, 0x06, 0x00, 0x00, 0x00, 0x77, 0x6f, 0x72, 0x6c, 0x64, 0x00, 0x10,
    0x32, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00 };

const char test_data_aggregate_double_key[] = "double_endpoint";
const double test_data_aggregate_double_value = 32.1;
const char test_data_aggregate_integer_key[] = "integer_endpoint";
const double test_data_aggregate_integer_value = 42;
const char test_data_aggregate_stringarray_key[] = "stringarray_endpoint";
const char *test_data_aggregate_stringarray_values[] = { "hello, world" };
const uint8_t test_data_serialized_aggregate[] = { 0x6b, 0x00, 0x00, 0x00, 0x03, 0x76, 0x00, 0x63,
    0x00, 0x00, 0x00, 0x01, 0x64, 0x6f, 0x75, 0x62, 0x6c, 0x65, 0x5f, 0x65, 0x6e, 0x64, 0x70, 0x6f,
    0x69, 0x6e, 0x74, 0x00, 0xcd, 0xcc, 0xcc, 0xcc, 0xcc, 0x0c, 0x40, 0x40, 0x10, 0x69, 0x6e, 0x74,
    0x65, 0x67, 0x65, 0x72, 0x5f, 0x65, 0x6e, 0x64, 0x70, 0x6f, 0x69, 0x6e, 0x74, 0x00, 0x2a, 0x00,
    0x00, 0x00, 0x04, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x61, 0x72, 0x72, 0x61, 0x79, 0x5f, 0x65,
    0x6e, 0x64, 0x70, 0x6f, 0x69, 0x6e, 0x74, 0x00, 0x19, 0x00, 0x00, 0x00, 0x02, 0x30, 0x00, 0x0d,
    0x00, 0x00, 0x00, 0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x2c, 0x20, 0x77, 0x6f, 0x72, 0x6c, 0x64, 0x00,
    0x00, 0x00, 0x00 };

const uint8_t test_data_serialized_empty_aggregate[]
    = { 0x0d, 0x00, 0x00, 0x00, 0x03, 0x76, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00 };

ZTEST(astarte_device_sdk_value, test_serialize_integer)
{
    astarte_value_t astarte_value = astarte_value_from_integer(test_data_integer);
    astarte_bson_serializer_handle_t bson = astarte_bson_serializer_new();
    astarte_value_serialize(bson, "v", astarte_value);
    astarte_bson_serializer_append_end_of_document(bson);
    int len = 0;
    const void *data = astarte_bson_serializer_get_document(bson, &len);
    zassert_equal(len, sizeof(test_data_serialized_integer));
    zassert_mem_equal(
        data, test_data_serialized_integer, len, "Serialized: %s", hex_to_str(data, len));
}

ZTEST(astarte_device_sdk_value, test_serialize_longinteger)
{
    astarte_value_t astarte_value = astarte_value_from_longinteger(test_data_longinteger);
    astarte_bson_serializer_handle_t bson = astarte_bson_serializer_new();
    astarte_value_serialize(bson, "v", astarte_value);
    astarte_bson_serializer_append_end_of_document(bson);
    int len = 0;
    const void *data = astarte_bson_serializer_get_document(bson, &len);
    zassert_equal(len, sizeof(test_data_serialized_longinteger));
    zassert_mem_equal(
        data, test_data_serialized_longinteger, len, "Serialized: %s", hex_to_str(data, len));
}

ZTEST(astarte_device_sdk_value, test_serialize_double)
{
    astarte_value_t astarte_value = astarte_value_from_double(test_data_double);
    astarte_bson_serializer_handle_t bson = astarte_bson_serializer_new();
    astarte_value_serialize(bson, "v", astarte_value);
    astarte_bson_serializer_append_end_of_document(bson);
    int len = 0;
    const void *data = astarte_bson_serializer_get_document(bson, &len);
    zassert_equal(len, sizeof(test_data_serialized_double));
    zassert_mem_equal(
        data, test_data_serialized_double, len, "Serialized: %s", hex_to_str(data, len));
}

ZTEST(astarte_device_sdk_value, test_serialize_boolean)
{
    astarte_value_t astarte_value = astarte_value_from_boolean(test_data_boolean);
    astarte_bson_serializer_handle_t bson = astarte_bson_serializer_new();
    astarte_value_serialize(bson, "v", astarte_value);
    astarte_bson_serializer_append_end_of_document(bson);
    int len = 0;
    const void *data = astarte_bson_serializer_get_document(bson, &len);
    zassert_equal(len, sizeof(test_data_serialized_boolean));
    zassert_mem_equal(
        data, test_data_serialized_boolean, len, "Serialized: %s", hex_to_str(data, len));
}

ZTEST(astarte_device_sdk_value, test_serialize_string)
{
    astarte_value_t astarte_value = astarte_value_from_string(test_data_string);
    astarte_bson_serializer_handle_t bson = astarte_bson_serializer_new();
    astarte_value_serialize(bson, "v", astarte_value);
    astarte_bson_serializer_append_end_of_document(bson);
    int len = 0;
    const void *data = astarte_bson_serializer_get_document(bson, &len);
    zassert_equal(len, sizeof(test_data_serialized_string));
    zassert_mem_equal(
        data, test_data_serialized_string, len, "Serialized: %s", hex_to_str(data, len));
}

ZTEST(astarte_device_sdk_value, test_serialize_integer_array)
{
    astarte_value_t astarte_value = astarte_value_from_integer_array(
        (int32_t *) &(test_data_integer_array), sizeof(test_data_integer_array) / sizeof(int32_t));
    astarte_bson_serializer_handle_t bson = astarte_bson_serializer_new();
    astarte_value_serialize(bson, "v", astarte_value);
    astarte_bson_serializer_append_end_of_document(bson);
    int len = 0;
    const void *data = astarte_bson_serializer_get_document(bson, &len);
    zassert_equal(len, sizeof(test_data_serialized_integer_array));
    zassert_mem_equal(
        data, test_data_serialized_integer_array, len, "Serialized: %s", hex_to_str(data, len));
}

ZTEST(astarte_device_sdk_value, test_serialize_string_array)
{
    astarte_value_t astarte_value
        = astarte_value_from_string_array((const char *const *) &(test_data_string_array),
            sizeof(test_data_string_array) / sizeof(const char *const));
    astarte_bson_serializer_handle_t bson = astarte_bson_serializer_new();
    astarte_value_serialize(bson, "v", astarte_value);
    astarte_bson_serializer_append_end_of_document(bson);
    int len = 0;
    const void *data = astarte_bson_serializer_get_document(bson, &len);
    zassert_equal(len, sizeof(test_data_serialized_string_array));
    zassert_mem_equal(
        data, test_data_serialized_string_array, len, "Serialized: %s", hex_to_str(data, len));
}

ZTEST(astarte_device_sdk_value, test_serialize_binaryblob_array)
{
    astarte_value_t astarte_value = astarte_value_from_binaryblob_array(test_data_binaryblob_array,
        test_data_binaryblob_sizes, sizeof(test_data_binaryblob_array) / sizeof(uint8_t *));

    astarte_bson_serializer_handle_t bson = astarte_bson_serializer_new();
    astarte_value_serialize(bson, "v", astarte_value);
    astarte_bson_serializer_append_end_of_document(bson);
    int len = 0;
    const void *data = astarte_bson_serializer_get_document(bson, &len);

    zassert_equal(len, sizeof(test_data_serialized_binaryblob_array));
    zassert_mem_equal(
        data, test_data_serialized_binaryblob_array, len, "Serialized: %s", hex_to_str(data, len));
}

ZTEST(astarte_device_sdk_value, test_deserialize_astarte_value_from_incorrect_type)
{
    astarte_bson_document_t full_document
        = astarte_bson_deserializer_init_doc(test_data_serialized_binaryblob);
    astarte_bson_element_t v_elem;
    astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem);

    astarte_value_t value = { 0 };
    astarte_result_t res
        = astarte_value_deserialize(v_elem, ASTARTE_MAPPING_TYPE_DATETIMEARRAY, &value);
    zassert_equal(
        res, ASTARTE_RESULT_BSON_DESERIALIZER_TYPES_ERROR, "%s", astarte_result_to_name(res));
}

ZTEST(astarte_device_sdk_value, test_deserialize_astarte_value_from_binblob)
{
    astarte_bson_document_t full_document
        = astarte_bson_deserializer_init_doc(test_data_serialized_binaryblob);
    astarte_bson_element_t v_elem;
    astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem);

    astarte_value_t value = { 0 };
    astarte_result_t res
        = astarte_value_deserialize(v_elem, ASTARTE_MAPPING_TYPE_BINARYBLOB, &value);
    zassert_equal(res, ASTARTE_RESULT_OK, "%s", astarte_result_to_name(res));
    zassert_equal(value.tag, ASTARTE_MAPPING_TYPE_BINARYBLOB);
    zassert_equal(value.data.binaryblob.len, 5);
    zassert_mem_equal(value.data.binaryblob.buf, test_data_binaryblob, value.data.binaryblob.len);
    astarte_value_destroy_deserialized(value);
}

ZTEST(astarte_device_sdk_value, test_deserialize_astarte_value_from_boolean)
{
    astarte_bson_document_t full_document
        = astarte_bson_deserializer_init_doc(test_data_serialized_boolean);
    astarte_bson_element_t v_elem;
    astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem);

    astarte_value_t value = { 0 };
    astarte_result_t res = astarte_value_deserialize(v_elem, ASTARTE_MAPPING_TYPE_BOOLEAN, &value);
    zassert_equal(res, ASTARTE_RESULT_OK, "%s", astarte_result_to_name(res));
    zassert_equal(value.tag, ASTARTE_MAPPING_TYPE_BOOLEAN);
    zassert_equal(value.data.boolean, test_data_boolean);
    astarte_value_destroy_deserialized(value);
}

ZTEST(astarte_device_sdk_value, test_deserialize_astarte_value_from_datetime)
{
    astarte_bson_document_t full_document
        = astarte_bson_deserializer_init_doc(test_data_serialized_datetime);
    astarte_bson_element_t v_elem;
    astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem);

    astarte_value_t value = { 0 };
    astarte_result_t res = astarte_value_deserialize(v_elem, ASTARTE_MAPPING_TYPE_DATETIME, &value);
    zassert_equal(res, ASTARTE_RESULT_OK, "%s", astarte_result_to_name(res));
    zassert_equal(value.tag, ASTARTE_MAPPING_TYPE_DATETIME);
    zassert_equal(value.data.datetime, test_data_datetime);
    astarte_value_destroy_deserialized(value);
}

ZTEST(astarte_device_sdk_value, test_deserialize_astarte_value_from_double)
{
    astarte_bson_document_t full_document
        = astarte_bson_deserializer_init_doc(test_data_serialized_double);
    astarte_bson_element_t v_elem;
    astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem);

    astarte_value_t value = { 0 };
    astarte_result_t res = astarte_value_deserialize(v_elem, ASTARTE_MAPPING_TYPE_DOUBLE, &value);
    zassert_equal(res, ASTARTE_RESULT_OK, "%s", astarte_result_to_name(res));
    zassert_equal(value.tag, ASTARTE_MAPPING_TYPE_DOUBLE);
    zassert_equal(value.data.dbl, test_data_double);
    astarte_value_destroy_deserialized(value);
}

ZTEST(astarte_device_sdk_value, test_deserialize_astarte_value_from_integer)
{
    astarte_bson_document_t full_document
        = astarte_bson_deserializer_init_doc(test_data_serialized_integer);
    astarte_bson_element_t v_elem;
    astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem);

    astarte_value_t value = { 0 };
    astarte_result_t res = astarte_value_deserialize(v_elem, ASTARTE_MAPPING_TYPE_INTEGER, &value);
    zassert_equal(res, ASTARTE_RESULT_OK, "%s", astarte_result_to_name(res));
    zassert_equal(value.tag, ASTARTE_MAPPING_TYPE_INTEGER);
    zassert_equal(value.data.integer, test_data_integer);
    astarte_value_destroy_deserialized(value);
}

ZTEST(astarte_device_sdk_value, test_deserialize_astarte_value_from_longinteger)
{
    astarte_bson_document_t full_document
        = astarte_bson_deserializer_init_doc(test_data_serialized_longinteger);
    astarte_bson_element_t v_elem;
    astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem);

    astarte_value_t value = { 0 };
    astarte_result_t res
        = astarte_value_deserialize(v_elem, ASTARTE_MAPPING_TYPE_LONGINTEGER, &value);
    zassert_equal(res, ASTARTE_RESULT_OK, "%s", astarte_result_to_name(res));
    zassert_equal(value.tag, ASTARTE_MAPPING_TYPE_LONGINTEGER);
    zassert_equal(value.data.longinteger, test_data_longinteger);
    astarte_value_destroy_deserialized(value);
}

ZTEST(astarte_device_sdk_value, test_deserialize_astarte_value_from_string)
{
    astarte_bson_document_t full_document
        = astarte_bson_deserializer_init_doc(test_data_serialized_string);
    astarte_bson_element_t v_elem;
    astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem);

    astarte_value_t value = { 0 };
    astarte_result_t res = astarte_value_deserialize(v_elem, ASTARTE_MAPPING_TYPE_STRING, &value);
    zassert_equal(res, ASTARTE_RESULT_OK, "%s", astarte_result_to_name(res));
    zassert_equal(value.tag, ASTARTE_MAPPING_TYPE_STRING);
    zassert_mem_equal(value.data.string, test_data_string, strlen(test_data_string) + 1);
    astarte_value_destroy_deserialized(value);
}

ZTEST(astarte_device_sdk_value, test_deserialize_astarte_value_from_binblob_array)
{
    astarte_bson_document_t full_document
        = astarte_bson_deserializer_init_doc(test_data_serialized_binaryblob_array);
    astarte_bson_element_t v_elem;
    astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem);

    astarte_value_t value = { 0 };
    astarte_result_t res
        = astarte_value_deserialize(v_elem, ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY, &value);
    zassert_equal(res, ASTARTE_RESULT_OK, "%s", astarte_result_to_name(res));
    zassert_equal(value.tag, ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY);
    zassert_equal(value.data.binaryblob_array.count, ARRAY_SIZE(test_data_binaryblob_array));
    for (size_t i = 0; i < ARRAY_SIZE(test_data_binaryblob_array); i++) {
        zassert_equal(value.data.binaryblob_array.sizes[i], test_data_binaryblob_sizes[i]);
        zassert_mem_equal(value.data.binaryblob_array.blobs[i], test_data_binaryblob_array[i],
            test_data_binaryblob_sizes[i]);
    }
    astarte_value_destroy_deserialized(value);
}

ZTEST(astarte_device_sdk_value, test_deserialize_astarte_value_from_boolean_array)
{
    astarte_bson_document_t full_document
        = astarte_bson_deserializer_init_doc(test_data_serialized_boolean_array);
    astarte_bson_element_t v_elem;
    astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem);

    astarte_value_t value = { 0 };
    astarte_result_t res
        = astarte_value_deserialize(v_elem, ASTARTE_MAPPING_TYPE_BOOLEANARRAY, &value);
    zassert_equal(res, ASTARTE_RESULT_OK, "%s", astarte_result_to_name(res));
    zassert_equal(value.tag, ASTARTE_MAPPING_TYPE_BOOLEANARRAY);
    zassert_equal(value.data.boolean_array.len, ARRAY_SIZE(test_data_boolean_array));
    zassert_mem_equal(
        value.data.boolean_array.buf, test_data_boolean_array, ARRAY_SIZE(test_data_boolean_array));
    astarte_value_destroy_deserialized(value);
}

ZTEST(astarte_device_sdk_value, test_deserialize_astarte_value_from_double_array)
{
    astarte_bson_document_t full_document
        = astarte_bson_deserializer_init_doc(test_data_serialized_double_array);
    astarte_bson_element_t v_elem;
    astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem);

    astarte_value_t value = { 0 };
    astarte_result_t res
        = astarte_value_deserialize(v_elem, ASTARTE_MAPPING_TYPE_DOUBLEARRAY, &value);
    zassert_equal(res, ASTARTE_RESULT_OK, "%d", res);
    zassert_equal(res, ASTARTE_RESULT_OK, "%s", astarte_result_to_name(res));
    zassert_equal(value.tag, ASTARTE_MAPPING_TYPE_DOUBLEARRAY);
    zassert_equal(value.data.double_array.len, ARRAY_SIZE(test_data_double_array));
    for (size_t i = 0; i < ARRAY_SIZE(test_data_double_array); i++) {
        zassert_within(value.data.double_array.buf[i], test_data_double_array[i], 0.01);
    }
    astarte_value_destroy_deserialized(value);
}

ZTEST(astarte_device_sdk_value, test_deserialize_astarte_value_from_datetime_array)
{
    astarte_bson_document_t full_document
        = astarte_bson_deserializer_init_doc(test_data_serialized_datetime_array);
    astarte_bson_element_t v_elem;
    astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem);

    astarte_value_t value = { 0 };
    astarte_result_t res
        = astarte_value_deserialize(v_elem, ASTARTE_MAPPING_TYPE_DATETIMEARRAY, &value);
    zassert_equal(res, ASTARTE_RESULT_OK, "%s", astarte_result_to_name(res));
    zassert_equal(value.tag, ASTARTE_MAPPING_TYPE_DATETIMEARRAY);
    zassert_equal(value.data.datetime_array.len, ARRAY_SIZE(test_data_datetime_array));
    for (size_t i = 0; i < ARRAY_SIZE(test_data_datetime_array); i++) {
        zassert_equal(value.data.datetime_array.buf[i], test_data_datetime_array[i]);
    }
    astarte_value_destroy_deserialized(value);
}

ZTEST(astarte_device_sdk_value, test_deserialize_astarte_value_from_integer_array)
{
    astarte_bson_document_t full_document
        = astarte_bson_deserializer_init_doc(test_data_serialized_integer_array);
    astarte_bson_element_t v_elem;
    astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem);

    astarte_value_t value = { 0 };
    astarte_result_t res
        = astarte_value_deserialize(v_elem, ASTARTE_MAPPING_TYPE_INTEGERARRAY, &value);
    zassert_equal(res, ASTARTE_RESULT_OK, "%s", astarte_result_to_name(res));
    zassert_equal(value.tag, ASTARTE_MAPPING_TYPE_INTEGERARRAY);
    zassert_equal(value.data.integer_array.len, ARRAY_SIZE(test_data_integer_array));
    for (size_t i = 0; i < ARRAY_SIZE(test_data_integer_array); i++) {
        zassert_equal(value.data.integer_array.buf[i], test_data_integer_array[i]);
    }
    astarte_value_destroy_deserialized(value);
}

ZTEST(astarte_device_sdk_value, test_deserialize_astarte_value_from_longinteger_array)
{
    astarte_bson_document_t full_document
        = astarte_bson_deserializer_init_doc(test_data_serialized_longinteger_array);
    astarte_bson_element_t v_elem;
    astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem);

    astarte_value_t value = { 0 };
    astarte_result_t res
        = astarte_value_deserialize(v_elem, ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY, &value);
    zassert_equal(res, ASTARTE_RESULT_OK, "%s", astarte_result_to_name(res));
    zassert_equal(value.tag, ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY);
    zassert_equal(value.data.longinteger_array.len, ARRAY_SIZE(test_data_longinteger_array));
    for (size_t i = 0; i < ARRAY_SIZE(test_data_longinteger_array); i++) {
        zassert_equal(value.data.longinteger_array.buf[i], test_data_longinteger_array[i]);
    }
    astarte_value_destroy_deserialized(value);
}

ZTEST(astarte_device_sdk_value, test_deserialize_astarte_value_from_string_array)
{
    astarte_bson_document_t full_document
        = astarte_bson_deserializer_init_doc(test_data_serialized_string_array);
    astarte_bson_element_t v_elem;
    astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem);

    astarte_value_t value = { 0 };
    astarte_result_t res
        = astarte_value_deserialize(v_elem, ASTARTE_MAPPING_TYPE_STRINGARRAY, &value);
    zassert_equal(res, ASTARTE_RESULT_OK, "%s", astarte_result_to_name(res));
    zassert_equal(value.tag, ASTARTE_MAPPING_TYPE_STRINGARRAY);
    zassert_equal(value.data.string_array.len, ARRAY_SIZE(test_data_string_array));
    for (size_t i = 0; i < ARRAY_SIZE(test_data_string_array); i++) {
        zassert_equal(strcmp(value.data.string_array.buf[i], test_data_string_array[i]), 0);
    }
    astarte_value_destroy_deserialized(value);
}

ZTEST(astarte_device_sdk_value, test_deserialize_astarte_value_from_empty_array)
{
    astarte_bson_document_t full_document
        = astarte_bson_deserializer_init_doc(test_data_serialized_empty_array);
    astarte_bson_element_t v_elem;
    astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem);

    astarte_value_t value = { 0 };
    astarte_result_t res
        = astarte_value_deserialize(v_elem, ASTARTE_MAPPING_TYPE_DOUBLEARRAY, &value);
    zassert_equal(res, ASTARTE_RESULT_OK, "%s", astarte_result_to_name(res));
    zassert_equal(value.tag, ASTARTE_MAPPING_TYPE_DOUBLEARRAY);
    zassert_equal(value.data.double_array.len, 0);
}

ZTEST(astarte_device_sdk_value, test_deserialize_astarte_value_from_mismatched_array_initial)
{
    astarte_bson_document_t full_document
        = astarte_bson_deserializer_init_doc(test_data_serialized_mismatched_array_initial);
    astarte_bson_element_t v_elem;
    astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem);

    astarte_value_t value = { 0 };
    astarte_result_t res
        = astarte_value_deserialize(v_elem, ASTARTE_MAPPING_TYPE_STRINGARRAY, &value);
    zassert_equal(
        res, ASTARTE_RESULT_BSON_DESERIALIZER_TYPES_ERROR, "%s", astarte_result_to_name(res));
}

ZTEST(astarte_device_sdk_value, test_deserialize_astarte_value_from_mismatched_array_final)
{
    astarte_bson_document_t full_document
        = astarte_bson_deserializer_init_doc(test_data_serialized_mismatched_array_final);
    astarte_bson_element_t v_elem;
    astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem);

    astarte_value_t value = { 0 };
    astarte_result_t res
        = astarte_value_deserialize(v_elem, ASTARTE_MAPPING_TYPE_STRINGARRAY, &value);
    zassert_equal(
        res, ASTARTE_RESULT_BSON_DESERIALIZER_TYPES_ERROR, "%s", astarte_result_to_name(res));
}

ZTEST(astarte_device_sdk_value, test_deserialize_astarte_value_pair_from_aggregate)
{
    const astarte_mapping_t mappings[3]
        = { {
                .endpoint = "/%{sensor_id}/double_endpoint",
                .regex_endpoint = "/[a-zA-Z_]+[a-zA-Z0-9_]*/double_endpoint",
                .type = ASTARTE_MAPPING_TYPE_DOUBLE,
                .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
                .explicit_timestamp = false,
                .allow_unset = false,
            },
              {
                  .endpoint = "/%{sensor_id}/integer_endpoint",
                  .regex_endpoint = "/[a-zA-Z_]+[a-zA-Z0-9_]*/integer_endpoint",
                  .type = ASTARTE_MAPPING_TYPE_INTEGER,
                  .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
                  .explicit_timestamp = false,
                  .allow_unset = false,
              },
              {
                  .endpoint = "/%{sensor_id}/stringarray_endpoint",
                  .regex_endpoint = "/[a-zA-Z_]+[a-zA-Z0-9_]*/stringarray_endpoint",
                  .type = ASTARTE_MAPPING_TYPE_STRINGARRAY,
                  .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
                  .explicit_timestamp = false,
                  .allow_unset = false,
              } };

    const astarte_interface_t interface = {
        .name = "org.astarteplatform.zephyr.test",
        .major_version = 0,
        .minor_version = 1,
        .type = ASTARTE_INTERFACE_TYPE_DATASTREAM,
        .ownership = ASTARTE_INTERFACE_OWNERSHIP_SERVER,
        .aggregation = ASTARTE_INTERFACE_AGGREGATION_OBJECT,
        .mappings = mappings,
        .mappings_length = 3U,
    };

    astarte_bson_document_t full_document
        = astarte_bson_deserializer_init_doc(test_data_serialized_aggregate);
    astarte_bson_element_t v_elem;
    astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem);

    astarte_value_pair_t *values;
    size_t values_length = 0;
    astarte_result_t res = astarte_value_pair_deserialize(
        v_elem, &interface, "/sensor33/stringarray_endpoint", &values, &values_length);
    zassert_equal(res, ASTARTE_RESULT_OK, "%s", astarte_result_to_name(res));
    zassert_equal(values_length, 3); // The bson contains two pairs

    astarte_value_pair_t value_pair_double = values[0];
    zassert_equal(strcmp(value_pair_double.endpoint, test_data_aggregate_double_key), 0);
    astarte_value_t value_double = value_pair_double.value;
    zassert_equal(value_double.tag, ASTARTE_MAPPING_TYPE_DOUBLE);
    zassert_equal(value_double.data.dbl, test_data_aggregate_double_value);

    astarte_value_pair_t value_pair_integer = values[1];
    zassert_equal(strcmp(value_pair_integer.endpoint, test_data_aggregate_integer_key), 0);
    astarte_value_t value_integer = value_pair_integer.value;
    zassert_equal(value_integer.tag, ASTARTE_MAPPING_TYPE_INTEGER);
    zassert_equal(value_integer.data.integer, test_data_aggregate_integer_value);

    astarte_value_pair_t value_pair_string = values[2];
    zassert_equal(strcmp(value_pair_string.endpoint, test_data_aggregate_stringarray_key), 0);
    astarte_value_t value_string = value_pair_string.value;
    zassert_equal(value_string.tag, ASTARTE_MAPPING_TYPE_STRINGARRAY);
    zassert_equal(
        value_string.data.string_array.len, ARRAY_SIZE(test_data_aggregate_stringarray_values));
    for (size_t i = 0; i < ARRAY_SIZE(test_data_aggregate_stringarray_values); i++) {
        zassert_equal(strcmp(value_string.data.string_array.buf[i],
                          test_data_aggregate_stringarray_values[i]),
            0);
    }

    astarte_value_pair_destroy_deserialized(values, values_length);
}

ZTEST(astarte_device_sdk_value, test_deserialize_astarte_value_pair_from_empty_aggregate)
{
    astarte_bson_document_t full_document
        = astarte_bson_deserializer_init_doc(test_data_serialized_empty_aggregate);
    astarte_bson_element_t v_elem;
    astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem);

    astarte_value_pair_t *values;
    size_t values_length = 0;
    astarte_result_t res
        = astarte_value_pair_deserialize(v_elem, NULL, NULL, &values, &values_length);
    zassert_equal(res, ASTARTE_RESULT_BSON_EMPTY_DOCUMENT_ERROR, "%s", astarte_result_to_name(res));
}
