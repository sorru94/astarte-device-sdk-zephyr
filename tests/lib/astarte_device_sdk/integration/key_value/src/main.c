/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/crc.h>
#include <zephyr/ztest.h>

#include "astarte_device_sdk/result.h"

#include "key_value/core.h"
#include "key_value/entry.h"
#include "key_value/entry_hash.h"
#include "key_value/entry_list.h"

#define NVS_PARTITION key_value_partition
#define NVS_PARTITION_DEVICE PARTITION_DEVICE(NVS_PARTITION)
#define NVS_PARTITION_OFFSET PARTITION_OFFSET(NVS_PARTITION)
#define NVS_PARTITION_SIZE PARTITION_SIZE(NVS_PARTITION)

struct astarte_device_sdk_key_value_fixture
{
    const struct device *flash_device;
    off_t flash_offset;
    uint16_t flash_sector_size;
    uint16_t flash_sector_count;
    uint64_t flash_partition_size;
    struct k_mutex test_mutex;
};

static void *astarte_storage_key_value_test_setup(void)
{
    struct flash_pages_info fp_info;
    const struct device *device = NVS_PARTITION_DEVICE;
    off_t offset = NVS_PARTITION_OFFSET;
    zassert(device_is_ready(device), "Flash device is not ready.");
    zassert_equal(flash_get_page_info_by_offs(device, offset, &fp_info), 0, "Can't get page info.");

    struct astarte_device_sdk_key_value_fixture *fixture
        = calloc(1, sizeof(struct astarte_device_sdk_key_value_fixture));
    zassert_not_null(fixture, "Failed allocating test fixture");

    fixture->flash_device = NVS_PARTITION_DEVICE;
    fixture->flash_offset = NVS_PARTITION_OFFSET;
    fixture->flash_sector_count = NVS_PARTITION_SIZE / fp_info.size;
    fixture->flash_sector_size = fp_info.size;
    fixture->flash_partition_size = NVS_PARTITION_SIZE;
    k_mutex_init(&fixture->test_mutex);

    return fixture;
}

static void astarte_storage_key_value_test_before(void *f)
{
    struct astarte_device_sdk_key_value_fixture *fixture
        = (struct astarte_device_sdk_key_value_fixture *) f;

    k_mutex_lock(&fixture->test_mutex, K_FOREVER);

    struct nvs_fs nvs_fs = { 0 };
    nvs_fs.flash_device = fixture->flash_device;
    nvs_fs.offset = fixture->flash_offset;
    nvs_fs.sector_size = fixture->flash_sector_size;
    nvs_fs.sector_count = fixture->flash_sector_count;

    zassert_equal(nvs_mount(&nvs_fs), 0, "NVS mounting failed.");
    zassert_equal(nvs_clear(&nvs_fs), 0, "NVS clear failed.");
}

static void astarte_storage_key_value_test_after(void *f)
{
    struct astarte_device_sdk_key_value_fixture *fixture
        = (struct astarte_device_sdk_key_value_fixture *) f;

    struct nvs_fs nvs_fs = { 0 };
    nvs_fs.flash_device = fixture->flash_device;
    nvs_fs.offset = fixture->flash_offset;
    nvs_fs.sector_size = fixture->flash_sector_size;
    nvs_fs.sector_count = fixture->flash_sector_count;

    zassert_equal(nvs_mount(&nvs_fs), 0, "NVS mounting failed.");
    zassert_equal(nvs_clear(&nvs_fs), 0, "NVS clear failed.");

    k_mutex_unlock(&fixture->test_mutex);
}

static void astarte_storage_key_value_test_teardown(void *f)
{
    struct astarte_device_sdk_key_value_fixture *fixture
        = (struct astarte_device_sdk_key_value_fixture *) f;

    free(fixture);
}

ZTEST_SUITE(astarte_device_sdk_key_value, NULL, astarte_storage_key_value_test_setup,
    astarte_storage_key_value_test_before, astarte_storage_key_value_test_after,
    astarte_storage_key_value_test_teardown);

// Helper to statically validate that the hardcoded keys actually collide
static void validate_collision(
    const char *namespace, const char *k1, const char *k2, const char *k3)
{
    uint16_t h1 = astarte_key_value_entry_hash_generate(namespace, k1);
    uint16_t h2 = astarte_key_value_entry_hash_generate(namespace, k2);
    uint16_t h3 = astarte_key_value_entry_hash_generate(namespace, k3);

    zassert_equal(h1, h2, "Collision validation failed: %s (%u) != %s (%u)", k1, h1, k2, h2);
    zassert_equal(h2, h3, "Collision validation failed: %s (%u) != %s (%u)", k2, h2, k3, h3);
}

