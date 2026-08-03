/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_storage_common.h"

#include "storage/introsp.h"
#include "storage/sync.h"
#include "storage/trans.h"

ZTEST_F(astarte_device_sdk_storage, test_device_astarte_storage_invalid_params)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    // Test passing NULL to the initialization function
    ares = astarte_storage_init(NULL);
    zassert_equal(ares, ASTARTE_RESULT_INVALID_PARAM, "Init with NULL should return INVALID_PARAM");

    // Test passing NULL to other subsystem functions
    astarte_storage_transmission_indexes_t indexes = { 0 };
    ares = astarte_storage_transmission_get_indexes(NULL, &indexes);
    zassert_equal(ares, ASTARTE_RESULT_INVALID_PARAM, "Expected INVALID_PARAM for NULL handle");
}

ZTEST_F(astarte_device_sdk_storage, test_device_astarte_storage_synchronization)
{
    bool sync = false;
    astarte_result_t ares = ASTARTE_RESULT_OK;

    ares = astarte_storage_synchronization_get(&fixture->caching_handle, &sync);
    zassert_equal(ares, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(sync, false, "sync variable has been modified");

    sync = true;
    ares = astarte_storage_synchronization_get(&fixture->caching_handle, &sync);
    zassert_equal(ares, ASTARTE_RESULT_NOT_FOUND, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(sync, true, "sync variable has been modified");

    sync = true;
    ares = astarte_storage_synchronization_set(&fixture->caching_handle, sync);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    sync = false;
    ares = astarte_storage_synchronization_get(&fixture->caching_handle, &sync);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
    zassert_equal(sync, true, "Sync variable not set correctly");
}

ZTEST_F(astarte_device_sdk_storage, test_device_astarte_storage_store_introspection)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    const char intr_1_str[] = "interface1;interface2;interface3";
    const char intr_2_str[] = "interface2;interface3";
    const char intr_3_str[] = "interface1;interface2;interface3;interface4";

    ares = astarte_storage_introspection_check(
        &fixture->caching_handle, intr_1_str, ARRAY_SIZE(intr_1_str));
    zassert_equal(ares, ASTARTE_RESULT_DEVICE_CACHING_OUTDATED_INTROSPECTION, "Res:%s",
        astarte_result_to_name(ares));

    ares = astarte_storage_introspection_store(
        &fixture->caching_handle, intr_1_str, ARRAY_SIZE(intr_1_str));
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    ares = astarte_storage_introspection_check(
        &fixture->caching_handle, intr_1_str, ARRAY_SIZE(intr_1_str));
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    ares = astarte_storage_introspection_store(
        &fixture->caching_handle, intr_2_str, ARRAY_SIZE(intr_2_str));
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    ares = astarte_storage_introspection_check(
        &fixture->caching_handle, intr_1_str, ARRAY_SIZE(intr_1_str));
    zassert_equal(ares, ASTARTE_RESULT_DEVICE_CACHING_OUTDATED_INTROSPECTION, "Res:%s",
        astarte_result_to_name(ares));

    ares = astarte_storage_introspection_check(
        &fixture->caching_handle, intr_2_str, ARRAY_SIZE(intr_2_str));
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    ares = astarte_storage_introspection_store(
        &fixture->caching_handle, intr_3_str, ARRAY_SIZE(intr_3_str));
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));

    ares = astarte_storage_introspection_check(
        &fixture->caching_handle, intr_1_str, ARRAY_SIZE(intr_1_str));
    zassert_equal(ares, ASTARTE_RESULT_DEVICE_CACHING_OUTDATED_INTROSPECTION, "Res:%s",
        astarte_result_to_name(ares));

    ares = astarte_storage_introspection_check(
        &fixture->caching_handle, intr_2_str, ARRAY_SIZE(intr_2_str));
    zassert_equal(ares, ASTARTE_RESULT_DEVICE_CACHING_OUTDATED_INTROSPECTION, "Res:%s",
        astarte_result_to_name(ares));

    ares = astarte_storage_introspection_check(
        &fixture->caching_handle, intr_3_str, ARRAY_SIZE(intr_3_str));
    zassert_equal(ares, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(ares));
}
