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

#include "astarte_device_sdk/individual.h"
#include "individual_private.h"
#include "lib/astarte_device_sdk/individual.c"

#include "bson_deserializer.h"
#include "bson_serializer.h"

ZTEST_SUITE(astarte_device_sdk_individual, NULL, NULL, NULL, NULL, NULL);

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

ZTEST(astarte_device_sdk_individual, test_serialize_integer)
{
    astarte_individual_t individual = astarte_individual_from_integer(test_data_integer);
    astarte_bson_serializer_t bson = { 0 };
    zassert_equal(astarte_bson_serializer_init(&bson), ASTARTE_RESULT_OK, "Initialization failure");
    astarte_individual_serialize(&bson, "v", individual);
    astarte_bson_serializer_append_end_of_document(&bson);
    int len = 0;
    const void *data = astarte_bson_serializer_get_serialized(bson, &len);
    zassert_equal(len, sizeof(test_data_serialized_integer));
    zassert_mem_equal(
        data, test_data_serialized_integer, len, "Serialized: %s", hex_to_str(data, len));
}

ZTEST(astarte_device_sdk_individual, test_serialize_longinteger)
{
    astarte_individual_t individual = astarte_individual_from_longinteger(test_data_longinteger);
    astarte_bson_serializer_t bson = { 0 };
    zassert_equal(astarte_bson_serializer_init(&bson), ASTARTE_RESULT_OK, "Initialization failure");
    astarte_individual_serialize(&bson, "v", individual);
    astarte_bson_serializer_append_end_of_document(&bson);
    int len = 0;
    const void *data = astarte_bson_serializer_get_serialized(bson, &len);
    zassert_equal(len, sizeof(test_data_serialized_longinteger));
    zassert_mem_equal(
        data, test_data_serialized_longinteger, len, "Serialized: %s", hex_to_str(data, len));
}

ZTEST(astarte_device_sdk_individual, test_serialize_double)
{
    astarte_individual_t individual = astarte_individual_from_double(test_data_double);
    astarte_bson_serializer_t bson = { 0 };
    zassert_equal(astarte_bson_serializer_init(&bson), ASTARTE_RESULT_OK, "Initialization failure");
    astarte_individual_serialize(&bson, "v", individual);
    astarte_bson_serializer_append_end_of_document(&bson);
    int len = 0;
    const void *data = astarte_bson_serializer_get_serialized(bson, &len);
    zassert_equal(len, sizeof(test_data_serialized_double));
    zassert_mem_equal(
        data, test_data_serialized_double, len, "Serialized: %s", hex_to_str(data, len));
}

ZTEST(astarte_device_sdk_individual, test_serialize_boolean)
{
    astarte_individual_t individual = astarte_individual_from_boolean(test_data_boolean);
    astarte_bson_serializer_t bson = { 0 };
    zassert_equal(astarte_bson_serializer_init(&bson), ASTARTE_RESULT_OK, "Initialization failure");
    astarte_individual_serialize(&bson, "v", individual);
    astarte_bson_serializer_append_end_of_document(&bson);
    int len = 0;
    const void *data = astarte_bson_serializer_get_serialized(bson, &len);
    zassert_equal(len, sizeof(test_data_serialized_boolean));
    zassert_mem_equal(
        data, test_data_serialized_boolean, len, "Serialized: %s", hex_to_str(data, len));
}

ZTEST(astarte_device_sdk_individual, test_serialize_string)
{
    astarte_individual_t individual = astarte_individual_from_string(test_data_string);
    astarte_bson_serializer_t bson = { 0 };
    zassert_equal(astarte_bson_serializer_init(&bson), ASTARTE_RESULT_OK, "Initialization failure");
    astarte_individual_serialize(&bson, "v", individual);
    astarte_bson_serializer_append_end_of_document(&bson);
    int len = 0;
    const void *data = astarte_bson_serializer_get_serialized(bson, &len);
    zassert_equal(len, sizeof(test_data_serialized_string));
    zassert_mem_equal(
        data, test_data_serialized_string, len, "Serialized: %s", hex_to_str(data, len));
}

ZTEST(astarte_device_sdk_individual, test_serialize_integer_array)
{
    astarte_individual_t individual = astarte_individual_from_integer_array(
        (int32_t *) &(test_data_integer_array), sizeof(test_data_integer_array) / sizeof(int32_t));
    astarte_bson_serializer_t bson = { 0 };
    zassert_equal(astarte_bson_serializer_init(&bson), ASTARTE_RESULT_OK, "Initialization failure");
    astarte_individual_serialize(&bson, "v", individual);
    astarte_bson_serializer_append_end_of_document(&bson);
    int len = 0;
    const void *data = astarte_bson_serializer_get_serialized(bson, &len);
    zassert_equal(len, sizeof(test_data_serialized_integer_array));
    zassert_mem_equal(
        data, test_data_serialized_integer_array, len, "Serialized: %s", hex_to_str(data, len));
}