ZTEST_F(astarte_device_sdk_key_value, test_key_value_hash_collision_probing)
{
    astarte_key_value_t key_value = { 0 };
    struct nvs_fs nvs_fs = { 0 };
    const char namespace[] = "collision_ns";

    astarte_key_value_cfg_t cfg = {
        .flash_device = fixture->flash_device,
        .flash_offset = fixture->flash_offset,
        .flash_partition_size = fixture->flash_partition_size,
    };

    zassert_equal(astarte_key_value_open(cfg, &nvs_fs), ASTARTE_RESULT_OK);
    zassert_equal(astarte_key_value_new(&nvs_fs, namespace, &key_value), ASTARTE_RESULT_OK);

    // Hardcoded keys known to collide at Target Hash: 23088
    const char *k1 = "key_1079";
    const char *k2 = "key_4703";
    const char *k3 = "key_21000";

    // Validate the hardcoded keys still collide in this environment
    validate_collision(namespace, k1, k2, k3);

    const char *v1 = "val1", *v2 = "val2", *v3 = "val3";

    // Insert colliding keys
    zassert_equal(astarte_key_value_insert(&key_value, k1, v1, strlen(v1) + 1), ASTARTE_RESULT_OK);
    zassert_equal(astarte_key_value_insert(&key_value, k2, v2, strlen(v2) + 1), ASTARTE_RESULT_OK);
    zassert_equal(astarte_key_value_insert(&key_value, k3, v3, strlen(v3) + 1), ASTARTE_RESULT_OK);

    // Verify all can be found correctly despite collisions
    char buf[16];
    size_t sz = sizeof(buf);

    zassert_equal(astarte_key_value_find(&key_value, k2, buf, &sz), ASTARTE_RESULT_OK);
    zassert_mem_equal(buf, v2, strlen(v2) + 1, "Probed key 2 mismatch");

    sz = sizeof(buf);
    zassert_equal(astarte_key_value_find(&key_value, k3, buf, &sz), ASTARTE_RESULT_OK);
    zassert_mem_equal(buf, v3, strlen(v3) + 1, "Probed key 3 mismatch");

    astarte_key_value_destroy(&key_value);
}

// Note: Depending on flash size, this can take a long time.
// Consider mocking ASTARTE_KEY_VALUE_ENTRY_MAX_USABLE_ID if execution time is an issue.
ZTEST_F(astarte_device_sdk_key_value, test_key_value_storage_exhaustion)
{
    astarte_key_value_t key_value = { 0 };
    struct nvs_fs nvs_fs = { 0 };

    astarte_key_value_cfg_t cfg = {
        .flash_device = fixture->flash_device,
        .flash_offset = fixture->flash_offset,
        .flash_partition_size = fixture->flash_partition_size,
    };

    zassert_equal(astarte_key_value_open(cfg, &nvs_fs), ASTARTE_RESULT_OK);
    zassert_equal(astarte_key_value_new(&nvs_fs, "exhaust_ns", &key_value), ASTARTE_RESULT_OK);

    char key[16];
    char val[] = "v";
    astarte_result_t res = ASTARTE_RESULT_OK;

    // Fill until NVS or our logic returns FULL/ERROR
    for (uint32_t i = 0; i < UINT16_MAX * 2; i++) {
        snprintf(key, sizeof(key), "k%d", i);
        res = astarte_key_value_insert(&key_value, key, val, sizeof(val));
        if (res != ASTARTE_RESULT_OK) {
            break;
        }
    }

    // It should eventually fail either by NVS full or our KV storage full
    zassert_not_equal(res, ASTARTE_RESULT_OK, "Storage should have exhausted");
    zassert_true(res == ASTARTE_RESULT_KEY_VALUE_FULL || res == ASTARTE_RESULT_NVS_ERROR,
        "Unexpected error on exhaustion: %s", astarte_result_to_name(res));

    astarte_key_value_destroy(&key_value);
}

