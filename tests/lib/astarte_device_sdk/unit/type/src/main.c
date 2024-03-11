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
#include "bson_serializer.h"
#include "lib/astarte_device_sdk/bson_serializer.c"
#include "lib/astarte_device_sdk/value.c"

ZTEST_SUITE(astarte_device_sdk_type, NULL, NULL, NULL, NULL, NULL);

// Define a minimal_log function to resolve the `undefined reference to z_log_minimal_printk` error,
// because the log environment is missing in the unit_testing platform.
void z_log_minimal_printk(const char *fmt, ...) {}

static void compare_buf(uint8_t *val_a, uint8_t *val_b, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        zassert_equal(val_a[i], val_b[i]);
    }
}

static void print_hex(const char *s, size_t len)
{
    for (int i = 0; i < len; ++i) {
        printf("%02x", (unsigned int) *s++);
    }

    printf("\n");
}

#define TEST_SINGLE_PROPERTY_SERIALIZATION(METHOD, VALUE, SERIALIZED_CONST)                        \
    ZTEST(astarte_device_sdk_type, test_serialize_##METHOD)                                        \
    {                                                                                              \
        astarte_value_t astarte_value = astarte_value_from_##METHOD(VALUE);                        \
        astarte_bson_serializer_handle_t bson = astarte_bson_serializer_new();                     \
        astarte_value_serialize(bson, "v", astarte_value);                                         \
        astarte_bson_serializer_append_end_of_document(bson);                                      \
        int len = 0;                                                                               \
        const void *data = astarte_bson_serializer_get_document(bson, &len);                       \
        print_hex(data, (size_t) len);                                                             \
        zassert_equal(len, sizeof(SERIALIZED_CONST));                                              \
        compare_buf((uint8_t *) data, (uint8_t *) (SERIALIZED_CONST), (size_t) len);               \
    }

const int32_t test_integer = 42;
const uint8_t serialized_integer[] = {
    0x0C,
    0x00,
    0x00,
    0x00,
    0x10,
    0x76,
    0x00,
    0x2a,
    0x00,
    0x00,
    0x00,
    0x00,
};
TEST_SINGLE_PROPERTY_SERIALIZATION(integer, test_integer, serialized_integer) // NOLINT

const int64_t test_longinteger = 3147483647;
const uint8_t serialized_longinteger[] = {
    0x10,
    0x00,
    0x00,
    0x00,
    0x12,
    0x76,
    0x00,
    0xff,
    0xc9,
    0x9a,
    0xbb,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
};
TEST_SINGLE_PROPERTY_SERIALIZATION(longinteger, test_longinteger, serialized_longinteger) // NOLINT

const double test_double = 432.4324;
const uint8_t serialized_double[] = { 0x10, 0x00, 0x00, 0x00, 0x01, 0x76, 0x00, 0xa5, 0x2c, 0x43,
    0x1c, 0xeb, 0x06, 0x7b, 0x40, 0x00 };
TEST_SINGLE_PROPERTY_SERIALIZATION(double, test_double, serialized_double) // NOLINT

const bool test_boolean = true;
const uint8_t serialized_boolean[] = {
    0x09,
    0x00,
    0x00,
    0x00,
    0x08,
    0x76,
    0x00,
    0x01,
    0x00,
};
TEST_SINGLE_PROPERTY_SERIALIZATION(boolean, test_boolean, serialized_boolean) // NOLINT

const char *test_string = "this is a test string";
const uint8_t serialized_string[] = {
    0x22,
    0x00,
    0x00,
    0x00,
    0x02,
    0x76,
    0x00,
    0x16,
    0x00,
    0x00,
    0x00,
    0x74,
    0x68,
    0x69,
    0x73,
    0x20,
    0x69,
    0x73,
    0x20,
    0x61,
    0x20,
    0x74,
    0x65,
    0x73,
    0x74,
    0x20,
    0x73,
    0x74,
    0x72,
    0x69,
    0x6e,
    0x67,
    0x00,
    0x00,
};
TEST_SINGLE_PROPERTY_SERIALIZATION(string, test_string, serialized_string) // NOLINT

#define TEST_ARRAY_SERIALIZATION(METHOD, ARRAY_ELEMENT_TYPE, ARRAY_CONST, SERIALIZED_CONST)        \
    ZTEST(astarte_device_sdk_type, test_serialize_##METHOD)                                        \
    {                                                                                              \
        astarte_value_t astarte_value                                                              \
            = astarte_value_from_##METHOD((ARRAY_ELEMENT_TYPE *) &(ARRAY_CONST),                   \
                sizeof(ARRAY_CONST) / sizeof(ARRAY_ELEMENT_TYPE));                                 \
        astarte_bson_serializer_handle_t bson = astarte_bson_serializer_new();                     \
        astarte_value_serialize(bson, "v", astarte_value);                                         \
        astarte_bson_serializer_append_end_of_document(bson);                                      \
        int len = 0;                                                                               \
        const void *data = astarte_bson_serializer_get_document(bson, &len);                       \
        print_hex(data, (size_t) len);                                                             \
        zassert_equal(len, sizeof(SERIALIZED_CONST));                                              \
        compare_buf((uint8_t *) data, (uint8_t *) (SERIALIZED_CONST), (size_t) len);               \
    }

const int32_t test_integer_array[] = {
    42,
    10,
    128,
    9,
    256,
};
const uint8_t serialized_integer_array[] = {
    0x30,
    0x00,
    0x00,
    0x00,
    0x04,
    0x76,
    0x00,
    0x28,
    0x00,
    0x00,
    0x00,
    0x10,
    0x30,
    0x00,
    0x2a,
    0x00,
    0x00,
    0x00,
    0x10,
    0x31,
    0x00,
    0x0a,
    0x00,
    0x00,
    0x00,
    0x10,
    0x32,
    0x00,
    0x80,
    0x00,
    0x00,
    0x00,
    0x10,
    0x33,
    0x00,
    0x09,
    0x00,
    0x00,
    0x00,
    0x10,
    0x34,
    0x00,
    0x00,
    0x01,
    0x00,
    0x00,
    0x00,
    0x00,
};
TEST_ARRAY_SERIALIZATION(integer_array, int32_t, test_integer_array, serialized_integer_array)

const char *test_string_array[] = { "this", "is", "a", "test", "string_array" };
const uint8_t serialized_string_array[] = {
    0x4c,
    0x00,
    0x00,
    0x00,
    0x04,
    0x76,
    0x00,
    0x44,
    0x00,
    0x00,
    0x00,
    0x02,
    0x30,
    0x00,
    0x05,
    0x00,
    0x00,
    0x00,
    0x74,
    0x68,
    0x69,
    0x73,
    0x00,
    0x02,
    0x31,
    0x00,
    0x03,
    0x00,
    0x00,
    0x00,
    0x69,
    0x73,
    0x00,
    0x02,
    0x32,
    0x00,
    0x02,
    0x00,
    0x00,
    0x00,
    0x61,
    0x00,
    0x02,
    0x33,
    0x00,
    0x05,
    0x00,
    0x00,
    0x00,
    0x74,
    0x65,
    0x73,
    0x74,
    0x00,
    0x02,
    0x34,
    0x00,
    0x0d,
    0x00,
    0x00,
    0x00,
    0x73,
    0x74,
    0x72,
    0x69,
    0x6e,
    0x67,
    0x5f,
    0x61,
    0x72,
    0x72,
    0x61,
    0x79,
    0x00,
    0x00,
    0x00,
};
TEST_ARRAY_SERIALIZATION(
    string_array, const char *const, test_string_array, serialized_string_array)

const uint8_t blob_1[] = {
    0x41,
    0x53,
    0x54,
    0x41,
    0x52,
    0x54,
    0x45,
};
const uint8_t blob_2[] = {
    0x49,
    0x53,
};
const uint8_t blob_3[] = {
    0x43,
    0x4F,
    0x4F,
    0x4C,
};
const void *test_binaryblob_array[] = {
    blob_1,
    blob_2,
    blob_3,
};
const int test_binaryblob_sizes[] = {
    sizeof(blob_1) / sizeof(uint8_t),
    sizeof(blob_2) / sizeof(uint8_t),
    sizeof(blob_3) / sizeof(uint8_t),
};
const uint8_t serialized_binaryblob_array[] = {
    0x32,
    0x00,
    0x00,
    0x00,
    0x04,
    0x76,
    0x00,
    0x2a,
    0x00,
    0x00,
    0x00,
    0x05,
    0x30,
    0x00,
    0x07,
    0x00,
    0x00,
    0x00,
    0x00,
    0x41,
    0x53,
    0x54,
    0x41,
    0x52,
    0x54,
    0x45,
    0x05,
    0x31,
    0x00,
    0x02,
    0x00,
    0x00,
    0x00,
    0x00,
    0x49,
    0x53,
    0x05,
    0x32,
    0x00,
    0x04,
    0x00,
    0x00,
    0x00,
    0x00,
    0x43,
    0x4f,
    0x4f,
    0x4c,
    0x00,
    0x00,
};
ZTEST(astarte_device_sdk_type, test_serialize_binaryblob_array)
{
    astarte_value_t astarte_value = astarte_value_from_binaryblob_array(test_binaryblob_array,
        test_binaryblob_sizes, sizeof(test_binaryblob_array) / sizeof(uint8_t *));

    astarte_bson_serializer_handle_t bson = astarte_bson_serializer_new();
    astarte_value_serialize(bson, "v", astarte_value);
    astarte_bson_serializer_append_end_of_document(bson);
    int len = 0;
    const void *data = astarte_bson_serializer_get_document(bson, &len);

    print_hex(data, len);
    zassert_equal(len, sizeof(serialized_binaryblob_array));
    compare_buf((uint8_t *) data, (uint8_t *) serialized_binaryblob_array, (size_t) len);
}
