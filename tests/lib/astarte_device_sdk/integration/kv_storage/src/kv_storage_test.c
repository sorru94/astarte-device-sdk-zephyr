/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>

#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/ztest.h>

#include "astarte_device_sdk/result.h"

#include "kv_storage.h"

/**
 * NOTE: The tests in this file have been placed al in a single ZTEST to avoid issues with
 * concurrent thread writing/reading from the same flash partition.
 */

#define NVS_PARTITION kv_partition
#define NVS_PARTITION_DEVICE FIXED_PARTITION_DEVICE(NVS_PARTITION)
#define NVS_PARTITION_OFFSET FIXED_PARTITION_OFFSET(NVS_PARTITION)
#define NVS_PARTITION_SIZE FIXED_PARTITION_SIZE(NVS_PARTITION)

struct astarte_device_sdk_kv_storage_fixture
{
    const struct device *flash_device;
    off_t flash_offset;
    uint16_t flash_sector_size;
    uint16_t flash_sector_count;
    struct k_mutex test_mutex;
};

static void *kv_storage_test_setup(void)
{
    struct flash_pages_info fp_info;
    const struct device *device = NVS_PARTITION_DEVICE;
    off_t offset = NVS_PARTITION_OFFSET;
    zassert(device_is_ready(device), "Flash device is not ready.");
    zassert_equal(flash_get_page_info_by_offs(device, offset, &fp_info), 0, "Can't get page info.");

    struct astarte_device_sdk_kv_storage_fixture *fixture
        = calloc(1, sizeof(struct astarte_device_sdk_kv_storage_fixture));
    zassert_not_null(fixture, "Failed allocating test fixture");

    fixture->flash_device = NVS_PARTITION_DEVICE;
    fixture->flash_offset = NVS_PARTITION_OFFSET;
    fixture->flash_sector_count = NVS_PARTITION_SIZE / fp_info.size;
    fixture->flash_sector_size = fp_info.size;
    k_mutex_init(&fixture->test_mutex);

    return fixture;
}

static void kv_storage_test_before(void *f)
{
    struct astarte_device_sdk_kv_storage_fixture *fixture
        = (struct astarte_device_sdk_kv_storage_fixture *) f;

    k_mutex_lock(&fixture->test_mutex, K_FOREVER);

    struct nvs_fs nvs_fs;
    nvs_fs.flash_device = fixture->flash_device;
    nvs_fs.offset = fixture->flash_offset;
    nvs_fs.sector_size = fixture->flash_sector_size;
    nvs_fs.sector_count = fixture->flash_sector_count;

    zassert_equal(nvs_mount(&nvs_fs), 0, "NVS mounting failed.");
    zassert_equal(nvs_clear(&nvs_fs), 0, "NVS clear failed.");
}

static void kv_storage_test_after(void *f)
{
    struct astarte_device_sdk_kv_storage_fixture *fixture
        = (struct astarte_device_sdk_kv_storage_fixture *) f;

    struct nvs_fs nvs_fs;
    nvs_fs.flash_device = fixture->flash_device;
    nvs_fs.offset = fixture->flash_offset;
    nvs_fs.sector_size = fixture->flash_sector_size;
    nvs_fs.sector_count = fixture->flash_sector_count;

    zassert_equal(nvs_mount(&nvs_fs), 0, "NVS mounting failed.");
    zassert_equal(nvs_clear(&nvs_fs), 0, "NVS clear failed.");

    k_mutex_unlock(&fixture->test_mutex);
}

static void kv_storage_test_teardown(void *f)
{
    struct astarte_device_sdk_kv_storage_fixture *fixture
        = (struct astarte_device_sdk_kv_storage_fixture *) f;

    free(fixture);
}

ZTEST_SUITE(astarte_device_sdk_kv_storage, NULL, kv_storage_test_setup, kv_storage_test_before,
    kv_storage_test_after, kv_storage_test_teardown); // NOLINT