ZTEST_F(astarte_device_sdk_key_value, test_key_value_linked_list_head_tail)
{
    astarte_key_value_t key_value = { 0 };
    struct nvs_fs nvs_fs = { 0 };

    astarte_key_value_cfg_t cfg = {
        .flash_device = fixture->flash_device,
        .flash_offset = fixture->flash_offset,
        .flash_partition_size = fixture->flash_partition_size,
    };

    zassert_equal(astarte_key_value_open(cfg, &nvs_fs), ASTARTE_RESULT_OK);
    zassert_equal(astarte_key_value_new(&nvs_fs, "ll_ns", &key_value), ASTARTE_RESULT_OK);

    uint16_t head_id, tail_id;

    // Initially empty
    zassert_equal(astarte_key_value_entry_list_read_head_and_tail_ids(&nvs_fs, &head_id, &tail_id),
        ASTARTE_RESULT_OK);
    zassert_equal(head_id, ASTARTE_KEY_VALUE_ENTRY_NULL_ID, "Empty head should be NULL");

    // Insert 1
    astarte_key_value_insert(&key_value, "k1", "v1", 3);
    astarte_key_value_entry_list_read_head_and_tail_ids(&nvs_fs, &head_id, &tail_id);
    zassert_not_equal(head_id, ASTARTE_KEY_VALUE_ENTRY_NULL_ID, "Head should be set");
    zassert_equal(head_id, tail_id, "Head and tail should be identical for 1 element");

    // Insert 2
    astarte_key_value_insert(&key_value, "k2", "v2", 3);
    uint16_t old_head = head_id;
    astarte_key_value_entry_list_read_head_and_tail_ids(&nvs_fs, &head_id, &tail_id);
    zassert_equal(head_id, old_head, "Head should remain the first element");
    zassert_not_equal(head_id, tail_id, "Tail should now be different");

    // Delete head
    astarte_key_value_delete(&key_value, "k1");
    astarte_key_value_entry_list_read_head_and_tail_ids(&nvs_fs, &head_id, &tail_id);
    zassert_equal(head_id, tail_id, "After deleting head, 1 element remains (head == tail)");

    astarte_key_value_destroy(&key_value);
}

ZTEST_F(astarte_device_sdk_key_value, test_key_value_deletion_shift_back)
{
    astarte_key_value_t key_value = { 0 };
    struct nvs_fs nvs_fs = { 0 };
    const char ns[] = "shift_ns";

    astarte_key_value_cfg_t cfg = {
        .flash_device = fixture->flash_device,
        .flash_offset = fixture->flash_offset,
        .flash_partition_size = fixture->flash_partition_size,
    };

    zassert_equal(astarte_key_value_open(cfg, &nvs_fs), ASTARTE_RESULT_OK);
    zassert_equal(astarte_key_value_new(&nvs_fs, ns, &key_value), ASTARTE_RESULT_OK);

    // Hardcoded keys known to collide at Target Hash: 9533
    const char *k1 = "key_1029";
    const char *k2 = "key_4753";
    const char *k3 = "key_11804";

    // Validate the hardcoded keys still collide in this environment
    validate_collision(ns, k1, k2, k3);

    astarte_key_value_insert(&key_value, k1, "v1", 3);
    astarte_key_value_insert(&key_value, k2, "v2", 3);
    astarte_key_value_insert(&key_value, k3, "v3", 3);

    // Delete the first colliding key to create a hole
    zassert_equal(astarte_key_value_delete(&key_value, k1), ASTARTE_RESULT_OK);

    // Verify shift-back works by looking up k3 and k2
    char buf[16];
    size_t sz = sizeof(buf);
    zassert_equal(astarte_key_value_find(&key_value, k3, buf, &sz), ASTARTE_RESULT_OK,
        "Shift-back broke probing for k3");
    zassert_equal(astarte_key_value_find(&key_value, k2, buf, &sz), ASTARTE_RESULT_OK,
        "Shift-back broke probing for k2");

    astarte_key_value_destroy(&key_value);
}

ZTEST_F(astarte_device_sdk_key_value, test_key_value_iterator_safe_deletion)
{
    astarte_key_value_t key_value = { 0 };
    struct nvs_fs nvs_fs = { 0 };

    astarte_key_value_cfg_t cfg = {
        .flash_device = fixture->flash_device,
        .flash_offset = fixture->flash_offset,
        .flash_partition_size = fixture->flash_partition_size,
    };

    astarte_key_value_open(cfg, &nvs_fs);
    astarte_key_value_new(&nvs_fs, "iter_del_ns", &key_value);

    astarte_key_value_insert(&key_value, "A", "1", 2);
    astarte_key_value_insert(&key_value, "B", "2", 2);
    astarte_key_value_insert(&key_value, "C", "3", 2);

    astarte_key_value_iter_t iter = { 0 };
    zassert_equal(astarte_key_value_iterator_init(&key_value, &iter), ASTARTE_RESULT_OK, "");

    int deleted_count = 0;
    astarte_result_t res = ASTARTE_RESULT_OK;

    // Iterate and delete every element
    while (res == ASTARTE_RESULT_OK) {
        zassert_equal(
            astarte_key_value_iterator_delete(&iter), ASTARTE_RESULT_OK, "Iterator delete failed");
        deleted_count++;
        res = astarte_key_value_iterator_next(&iter);
    }

    zassert_equal(res, ASTARTE_RESULT_NOT_FOUND, "Iteration did not finish cleanly");
    zassert_equal(deleted_count, 3, "Failed to delete all elements via iterator");

    // Ensure storage is actually empty
    size_t sz = 16;
    char buf[16];
    zassert_equal(astarte_key_value_find(&key_value, "B", buf, &sz), ASTARTE_RESULT_NOT_FOUND,
        "Element survived iterator deletion");

    astarte_key_value_destroy(&key_value);
}

