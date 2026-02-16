/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>

#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/ztest.h>

#include "astarte_device_sdk/data.h"
#include "astarte_device_sdk/result.h"

#include "device_caching.h"
#include "generated_interfaces.h"

struct astarte_device_sdk_device_caching_fixture
{
    const struct device *flash_device;
    introspection_t introspection;
    off_t flash_offset;
    uint16_t flash_sector_size;
    uint16_t flash_sector_count;
    struct k_mutex test_mutex;
    astarte_device_caching_t caching_handle;
};

static void *device_caching_test_setup(void)
{
    struct flash_pages_info fp_info;
    const struct device *device = FIXED_PARTITION_DEVICE(astarte_partition);
    off_t offset = FIXED_PARTITION_OFFSET(astarte_partition);
    zassert(device_is_ready(device), "Flash device is not ready.");
    zassert_equal(flash_get_page_info_by_offs(device, offset, &fp_info), 0, "Can't get page info.");

    struct astarte_device_sdk_device_caching_fixture *fixture
        = calloc(1, sizeof(struct astarte_device_sdk_device_caching_fixture));
    zassert_not_null(fixture, "Failed allocating test fixture");

    (void) introspection_init(&fixture->introspection);
    (void) introspection_add(
        &fixture->introspection, &org_astarteplatform_zephyr_examples_DeviceProperty);
    (void) introspection_add(
        &fixture->introspection, &org_astarteplatform_zephyr_examples_ServerProperty);
    fixture->flash_device = FIXED_PARTITION_DEVICE(astarte_partition);
    fixture->flash_offset = FIXED_PARTITION_OFFSET(astarte_partition);
    fixture->flash_sector_count = FIXED_PARTITION_SIZE(astarte_partition) / fp_info.size;
    fixture->flash_sector_size = fp_info.size;
    k_mutex_init(&fixture->test_mutex);

    return fixture;
}

static void device_caching_test_before(void *f)
{
    struct astarte_device_sdk_device_caching_fixture *fixture
        = (struct astarte_device_sdk_device_caching_fixture *) f;

    k_mutex_lock(&fixture->test_mutex, K_FOREVER);

    struct nvs_fs nvs_fs;
    nvs_fs.flash_device = fixture->flash_device;
    nvs_fs.offset = fixture->flash_offset;
    nvs_fs.sector_size = fixture->flash_sector_size;
    nvs_fs.sector_count = fixture->flash_sector_count;

    zassert_equal(nvs_mount(&nvs_fs), 0, "NVS mounting failed.");
    zassert_equal(nvs_clear(&nvs_fs), 0, "NVS clear failed.");

    astarte_result_t ares = astarte_device_caching_init(&fixture->caching_handle);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Init failed: %s", astarte_result_to_name(ares));
}

static void device_caching_test_after(void *f)
{
    struct astarte_device_sdk_device_caching_fixture *fixture
        = (struct astarte_device_sdk_device_caching_fixture *) f;

    astarte_device_caching_destroy(&fixture->caching_handle);

    struct nvs_fs nvs_fs;
    nvs_fs.flash_device = fixture->flash_device;
    nvs_fs.offset = fixture->flash_offset;
    nvs_fs.sector_size = fixture->flash_sector_size;
    nvs_fs.sector_count = fixture->flash_sector_count;

    zassert_equal(nvs_mount(&nvs_fs), 0, "NVS mounting failed.");
    zassert_equal(nvs_clear(&nvs_fs), 0, "NVS clear failed.");

    k_mutex_unlock(&fixture->test_mutex);
}

static void device_caching_test_teardown(void *f)
{
    struct astarte_device_sdk_device_caching_fixture *fixture
        = (struct astarte_device_sdk_device_caching_fixture *) f;

    free(fixture);
}

