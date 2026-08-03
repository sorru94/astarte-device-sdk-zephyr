/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "generated_interfaces.h"
#include "storage/prop.h"
#include "test_storage_common.h"

struct property
{
    const char *interface_name;
    const char *path;
    uint32_t major;
    astarte_data_t data;
};

ZTEST_F(astarte_device_sdk_storage, test_device_astarte_storage_store_load_property)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    int32_t read_major = 0;
    astarte_data_t read_data = { 0 };

    struct property property_1 = {
        .interface_name = "first.interface",
        .path = "/first/path/to/property",
        .major = 13,
        .data = astarte_data_from_integer(11),
    };
    struct property property_2 = {
        .interface_name = "second.interface",
        .path = "/third/path/to/property",
        .major = 45,
        .data = astarte_data_from_boolean(false),
    };
    struct property property_3 = {
        .interface_name = "first.interface",
        .path = "/second/path/to/property",
        .major = 12,
        .data = astarte_data_from_double(23.4),
    };
    struct property property_4 = {
        .interface_name = "first.interface",
        .path = "/first/path/to/property",
        .major = 12,
        .data = astarte_data_from_longinteger(55),
    };

    ares = astarte_storage_property_store(&fixture->caching_handle, property_1.interface_name,
        property_1.path, property_1.major, property_1.data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    ares = astarte_storage_property_store(&fixture->caching_handle, property_2.interface_name,
        property_2.path, property_2.major, property_2.data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    ares = astarte_storage_property_store(&fixture->caching_handle, property_3.interface_name,
        property_3.path, property_3.major, property_3.data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    ares = astarte_storage_property_store(&fixture->caching_handle, property_4.interface_name,
        property_4.path, property_4.major, property_4.data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    read_major = 0;
    read_data = (astarte_data_t){ 0 };
    ares = astarte_storage_property_load(&fixture->caching_handle, property_2.interface_name,
        property_2.path, (uint32_t *) &read_major, &read_data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(read_major, property_2.major, "Read major: %d", read_major);
    zassert_true(astarte_data_is_equal(property_2.data, read_data));

    astarte_storage_property_destroy_loaded(read_data);

    read_major = 0;
    read_data = (astarte_data_t){ 0 };
    ares = astarte_storage_property_load(&fixture->caching_handle, property_3.interface_name,
        property_3.path, (uint32_t *) &read_major, &read_data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(read_major, property_3.major, "Read major: %d", read_major);
    zassert_true(astarte_data_is_equal(property_3.data, read_data));

    astarte_storage_property_destroy_loaded(read_data);

    read_major = 0;
    read_data = (astarte_data_t){ 0 };
    ares = astarte_storage_property_load(&fixture->caching_handle, property_4.interface_name,
        property_4.path, (uint32_t *) &read_major, &read_data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(read_major, property_4.major, "Read major: %d", read_major);
    zassert_true(astarte_data_is_equal(property_4.data, read_data));

    astarte_storage_property_destroy_loaded(read_data);
}

ZTEST_F(astarte_device_sdk_storage, test_device_astarte_storage_iterate)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    char interface_name_buffer[100] = { 0 };
    char path_buffer[100] = { 0 };
    size_t interface_name_size = 0U;
    size_t path_size = 0U;

    struct property property_1 = {
        .interface_name = "first.interface",
        .path = "/first/path/to/property",
        .major = 12,
        .data = astarte_data_from_integer(11),
    };
    struct property property_2 = {
        .interface_name = "second.interface",
        .path = "/third/path/to/property",
        .major = 45,
        .data = astarte_data_from_boolean(false),
    };
    struct property property_3 = {
        .interface_name = "first.interface",
        .path = "/second/path/to/property",
        .major = 12,
        .data = astarte_data_from_double(23.4),
    };

    ares = astarte_storage_property_store(&fixture->caching_handle, property_1.interface_name,
        property_1.path, property_1.major, property_1.data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    ares = astarte_storage_property_store(&fixture->caching_handle, property_2.interface_name,
        property_2.path, property_2.major, property_2.data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    ares = astarte_storage_property_store(&fixture->caching_handle, property_3.interface_name,
        property_3.path, property_3.major, property_3.data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    astarte_storage_property_iter_t iter = { 0 };
    ares = astarte_storage_property_iterator_new(&fixture->caching_handle, &iter);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    interface_name_size = 0U;
    path_size = 0U;
    ares = astarte_storage_property_iterator_get(
        &iter, NULL, &interface_name_size, NULL, &path_size);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(interface_name_size, strlen(property_1.interface_name) + 1,
        "Incorrect interface name size:%d", interface_name_size);
    zassert_equal(path_size, strlen(property_1.path) + 1, "Incorrect path size:%d", path_size);
    ares = astarte_storage_property_iterator_get(
        &iter, interface_name_buffer, &interface_name_size, path_buffer, &path_size);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(interface_name_size, strlen(property_1.interface_name) + 1,
        "Incorrect interface name size:%d", interface_name_size);
    zassert_equal(path_size, strlen(property_1.path) + 1, "Incorrect path size:%d", path_size);
    zassert_mem_equal(
        interface_name_buffer, property_1.interface_name, strlen(property_1.interface_name) + 1);
    zassert_mem_equal(path_buffer, property_1.path, strlen(property_1.path) + 1);

    ares = astarte_storage_property_iterator_next(&iter);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    interface_name_size = 0U;
    path_size = 0U;
    ares = astarte_storage_property_iterator_get(
        &iter, NULL, &interface_name_size, NULL, &path_size);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(interface_name_size, strlen(property_2.interface_name) + 1,
        "Incorrect interface name size:%d", interface_name_size);
    zassert_equal(path_size, strlen(property_2.path) + 1, "Incorrect path size:%d", path_size);
    memset(interface_name_buffer, '\0', ARRAY_SIZE(interface_name_buffer));
    memset(path_buffer, '\0', ARRAY_SIZE(path_buffer));
    ares = astarte_storage_property_iterator_get(
        &iter, interface_name_buffer, &interface_name_size, path_buffer, &path_size);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(interface_name_size, strlen(property_2.interface_name) + 1,
        "Incorrect interface name size:%d", interface_name_size);
    zassert_equal(path_size, strlen(property_2.path) + 1, "Incorrect path size:%d", path_size);
    zassert_mem_equal(
        interface_name_buffer, property_2.interface_name, strlen(property_2.interface_name) + 1);
    zassert_mem_equal(path_buffer, property_2.path, strlen(property_2.path) + 1);

    ares = astarte_storage_property_iterator_next(&iter);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    interface_name_size = 0U;
    path_size = 0U;
    ares = astarte_storage_property_iterator_get(
        &iter, NULL, &interface_name_size, NULL, &path_size);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(interface_name_size, strlen(property_3.interface_name) + 1,
        "Incorrect interface name size:%d", interface_name_size);
    zassert_equal(path_size, strlen(property_3.path) + 1, "Incorrect path size:%d", path_size);
    memset(interface_name_buffer, '\0', ARRAY_SIZE(interface_name_buffer));
    memset(path_buffer, '\0', ARRAY_SIZE(path_buffer));
    ares = astarte_storage_property_iterator_get(
        &iter, interface_name_buffer, &interface_name_size, path_buffer, &path_size);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(interface_name_size, strlen(property_3.interface_name) + 1,
        "Incorrect interface name size:%d", interface_name_size);
    zassert_equal(path_size, strlen(property_3.path) + 1, "Incorrect path size:%d", path_size);
    zassert_mem_equal(
        interface_name_buffer, property_3.interface_name, strlen(property_3.interface_name) + 1);
    zassert_mem_equal(path_buffer, property_3.path, strlen(property_3.path) + 1);

    ares = astarte_storage_property_iterator_next(&iter);
    zassert_equal(ares, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ares));
}

ZTEST_F(astarte_device_sdk_storage, test_device_astarte_storage_iterate_empty)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    astarte_storage_property_iter_t iter = { 0 };
    ares = astarte_storage_property_iterator_new(&fixture->caching_handle, &iter);
    zassert_equal(ares, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ares));
}

ZTEST_F(astarte_device_sdk_storage, test_device_astarte_storage_delete)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    char interface_name_buffer[100] = { 0 };
    char path_buffer[100] = { 0 };
    size_t interface_name_size = 0U;
    size_t path_size = 0U;

    struct property property_1 = {
        .interface_name = "first.interface",
        .path = "/first/path/to/property",
        .major = 12,
        .data = astarte_data_from_integer(11),
    };
    struct property property_2 = {
        .interface_name = "second.interface",
        .path = "/third/path/to/property",
        .major = 45,
        .data = astarte_data_from_boolean(false),
    };
    struct property property_3 = {
        .interface_name = "first.interface",
        .path = "/second/path/to/property",
        .major = 12,
        .data = astarte_data_from_double(23.4),
    };
    struct property property_4 = {
        .interface_name = "third.interface",
        .path = "/fourth/path/to/property",
        .major = 33,
        .data = astarte_data_from_double(11.5),
    };
    struct property property_5 = {
        .interface_name = "fourth.interface",
        .path = "/fifth/path/to/property",
        .major = 33,
        .data = astarte_data_from_boolean(true),
    };
    struct property property_6 = {
        .interface_name = "fourth.interface",
        .path = "/sixth/path/to/property",
        .major = 33,
        .data = astarte_data_from_boolean(false),
    };

    // Store a bunch of properties
    ares = astarte_storage_property_store(&fixture->caching_handle, property_1.interface_name,
        property_1.path, property_1.major, property_1.data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    ares = astarte_storage_property_store(&fixture->caching_handle, property_2.interface_name,
        property_2.path, property_2.major, property_2.data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    ares = astarte_storage_property_store(&fixture->caching_handle, property_3.interface_name,
        property_3.path, property_3.major, property_3.data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    ares = astarte_storage_property_store(&fixture->caching_handle, property_4.interface_name,
        property_4.path, property_4.major, property_4.data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    ares = astarte_storage_property_store(&fixture->caching_handle, property_5.interface_name,
        property_5.path, property_5.major, property_5.data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    ares = astarte_storage_property_store(&fixture->caching_handle, property_6.interface_name,
        property_6.path, property_6.major, property_6.data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    // Delete a stored property using key-based deletion before we start iterating
    ares = astarte_storage_property_delete(
        &fixture->caching_handle, property_2.interface_name, property_2.path);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    ares = astarte_storage_property_delete(
        &fixture->caching_handle, property_1.interface_name, property_1.path);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    // Loop over all the stored properties (remaining expected: 3 -> 4 -> 5 -> 6)
    astarte_storage_property_iter_t iter = { 0 };
    ares = astarte_storage_property_iterator_new(&fixture->caching_handle, &iter);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    interface_name_size = ARRAY_SIZE(interface_name_buffer);
    path_size = ARRAY_SIZE(path_buffer);
    ares = astarte_storage_property_iterator_get(
        &iter, interface_name_buffer, &interface_name_size, path_buffer, &path_size);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(interface_name_size, strlen(property_3.interface_name) + 1,
        "Incorrect interface name size:%d", interface_name_size);
    zassert_equal(path_size, strlen(property_3.path) + 1, "Incorrect path size:%d", path_size);
    zassert_mem_equal(
        interface_name_buffer, property_3.interface_name, strlen(property_3.interface_name) + 1);
    zassert_mem_equal(path_buffer, property_3.path, strlen(property_3.path) + 1);

    // Delete a stored property using the safe mid-iteration delete call.
    ares = astarte_storage_property_iterator_delete(&iter);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    ares = astarte_storage_property_iterator_next(&iter);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    interface_name_size = ARRAY_SIZE(interface_name_buffer);
    path_size = ARRAY_SIZE(path_buffer);
    ares = astarte_storage_property_iterator_get(
        &iter, interface_name_buffer, &interface_name_size, path_buffer, &path_size);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(interface_name_size, strlen(property_4.interface_name) + 1,
        "Incorrect interface name size:%d", interface_name_size);
    zassert_equal(path_size, strlen(property_4.path) + 1, "Incorrect path size:%d", path_size);
    zassert_mem_equal(
        interface_name_buffer, property_4.interface_name, strlen(property_4.interface_name) + 1);
    zassert_mem_equal(path_buffer, property_4.path, strlen(property_4.path) + 1);

    ares = astarte_storage_property_iterator_next(&iter);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    interface_name_size = ARRAY_SIZE(interface_name_buffer);
    path_size = ARRAY_SIZE(path_buffer);
    ares = astarte_storage_property_iterator_get(
        &iter, interface_name_buffer, &interface_name_size, path_buffer, &path_size);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(interface_name_size, strlen(property_5.interface_name) + 1,
        "Incorrect interface name size:%d", interface_name_size);
    zassert_equal(path_size, strlen(property_5.path) + 1, "Incorrect path size:%d", path_size);
    zassert_mem_equal(
        interface_name_buffer, property_5.interface_name, strlen(property_5.interface_name) + 1);
    zassert_mem_equal(path_buffer, property_5.path, strlen(property_5.path) + 1);

    ares = astarte_storage_property_iterator_next(&iter);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    interface_name_size = ARRAY_SIZE(interface_name_buffer);
    path_size = ARRAY_SIZE(path_buffer);
    ares = astarte_storage_property_iterator_get(
        &iter, interface_name_buffer, &interface_name_size, path_buffer, &path_size);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(interface_name_size, strlen(property_6.interface_name) + 1,
        "Incorrect interface name size:%d", interface_name_size);
    zassert_equal(path_size, strlen(property_6.path) + 1, "Incorrect path size:%d", path_size);
    zassert_mem_equal(
        interface_name_buffer, property_6.interface_name, strlen(property_6.interface_name) + 1);
    zassert_mem_equal(path_buffer, property_6.path, strlen(property_6.path) + 1);

    ares = astarte_storage_property_iterator_next(&iter);
    zassert_equal(ares, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ares));

    // Loop over all the stored properties (remaining expected: 4 -> 5 -> 6)
    iter = (astarte_storage_property_iter_t){ 0 };
    ares = astarte_storage_property_iterator_new(&fixture->caching_handle, &iter);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    interface_name_size = ARRAY_SIZE(interface_name_buffer);
    path_size = ARRAY_SIZE(path_buffer);
    ares = astarte_storage_property_iterator_get(
        &iter, interface_name_buffer, &interface_name_size, path_buffer, &path_size);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(interface_name_size, strlen(property_4.interface_name) + 1,
        "Incorrect interface name size:%d", interface_name_size);
    zassert_equal(path_size, strlen(property_4.path) + 1, "Incorrect path size:%d", path_size);
    zassert_mem_equal(
        interface_name_buffer, property_4.interface_name, strlen(property_4.interface_name) + 1);
    zassert_mem_equal(path_buffer, property_4.path, strlen(property_4.path) + 1);

    ares = astarte_storage_property_iterator_next(&iter);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    interface_name_size = ARRAY_SIZE(interface_name_buffer);
    path_size = ARRAY_SIZE(path_buffer);
    ares = astarte_storage_property_iterator_get(
        &iter, interface_name_buffer, &interface_name_size, path_buffer, &path_size);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(interface_name_size, strlen(property_5.interface_name) + 1,
        "Incorrect interface name size:%d", interface_name_size);
    zassert_equal(path_size, strlen(property_5.path) + 1, "Incorrect path size:%d", path_size);
    zassert_mem_equal(
        interface_name_buffer, property_5.interface_name, strlen(property_5.interface_name) + 1);
    zassert_mem_equal(path_buffer, property_5.path, strlen(property_5.path) + 1);

    ares = astarte_storage_property_iterator_next(&iter);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    interface_name_size = ARRAY_SIZE(interface_name_buffer);
    path_size = ARRAY_SIZE(path_buffer);
    ares = astarte_storage_property_iterator_get(
        &iter, interface_name_buffer, &interface_name_size, path_buffer, &path_size);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(interface_name_size, strlen(property_6.interface_name) + 1,
        "Incorrect interface name size:%d", interface_name_size);
    zassert_equal(path_size, strlen(property_6.path) + 1, "Incorrect path size:%d", path_size);
    zassert_mem_equal(
        interface_name_buffer, property_6.interface_name, strlen(property_6.interface_name) + 1);
    zassert_mem_equal(path_buffer, property_6.path, strlen(property_6.path) + 1);

    ares = astarte_storage_property_iterator_next(&iter);
    zassert_equal(ares, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ares));
}

ZTEST_F(astarte_device_sdk_storage, test_device_astarte_storage_get_properties_string)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    struct property property_1 = {
        .interface_name = org_astarteplatform_zephyr_examples_DeviceProperty.name,
        .path = "/12/integer_endpoint",
        .major = 12,
        .data = astarte_data_from_integer(11),
    };
    struct property property_2 = {
        .interface_name = org_astarteplatform_zephyr_examples_DeviceProperty.name,
        .path = "/24/boolean_endpoint",
        .major = 45,
        .data = astarte_data_from_boolean(false),
    };
    struct property property_3 = {
        .interface_name = org_astarteplatform_zephyr_examples_DeviceProperty.name,
        .path = "/45/double_endpoint",
        .major = 12,
        .data = astarte_data_from_double(23.4),
    };
    struct property property_4 = {
        .interface_name = org_astarteplatform_zephyr_examples_DeviceProperty.name,
        .path = "/11/double_endpoint",
        .major = 33,
        .data = astarte_data_from_double(11.5),
    };
    struct property property_5 = {
        .interface_name = org_astarteplatform_zephyr_examples_ServerProperty.name,
        .path = "/11/boolean_endpoint",
        .major = 33,
        .data = astarte_data_from_boolean(true),
    };
    struct property property_6 = {
        .interface_name = org_astarteplatform_zephyr_examples_ServerProperty.name,
        .path = "/10/boolean_endpoint",
        .major = 33,
        .data = astarte_data_from_boolean(false),
    };

    const char properties_string[]
        = "org.astarteplatform.zephyr.examples.DeviceProperty/12/integer_endpoint;"
          "org.astarteplatform.zephyr.examples.DeviceProperty/24/boolean_endpoint;"
          "org.astarteplatform.zephyr.examples.DeviceProperty/45/double_endpoint;"
          "org.astarteplatform.zephyr.examples.DeviceProperty/11/double_endpoint";
    char read_properties_string[ARRAY_SIZE(properties_string)] = { 0 };

    // Store a bunch of properties
    ares = astarte_storage_property_store(&fixture->caching_handle, property_1.interface_name,
        property_1.path, property_1.major, property_1.data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    ares = astarte_storage_property_store(&fixture->caching_handle, property_2.interface_name,
        property_2.path, property_2.major, property_2.data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    ares = astarte_storage_property_store(&fixture->caching_handle, property_3.interface_name,
        property_3.path, property_3.major, property_3.data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    ares = astarte_storage_property_store(&fixture->caching_handle, property_4.interface_name,
        property_4.path, property_4.major, property_4.data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    ares = astarte_storage_property_store(&fixture->caching_handle, property_5.interface_name,
        property_5.path, property_5.major, property_5.data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    ares = astarte_storage_property_store(&fixture->caching_handle, property_6.interface_name,
        property_6.path, property_6.major, property_6.data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    size_t output_size = 0U;
    ares = astarte_storage_property_get_device_string(
        &fixture->caching_handle, &fixture->introspection, NULL, &output_size);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(output_size, ARRAY_SIZE(properties_string), "Read size:%d", output_size);

    output_size = ARRAY_SIZE(properties_string);
    ares = astarte_storage_property_get_device_string(
        &fixture->caching_handle, &fixture->introspection, read_properties_string, &output_size);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(output_size, ARRAY_SIZE(properties_string), "Read size:%d", output_size);
    zassert_mem_equal(properties_string, read_properties_string, ARRAY_SIZE(properties_string),
        "'%s' '%s'", properties_string, read_properties_string);
}