const char key1[] = "first key";
char res_key1[ARRAY_SIZE(key1)] = { 0 };
const char value1[] = "first value";
char res_value1[ARRAY_SIZE(value1)] = { 0 };
const char key2[] = "second key";
char res_key2[ARRAY_SIZE(key2)] = { 0 };
const char value2[] = "second value";
char res_value2[ARRAY_SIZE(value2)] = { 0 };
const char key3[] = "third key";
char res_key3[ARRAY_SIZE(key3)] = { 0 };
const char value3[] = "third value";
char res_value3[ARRAY_SIZE(value3)] = { 0 };
const char key4[] = "fourth key";
char res_key4[ARRAY_SIZE(key4)] = { 0 };
const char value4[] = "fourth value";
char res_value4[ARRAY_SIZE(value4)] = { 0 };
const char key5[] = "fifth key";
char res_key5[ARRAY_SIZE(key5)] = { 0 };
const char value5[] = "fifth value";
char res_value5[ARRAY_SIZE(value5)] = { 0 };

ZTEST_F(astarte_device_sdk_kv_storage, test_kv_storage_store_and_find) // NOLINT
{
    size_t value_size = 0U;

    // Initialize storage driver
    astarte_kv_storage_t kv_storage = { 0 };
    const char namespace[] = "simple namespace";

    astarte_kv_storage_cfg_t storage_cfg = {
        .flash_device = fixture->flash_device,
        .flash_offset = fixture->flash_offset,
        .flash_sector_count = fixture->flash_sector_count,
        .flash_sector_size = fixture->flash_sector_size,
    };

    astarte_result_t ret = astarte_kv_storage_init(storage_cfg, namespace, &kv_storage);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    // Insert some key-value pairs
    ret = astarte_kv_storage_insert(&kv_storage, key1, value1, ARRAY_SIZE(value1));
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    ret = astarte_kv_storage_insert(&kv_storage, key2, value2, ARRAY_SIZE(value2));
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    ret = astarte_kv_storage_insert(&kv_storage, key3, value3, ARRAY_SIZE(value3));
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    ret = astarte_kv_storage_insert(&kv_storage, key4, value4, ARRAY_SIZE(value4));
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    // Check content of storage
    value_size = ARRAY_SIZE(res_value2);
    ret = astarte_kv_storage_find(&kv_storage, key2, res_value2, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value2), "Incorrect value size:%s", value_size);
    zassert_mem_equal(res_value2, value2, ARRAY_SIZE(value2), "Mismatched values.");
    value_size = ARRAY_SIZE(res_value1);
    ret = astarte_kv_storage_find(&kv_storage, key1, res_value1, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value1), "Incorrect value size:%s", value_size);
    zassert_mem_equal(res_value1, value1, ARRAY_SIZE(value1), "Mismatched values.");
    value_size = 0;
    ret = astarte_kv_storage_find(&kv_storage, key5, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));
    value_size = ARRAY_SIZE(res_value3);
    ret = astarte_kv_storage_find(&kv_storage, key3, res_value3, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value3), "Incorrect value size:%s", value_size);
    zassert_mem_equal(res_value3, value3, ARRAY_SIZE(value3), "Mismatched values.");
    value_size = ARRAY_SIZE(res_value4);
    ret = astarte_kv_storage_find(&kv_storage, key4, res_value4, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value4), "Incorrect value size:%s", value_size);
    zassert_mem_equal(res_value4, value4, ARRAY_SIZE(value4), "Mismatched values.");

    // Delete some key-value pairs
    ret = astarte_kv_storage_delete(&kv_storage, key2);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    // Check content of storage
    value_size = 0;
    ret = astarte_kv_storage_find(&kv_storage, key2, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));
    value_size = ARRAY_SIZE(res_value1);
    ret = astarte_kv_storage_find(&kv_storage, key1, res_value1, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value1), "Incorrect value size:%s", value_size);
    zassert_mem_equal(res_value1, value1, ARRAY_SIZE(value1), "Mismatched values.");
    value_size = 0;
    ret = astarte_kv_storage_find(&kv_storage, key5, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));
    value_size = ARRAY_SIZE(res_value3);
    ret = astarte_kv_storage_find(&kv_storage, key3, res_value3, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value3), "Incorrect value size:%s", value_size);
    zassert_mem_equal(res_value3, value3, ARRAY_SIZE(value3), "Mismatched values.");
    value_size = ARRAY_SIZE(res_value4);
    ret = astarte_kv_storage_find(&kv_storage, key4, res_value4, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value4), "Incorrect value size:%s", value_size);
    zassert_mem_equal(res_value4, value4, ARRAY_SIZE(value4), "Mismatched values.");

    // Delete some key-value pairs
    ret = astarte_kv_storage_delete(&kv_storage, key4);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    ret = astarte_kv_storage_delete(&kv_storage, key1);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    // Check content of storage
    value_size = 0;
    ret = astarte_kv_storage_find(&kv_storage, key2, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));
    value_size = 0;
    ret = astarte_kv_storage_find(&kv_storage, key1, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));
    value_size = 0;
    ret = astarte_kv_storage_find(&kv_storage, key5, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));
    value_size = ARRAY_SIZE(res_value3);
    ret = astarte_kv_storage_find(&kv_storage, key3, res_value3, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value3), "Incorrect value size:%s", value_size);
    zassert_mem_equal(res_value3, value3, ARRAY_SIZE(value3), "Mismatched values.");
    value_size = 0;
    ret = astarte_kv_storage_find(&kv_storage, key4, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));

    // Delete some key-value pairs
    ret = astarte_kv_storage_delete(&kv_storage, key3);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    // Check content of storage
    value_size = 0;
    ret = astarte_kv_storage_find(&kv_storage, key2, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));
    value_size = 0;
    ret = astarte_kv_storage_find(&kv_storage, key1, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));
    value_size = 0;
    ret = astarte_kv_storage_find(&kv_storage, key5, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));
    value_size = 0;
    ret = astarte_kv_storage_find(&kv_storage, key3, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));
    value_size = 0;
    ret = astarte_kv_storage_find(&kv_storage, key4, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));
}