ZTEST(astarte_device_sdk_individual, test_serialize_string_array)
{
    astarte_individual_t individual
        = astarte_individual_from_string_array((const char *const *) &(test_data_string_array),
            sizeof(test_data_string_array) / sizeof(const char *const));
    astarte_bson_serializer_t bson = { 0 };
    zassert_equal(astarte_bson_serializer_init(&bson), ASTARTE_RESULT_OK, "Initialization failure");
    astarte_individual_serialize(&bson, "v", individual);
    astarte_bson_serializer_append_end_of_document(&bson);
    int len = 0;
    const void *data = astarte_bson_serializer_get_serialized(bson, &len);
    zassert_equal(len, sizeof(test_data_serialized_string_array));
    zassert_mem_equal(
        data, test_data_serialized_string_array, len, "Serialized: %s", hex_to_str(data, len));
}

ZTEST(astarte_device_sdk_individual, test_serialize_binaryblob_array)
{
    astarte_individual_t individual
        = astarte_individual_from_binaryblob_array(test_data_binaryblob_array,
            test_data_binaryblob_sizes, sizeof(test_data_binaryblob_array) / sizeof(uint8_t *));

    astarte_bson_serializer_t bson = { 0 };
    zassert_equal(astarte_bson_serializer_init(&bson), ASTARTE_RESULT_OK, "Initialization failure");
    astarte_individual_serialize(&bson, "v", individual);
    astarte_bson_serializer_append_end_of_document(&bson);
    int len = 0;
    const void *data = astarte_bson_serializer_get_serialized(bson, &len);

    zassert_equal(len, sizeof(test_data_serialized_binaryblob_array));
    zassert_mem_equal(
        data, test_data_serialized_binaryblob_array, len, "Serialized: %s", hex_to_str(data, len));
}

ZTEST(astarte_device_sdk_individual, test_deserialize_astarte_individual_from_incorrect_type)
{
    astarte_bson_document_t full_document
        = astarte_bson_deserializer_init_doc(test_data_serialized_binaryblob);
    astarte_bson_element_t v_elem;
    astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem);

    astarte_individual_t individual = { 0 };
    astarte_result_t res
        = astarte_individual_deserialize(v_elem, ASTARTE_MAPPING_TYPE_DATETIMEARRAY, &individual);
    zassert_equal(
        res, ASTARTE_RESULT_BSON_DESERIALIZER_TYPES_ERROR, "%s", astarte_result_to_name(res));
}

ZTEST(astarte_device_sdk_individual, test_deserialize_astarte_individual_from_binblob)
{
    astarte_bson_document_t full_document
        = astarte_bson_deserializer_init_doc(test_data_serialized_binaryblob);
    astarte_bson_element_t v_elem;
    astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem);

    astarte_individual_t individual = { 0 };
    astarte_result_t res
        = astarte_individual_deserialize(v_elem, ASTARTE_MAPPING_TYPE_BINARYBLOB, &individual);
    zassert_equal(res, ASTARTE_RESULT_OK, "%s", astarte_result_to_name(res));
    zassert_equal(individual.tag, ASTARTE_MAPPING_TYPE_BINARYBLOB);
    zassert_equal(individual.data.binaryblob.len, 5);
    zassert_mem_equal(
        individual.data.binaryblob.buf, test_data_binaryblob, individual.data.binaryblob.len);
    astarte_individual_destroy_deserialized(individual);
}

ZTEST(astarte_device_sdk_individual, test_deserialize_astarte_individual_from_boolean)
{
    astarte_bson_document_t full_document
        = astarte_bson_deserializer_init_doc(test_data_serialized_boolean);
    astarte_bson_element_t v_elem;
    astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem);

    astarte_individual_t individual = { 0 };
    astarte_result_t res
        = astarte_individual_deserialize(v_elem, ASTARTE_MAPPING_TYPE_BOOLEAN, &individual);
    zassert_equal(res, ASTARTE_RESULT_OK, "%s", astarte_result_to_name(res));
    zassert_equal(individual.tag, ASTARTE_MAPPING_TYPE_BOOLEAN);
    zassert_equal(individual.data.boolean, test_data_boolean);
    astarte_individual_destroy_deserialized(individual);
}