static bool astarte_data_is_equal(astarte_data_t first, astarte_data_t second)
{
    if (first.tag != second.tag) {
        return false;
    }
    switch (first.tag) {
        case ASTARTE_MAPPING_TYPE_BOOLEAN:
            if (first.data.boolean != second.data.boolean) {
                return false;
            }
            break;
        case ASTARTE_MAPPING_TYPE_DATETIME:
            if (first.data.datetime != second.data.datetime) {
                return false;
            }
            break;
        case ASTARTE_MAPPING_TYPE_DOUBLE:
            if (first.data.dbl != second.data.dbl) {
                return false;
            }
            break;
        case ASTARTE_MAPPING_TYPE_INTEGER:
            if (first.data.integer != second.data.integer) {
                return false;
            }
            break;
        case ASTARTE_MAPPING_TYPE_LONGINTEGER:
            if (first.data.longinteger != second.data.longinteger) {
                return false;
            }
            break;
        case ASTARTE_MAPPING_TYPE_STRING:
            if (strcmp(first.data.string, second.data.string) != 0) {
                return false;
            }
            break;
        case ASTARTE_MAPPING_TYPE_BINARYBLOB:
        case ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY:
        case ASTARTE_MAPPING_TYPE_BOOLEANARRAY:
        case ASTARTE_MAPPING_TYPE_DATETIMEARRAY:
        case ASTARTE_MAPPING_TYPE_DOUBLEARRAY:
        case ASTARTE_MAPPING_TYPE_INTEGERARRAY:
        case ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY:
        case ASTARTE_MAPPING_TYPE_STRINGARRAY:
            return false; // Implement when needed
            break;
        default:
            break;
    }
    return true;
}

ZTEST_SUITE(astarte_device_sdk_device_caching, NULL, device_caching_test_setup,
    device_caching_test_before, device_caching_test_after, device_caching_test_teardown); // NOLINT