ZTEST_F(astarte_device_sdk_kv_storage, test_kv_storage_read_sizes) // NOLINT
{
    size_t value_size = 0U;

    // Initialize storage driver
    astarte_kv_storage_t kv_storage = { 0 };
    const char namespace[] = "simple namespace";

    astarte_kv_storage_cfg_t storage_cfg = {
        .flash_device = fixture->flash_device,
        .flash_offset = fixture->flash_offset,
        .flash_sector_count = fixture->flash_sector_count,
        .flash_sector_size = fixture->flash_sector_size,
    };

    astarte_result_t ret = astarte_kv_storage_init(storage_cfg, namespace, &kv_storage);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    // Insert some key-value pairs
    ret = astarte_kv_storage_insert(&kv_storage, key1, value1, ARRAY_SIZE(value1));
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    ret = astarte_kv_storage_insert(&kv_storage, key2, value2, ARRAY_SIZE(value2));
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    ret = astarte_kv_storage_insert(&kv_storage, key3, value3, ARRAY_SIZE(value3));
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    ret = astarte_kv_storage_insert(&kv_storage, key4, value4, ARRAY_SIZE(value4));
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    // Check content of storage
    value_size = 0;
    ret = astarte_kv_storage_find(&kv_storage, key2, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value2), "Incorrect value size:%s", value_size);
    ret = astarte_kv_storage_find(&kv_storage, key2, res_value2, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value2), "Incorrect value size:%s", value_size);
    zassert_mem_equal(res_value2, value2, ARRAY_SIZE(value2), "Mismatched values.");
    value_size = 0;
    ret = astarte_kv_storage_find(&kv_storage, key1, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value1), "Incorrect value size:%s", value_size);
    ret = astarte_kv_storage_find(&kv_storage, key1, res_value1, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value1), "Incorrect value size:%s", value_size);
    zassert_mem_equal(res_value1, value1, ARRAY_SIZE(value1), "Mismatched values.");
    value_size = 0;
    ret = astarte_kv_storage_find(&kv_storage, key5, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));
    value_size = 0;
    ret = astarte_kv_storage_find(&kv_storage, key3, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value3), "Incorrect value size:%s", value_size);
    ret = astarte_kv_storage_find(&kv_storage, key3, res_value3, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value3), "Incorrect value size:%s", value_size);
    zassert_mem_equal(res_value3, value3, ARRAY_SIZE(value3), "Mismatched values.");
    value_size = 0;
    ret = astarte_kv_storage_find(&kv_storage, key4, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value4), "Incorrect value size:%s", value_size);
    ret = astarte_kv_storage_find(&kv_storage, key4, res_value4, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value4), "Incorrect value size:%s", value_size);
    zassert_mem_equal(res_value4, value4, ARRAY_SIZE(value4), "Mismatched values.");
}