ZTEST(astarte_device_sdk_individual, test_deserialize_astarte_individual_from_datetime)
{
    astarte_bson_document_t full_document
        = astarte_bson_deserializer_init_doc(test_data_serialized_datetime);
    astarte_bson_element_t v_elem;
    astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem);

    astarte_individual_t individual = { 0 };
    astarte_result_t res
        = astarte_individual_deserialize(v_elem, ASTARTE_MAPPING_TYPE_DATETIME, &individual);
    zassert_equal(res, ASTARTE_RESULT_OK, "%s", astarte_result_to_name(res));
    zassert_equal(individual.tag, ASTARTE_MAPPING_TYPE_DATETIME);
    zassert_equal(individual.data.datetime, test_data_datetime);
    astarte_individual_destroy_deserialized(individual);
}

ZTEST(astarte_device_sdk_individual, test_deserialize_astarte_individual_from_double)
{
    astarte_bson_document_t full_document
        = astarte_bson_deserializer_init_doc(test_data_serialized_double);
    astarte_bson_element_t v_elem;
    astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem);

    astarte_individual_t individual = { 0 };
    astarte_result_t res
        = astarte_individual_deserialize(v_elem, ASTARTE_MAPPING_TYPE_DOUBLE, &individual);
    zassert_equal(res, ASTARTE_RESULT_OK, "%s", astarte_result_to_name(res));
    zassert_equal(individual.tag, ASTARTE_MAPPING_TYPE_DOUBLE);
    zassert_equal(individual.data.dbl, test_data_double);
    astarte_individual_destroy_deserialized(individual);
}

ZTEST(astarte_device_sdk_individual, test_deserialize_astarte_individual_from_integer)
{
    astarte_bson_document_t full_document
        = astarte_bson_deserializer_init_doc(test_data_serialized_integer);
    astarte_bson_element_t v_elem;
    astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem);

    astarte_individual_t individual = { 0 };
    astarte_result_t res
        = astarte_individual_deserialize(v_elem, ASTARTE_MAPPING_TYPE_INTEGER, &individual);
    zassert_equal(res, ASTARTE_RESULT_OK, "%s", astarte_result_to_name(res));
    zassert_equal(individual.tag, ASTARTE_MAPPING_TYPE_INTEGER);
    zassert_equal(individual.data.integer, test_data_integer);
    astarte_individual_destroy_deserialized(individual);
}

ZTEST(astarte_device_sdk_individual, test_deserialize_astarte_individual_from_longinteger)
{
    astarte_bson_document_t full_document
        = astarte_bson_deserializer_init_doc(test_data_serialized_longinteger);
    astarte_bson_element_t v_elem;
    astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem);

    astarte_individual_t individual = { 0 };
    astarte_result_t res
        = astarte_individual_deserialize(v_elem, ASTARTE_MAPPING_TYPE_LONGINTEGER, &individual);
    zassert_equal(res, ASTARTE_RESULT_OK, "%s", astarte_result_to_name(res));
    zassert_equal(individual.tag, ASTARTE_MAPPING_TYPE_LONGINTEGER);
    zassert_equal(individual.data.longinteger, test_data_longinteger);
    astarte_individual_destroy_deserialized(individual);
}

ZTEST(astarte_device_sdk_individual, test_deserialize_astarte_individual_from_string)
{
    astarte_bson_document_t full_document
        = astarte_bson_deserializer_init_doc(test_data_serialized_string);
    astarte_bson_element_t v_elem;
    astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem);

    astarte_individual_t individual = { 0 };
    astarte_result_t res
        = astarte_individual_deserialize(v_elem, ASTARTE_MAPPING_TYPE_STRING, &individual);
    zassert_equal(res, ASTARTE_RESULT_OK, "%s", astarte_result_to_name(res));
    zassert_equal(individual.tag, ASTARTE_MAPPING_TYPE_STRING);
    zassert_mem_equal(individual.data.string, test_data_string, strlen(test_data_string) + 1);
    astarte_individual_destroy_deserialized(individual);
}

