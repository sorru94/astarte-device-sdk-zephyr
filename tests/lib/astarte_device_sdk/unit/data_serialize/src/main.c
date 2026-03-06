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
#include <stdlib.h>

#include <zephyr/ztest.h>

#include "astarte_device_sdk/data.h"
#include "data_serialize.h"
#include "lib/astarte_device_sdk/data_serialize.c"

#include "bson_serializer.h"

ZTEST_SUITE(astarte_device_sdk_astarte_data_serialize, NULL, NULL, NULL, NULL, NULL);

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
        const uint8_t *byte = input + i;
        cursor = cursor + sprintf(cursor, "0x%02x", *byte);
        if (i < input_len - 1) {
            cursor = cursor + sprintf(cursor, ", ");
        }
    }
    sprintf(cursor, "}");
    return dyn_buf;
}

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

const double test_data_double = 432.4324;
const uint8_t test_data_serialized_double[] = { 0x10, 0x00, 0x00, 0x00, 0x01, 0x76, 0x00, 0xa5,
    0x2c, 0x43, 0x1c, 0xeb, 0x06, 0x7b, 0x40, 0x00 };

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

ZTEST(astarte_device_sdk_astarte_data_serialize, test_serialize_integer)
{
    astarte_data_t data = astarte_data_from_integer(test_data_integer);
    astarte_bson_serializer_t bson = { 0 };
    zassert_equal(astarte_bson_serializer_init(&bson), ASTARTE_RESULT_OK, "Initialization failure");
    data_serialize(&bson, "v", data);
    astarte_bson_serializer_append_end_of_document(&bson);
    int len = 0;
    const void *data_ser = astarte_bson_serializer_get_serialized(&bson, &len);
    zassert_equal(len, sizeof(test_data_serialized_integer));
    zassert_mem_equal(
        data_ser, test_data_serialized_integer, len, "Serialized: %s", hex_to_str(data_ser, len));
}

ZTEST(astarte_device_sdk_astarte_data_serialize, test_serialize_longinteger)
{
    astarte_data_t data = astarte_data_from_longinteger(test_data_longinteger);
    astarte_bson_serializer_t bson = { 0 };
    zassert_equal(astarte_bson_serializer_init(&bson), ASTARTE_RESULT_OK, "Initialization failure");
    data_serialize(&bson, "v", data);
    astarte_bson_serializer_append_end_of_document(&bson);
    int len = 0;
    const void *data_ser = astarte_bson_serializer_get_serialized(&bson, &len);
    zassert_equal(len, sizeof(test_data_serialized_longinteger));
    zassert_mem_equal(data_ser, test_data_serialized_longinteger, len, "Serialized: %s",
        hex_to_str(data_ser, len));
}

ZTEST(astarte_device_sdk_astarte_data_serialize, test_serialize_double)
{
    astarte_data_t data = astarte_data_from_double(test_data_double);
    astarte_bson_serializer_t bson = { 0 };
    zassert_equal(astarte_bson_serializer_init(&bson), ASTARTE_RESULT_OK, "Initialization failure");
    data_serialize(&bson, "v", data);
    astarte_bson_serializer_append_end_of_document(&bson);
    int len = 0;
    const void *data_ser = astarte_bson_serializer_get_serialized(&bson, &len);
    zassert_equal(len, sizeof(test_data_serialized_double));
    zassert_mem_equal(
        data_ser, test_data_serialized_double, len, "Serialized: %s", hex_to_str(data_ser, len));
}

ZTEST(astarte_device_sdk_astarte_data_serialize, test_serialize_boolean)
{
    astarte_data_t data = astarte_data_from_boolean(test_data_boolean);
    astarte_bson_serializer_t bson = { 0 };
    zassert_equal(astarte_bson_serializer_init(&bson), ASTARTE_RESULT_OK, "Initialization failure");
    data_serialize(&bson, "v", data);
    astarte_bson_serializer_append_end_of_document(&bson);
    int len = 0;
    const void *data_ser = astarte_bson_serializer_get_serialized(&bson, &len);
    zassert_equal(len, sizeof(test_data_serialized_boolean));
    zassert_mem_equal(
        data_ser, test_data_serialized_boolean, len, "Serialized: %s", hex_to_str(data_ser, len));
}

ZTEST(astarte_device_sdk_astarte_data_serialize, test_serialize_string)
{
    astarte_data_t data = astarte_data_from_string(test_data_string);
    astarte_bson_serializer_t bson = { 0 };
    zassert_equal(astarte_bson_serializer_init(&bson), ASTARTE_RESULT_OK, "Initialization failure");
    data_serialize(&bson, "v", data);
    astarte_bson_serializer_append_end_of_document(&bson);
    int len = 0;
    const void *data_ser = astarte_bson_serializer_get_serialized(&bson, &len);
    zassert_equal(len, sizeof(test_data_serialized_string));
    zassert_mem_equal(
        data_ser, test_data_serialized_string, len, "Serialized: %s", hex_to_str(data_ser, len));
}

ZTEST(astarte_device_sdk_astarte_data_serialize, test_serialize_integer_array)
{
    astarte_data_t data = astarte_data_from_integer_array(
        (int32_t *) &(test_data_integer_array), sizeof(test_data_integer_array) / sizeof(int32_t));
    astarte_bson_serializer_t bson = { 0 };
    zassert_equal(astarte_bson_serializer_init(&bson), ASTARTE_RESULT_OK, "Initialization failure");
    data_serialize(&bson, "v", data);
    astarte_bson_serializer_append_end_of_document(&bson);
    int len = 0;
    const void *data_ser = astarte_bson_serializer_get_serialized(&bson, &len);
    zassert_equal(len, sizeof(test_data_serialized_integer_array));
    zassert_mem_equal(data_ser, test_data_serialized_integer_array, len, "Serialized: %s",
        hex_to_str(data_ser, len));
}

ZTEST(astarte_device_sdk_astarte_data_serialize, test_serialize_string_array)
{
    astarte_data_t data = astarte_data_from_string_array((const char **) &(test_data_string_array),
        sizeof(test_data_string_array) / sizeof(const char *const));
    astarte_bson_serializer_t bson = { 0 };
    zassert_equal(astarte_bson_serializer_init(&bson), ASTARTE_RESULT_OK, "Initialization failure");
    data_serialize(&bson, "v", data);
    astarte_bson_serializer_append_end_of_document(&bson);
    int len = 0;
    const void *data_ser = astarte_bson_serializer_get_serialized(&bson, &len);
    zassert_equal(len, sizeof(test_data_serialized_string_array));
    zassert_mem_equal(data_ser, test_data_serialized_string_array, len, "Serialized: %s",
        hex_to_str(data_ser, len));
}

ZTEST(astarte_device_sdk_astarte_data_serialize, test_serialize_binaryblob_array)
{
    astarte_data_t data = astarte_data_from_binaryblob_array(test_data_binaryblob_array,
        (size_t *) test_data_binaryblob_sizes,
        sizeof(test_data_binaryblob_array) / sizeof(uint8_t *));

    astarte_bson_serializer_t bson = { 0 };
    zassert_equal(astarte_bson_serializer_init(&bson), ASTARTE_RESULT_OK, "Initialization failure");
    data_serialize(&bson, "v", data);
    astarte_bson_serializer_append_end_of_document(&bson);
    int len = 0;
    const void *data_ser = astarte_bson_serializer_get_serialized(&bson, &len);

    zassert_equal(len, sizeof(test_data_serialized_binaryblob_array));
    zassert_mem_equal(data_ser, test_data_serialized_binaryblob_array, len, "Serialized: %s",
        hex_to_str(data_ser, len));
}