ZTEST_F(astarte_device_sdk_key_value, test_key_value_invalid_buffer_sizes)
{
    astarte_key_value_t key_value = { 0 };
    struct nvs_fs nvs_fs = { 0 };

    astarte_key_value_cfg_t cfg = {
        .flash_device = fixture->flash_device,
        .flash_offset = fixture->flash_offset,
        .flash_partition_size = fixture->flash_partition_size,
    };

    astarte_key_value_open(cfg, &nvs_fs);
    astarte_key_value_new(&nvs_fs, "err_ns", &key_value);

    const char *long_val = "this_is_a_long_value";
    astarte_key_value_insert(&key_value, "k1", long_val, strlen(long_val) + 1);

    // Provide a buffer size explicitly smaller than stored payload
    char small_buf[5];
    size_t sz = sizeof(small_buf);

    astarte_result_t res = astarte_key_value_find(&key_value, "k1", small_buf, &sz);
    zassert_equal(res, ASTARTE_RESULT_INVALID_PARAM, "Driver failed to catch undersized buffer: %s",
        astarte_result_to_name(res));

    astarte_key_value_destroy(&key_value);
}

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

ZTEST_F(astarte_device_sdk_key_value, test_astarte_storage_key_value_store_and_find)
{
    size_t value_size = 0U;

    // Initialize storage driver
    astarte_key_value_t kv_storage = { 0 };
    struct nvs_fs nvs_fs = { 0 };
    const char namespace[] = "simple namespace";

    astarte_key_value_cfg_t astarte_storage_cfg = {
        .flash_device = fixture->flash_device,
        .flash_offset = fixture->flash_offset,
        .flash_partition_size = fixture->flash_partition_size,
    };

    astarte_result_t ret = astarte_key_value_open(astarte_storage_cfg, &nvs_fs);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    ret = astarte_key_value_new(&nvs_fs, namespace, &kv_storage);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    // Insert some key-value pairs
    ret = astarte_key_value_insert(&kv_storage, key1, value1, ARRAY_SIZE(value1));
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    ret = astarte_key_value_insert(&kv_storage, key2, value2, ARRAY_SIZE(value2));
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    ret = astarte_key_value_insert(&kv_storage, key3, value3, ARRAY_SIZE(value3));
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    ret = astarte_key_value_insert(&kv_storage, key4, value4, ARRAY_SIZE(value4));
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    // Check content of storage
    value_size = ARRAY_SIZE(res_value2);
    ret = astarte_key_value_find(&kv_storage, key2, res_value2, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value2), "Incorrect value size:%zu", value_size);
    zassert_mem_equal(res_value2, value2, ARRAY_SIZE(value2), "Mismatched values.");
    value_size = ARRAY_SIZE(res_value1);
    ret = astarte_key_value_find(&kv_storage, key1, res_value1, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value1), "Incorrect value size:%zu", value_size);
    zassert_mem_equal(res_value1, value1, ARRAY_SIZE(value1), "Mismatched values.");
    value_size = 0;
    ret = astarte_key_value_find(&kv_storage, key5, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));
    value_size = ARRAY_SIZE(res_value3);
    ret = astarte_key_value_find(&kv_storage, key3, res_value3, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value3), "Incorrect value size:%zu", value_size);
    zassert_mem_equal(res_value3, value3, ARRAY_SIZE(value3), "Mismatched values.");
    value_size = ARRAY_SIZE(res_value4);
    ret = astarte_key_value_find(&kv_storage, key4, res_value4, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value4), "Incorrect value size:%zu", value_size);
    zassert_mem_equal(res_value4, value4, ARRAY_SIZE(value4), "Mismatched values.");

    // Delete some key-value pairs
    ret = astarte_key_value_delete(&kv_storage, key2);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    // Check content of storage
    value_size = 0;
    ret = astarte_key_value_find(&kv_storage, key2, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));
    value_size = ARRAY_SIZE(res_value1);
    ret = astarte_key_value_find(&kv_storage, key1, res_value1, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value1), "Incorrect value size:%zu", value_size);
    zassert_mem_equal(res_value1, value1, ARRAY_SIZE(value1), "Mismatched values.");
    value_size = 0;
    ret = astarte_key_value_find(&kv_storage, key5, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));
    value_size = ARRAY_SIZE(res_value3);
    ret = astarte_key_value_find(&kv_storage, key3, res_value3, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value3), "Incorrect value size:%zu", value_size);
    zassert_mem_equal(res_value3, value3, ARRAY_SIZE(value3), "Mismatched values.");
    value_size = ARRAY_SIZE(res_value4);
    ret = astarte_key_value_find(&kv_storage, key4, res_value4, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value4), "Incorrect value size:%zu", value_size);
    zassert_mem_equal(res_value4, value4, ARRAY_SIZE(value4), "Mismatched values.");

    // Delete some key-value pairs
    ret = astarte_key_value_delete(&kv_storage, key4);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    ret = astarte_key_value_delete(&kv_storage, key1);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    // Check content of storage
    value_size = 0;
    ret = astarte_key_value_find(&kv_storage, key2, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));
    value_size = 0;
    ret = astarte_key_value_find(&kv_storage, key1, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));
    value_size = 0;
    ret = astarte_key_value_find(&kv_storage, key5, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));
    value_size = ARRAY_SIZE(res_value3);
    ret = astarte_key_value_find(&kv_storage, key3, res_value3, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value3), "Incorrect value size:%zu", value_size);
    zassert_mem_equal(res_value3, value3, ARRAY_SIZE(value3), "Mismatched values.");
    value_size = 0;
    ret = astarte_key_value_find(&kv_storage, key4, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));

    // Delete some key-value pairs
    ret = astarte_key_value_delete(&kv_storage, key3);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    // Check content of storage
    value_size = 0;
    ret = astarte_key_value_find(&kv_storage, key2, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));
    value_size = 0;
    ret = astarte_key_value_find(&kv_storage, key1, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));
    value_size = 0;
    ret = astarte_key_value_find(&kv_storage, key5, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));
    value_size = 0;
    ret = astarte_key_value_find(&kv_storage, key3, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));
    value_size = 0;
    ret = astarte_key_value_find(&kv_storage, key4, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));

    astarte_key_value_destroy(&kv_storage);
}