ZTEST(astarte_device_sdk_individual, test_deserialize_astarte_individual_from_binblob_array)
{
    astarte_bson_document_t full_document
        = astarte_bson_deserializer_init_doc(test_data_serialized_binaryblob_array);
    astarte_bson_element_t v_elem;
    astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem);

    astarte_individual_t individual = { 0 };
    astarte_result_t res
        = astarte_individual_deserialize(v_elem, ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY, &individual);
    zassert_equal(res, ASTARTE_RESULT_OK, "%s", astarte_result_to_name(res));
    zassert_equal(individual.tag, ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY);
    zassert_equal(individual.data.binaryblob_array.count, ARRAY_SIZE(test_data_binaryblob_array));
    for (size_t i = 0; i < ARRAY_SIZE(test_data_binaryblob_array); i++) {
        zassert_equal(individual.data.binaryblob_array.sizes[i], test_data_binaryblob_sizes[i]);
        zassert_mem_equal(individual.data.binaryblob_array.blobs[i], test_data_binaryblob_array[i],
            test_data_binaryblob_sizes[i]);
    }
    astarte_individual_destroy_deserialized(individual);
}

ZTEST(astarte_device_sdk_individual, test_deserialize_astarte_individual_from_boolean_array)
{
    astarte_bson_document_t full_document
        = astarte_bson_deserializer_init_doc(test_data_serialized_boolean_array);
    astarte_bson_element_t v_elem;
    astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem);

    astarte_individual_t individual = { 0 };
    astarte_result_t res
        = astarte_individual_deserialize(v_elem, ASTARTE_MAPPING_TYPE_BOOLEANARRAY, &individual);
    zassert_equal(res, ASTARTE_RESULT_OK, "%s", astarte_result_to_name(res));
    zassert_equal(individual.tag, ASTARTE_MAPPING_TYPE_BOOLEANARRAY);
    zassert_equal(individual.data.boolean_array.len, ARRAY_SIZE(test_data_boolean_array));
    zassert_mem_equal(individual.data.boolean_array.buf, test_data_boolean_array,
        ARRAY_SIZE(test_data_boolean_array));
    astarte_individual_destroy_deserialized(individual);
}

ZTEST(astarte_device_sdk_individual, test_deserialize_astarte_individual_from_double_array)
{
    astarte_bson_document_t full_document
        = astarte_bson_deserializer_init_doc(test_data_serialized_double_array);
    astarte_bson_element_t v_elem;
    astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem);

    astarte_individual_t individual = { 0 };
    astarte_result_t res
        = astarte_individual_deserialize(v_elem, ASTARTE_MAPPING_TYPE_DOUBLEARRAY, &individual);
    zassert_equal(res, ASTARTE_RESULT_OK, "%d", res);
    zassert_equal(res, ASTARTE_RESULT_OK, "%s", astarte_result_to_name(res));
    zassert_equal(individual.tag, ASTARTE_MAPPING_TYPE_DOUBLEARRAY);
    zassert_equal(individual.data.double_array.len, ARRAY_SIZE(test_data_double_array));
    for (size_t i = 0; i < ARRAY_SIZE(test_data_double_array); i++) {
        zassert_within(individual.data.double_array.buf[i], test_data_double_array[i], 0.01);
    }
    astarte_individual_destroy_deserialized(individual);
}

ZTEST(astarte_device_sdk_individual, test_deserialize_astarte_individual_from_datetime_array)
{
    astarte_bson_document_t full_document
        = astarte_bson_deserializer_init_doc(test_data_serialized_datetime_array);
    astarte_bson_element_t v_elem;
    astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem);

    astarte_individual_t individual = { 0 };
    astarte_result_t res
        = astarte_individual_deserialize(v_elem, ASTARTE_MAPPING_TYPE_DATETIMEARRAY, &individual);
    zassert_equal(res, ASTARTE_RESULT_OK, "%s", astarte_result_to_name(res));
    zassert_equal(individual.tag, ASTARTE_MAPPING_TYPE_DATETIMEARRAY);
    zassert_equal(individual.data.datetime_array.len, ARRAY_SIZE(test_data_datetime_array));
    for (size_t i = 0; i < ARRAY_SIZE(test_data_datetime_array); i++) {
        zassert_equal(individual.data.datetime_array.buf[i], test_data_datetime_array[i]);
    }
    astarte_individual_destroy_deserialized(individual);
}

ZTEST(astarte_device_sdk_individual, test_deserialize_astarte_individual_from_integer_array)
{
    astarte_bson_document_t full_document
        = astarte_bson_deserializer_init_doc(test_data_serialized_integer_array);
    astarte_bson_element_t v_elem;
    astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem);

    astarte_individual_t individual = { 0 };
    astarte_result_t res
        = astarte_individual_deserialize(v_elem, ASTARTE_MAPPING_TYPE_INTEGERARRAY, &individual);
    zassert_equal(res, ASTARTE_RESULT_OK, "%s", astarte_result_to_name(res));
    zassert_equal(individual.tag, ASTARTE_MAPPING_TYPE_INTEGERARRAY);
    zassert_equal(individual.data.integer_array.len, ARRAY_SIZE(test_data_integer_array));
    for (size_t i = 0; i < ARRAY_SIZE(test_data_integer_array); i++) {
        zassert_equal(individual.data.integer_array.buf[i], test_data_integer_array[i]);
    }
    astarte_individual_destroy_deserialized(individual);
}