ZTEST_F(astarte_device_sdk_device_caching, test_device_caching_synchronization) // NOLINT
{
    bool sync = false;
    astarte_result_t ares = ASTARTE_RESULT_OK;

    ares = astarte_device_caching_synchronization_get(&fixture->caching_handle, &sync);
    zassert_equal(ares, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(sync, false, "sync variable has been modified");

    sync = true;
    ares = astarte_device_caching_synchronization_get(&fixture->caching_handle, &sync);
    zassert_equal(ares, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(sync, true, "sync variable has been modified");

    sync = true;
    ares = astarte_device_caching_synchronization_set(&fixture->caching_handle, sync);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    sync = false;
    ares = astarte_device_caching_synchronization_get(&fixture->caching_handle, &sync);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(sync, true, "Sync variable not set correctly");
}

ZTEST_F(astarte_device_sdk_device_caching, test_device_caching_store_introspection) // NOLINT
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    const char intr_1_str[] = "interface1;interface2;interface3";
    const char intr_2_str[] = "interface2;interface3";
    const char intr_3_str[] = "interface1;interface2;interface3;interface4";

    ares = astarte_device_caching_introspection_check(
        &fixture->caching_handle, intr_1_str, ARRAY_SIZE(intr_1_str));
    zassert_equal(ares, ASTARTE_RESULT_DEVICE_CACHING_OUTDATED_INTROSPECTION, "Res:%s",
        astarte_result_to_name(ares));

    ares = astarte_device_caching_introspection_store(
        &fixture->caching_handle, intr_1_str, ARRAY_SIZE(intr_1_str));
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    ares = astarte_device_caching_introspection_check(
        &fixture->caching_handle, intr_1_str, ARRAY_SIZE(intr_1_str));
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    ares = astarte_device_caching_introspection_store(
        &fixture->caching_handle, intr_2_str, ARRAY_SIZE(intr_2_str));
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    ares = astarte_device_caching_introspection_check(
        &fixture->caching_handle, intr_1_str, ARRAY_SIZE(intr_1_str));
    zassert_equal(ares, ASTARTE_RESULT_DEVICE_CACHING_OUTDATED_INTROSPECTION, "Res:%s",
        astarte_result_to_name(ares));

    ares = astarte_device_caching_introspection_check(
        &fixture->caching_handle, intr_2_str, ARRAY_SIZE(intr_2_str));
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    ares = astarte_device_caching_introspection_store(
        &fixture->caching_handle, intr_3_str, ARRAY_SIZE(intr_3_str));
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    ares = astarte_device_caching_introspection_check(
        &fixture->caching_handle, intr_1_str, ARRAY_SIZE(intr_1_str));
    zassert_equal(ares, ASTARTE_RESULT_DEVICE_CACHING_OUTDATED_INTROSPECTION, "Res:%s",
        astarte_result_to_name(ares));

    ares = astarte_device_caching_introspection_check(
        &fixture->caching_handle, intr_2_str, ARRAY_SIZE(intr_2_str));
    zassert_equal(ares, ASTARTE_RESULT_DEVICE_CACHING_OUTDATED_INTROSPECTION, "Res:%s",
        astarte_result_to_name(ares));

    ares = astarte_device_caching_introspection_check(
        &fixture->caching_handle, intr_3_str, ARRAY_SIZE(intr_3_str));
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
}

struct property
{
    const char *interface_name;
    const char *path;
    uint32_t major;
    astarte_data_t data;
};

ZTEST_F(astarte_device_sdk_device_caching, test_device_caching_store_load_property) // NOLINT
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

    ares = astarte_device_caching_property_store(&fixture->caching_handle,
        property_1.interface_name, property_1.path, property_1.major, property_1.data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    ares = astarte_device_caching_property_store(&fixture->caching_handle,
        property_2.interface_name, property_2.path, property_2.major, property_2.data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    ares = astarte_device_caching_property_store(&fixture->caching_handle,
        property_3.interface_name, property_3.path, property_3.major, property_3.data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    ares = astarte_device_caching_property_store(&fixture->caching_handle,
        property_4.interface_name, property_4.path, property_4.major, property_4.data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    read_major = 0;
    read_data = (astarte_data_t) { 0 };
    ares = astarte_device_caching_property_load(&fixture->caching_handle, property_2.interface_name,
        property_2.path, (uint32_t *) &read_major, &read_data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(read_major, property_2.major, "Read major: %d", read_major);
    zassert_true(astarte_data_is_equal(property_2.data, read_data));

    astarte_device_caching_property_destroy_loaded(read_data);

    read_major = 0;
    read_data = (astarte_data_t) { 0 };
    ares = astarte_device_caching_property_load(&fixture->caching_handle, property_3.interface_name,
        property_3.path, (uint32_t *) &read_major, &read_data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(read_major, property_3.major, "Read major: %d", read_major);
    zassert_true(astarte_data_is_equal(property_3.data, read_data));

    astarte_device_caching_property_destroy_loaded(read_data);

    // The first property has been overwritten by the last one
    read_major = 0;
    read_data = (astarte_data_t) { 0 };
    ares = astarte_device_caching_property_load(&fixture->caching_handle, property_4.interface_name,
        property_4.path, (uint32_t *) &read_major, &read_data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(read_major, property_4.major, "Read major: %d", read_major);
    zassert_true(astarte_data_is_equal(property_4.data, read_data));

    astarte_device_caching_property_destroy_loaded(read_data);
}

ZTEST_F(astarte_device_sdk_device_caching, test_device_caching_iterate) // NOLINT
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

    ares = astarte_device_caching_property_store(&fixture->caching_handle,
        property_1.interface_name, property_1.path, property_1.major, property_1.data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    ares = astarte_device_caching_property_store(&fixture->caching_handle,
        property_2.interface_name, property_2.path, property_2.major, property_2.data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    ares = astarte_device_caching_property_store(&fixture->caching_handle,
        property_3.interface_name, property_3.path, property_3.major, property_3.data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    astarte_device_caching_property_iter_t iter = { 0 };
    ares = astarte_device_caching_property_iterator_new(&fixture->caching_handle, &iter);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    interface_name_size = 0U;
    path_size = 0U;
    ares = astarte_device_caching_property_iterator_get(
        &iter, NULL, &interface_name_size, NULL, &path_size);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(interface_name_size, strlen(property_3.interface_name) + 1,
        "Incorrect interface name size:%d", interface_name_size);
    zassert_equal(path_size, strlen(property_3.path) + 1, "Incorrect path size:%d", path_size);
    ares = astarte_device_caching_property_iterator_get(
        &iter, interface_name_buffer, &interface_name_size, path_buffer, &path_size);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(interface_name_size, strlen(property_3.interface_name) + 1,
        "Incorrect interface name size:%d", interface_name_size);
    zassert_equal(path_size, strlen(property_3.path) + 1, "Incorrect path size:%d", path_size);
    zassert_mem_equal(
        interface_name_buffer, property_3.interface_name, strlen(property_3.interface_name) + 1);
    zassert_mem_equal(path_buffer, property_3.path, strlen(property_3.path) + 1);

    ares = astarte_device_caching_property_iterator_next(&iter);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    interface_name_size = 0U;
    path_size = 0U;
    ares = astarte_device_caching_property_iterator_get(
        &iter, NULL, &interface_name_size, NULL, &path_size);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(interface_name_size, strlen(property_2.interface_name) + 1,
        "Incorrect interface name size:%d", interface_name_size);
    zassert_equal(path_size, strlen(property_2.path) + 1, "Incorrect path size:%d", path_size);
    memset(interface_name_buffer, '\0', ARRAY_SIZE(interface_name_buffer));
    memset(path_buffer, '\0', ARRAY_SIZE(path_buffer));
    ares = astarte_device_caching_property_iterator_get(
        &iter, interface_name_buffer, &interface_name_size, path_buffer, &path_size);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(interface_name_size, strlen(property_2.interface_name) + 1,
        "Incorrect interface name size:%d", interface_name_size);
    zassert_equal(path_size, strlen(property_2.path) + 1, "Incorrect path size:%d", path_size);
    zassert_mem_equal(
        interface_name_buffer, property_2.interface_name, strlen(property_2.interface_name) + 1);
    zassert_mem_equal(path_buffer, property_2.path, strlen(property_2.path) + 1);

    ares = astarte_device_caching_property_iterator_next(&iter);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    interface_name_size = 0U;
    path_size = 0U;
    ares = astarte_device_caching_property_iterator_get(
        &iter, NULL, &interface_name_size, NULL, &path_size);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(interface_name_size, strlen(property_1.interface_name) + 1,
        "Incorrect interface name size:%d", interface_name_size);
    zassert_equal(path_size, strlen(property_1.path) + 1, "Incorrect path size:%d", path_size);
    memset(interface_name_buffer, '\0', ARRAY_SIZE(interface_name_buffer));
    memset(path_buffer, '\0', ARRAY_SIZE(path_buffer));
    ares = astarte_device_caching_property_iterator_get(
        &iter, interface_name_buffer, &interface_name_size, path_buffer, &path_size);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(interface_name_size, strlen(property_1.interface_name) + 1,
        "Incorrect interface name size:%d", interface_name_size);
    zassert_equal(path_size, strlen(property_1.path) + 1, "Incorrect path size:%d", path_size);
    zassert_mem_equal(
        interface_name_buffer, property_1.interface_name, strlen(property_1.interface_name) + 1);
    zassert_mem_equal(path_buffer, property_1.path, strlen(property_1.path) + 1);

    ares = astarte_device_caching_property_iterator_next(&iter);
    zassert_equal(ares, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ares));
}

ZTEST_F(astarte_device_sdk_device_caching, test_device_caching_iterate_empty) // NOLINT
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    astarte_device_caching_property_iter_t iter = { 0 };
    ares = astarte_device_caching_property_iterator_new(&fixture->caching_handle, &iter);
    zassert_equal(ares, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ares));
}

ZTEST_F(astarte_device_sdk_device_caching, test_device_caching_delete) // NOLINT
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
    ares = astarte_device_caching_property_store(&fixture->caching_handle,
        property_1.interface_name, property_1.path, property_1.major, property_1.data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    ares = astarte_device_caching_property_store(&fixture->caching_handle,
        property_2.interface_name, property_2.path, property_2.major, property_2.data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    ares = astarte_device_caching_property_store(&fixture->caching_handle,
        property_3.interface_name, property_3.path, property_3.major, property_3.data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    ares = astarte_device_caching_property_store(&fixture->caching_handle,
        property_4.interface_name, property_4.path, property_4.major, property_4.data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    ares = astarte_device_caching_property_store(&fixture->caching_handle,
        property_5.interface_name, property_5.path, property_5.major, property_5.data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    ares = astarte_device_caching_property_store(&fixture->caching_handle,
        property_6.interface_name, property_6.path, property_6.major, property_6.data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    // Delete a stored property
    ares = astarte_device_caching_property_delete(
        &fixture->caching_handle, property_2.interface_name, property_2.path);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    ares = astarte_device_caching_property_delete(
        &fixture->caching_handle, property_1.interface_name, property_1.path);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    // Loop over all the stored properties
    astarte_device_caching_property_iter_t iter = { 0 };
    ares = astarte_device_caching_property_iterator_new(&fixture->caching_handle, &iter);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    interface_name_size = ARRAY_SIZE(interface_name_buffer);
    path_size = ARRAY_SIZE(path_buffer);
    ares = astarte_device_caching_property_iterator_get(
        &iter, interface_name_buffer, &interface_name_size, path_buffer, &path_size);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(interface_name_size, strlen(property_4.interface_name) + 1,
        "Incorrect interface name size:%d", interface_name_size);
    zassert_equal(path_size, strlen(property_4.path) + 1, "Incorrect path size:%d", path_size);
    zassert_mem_equal(
        interface_name_buffer, property_4.interface_name, strlen(property_4.interface_name) + 1);
    zassert_mem_equal(path_buffer, property_4.path, strlen(property_4.path) + 1);

    ares = astarte_device_caching_property_iterator_next(&iter);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    interface_name_size = ARRAY_SIZE(interface_name_buffer);
    path_size = ARRAY_SIZE(path_buffer);
    ares = astarte_device_caching_property_iterator_get(
        &iter, interface_name_buffer, &interface_name_size, path_buffer, &path_size);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(interface_name_size, strlen(property_3.interface_name) + 1,
        "Incorrect interface name size:%d", interface_name_size);
    zassert_equal(path_size, strlen(property_3.path) + 1, "Incorrect path size:%d", path_size);
    zassert_mem_equal(
        interface_name_buffer, property_3.interface_name, strlen(property_3.interface_name) + 1);
    zassert_mem_equal(path_buffer, property_3.path, strlen(property_3.path) + 1);

    // Delete a stored property
    ares = astarte_device_caching_property_delete(
        &fixture->caching_handle, property_5.interface_name, property_5.path);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    ares = astarte_device_caching_property_iterator_next(&iter);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    interface_name_size = ARRAY_SIZE(interface_name_buffer);
    path_size = ARRAY_SIZE(path_buffer);
    ares = astarte_device_caching_property_iterator_get(
        &iter, interface_name_buffer, &interface_name_size, path_buffer, &path_size);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(interface_name_size, strlen(property_6.interface_name) + 1,
        "Incorrect interface name size:%d", interface_name_size);
    zassert_equal(path_size, strlen(property_6.path) + 1, "Incorrect path size:%d", path_size);
    zassert_mem_equal(
        interface_name_buffer, property_6.interface_name, strlen(property_6.interface_name) + 1);
    zassert_mem_equal(path_buffer, property_6.path, strlen(property_6.path) + 1);

    ares = astarte_device_caching_property_iterator_next(&iter);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    interface_name_size = ARRAY_SIZE(interface_name_buffer);
    path_size = ARRAY_SIZE(path_buffer);
    ares = astarte_device_caching_property_iterator_get(
        &iter, interface_name_buffer, &interface_name_size, path_buffer, &path_size);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(interface_name_size, strlen(property_4.interface_name) + 1,
        "Incorrect interface name size:%d", interface_name_size);
    zassert_equal(path_size, strlen(property_4.path) + 1, "Incorrect path size:%d", path_size);
    zassert_mem_equal(
        interface_name_buffer, property_4.interface_name, strlen(property_4.interface_name) + 1);
    zassert_mem_equal(path_buffer, property_4.path, strlen(property_4.path) + 1);

    ares = astarte_device_caching_property_iterator_next(&iter);
    zassert_equal(ares, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ares));

    // Loop over all the stored properties
    iter = (astarte_device_caching_property_iter_t) { 0 };
    ares = astarte_device_caching_property_iterator_new(&fixture->caching_handle, &iter);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    interface_name_size = ARRAY_SIZE(interface_name_buffer);
    path_size = ARRAY_SIZE(path_buffer);
    ares = astarte_device_caching_property_iterator_get(
        &iter, interface_name_buffer, &interface_name_size, path_buffer, &path_size);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(interface_name_size, strlen(property_3.interface_name) + 1,
        "Incorrect interface name size:%d", interface_name_size);
    zassert_equal(path_size, strlen(property_3.path) + 1, "Incorrect path size:%d", path_size);
    zassert_mem_equal(
        interface_name_buffer, property_3.interface_name, strlen(property_3.interface_name) + 1);
    zassert_mem_equal(path_buffer, property_3.path, strlen(property_3.path) + 1);

    ares = astarte_device_caching_property_iterator_next(&iter);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    interface_name_size = ARRAY_SIZE(interface_name_buffer);
    path_size = ARRAY_SIZE(path_buffer);
    ares = astarte_device_caching_property_iterator_get(
        &iter, interface_name_buffer, &interface_name_size, path_buffer, &path_size);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(interface_name_size, strlen(property_6.interface_name) + 1,
        "Incorrect interface name size:%d", interface_name_size);
    zassert_equal(path_size, strlen(property_6.path) + 1, "Incorrect path size:%d", path_size);
    zassert_mem_equal(
        interface_name_buffer, property_6.interface_name, strlen(property_6.interface_name) + 1);
    zassert_mem_equal(path_buffer, property_6.path, strlen(property_6.path) + 1);

    ares = astarte_device_caching_property_iterator_next(&iter);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    interface_name_size = ARRAY_SIZE(interface_name_buffer);
    path_size = ARRAY_SIZE(path_buffer);
    ares = astarte_device_caching_property_iterator_get(
        &iter, interface_name_buffer, &interface_name_size, path_buffer, &path_size);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(interface_name_size, strlen(property_4.interface_name) + 1,
        "Incorrect interface name size:%d", interface_name_size);
    zassert_equal(path_size, strlen(property_4.path) + 1, "Incorrect path size:%d", path_size);
    zassert_mem_equal(
        interface_name_buffer, property_4.interface_name, strlen(property_4.interface_name) + 1);
    zassert_mem_equal(path_buffer, property_4.path, strlen(property_4.path) + 1);

    ares = astarte_device_caching_property_iterator_next(&iter);
    zassert_equal(ares, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ares));
}

ZTEST_F(astarte_device_sdk_device_caching, test_device_caching_get_properties_string) // NOLINT
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
        = "org.astarteplatform.zephyr.examples.DeviceProperty/11/double_endpoint;"
          "org.astarteplatform.zephyr.examples.DeviceProperty/45/double_endpoint;"
          "org.astarteplatform.zephyr.examples.DeviceProperty/24/boolean_endpoint;"
          "org.astarteplatform.zephyr.examples.DeviceProperty/12/integer_endpoint";
    char read_properties_string[ARRAY_SIZE(properties_string)] = { 0 };

    // Store a bunch of properties
    ares = astarte_device_caching_property_store(&fixture->caching_handle,
        property_1.interface_name, property_1.path, property_1.major, property_1.data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    ares = astarte_device_caching_property_store(&fixture->caching_handle,
        property_2.interface_name, property_2.path, property_2.major, property_2.data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    ares = astarte_device_caching_property_store(&fixture->caching_handle,
        property_3.interface_name, property_3.path, property_3.major, property_3.data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    ares = astarte_device_caching_property_store(&fixture->caching_handle,
        property_4.interface_name, property_4.path, property_4.major, property_4.data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    ares = astarte_device_caching_property_store(&fixture->caching_handle,
        property_5.interface_name, property_5.path, property_5.major, property_5.data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    ares = astarte_device_caching_property_store(&fixture->caching_handle,
        property_6.interface_name, property_6.path, property_6.major, property_6.data);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    size_t output_size = 0U;
    ares = astarte_device_caching_property_get_device_string(
        &fixture->caching_handle, &fixture->introspection, NULL, &output_size);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(output_size, ARRAY_SIZE(properties_string), "Read size:%d", output_size);

    output_size = ARRAY_SIZE(properties_string);
    ares = astarte_device_caching_property_get_device_string(
        &fixture->caching_handle, &fixture->introspection, read_properties_string, &output_size);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(output_size, ARRAY_SIZE(properties_string), "Read size:%d", output_size);
    zassert_mem_equal(properties_string, read_properties_string, ARRAY_SIZE(properties_string),
        "'%s' '%s'", properties_string, read_properties_string);
}
