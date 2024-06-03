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

#include "astarte_device_sdk/object.h"
#include "lib/astarte_device_sdk/object.c"
#include "object_private.h"

#include "bson_deserializer.h"
#include "bson_serializer.h"

ZTEST_SUITE(astarte_device_sdk_object, NULL, NULL, NULL, NULL, NULL);

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

const char test_data_double_path[] = "double_endpoint";
const double test_data_double = 32.1;
const char test_data_integer_path[] = "integer_endpoint";
const double test_data_integer = 42;
const char test_data_stringarray_path[] = "stringarray_endpoint";
const char *test_data_stringarray[] = { "hello, world" };
const uint8_t test_data_serialized[] = { 0x6b, 0x00, 0x00, 0x00, 0x03, 0x76, 0x00, 0x63, 0x00, 0x00,
    0x00, 0x01, 0x64, 0x6f, 0x75, 0x62, 0x6c, 0x65, 0x5f, 0x65, 0x6e, 0x64, 0x70, 0x6f, 0x69, 0x6e,
    0x74, 0x00, 0xcd, 0xcc, 0xcc, 0xcc, 0xcc, 0x0c, 0x40, 0x40, 0x10, 0x69, 0x6e, 0x74, 0x65, 0x67,
    0x65, 0x72, 0x5f, 0x65, 0x6e, 0x64, 0x70, 0x6f, 0x69, 0x6e, 0x74, 0x00, 0x2a, 0x00, 0x00, 0x00,
    0x04, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x61, 0x72, 0x72, 0x61, 0x79, 0x5f, 0x65, 0x6e, 0x64,
    0x70, 0x6f, 0x69, 0x6e, 0x74, 0x00, 0x19, 0x00, 0x00, 0x00, 0x02, 0x30, 0x00, 0x0d, 0x00, 0x00,
    0x00, 0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x2c, 0x20, 0x77, 0x6f, 0x72, 0x6c, 0x64, 0x00, 0x00, 0x00,
    0x00 };

const uint8_t test_data_serialized_empty[]
    = { 0x0d, 0x00, 0x00, 0x00, 0x03, 0x76, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00 };

ZTEST(astarte_device_sdk_object, test_deserialize_astarte_object_from_aggregate)
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
        = astarte_bson_deserializer_init_doc(test_data_serialized);
    astarte_bson_element_t v_elem;
    astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem);

    astarte_object_entry_t *entries = NULL;
    size_t entries_length = 0;
    astarte_result_t res = astarte_object_entries_deserialize(
        v_elem, &interface, "/sensor33/stringarray_endpoint", &entries, &entries_length);
    zassert_equal(res, ASTARTE_RESULT_OK, "%s", astarte_result_to_name(res));
    zassert_equal(entries_length, 3); // The bson contains two pairs

    astarte_object_entry_t entry_double = entries[0];
    zassert_equal(strcmp(entry_double.endpoint, test_data_double_path), 0);
    astarte_individual_t individual_double = entry_double.individual;
    zassert_equal(individual_double.tag, ASTARTE_MAPPING_TYPE_DOUBLE);
    zassert_equal(individual_double.data.dbl, test_data_double);

    astarte_object_entry_t entry_integer = entries[1];
    zassert_equal(strcmp(entry_integer.endpoint, test_data_integer_path), 0);
    astarte_individual_t individual_integer = entry_integer.individual;
    zassert_equal(individual_integer.tag, ASTARTE_MAPPING_TYPE_INTEGER);
    zassert_equal(individual_integer.data.integer, test_data_integer);

    astarte_object_entry_t object_string = entries[2];
    zassert_equal(strcmp(object_string.endpoint, test_data_stringarray_path), 0);
    astarte_individual_t individual_string = object_string.individual;
    zassert_equal(individual_string.tag, ASTARTE_MAPPING_TYPE_STRINGARRAY);
    zassert_equal(individual_string.data.string_array.len, ARRAY_SIZE(test_data_stringarray));
    for (size_t i = 0; i < ARRAY_SIZE(test_data_stringarray); i++) {
        zassert_equal(
            strcmp(individual_string.data.string_array.buf[i], test_data_stringarray[i]), 0);
    }

    astarte_object_entries_destroy_deserialized(entries, entries_length);
}

ZTEST(astarte_device_sdk_object, test_deserialize_astarte_object_from_empty_aggregate)
{
    astarte_bson_document_t full_document
        = astarte_bson_deserializer_init_doc(test_data_serialized_empty);
    astarte_bson_element_t v_elem;
    astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem);

    astarte_object_entry_t *entries = NULL;
    size_t entries_length = 0;
    astarte_result_t res
        = astarte_object_entries_deserialize(v_elem, NULL, NULL, &entries, &entries_length);
    zassert_equal(res, ASTARTE_RESULT_BSON_EMPTY_DOCUMENT_ERROR, "%s", astarte_result_to_name(res));
}
