/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * @file astarte-device-sdk-zephyr/tests/lib/astarte_device_sdk/unit/bson/src/main.c
 *
 * @details This test suite verifies that the methods provided with the unit_testable_component
 * module works correctly.
 *
 * @note This should be run with the latest version of zephyr present on master (or 3.6.0)
 */

#include <zephyr/ztest.h>

#include <astarte_device_sdk/bson_deserializer.h>
#include <lib/astarte_device_sdk/bson_deserializer.c>

static const uint8_t empty_bson_document[] = { 0x05, 0x00, 0x00, 0x00, 0x00 };

/** Complete BSON corresponds to:
 * {
 *     "element double": 42.3, # Double
 *     "element string": "hello world", # String
 *     "element document": {"subelement int32": 10, "subelement bool true": True}, # Document
 *     "element array": [10, 42.3], # Array
 *     "element binary": b'bin encoded string', # Binary
 *     "element bool false": False, # Boolean false
 *     "element bool true": True, # Boolean false
 *     "element UTC datetime": datetime.now(timezone.utc), # UTC timestamp
 *     "element int32":10, # Integer 32
 *     "element int64":17179869184, # Integer 64
 * }
 */

static const uint8_t complete_bson_document[] = { 0x3f, 0x1, 0x0, 0x0, 0x1, 0x65, 0x6c, 0x65, 0x6d,
    0x65, 0x6e, 0x74, 0x20, 0x64, 0x6f, 0x75, 0x62, 0x6c, 0x65, 0x0, 0x66, 0x66, 0x66, 0x66, 0x66,
    0x26, 0x45, 0x40, 0x2, 0x65, 0x6c, 0x65, 0x6d, 0x65, 0x6e, 0x74, 0x20, 0x73, 0x74, 0x72, 0x69,
    0x6e, 0x67, 0x0, 0xc, 0x0, 0x0, 0x0, 0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x77, 0x6f, 0x72, 0x6c,
    0x64, 0x0, 0x3, 0x65, 0x6c, 0x65, 0x6d, 0x65, 0x6e, 0x74, 0x20, 0x64, 0x6f, 0x63, 0x75, 0x6d,
    0x65, 0x6e, 0x74, 0x0, 0x32, 0x0, 0x0, 0x0, 0x10, 0x73, 0x75, 0x62, 0x65, 0x6c, 0x65, 0x6d,
    0x65, 0x6e, 0x74, 0x20, 0x69, 0x6e, 0x74, 0x33, 0x32, 0x0, 0xa, 0x0, 0x0, 0x0, 0x8, 0x73, 0x75,
    0x62, 0x65, 0x6c, 0x65, 0x6d, 0x65, 0x6e, 0x74, 0x20, 0x62, 0x6f, 0x6f, 0x6c, 0x20, 0x74, 0x72,
    0x75, 0x65, 0x0, 0x1, 0x0, 0x4, 0x65, 0x6c, 0x65, 0x6d, 0x65, 0x6e, 0x74, 0x20, 0x61, 0x72,
    0x72, 0x61, 0x79, 0x0, 0x17, 0x0, 0x0, 0x0, 0x10, 0x30, 0x0, 0xa, 0x0, 0x0, 0x0, 0x1, 0x31, 0x0,
    0x66, 0x66, 0x66, 0x66, 0x66, 0x26, 0x45, 0x40, 0x0, 0x5, 0x65, 0x6c, 0x65, 0x6d, 0x65, 0x6e,
    0x74, 0x20, 0x62, 0x69, 0x6e, 0x61, 0x72, 0x79, 0x0, 0x12, 0x0, 0x0, 0x0, 0x0, 0x62, 0x69, 0x6e,
    0x20, 0x65, 0x6e, 0x63, 0x6f, 0x64, 0x65, 0x64, 0x20, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x8,
    0x65, 0x6c, 0x65, 0x6d, 0x65, 0x6e, 0x74, 0x20, 0x62, 0x6f, 0x6f, 0x6c, 0x20, 0x66, 0x61, 0x6c,
    0x73, 0x65, 0x0, 0x0, 0x8, 0x65, 0x6c, 0x65, 0x6d, 0x65, 0x6e, 0x74, 0x20, 0x62, 0x6f, 0x6f,
    0x6c, 0x20, 0x74, 0x72, 0x75, 0x65, 0x0, 0x1, 0x9, 0x65, 0x6c, 0x65, 0x6d, 0x65, 0x6e, 0x74,
    0x20, 0x55, 0x54, 0x43, 0x20, 0x64, 0x61, 0x74, 0x65, 0x74, 0x69, 0x6d, 0x65, 0x0, 0x3e, 0x20,
    0x93, 0x9f, 0x88, 0x1, 0x0, 0x0, 0x10, 0x65, 0x6c, 0x65, 0x6d, 0x65, 0x6e, 0x74, 0x20, 0x69,
    0x6e, 0x74, 0x33, 0x32, 0x0, 0xa, 0x0, 0x0, 0x0, 0x12, 0x65, 0x6c, 0x65, 0x6d, 0x65, 0x6e, 0x74,
    0x20, 0x69, 0x6e, 0x74, 0x36, 0x34, 0x0, 0x0, 0x0, 0x0, 0x0, 0x4, 0x0, 0x0, 0x0, 0x0 };