ZTEST_F(astarte_device_sdk_key_value, test_astarte_storage_key_value_read_sizes)
{
    size_t value_size = 0U;

    // Initialize storage driver
    astarte_key_value_t kv_storage = { 0 };
    struct nvs_fs nvs_fs = { 0 };
    const char namespace[] = "simple namespace";

    astarte_key_value_cfg_t astarte_storage_cfg = {
        .flash_device = fixture->flash_device,
        .flash_offset = fixture->flash_offset,
        .flash_partition_size = fixture->flash_partition_size,
    };

    astarte_result_t ret = astarte_key_value_open(astarte_storage_cfg, &nvs_fs);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    ret = astarte_key_value_new(&nvs_fs, namespace, &kv_storage);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    // Insert some key-value pairs
    ret = astarte_key_value_insert(&kv_storage, key1, value1, ARRAY_SIZE(value1));
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    ret = astarte_key_value_insert(&kv_storage, key2, value2, ARRAY_SIZE(value2));
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    ret = astarte_key_value_insert(&kv_storage, key3, value3, ARRAY_SIZE(value3));
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    ret = astarte_key_value_insert(&kv_storage, key4, value4, ARRAY_SIZE(value4));
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    // Check content of storage
    value_size = 0;
    ret = astarte_key_value_find(&kv_storage, key2, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value2), "Incorrect value size:%zu", value_size);
    ret = astarte_key_value_find(&kv_storage, key2, res_value2, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value2), "Incorrect value size:%zu", value_size);
    zassert_mem_equal(res_value2, value2, ARRAY_SIZE(value2), "Mismatched values.");
    value_size = 0;
    ret = astarte_key_value_find(&kv_storage, key1, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value1), "Incorrect value size:%zu", value_size);
    ret = astarte_key_value_find(&kv_storage, key1, res_value1, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value1), "Incorrect value size:%zu", value_size);
    zassert_mem_equal(res_value1, value1, ARRAY_SIZE(value1), "Mismatched values.");
    value_size = 0;
    ret = astarte_key_value_find(&kv_storage, key5, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));
    value_size = 0;
    ret = astarte_key_value_find(&kv_storage, key3, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value3), "Incorrect value size:%zu", value_size);
    ret = astarte_key_value_find(&kv_storage, key3, res_value3, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value3), "Incorrect value size:%zu", value_size);
    zassert_mem_equal(res_value3, value3, ARRAY_SIZE(value3), "Mismatched values.");
    value_size = 0;
    ret = astarte_key_value_find(&kv_storage, key4, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value4), "Incorrect value size:%zu", value_size);
    ret = astarte_key_value_find(&kv_storage, key4, res_value4, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value4), "Incorrect value size:%zu", value_size);
    zassert_mem_equal(res_value4, value4, ARRAY_SIZE(value4), "Mismatched values.");

    astarte_key_value_destroy(&kv_storage);
}

