/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>

#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/version.h>
#include <zephyr/ztest.h>

#if KERNEL_VERSION_NUMBER >= ZEPHYR_VERSION(4, 4, 0)
#include <zephyr/kvss/zms.h>
#else
#include <zephyr/fs/zms.h>
#endif

#include "alloc.h"
#include "generated_interfaces.h"
#include "test_storage_common.h"

static void *astarte_storage_test_setup(void)
{
    struct flash_pages_info fp_info;
    const struct device *device = PARTITION_DEVICE(astarte_partition);
    off_t offset = PARTITION_OFFSET(astarte_partition);
    zassert(device_is_ready(device), "Flash device is not ready.");
    zassert_equal(flash_get_page_info_by_offs(device, offset, &fp_info), 0, "Can't get page info.");

    struct astarte_device_sdk_storage_fixture *fixture
        = astarte_calloc(1, sizeof(struct astarte_device_sdk_storage_fixture));
    zassert_not_null(fixture, "Failed allocating test fixture");

    (void) introspection_init(&fixture->introspection);
    (void) introspection_add(
        &fixture->introspection, &org_astarteplatform_zephyr_examples_DeviceProperty);
    (void) introspection_add(
        &fixture->introspection, &org_astarteplatform_zephyr_examples_ServerProperty);
    fixture->flash_device = PARTITION_DEVICE(astarte_partition);
    fixture->flash_offset = PARTITION_OFFSET(astarte_partition);
    fixture->flash_sector_count = PARTITION_SIZE(astarte_partition) / fp_info.size;
    fixture->flash_sector_size = fp_info.size;
    k_mutex_init(&fixture->test_mutex);

    return fixture;
}

static void astarte_storage_test_before(void *f)
{
    struct astarte_device_sdk_storage_fixture *fixture
        = (struct astarte_device_sdk_storage_fixture *) f;

    k_mutex_lock(&fixture->test_mutex, K_FOREVER);

    struct zms_fs zms_fs;
    zms_fs.flash_device = fixture->flash_device;
    zms_fs.offset = fixture->flash_offset;
    zms_fs.sector_size = fixture->flash_sector_size;
    zms_fs.sector_count = fixture->flash_sector_count;

    zassert_equal(zms_mount(&zms_fs), 0, "ZMS mounting failed.");
    zassert_equal(zms_clear(&zms_fs), 0, "ZMS clear failed.");

    astarte_result_t ares = astarte_storage_init(&fixture->caching_handle);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Init failed: %s", astarte_result_to_name(ares));
}

static void astarte_storage_test_after(void *f)
{
    struct astarte_device_sdk_storage_fixture *fixture
        = (struct astarte_device_sdk_storage_fixture *) f;

    astarte_storage_destroy(&fixture->caching_handle);

    struct zms_fs zms_fs;
    zms_fs.flash_device = fixture->flash_device;
    zms_fs.offset = fixture->flash_offset;
    zms_fs.sector_size = fixture->flash_sector_size;
    zms_fs.sector_count = fixture->flash_sector_count;

    zassert_equal(zms_mount(&zms_fs), 0, "ZMS mounting failed.");
    zassert_equal(zms_clear(&zms_fs), 0, "ZMS clear failed.");

    k_mutex_unlock(&fixture->test_mutex);
}

static void astarte_storage_test_teardown(void *f)
{
    struct astarte_device_sdk_storage_fixture *fixture
        = (struct astarte_device_sdk_storage_fixture *) f;

    astarte_free(fixture);
}

bool astarte_data_is_equal(astarte_data_t first, astarte_data_t second)
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

ZTEST_SUITE(astarte_device_sdk_storage, NULL, astarte_storage_test_setup,
    astarte_storage_test_before, astarte_storage_test_after, astarte_storage_test_teardown);