ZTEST_F(astarte_device_sdk_kv_storage, test_kv_storage_overwrite) // NOLINT
{
    size_t value_size = 0U;

    // Initialize storage driver
    astarte_kv_storage_t kv_storage = { 0 };
    const char namespace[] = "simple namespace";

    astarte_kv_storage_cfg_t storage_cfg = {
        .flash_device = fixture->flash_device,
        .flash_offset = fixture->flash_offset,
        .flash_sector_count = fixture->flash_sector_count,
        .flash_sector_size = fixture->flash_sector_size,
    };

    astarte_result_t ret = astarte_kv_storage_init(storage_cfg, namespace, &kv_storage);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    // Insert some key-value pairs
    ret = astarte_kv_storage_insert(&kv_storage, key1, value1, ARRAY_SIZE(value1));
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    ret = astarte_kv_storage_insert(&kv_storage, key3, value3, ARRAY_SIZE(value3));
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    ret = astarte_kv_storage_insert(&kv_storage, key4, value4, ARRAY_SIZE(value4));
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    // Check content of storage
    value_size = 0;
    ret = astarte_kv_storage_find(&kv_storage, key2, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));
    value_size = ARRAY_SIZE(res_value1);
    ret = astarte_kv_storage_find(&kv_storage, key1, res_value1, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value1), "Incorrect value size:%s", value_size);
    zassert_mem_equal(res_value1, value1, ARRAY_SIZE(value1), "Mismatched values.");
    value_size = 0;
    ret = astarte_kv_storage_find(&kv_storage, key5, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));
    value_size = ARRAY_SIZE(res_value3);
    ret = astarte_kv_storage_find(&kv_storage, key3, res_value3, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value3), "Incorrect value size:%s", value_size);
    zassert_mem_equal(res_value3, value3, ARRAY_SIZE(value3), "Mismatched values.");
    value_size = ARRAY_SIZE(res_value4);
    ret = astarte_kv_storage_find(&kv_storage, key4, res_value4, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value4), "Incorrect value size:%s", value_size);
    zassert_mem_equal(res_value4, value4, ARRAY_SIZE(value4), "Mismatched values.");

    // Overwrite a storage entry
    ret = astarte_kv_storage_insert(&kv_storage, key1, value5, ARRAY_SIZE(value5));
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    // Check content of storage
    value_size = 0;
    ret = astarte_kv_storage_find(&kv_storage, key2, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));
    value_size = ARRAY_SIZE(res_value5);
    ret = astarte_kv_storage_find(&kv_storage, key1, res_value5, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value5), "Incorrect value size:%s", value_size);
    zassert_mem_equal(res_value5, value5, ARRAY_SIZE(value5), "Mismatched values.");
    value_size = 0;
    ret = astarte_kv_storage_find(&kv_storage, key5, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));
    value_size = ARRAY_SIZE(res_value3);
    ret = astarte_kv_storage_find(&kv_storage, key3, res_value3, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value3), "Incorrect value size:%s", value_size);
    zassert_mem_equal(res_value3, value3, ARRAY_SIZE(value3), "Mismatched values.");
    value_size = ARRAY_SIZE(res_value4);
    ret = astarte_kv_storage_find(&kv_storage, key4, res_value4, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value4), "Incorrect value size:%s", value_size);
    zassert_mem_equal(res_value4, value4, ARRAY_SIZE(value4), "Mismatched values.");
}