ZTEST_F(astarte_device_sdk_key_value, test_astarte_storage_key_value_overwrite)
{
    size_t value_size = 0U;

    // Initialize storage driver
    astarte_key_value_t kv_storage = { 0 };
    struct nvs_fs nvs_fs = { 0 };
    const char namespace[] = "simple namespace";

    astarte_key_value_cfg_t astarte_storage_cfg = {
        .flash_device = fixture->flash_device,
        .flash_offset = fixture->flash_offset,
        .flash_partition_size = fixture->flash_partition_size,
    };

    astarte_result_t ret = astarte_key_value_open(astarte_storage_cfg, &nvs_fs);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    ret = astarte_key_value_new(&nvs_fs, namespace, &kv_storage);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    // Insert some key-value pairs
    ret = astarte_key_value_insert(&kv_storage, key1, value1, ARRAY_SIZE(value1));
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    ret = astarte_key_value_insert(&kv_storage, key3, value3, ARRAY_SIZE(value3));
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    ret = astarte_key_value_insert(&kv_storage, key4, value4, ARRAY_SIZE(value4));
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    // Check content of storage
    value_size = 0;
    ret = astarte_key_value_find(&kv_storage, key2, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));
    value_size = ARRAY_SIZE(res_value1);
    ret = astarte_key_value_find(&kv_storage, key1, res_value1, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value1), "Incorrect value size:%zu", value_size);
    zassert_mem_equal(res_value1, value1, ARRAY_SIZE(value1), "Mismatched values.");
    value_size = 0;
    ret = astarte_key_value_find(&kv_storage, key5, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));
    value_size = ARRAY_SIZE(res_value3);
    ret = astarte_key_value_find(&kv_storage, key3, res_value3, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value3), "Incorrect value size:%zu", value_size);
    zassert_mem_equal(res_value3, value3, ARRAY_SIZE(value3), "Mismatched values.");
    value_size = ARRAY_SIZE(res_value4);
    ret = astarte_key_value_find(&kv_storage, key4, res_value4, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value4), "Incorrect value size:%zu", value_size);
    zassert_mem_equal(res_value4, value4, ARRAY_SIZE(value4), "Mismatched values.");

    // Overwrite a storage entry
    ret = astarte_key_value_insert(&kv_storage, key1, value5, ARRAY_SIZE(value5));
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    // Check content of storage
    value_size = 0;
    ret = astarte_key_value_find(&kv_storage, key2, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));
    value_size = ARRAY_SIZE(res_value5);
    ret = astarte_key_value_find(&kv_storage, key1, res_value5, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value5), "Incorrect value size:%zu", value_size);
    zassert_mem_equal(res_value5, value5, ARRAY_SIZE(value5), "Mismatched values.");
    value_size = 0;
    ret = astarte_key_value_find(&kv_storage, key5, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));
    value_size = ARRAY_SIZE(res_value3);
    ret = astarte_key_value_find(&kv_storage, key3, res_value3, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value3), "Incorrect value size:%zu", value_size);
    zassert_mem_equal(res_value3, value3, ARRAY_SIZE(value3), "Mismatched values.");
    value_size = ARRAY_SIZE(res_value4);
    ret = astarte_key_value_find(&kv_storage, key4, res_value4, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value4), "Incorrect value size:%zu", value_size);
    zassert_mem_equal(res_value4, value4, ARRAY_SIZE(value4), "Mismatched values.");

    astarte_key_value_destroy(&kv_storage);
}