ZTEST(astarte_device_sdk_bson, test_bson_deserializer_check_validity)
{
    uint8_t empty_buffer[] = {};
    zassert_false(astarte_bson_deserializer_check_validity(empty_buffer, sizeof(empty_buffer)));

    uint8_t minimal_doc[] = { 0x8, 0x0, 0x0, 0x0, 0x8, 0x0, 0x1, 0x0 };
    zassert_false(astarte_bson_deserializer_check_validity(minimal_doc, sizeof(minimal_doc) - 1));

    uint8_t empty_doc_incorrect_termination[] = { 0x05, 0x00, 0x00, 0x00, 0x01 };
    zassert_false(astarte_bson_deserializer_check_validity(
        empty_doc_incorrect_termination, sizeof(empty_doc_incorrect_termination)));

    zassert_true(
        astarte_bson_deserializer_check_validity(empty_bson_document, sizeof(empty_bson_document)));

    uint8_t too_small_doc[] = { 0x7, 0x0, 0x0, 0x0, 0x8, 0x0, 0x0 };
    zassert_false(astarte_bson_deserializer_check_validity(too_small_doc, sizeof(too_small_doc)));

    uint8_t fist_element_incorrect_doc[] = { 0x8, 0x0, 0x0, 0x0, 0x6, 0x0, 0x1, 0x0 };
    zassert_false(astarte_bson_deserializer_check_validity(
        fist_element_incorrect_doc, sizeof(fist_element_incorrect_doc)));

    zassert_true(astarte_bson_deserializer_check_validity(minimal_doc, sizeof(minimal_doc)));

    zassert_true(astarte_bson_deserializer_check_validity(
        complete_bson_document, sizeof(complete_bson_document)));
}

ZTEST(astarte_device_sdk_bson, test_bson_deserializer_empty_bson_document)
{
    astarte_bson_document_t doc = astarte_bson_deserializer_init_doc(empty_bson_document);
    zassert_equal(5, doc.size, "doc.size != from 5");

    astarte_bson_element_t element;
    zassert_equal(ASTARTE_RESULT_NOT_FOUND, astarte_bson_deserializer_first_element(doc, &element),
        "astarte err returned != from ASTARTE_RESULT_NOT_FOUND");
}