ZTEST(astarte_device_sdk_individual, test_deserialize_astarte_individual_from_longinteger_array)
{
    astarte_bson_document_t full_document
        = astarte_bson_deserializer_init_doc(test_data_serialized_longinteger_array);
    astarte_bson_element_t v_elem;
    astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem);

    astarte_individual_t individual = { 0 };
    astarte_result_t res = astarte_individual_deserialize(
        v_elem, ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY, &individual);
    zassert_equal(res, ASTARTE_RESULT_OK, "%s", astarte_result_to_name(res));
    zassert_equal(individual.tag, ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY);
    zassert_equal(individual.data.longinteger_array.len, ARRAY_SIZE(test_data_longinteger_array));
    for (size_t i = 0; i < ARRAY_SIZE(test_data_longinteger_array); i++) {
        zassert_equal(individual.data.longinteger_array.buf[i], test_data_longinteger_array[i]);
    }
    astarte_individual_destroy_deserialized(individual);
}

ZTEST(astarte_device_sdk_individual, test_deserialize_astarte_individual_from_string_array)
{
    astarte_bson_document_t full_document
        = astarte_bson_deserializer_init_doc(test_data_serialized_string_array);
    astarte_bson_element_t v_elem;
    astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem);

    astarte_individual_t individual = { 0 };
    astarte_result_t res
        = astarte_individual_deserialize(v_elem, ASTARTE_MAPPING_TYPE_STRINGARRAY, &individual);
    zassert_equal(res, ASTARTE_RESULT_OK, "%s", astarte_result_to_name(res));
    zassert_equal(individual.tag, ASTARTE_MAPPING_TYPE_STRINGARRAY);
    zassert_equal(individual.data.string_array.len, ARRAY_SIZE(test_data_string_array));
    for (size_t i = 0; i < ARRAY_SIZE(test_data_string_array); i++) {
        zassert_equal(strcmp(individual.data.string_array.buf[i], test_data_string_array[i]), 0);
    }
    astarte_individual_destroy_deserialized(individual);
}

ZTEST(astarte_device_sdk_individual, test_deserialize_astarte_individual_from_empty_array)
{
    astarte_bson_document_t full_document
        = astarte_bson_deserializer_init_doc(test_data_serialized_empty_array);
    astarte_bson_element_t v_elem;
    astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem);

    astarte_individual_t individual = { 0 };
    astarte_result_t res
        = astarte_individual_deserialize(v_elem, ASTARTE_MAPPING_TYPE_DOUBLEARRAY, &individual);
    zassert_equal(res, ASTARTE_RESULT_OK, "%s", astarte_result_to_name(res));
    zassert_equal(individual.tag, ASTARTE_MAPPING_TYPE_DOUBLEARRAY);
    zassert_equal(individual.data.double_array.len, 0);
}

ZTEST(astarte_device_sdk_individual,
    test_deserialize_astarte_individual_from_mismatched_array_initial)
{
    astarte_bson_document_t full_document
        = astarte_bson_deserializer_init_doc(test_data_serialized_mismatched_array_initial);
    astarte_bson_element_t v_elem;
    astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem);

    astarte_individual_t individual = { 0 };
    astarte_result_t res
        = astarte_individual_deserialize(v_elem, ASTARTE_MAPPING_TYPE_STRINGARRAY, &individual);
    zassert_equal(
        res, ASTARTE_RESULT_BSON_DESERIALIZER_TYPES_ERROR, "%s", astarte_result_to_name(res));
}

ZTEST(
    astarte_device_sdk_individual, test_deserialize_astarte_individual_from_mismatched_array_final)
{
    astarte_bson_document_t full_document
        = astarte_bson_deserializer_init_doc(test_data_serialized_mismatched_array_final);
    astarte_bson_element_t v_elem;
    astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem);

    astarte_individual_t individual = { 0 };
    astarte_result_t res
        = astarte_individual_deserialize(v_elem, ASTARTE_MAPPING_TYPE_STRINGARRAY, &individual);
    zassert_equal(
        res, ASTARTE_RESULT_BSON_DESERIALIZER_TYPES_ERROR, "%s", astarte_result_to_name(res));
}