ZTEST_F(astarte_device_sdk_kv_storage, test_kv_storage_iteration) // NOLINT
{
    size_t value_size = 0U;

    // Initialize storage driver
    astarte_kv_storage_t kv_storage = { 0 };
    const char namespace[] = "simple namespace";

    astarte_kv_storage_cfg_t storage_cfg = {
        .flash_device = fixture->flash_device,
        .flash_offset = fixture->flash_offset,
        .flash_sector_count = fixture->flash_sector_count,
        .flash_sector_size = fixture->flash_sector_size,
    };

    astarte_result_t ret = astarte_kv_storage_init(storage_cfg, namespace, &kv_storage);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    // Insert some key-value pairs
    ret = astarte_kv_storage_insert(&kv_storage, key1, value1, ARRAY_SIZE(value1));
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    ret = astarte_kv_storage_insert(&kv_storage, key3, value3, ARRAY_SIZE(value3));
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    ret = astarte_kv_storage_insert(&kv_storage, key4, value4, ARRAY_SIZE(value4));
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    // Check content of storage
    value_size = 0;
    ret = astarte_kv_storage_find(&kv_storage, key2, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));
    value_size = ARRAY_SIZE(res_value1);
    ret = astarte_kv_storage_find(&kv_storage, key1, res_value1, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value1), "Incorrect value size:%s", value_size);
    zassert_mem_equal(res_value1, value1, ARRAY_SIZE(value1), "Mismatched values.");
    value_size = 0;
    ret = astarte_kv_storage_find(&kv_storage, key5, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));
    value_size = ARRAY_SIZE(res_value3);
    ret = astarte_kv_storage_find(&kv_storage, key3, res_value3, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value3), "Incorrect value size:%s", value_size);
    zassert_mem_equal(res_value3, value3, ARRAY_SIZE(value3), "Mismatched values.");
    value_size = ARRAY_SIZE(res_value4);
    ret = astarte_kv_storage_find(&kv_storage, key4, res_value4, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value4), "Incorrect value size:%s", value_size);
    zassert_mem_equal(res_value4, value4, ARRAY_SIZE(value4), "Mismatched values.");

    // Iterate over storage
    astarte_kv_storage_iter_t iter = { 0 };
    ret = astarte_kv_storage_iterator_init(&kv_storage, &iter);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    value_size = 0U;
    ret = astarte_kv_storage_iterator_get(&iter, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(key4), "Incorrect value size:%s", value_size);
    ret = astarte_kv_storage_iterator_get(&iter, res_key4, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(key4), "Incorrect value size:%s", value_size);
    zassert_mem_equal(res_key4, key4, ARRAY_SIZE(key4), "Mismatched values.");

    ret = astarte_kv_storage_iterator_next(&iter);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    value_size = 0U;
    ret = astarte_kv_storage_iterator_get(&iter, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(key3), "Incorrect value size:%s", value_size);
    ret = astarte_kv_storage_iterator_get(&iter, res_key3, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(key3), "Incorrect value size:%s", value_size);
    zassert_mem_equal(res_key3, key3, ARRAY_SIZE(key3), "Mismatched values.");

    ret = astarte_kv_storage_iterator_next(&iter);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    value_size = 0U;
    ret = astarte_kv_storage_iterator_get(&iter, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(key1), "Incorrect value size:%s", value_size);
    ret = astarte_kv_storage_iterator_get(&iter, res_key1, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(key1), "Incorrect value size:%s", value_size);
    zassert_mem_equal(res_key1, key1, ARRAY_SIZE(key1), "Mismatched values.");

    ret = astarte_kv_storage_iterator_next(&iter);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));
}

ZTEST_F(astarte_device_sdk_kv_storage, test_kv_storage_iteration_empty_storage) // NOLINT
{
    // Initialize storage driver
    astarte_kv_storage_t kv_storage = { 0 };
    const char namespace[] = "simple namespace";

    astarte_kv_storage_cfg_t storage_cfg = {
        .flash_device = fixture->flash_device,
        .flash_offset = fixture->flash_offset,
        .flash_sector_count = fixture->flash_sector_count,
        .flash_sector_size = fixture->flash_sector_size,
    };

    astarte_result_t ret = astarte_kv_storage_init(storage_cfg, namespace, &kv_storage);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    // Iterate over storage
    astarte_kv_storage_iter_t iter = { 0 };
    ret = astarte_kv_storage_iterator_init(&kv_storage, &iter);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));
}