ZTEST(astarte_device_sdk_bson, test_bson_deserializer_complete_bson_document)
{
    astarte_bson_document_t doc = astarte_bson_deserializer_init_doc(complete_bson_document);
    zassert_equal(319, doc.size);

    astarte_bson_element_t element_d;
    zassert_equal(ASTARTE_RESULT_OK, astarte_bson_deserializer_first_element(doc, &element_d));

    zassert_equal(ASTARTE_BSON_TYPE_DOUBLE, element_d.type);
    zassert_mem_equal("element double", element_d.name, sizeof("element double"));
    double value_d = astarte_bson_deserializer_element_to_double(element_d);
    zassert_within(0.01, 42.3, value_d);

    astarte_bson_element_t element_s;
    zassert_equal(
        ASTARTE_RESULT_OK, astarte_bson_deserializer_next_element(doc, element_d, &element_s));

    zassert_equal(ASTARTE_BSON_TYPE_STRING, element_s.type);
    zassert_mem_equal("element string", element_s.name, sizeof("element string"));
    uint32_t size;
    const char *value_s = astarte_bson_deserializer_element_to_string(element_s, &size);
    zassert_mem_equal("hello world", value_s, sizeof("hello world"));

    astarte_bson_element_t element_doc;
    zassert_equal(
        ASTARTE_RESULT_OK, astarte_bson_deserializer_next_element(doc, element_s, &element_doc));

    zassert_equal(ASTARTE_BSON_TYPE_DOCUMENT, element_doc.type);
    zassert_mem_equal("element document", element_doc.name, sizeof("element document"));
    astarte_bson_document_t subdocument
        = astarte_bson_deserializer_element_to_document(element_doc);
    zassert_equal(50, subdocument.size);

    astarte_bson_element_t subelement_int32;
    zassert_equal(
        ASTARTE_RESULT_OK, astarte_bson_deserializer_first_element(subdocument, &subelement_int32));

    zassert_equal(ASTARTE_BSON_TYPE_INT32, subelement_int32.type);
    zassert_mem_equal("subelement int32", subelement_int32.name, sizeof("subelement int32"));
    int32_t subvalue_int32 = astarte_bson_deserializer_element_to_int32(subelement_int32);
    zassert_equal(10, subvalue_int32);

    astarte_bson_element_t subelement_bool;
    zassert_equal(ASTARTE_RESULT_OK,
        astarte_bson_deserializer_next_element(subdocument, subelement_int32, &subelement_bool));

    zassert_equal(ASTARTE_BSON_TYPE_BOOLEAN, subelement_bool.type);
    zassert_mem_equal("subelement bool true", subelement_bool.name, sizeof("subelement bool true"));
    bool subvalue_bool = astarte_bson_deserializer_element_to_bool(subelement_bool);
    zassert_true(subvalue_bool);

    astarte_bson_element_t element_arr;
    zassert_equal(
        ASTARTE_RESULT_OK, astarte_bson_deserializer_next_element(doc, element_doc, &element_arr));

    zassert_equal(ASTARTE_BSON_TYPE_ARRAY, element_arr.type);
    zassert_mem_equal("element array", element_arr.name, sizeof("element array"));
    astarte_bson_document_t subdoc_arr = astarte_bson_deserializer_element_to_document(element_arr);
    zassert_equal(23, subdoc_arr.size);

    astarte_bson_element_t subelement_arr_1;
    zassert_equal(
        ASTARTE_RESULT_OK, astarte_bson_deserializer_first_element(subdoc_arr, &subelement_arr_1));

    zassert_equal(ASTARTE_BSON_TYPE_INT32, subelement_arr_1.type);
    zassert_mem_equal("0", subelement_arr_1.name, 1);
    int32_t subvalue_arr_1 = astarte_bson_deserializer_element_to_int32(subelement_arr_1);
    zassert_equal(10, subvalue_arr_1);

    astarte_bson_element_t subelement_arr_2;
    zassert_equal(ASTARTE_RESULT_OK,
        astarte_bson_deserializer_next_element(subdoc_arr, subelement_arr_1, &subelement_arr_2));

    zassert_equal(ASTARTE_BSON_TYPE_DOUBLE, subelement_arr_2.type);
    zassert_mem_equal("1", subelement_arr_2.name, 1);
    double subvalue_arr_2 = astarte_bson_deserializer_element_to_double(subelement_arr_2);
    zassert_within(0.01, 42.3, subvalue_arr_2);

    astarte_bson_element_t element_bin;
    zassert_equal(
        ASTARTE_RESULT_OK, astarte_bson_deserializer_next_element(doc, element_arr, &element_bin));

    zassert_equal(ASTARTE_BSON_TYPE_BINARY, element_bin.type);
    zassert_mem_equal("element binary", element_bin.name, sizeof("element binary"));
    const uint8_t *value_bin = astarte_bson_deserializer_element_to_binary(element_bin, &size);
    zassert_equal(18, size);
    uint8_t expected_value_bin[] = { 0x62, 0x69, 0x6e, 0x20, 0x65, 0x6e, 0x63, 0x6f, 0x64, 0x65,
        0x64, 0x20, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67 };
    zassert_mem_equal(expected_value_bin, value_bin, size);

    astarte_bson_element_t element_bool_false;
    zassert_equal(ASTARTE_RESULT_OK,
        astarte_bson_deserializer_next_element(doc, element_bin, &element_bool_false));

    zassert_equal(ASTARTE_BSON_TYPE_BOOLEAN, element_bool_false.type);
    zassert_mem_equal("element bool false", element_bool_false.name, sizeof("element bool false"));
    bool value_bool = astarte_bson_deserializer_element_to_bool(element_bool_false);
    zassert_false(value_bool);

    astarte_bson_element_t element_bool_true;
    zassert_equal(ASTARTE_RESULT_OK,
        astarte_bson_deserializer_next_element(doc, element_bool_false, &element_bool_true));

    zassert_equal(ASTARTE_BSON_TYPE_BOOLEAN, element_bool_true.type);
    zassert_mem_equal("element bool true", element_bool_true.name, sizeof("element bool true"));
    value_bool = astarte_bson_deserializer_element_to_bool(element_bool_true);
    zassert_true(value_bool);

    astarte_bson_element_t element_utc;
    zassert_equal(ASTARTE_RESULT_OK,
        astarte_bson_deserializer_next_element(doc, element_bool_true, &element_utc));

    zassert_equal(ASTARTE_BSON_TYPE_DATETIME, element_utc.type);
    zassert_mem_equal("element UTC datetime", element_utc.name, sizeof("element UTC datetime"));
    int64_t value_utc = astarte_bson_deserializer_element_to_datetime(element_utc);
    zassert_equal(1686304399422, value_utc);

    astarte_bson_element_t element_int32;
    zassert_equal(ASTARTE_RESULT_OK,
        astarte_bson_deserializer_next_element(doc, element_utc, &element_int32));

    zassert_equal(ASTARTE_BSON_TYPE_INT32, element_int32.type);
    zassert_mem_equal("element int32", element_int32.name, sizeof("element int32"));
    int32_t value_int32 = astarte_bson_deserializer_element_to_int32(element_int32);
    zassert_equal(10, value_int32);

    astarte_bson_element_t element_int64;
    zassert_equal(ASTARTE_RESULT_OK,
        astarte_bson_deserializer_next_element(doc, element_int32, &element_int64));

    zassert_equal(ASTARTE_BSON_TYPE_INT64, element_int64.type);
    zassert_mem_equal("element int64", element_int64.name, sizeof("element int64"));
    int64_t value_int64 = astarte_bson_deserializer_element_to_int64(element_int64);
    zassert_equal(17179869184, value_int64);

    astarte_bson_element_t element_non_existant;
    zassert_equal(ASTARTE_RESULT_NOT_FOUND,
        astarte_bson_deserializer_next_element(doc, element_int64, &element_non_existant));
}