ZTEST_F(astarte_device_sdk_key_value, test_astarte_storage_key_value_iteration)
{
    size_t value_size = 0U;

    // Initialize storage driver
    astarte_key_value_t kv_storage = { 0 };
    struct nvs_fs nvs_fs = { 0 };
    const char namespace[] = "simple namespace";

    astarte_key_value_cfg_t astarte_storage_cfg = {
        .flash_device = fixture->flash_device,
        .flash_offset = fixture->flash_offset,
        .flash_partition_size = fixture->flash_partition_size,
    };

    astarte_result_t ret = astarte_key_value_open(astarte_storage_cfg, &nvs_fs);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    ret = astarte_key_value_new(&nvs_fs, namespace, &kv_storage);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    // Insert some key-value pairs
    ret = astarte_key_value_insert(&kv_storage, key1, value1, ARRAY_SIZE(value1));
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    ret = astarte_key_value_insert(&kv_storage, key3, value3, ARRAY_SIZE(value3));
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    ret = astarte_key_value_insert(&kv_storage, key4, value4, ARRAY_SIZE(value4));
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    // Check content of storage
    value_size = 0;
    ret = astarte_key_value_find(&kv_storage, key2, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));
    value_size = ARRAY_SIZE(res_value1);
    ret = astarte_key_value_find(&kv_storage, key1, res_value1, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value1), "Incorrect value size:%zu", value_size);
    zassert_mem_equal(res_value1, value1, ARRAY_SIZE(value1), "Mismatched values.");
    value_size = 0;
    ret = astarte_key_value_find(&kv_storage, key5, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));
    value_size = ARRAY_SIZE(res_value3);
    ret = astarte_key_value_find(&kv_storage, key3, res_value3, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value3), "Incorrect value size:%zu", value_size);
    zassert_mem_equal(res_value3, value3, ARRAY_SIZE(value3), "Mismatched values.");
    value_size = ARRAY_SIZE(res_value4);
    ret = astarte_key_value_find(&kv_storage, key4, res_value4, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(value4), "Incorrect value size:%zu", value_size);
    zassert_mem_equal(res_value4, value4, ARRAY_SIZE(value4), "Mismatched values.");

    // Iterate over storage
    astarte_key_value_iter_t iter = { 0 };
    ret = astarte_key_value_iterator_init(&kv_storage, &iter);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    value_size = 0U;
    ret = astarte_key_value_iterator_get(&iter, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(key1), "Incorrect value size:%zu", value_size);
    ret = astarte_key_value_iterator_get(&iter, res_key1, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(key1), "Incorrect value size:%zu", value_size);
    zassert_mem_equal(res_key1, key1, ARRAY_SIZE(key1), "Mismatched values.");

    ret = astarte_key_value_iterator_next(&iter);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    value_size = 0U;
    ret = astarte_key_value_iterator_get(&iter, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(key3), "Incorrect value size:%zu", value_size);
    ret = astarte_key_value_iterator_get(&iter, res_key3, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(key3), "Incorrect value size:%zu", value_size);
    zassert_mem_equal(res_key3, key3, ARRAY_SIZE(key3), "Mismatched values.");

    ret = astarte_key_value_iterator_next(&iter);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    value_size = 0U;
    ret = astarte_key_value_iterator_get(&iter, NULL, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(key4), "Incorrect value size:%zu", value_size);
    ret = astarte_key_value_iterator_get(&iter, res_key4, &value_size);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    zassert_equal(value_size, ARRAY_SIZE(key4), "Incorrect value size:%zu", value_size);
    zassert_mem_equal(res_key4, key4, ARRAY_SIZE(key4), "Mismatched values.");

    ret = astarte_key_value_iterator_next(&iter);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));

    astarte_key_value_destroy(&kv_storage);
}

ZTEST_F(astarte_device_sdk_key_value, test_astarte_storage_key_value_iteration_empty_storage)
{
    // Initialize storage driver
    astarte_key_value_t kv_storage = { 0 };
    struct nvs_fs nvs_fs = { 0 };
    const char namespace[] = "simple namespace";

    astarte_key_value_cfg_t astarte_storage_cfg = {
        .flash_device = fixture->flash_device,
        .flash_offset = fixture->flash_offset,
        .flash_partition_size = fixture->flash_partition_size,
    };

    astarte_result_t ret = astarte_key_value_open(astarte_storage_cfg, &nvs_fs);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    ret = astarte_key_value_new(&nvs_fs, namespace, &kv_storage);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    // Iterate over storage
    astarte_key_value_iter_t iter = { 0 };
    ret = astarte_key_value_iterator_init(&kv_storage, &iter);
    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));

    astarte_key_value_destroy(&kv_storage);
}

