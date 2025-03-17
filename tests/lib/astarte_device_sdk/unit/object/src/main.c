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
                .type = ASTARTE_MAPPING_TYPE_DOUBLE,
                .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
                .explicit_timestamp = false,
                .allow_unset = false,
            },
              {
                  .endpoint = "/%{sensor_id}/integer_endpoint",
                  .type = ASTARTE_MAPPING_TYPE_INTEGER,
                  .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
                  .explicit_timestamp = false,
                  .allow_unset = false,
              },
              {
                  .endpoint = "/%{sensor_id}/stringarray_endpoint",
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
        v_elem, &interface, "/sensor33", &entries, &entries_length);
    zassert_equal(res, ASTARTE_RESULT_OK, "%s", astarte_result_to_name(res));
    zassert_equal(entries_length, 3); // The bson contains two pairs

    astarte_object_entry_t entry_double = entries[0];
    zassert_equal(strcmp(entry_double.path, test_data_double_path), 0);
    astarte_data_t data_double = entry_double.data;
    zassert_equal(data_double.tag, ASTARTE_MAPPING_TYPE_DOUBLE);
    zassert_equal(data_double.data.dbl, test_data_double);

    astarte_object_entry_t entry_integer = entries[1];
    zassert_equal(strcmp(entry_integer.path, test_data_integer_path), 0);
    astarte_data_t data_integer = entry_integer.data;
    zassert_equal(data_integer.tag, ASTARTE_MAPPING_TYPE_INTEGER);
    zassert_equal(data_integer.data.integer, test_data_integer);

    astarte_object_entry_t object_string = entries[2];
    zassert_equal(strcmp(object_string.path, test_data_stringarray_path), 0);
    astarte_data_t data_string = object_string.data;
    zassert_equal(data_string.tag, ASTARTE_MAPPING_TYPE_STRINGARRAY);
    zassert_equal(data_string.data.string_array.len, ARRAY_SIZE(test_data_stringarray));
    for (size_t i = 0; i < ARRAY_SIZE(test_data_stringarray); i++) {
        zassert_equal(strcmp(data_string.data.string_array.buf[i], test_data_stringarray[i]), 0);
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