ZTEST(astarte_device_sdk_bson, test_bson_deserializer_bson_document_lookup)
{
    astarte_bson_document_t doc = astarte_bson_deserializer_init_doc(complete_bson_document);
    zassert_equal(319, doc.size);

    // Lookup for the first element
    astarte_bson_element_t element_double;
    zassert_equal(ASTARTE_RESULT_OK,
        astarte_bson_deserializer_element_lookup(doc, "element double", &element_double));

    zassert_equal(ASTARTE_BSON_TYPE_DOUBLE, element_double.type);
    zassert_mem_equal("element double", element_double.name, sizeof("element double"));
    double value_double = astarte_bson_deserializer_element_to_double(element_double);
    zassert_within(0.01, 42.3, value_double);

    // Lookup for an element in the middle
    astarte_bson_element_t element_bool;
    zassert_equal(ASTARTE_RESULT_OK,
        astarte_bson_deserializer_element_lookup(doc, "element bool true", &element_bool));

    zassert_equal(ASTARTE_BSON_TYPE_BOOLEAN, element_bool.type);
    zassert_mem_equal("element bool true", element_bool.name, sizeof("element bool true"));
    bool value_bool = astarte_bson_deserializer_element_to_bool(element_bool);
    zassert_true(value_bool);

    // Lookup for the last element
    astarte_bson_element_t element_int64;
    zassert_equal(ASTARTE_RESULT_OK,
        astarte_bson_deserializer_element_lookup(doc, "element int64", &element_int64));

    zassert_equal(ASTARTE_BSON_TYPE_INT64, element_int64.type);
    zassert_mem_equal("element int64", element_int64.name, sizeof("element int64"));
    int64_t value_int64 = astarte_bson_deserializer_element_to_int64(element_int64);
    zassert_equal(17179869184, value_int64);

    // Lookup non existing element
    astarte_bson_element_t element_foo;
    zassert_equal(ASTARTE_RESULT_NOT_FOUND,
        astarte_bson_deserializer_element_lookup(doc, "foo", &element_foo));

    // Lookup long key that starts with valid key
    zassert_equal(ASTARTE_RESULT_NOT_FOUND,
        astarte_bson_deserializer_element_lookup(doc, "element string foo", &element_foo));
}