ZTEST_F(astarte_device_sdk_kv_storage, test_kv_storage_multiple_namespaces) // NOLINT
{
    astarte_result_t ret = ASTARTE_RESULT_OK;
    size_t value_size = 0U;

    // Initialize first storage driver
    astarte_kv_storage_t kv_storage_1 = { 0 };
    const char namespace_1[] = "first namespace";
    astarte_kv_storage_cfg_t storage_1_cfg = {
        .flash_device = fixture->flash_device,
        .flash_offset = fixture->flash_offset,
        .flash_sector_count = fixture->flash_sector_count,
        .flash_sector_size = fixture->flash_sector_size,
    };
    ret = astarte_kv_storage_init(storage_1_cfg, namespace_1, &kv_storage_1);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    // Initialize second storage driver
    astarte_kv_storage_t kv_storage_2 = { 0 };
    const char namespace_2[] = "second namespace";
    astarte_kv_storage_cfg_t storage_2_cfg = {
        .flash_device = fixture->flash_device,
        .flash_offset = fixture->flash_offset,
        .flash_sector_count = fixture->flash_sector_count,
        .flash_sector_size = fixture->flash_sector_size,
    };
    ret = astarte_kv_storage_init(storage_2_cfg, namespace_2, &kv_storage_2);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    // Insert some key-value pairs with multiple namespaces
    ret = astarte_kv_storage_insert(&kv_storage_1, key1, value1, ARRAY_SIZE(value1));
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    ret = astarte_kv_storage_insert(&kv_storage_2, key2, value2, ARRAY_SIZE(value2));
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    ret = astarte_kv_storage_insert(&kv_storage_2, key3, value3, ARRAY_SIZE(value3));
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    ret = astarte_kv_storage_insert(&kv_storage_1, key4, value4, ARRAY_SIZE(value4));
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    ret = astarte_kv_storage_insert(&kv_storage_2, key5, value5, ARRAY_SIZE(value5));
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    // Iterate over first storage
    astarte_kv_storage_iter_t iter_1 = { 0 };
    ret = astarte_kv_storage_iterator_init(&kv_storage_1, &iter_1);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    value_size = ARRAY_SIZE(res_key4);
    ret = astarte_kv_storage_iterator_get(&iter_1, res_key4, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(key4), "Incorrect value size:%s", value_size);
    zassert_mem_equal(res_key4, key4, ARRAY_SIZE(key4), "Mismatched values", res_key4);

    ret = astarte_kv_storage_iterator_next(&iter_1);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    value_size = ARRAY_SIZE(res_key1);
    ret = astarte_kv_storage_iterator_get(&iter_1, res_key1, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(key1), "Incorrect value size:%s", value_size);
    zassert_mem_equal(res_key1, key1, ARRAY_SIZE(key1), "Mismatched values", res_key1);

    ret = astarte_kv_storage_iterator_next(&iter_1);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));

    // Iterate over second storage
    astarte_kv_storage_iter_t iter_2 = { 0 };
    ret = astarte_kv_storage_iterator_init(&kv_storage_2, &iter_2);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    value_size = ARRAY_SIZE(res_key5);
    ret = astarte_kv_storage_iterator_get(&iter_2, res_key5, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(key5), "Incorrect value size:%s", value_size);
    zassert_mem_equal(res_key5, key5, ARRAY_SIZE(key5), "Mismatched values", res_key5);

    ret = astarte_kv_storage_iterator_next(&iter_2);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    value_size = ARRAY_SIZE(res_key3);
    ret = astarte_kv_storage_iterator_get(&iter_2, res_key3, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(key3), "Incorrect value size:%s", value_size);
    zassert_mem_equal(res_key3, key3, ARRAY_SIZE(key3), "Mismatched values", res_key3);

    ret = astarte_kv_storage_iterator_next(&iter_2);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    value_size = ARRAY_SIZE(res_key2);
    ret = astarte_kv_storage_iterator_get(&iter_2, res_key2, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(key2), "Incorrect value size:%s", value_size);
    zassert_mem_equal(res_key2, key2, ARRAY_SIZE(key2), "Mismatched values", res_key2);

    ret = astarte_kv_storage_iterator_next(&iter_2);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));
}