ZTEST_F(astarte_device_sdk_key_value, test_astarte_storage_key_value_multiple_namespaces)
{
    astarte_result_t ret = ASTARTE_RESULT_OK;
    size_t value_size = 0U;

    struct nvs_fs nvs_fs = { 0 };
    astarte_key_value_cfg_t astarte_storage_cfg = {
        .flash_device = fixture->flash_device,
        .flash_offset = fixture->flash_offset,
        .flash_partition_size = fixture->flash_partition_size,
    };

    ret = astarte_key_value_open(astarte_storage_cfg, &nvs_fs);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    // Initialize first storage driver
    astarte_key_value_t kv_astarte_storage_1 = { 0 };
    const char namespace_1[] = "first namespace";
    ret = astarte_key_value_new(&nvs_fs, namespace_1, &kv_astarte_storage_1);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    // Initialize second storage driver
    astarte_key_value_t kv_astarte_storage_2 = { 0 };
    const char namespace_2[] = "second namespace";
    ret = astarte_key_value_new(&nvs_fs, namespace_2, &kv_astarte_storage_2);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    // Insert some key-value pairs with multiple namespaces
    ret = astarte_key_value_insert(&kv_astarte_storage_1, key1, value1, ARRAY_SIZE(value1));
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    ret = astarte_key_value_insert(&kv_astarte_storage_2, key2, value2, ARRAY_SIZE(value2));
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    ret = astarte_key_value_insert(&kv_astarte_storage_2, key3, value3, ARRAY_SIZE(value3));
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    ret = astarte_key_value_insert(&kv_astarte_storage_1, key4, value4, ARRAY_SIZE(value4));
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));
    ret = astarte_key_value_insert(&kv_astarte_storage_2, key5, value5, ARRAY_SIZE(value5));
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    // Iterate over first storage dynamically
    astarte_key_value_iter_t iter_1 = { 0 };
    ret = astarte_key_value_iterator_init(&kv_astarte_storage_1, &iter_1);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    bool found_key1 = false;
    bool found_key4 = false;

    while (ret == ASTARTE_RESULT_OK) {
        value_size = 0;
        ret = astarte_key_value_iterator_get(&iter_1, NULL, &value_size);
        zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

        char *dynamic_key = calloc(value_size, sizeof(char));
        zassert_not_null(dynamic_key, "Failed to allocate memory for key");

        ret = astarte_key_value_iterator_get(&iter_1, dynamic_key, &value_size);
        zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

        if (strcmp(dynamic_key, key1) == 0) {
            found_key1 = true;
        } else if (strcmp(dynamic_key, key4) == 0) {
            found_key4 = true;
        } else {
            zassert_unreachable("Unexpected key found during iteration: %s", dynamic_key);
        }

        free(dynamic_key);
        ret = astarte_key_value_iterator_next(&iter_1);
    }

    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));
    zassert_true(found_key1, "key1 was not found during iteration");
    zassert_true(found_key4, "key4 was not found during iteration");

    // Iterate over second storage dynamically
    astarte_key_value_iter_t iter_2 = { 0 };
    ret = astarte_key_value_iterator_init(&kv_astarte_storage_2, &iter_2);
    zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

    bool found_key2 = false;
    bool found_key3 = false;
    bool found_key5 = false;

    while (ret == ASTARTE_RESULT_OK) {
        value_size = 0;
        ret = astarte_key_value_iterator_get(&iter_2, NULL, &value_size);
        zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

        char *dynamic_key = calloc(value_size, sizeof(char));
        zassert_not_null(dynamic_key, "Failed to allocate memory for key");

        ret = astarte_key_value_iterator_get(&iter_2, dynamic_key, &value_size);
        zassert_equal(ret, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ret));

        if (strcmp(dynamic_key, key2) == 0) {
            found_key2 = true;
        } else if (strcmp(dynamic_key, key3) == 0) {
            found_key3 = true;
        } else if (strcmp(dynamic_key, key5) == 0) {
            found_key5 = true;
        } else {
            zassert_unreachable("Unexpected key found during iteration: %s", dynamic_key);
        }

        free(dynamic_key);
        ret = astarte_key_value_iterator_next(&iter_2);
    }

    zassert_equal(ret, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ret));
    zassert_true(found_key2, "key2 was not found during iteration");
    zassert_true(found_key3, "key3 was not found during iteration");
    zassert_true(found_key5, "key5 was not found during iteration");

    astarte_key_value_destroy(&kv_astarte_storage_1);
    astarte_key_value_destroy(&kv_astarte_storage_2);
}
