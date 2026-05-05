/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <zephyr/ztest.h>

#include "astarte_device_sdk/device_id.h"
#include "astarte_device_sdk/result.h"
#include <zephyr/sys/uuid.h>

ZTEST_SUITE(astarte_device_sdk_device_id, NULL, NULL, NULL, NULL, NULL);

ZTEST(astarte_device_sdk_device_id, test_device_id_generate_random)
{
    char device_id_1[ASTARTE_DEVICE_ID_LEN + 1] = { 0 };
    char device_id_2[ASTARTE_DEVICE_ID_LEN + 1] = { 0 };

    /* Generate first random ID */
    astarte_result_t res = astarte_device_id_generate_random(device_id_1);
    zassert_equal(
        ASTARTE_RESULT_OK, res, "astarte_device_id_generate_random returned an error: %d", res);
    zassert_equal(
        ASTARTE_DEVICE_ID_LEN, strlen(device_id_1), "Generated device ID 1 length is incorrect");

    /* Generate second random ID */
    res = astarte_device_id_generate_random(device_id_2);
    zassert_equal(
        ASTARTE_RESULT_OK, res, "astarte_device_id_generate_random returned an error: %d", res);
    zassert_equal(
        ASTARTE_DEVICE_ID_LEN, strlen(device_id_2), "Generated device ID 2 length is incorrect");

    /* Since they are UUIDv4-based, they should practically never be identical */
    zassert_true(
        strcmp(device_id_1, device_id_2) != 0, "Generated random device IDs should be unique");
}

ZTEST(astarte_device_sdk_device_id, test_device_id_generate_deterministic_example_from_standard)
{
    struct uuid namespace;
    int result = uuid_from_string("6ba7b810-9dad-11d1-80b4-00c04fd430c8", &namespace);
    zassert_true(result == 0, "uuid_from_string returned an error");

    const char *name = "www.example.com";
    char device_id[ASTARTE_DEVICE_ID_LEN + 1] = { 0 };

    astarte_result_t res = astarte_device_id_generate_deterministic(
        namespace.val, (const uint8_t *) name, strlen(name), device_id);

    zassert_equal(ASTARTE_RESULT_OK, res,
        "astarte_device_id_generate_deterministic returned an error: %d", res);
    zassert_equal(ASTARTE_DEVICE_ID_LEN, strlen(device_id),
        "Generated deterministic device ID length is incorrect");

    /* * The expected UUIDv5 for namespace "6ba7b810-9dad-11d1-80b4-00c04fd430c8" and
     * name "www.example.com" is "2ed6657d-e927-568b-95e1-2665a8aea6a2".
     * Converting those bytes to Base64URL encoding results in "LtZlfeknVouV4SZlqK6mog".
     */
    const char *expected_device_id = "LtZlfeknVouV4SZlqK6mog";
    zassert_true(strcmp(expected_device_id, device_id) == 0, "device_id != %s. Got: %s",
        expected_device_id, device_id);
}

ZTEST(astarte_device_sdk_device_id, test_device_id_generate_deterministic)
{
    struct uuid namespace;
    int result = uuid_from_string("c21fb11c-b6c9-452a-9e86-6075e313d7e2", &namespace);
    zassert_true(result == 0, "uuid_from_string returned an error");

    const char *name = "00225588";
    char device_id[ASTARTE_DEVICE_ID_LEN + 1] = { 0 };

    astarte_result_t res = astarte_device_id_generate_deterministic(
        namespace.val, (const uint8_t *) name, strlen(name), device_id);

    zassert_equal(ASTARTE_RESULT_OK, res,
        "astarte_device_id_generate_deterministic returned an error: %d", res);
    zassert_equal(ASTARTE_DEVICE_ID_LEN, strlen(device_id),
        "Generated deterministic device ID length is incorrect");

    /*
     * The expected UUIDv5 for namespace "c21fb11c-b6c9-452a-9e86-6075e313d7e2" and
     * name "00225588" is "63c8fb48-02ab-53f4-a254-52956dcbbce4".
     * Converting those bytes to Base64URL encoding results in "Y8j7SAKrU_SiVFKVbcu85A".
     */
    const char *expected_device_id = "Y8j7SAKrU_SiVFKVbcu85A";
    zassert_true(strcmp(expected_device_id, device_id) == 0, "device_id != %s. Got: %s",
        expected_device_id, device_id);
}

ZTEST(astarte_device_sdk_device_id, test_device_id_generate_deterministic_consistency)
{
    struct uuid namespace;
    uuid_from_string("c21fb11c-b6c9-452a-9e86-6075e313d7e2", &namespace);

    const char *name_1 = "astarte_device_1";
    char device_id_1[ASTARTE_DEVICE_ID_LEN + 1] = { 0 };
    char device_id_2[ASTARTE_DEVICE_ID_LEN + 1] = { 0 };

    astarte_result_t res = astarte_device_id_generate_deterministic(
        namespace.val, (const uint8_t *) name_1, strlen(name_1), device_id_1);
    zassert_equal(
        ASTARTE_RESULT_OK, res, "astarte_device_id_generate_deterministic returned an error");

    res = astarte_device_id_generate_deterministic(
        namespace.val, (const uint8_t *) name_1, strlen(name_1), device_id_2);
    zassert_equal(
        ASTARTE_RESULT_OK, res, "astarte_device_id_generate_deterministic returned an error");

    /* Passing the exact same namespace and name must yield the exact same device ID */
    zassert_true(strcmp(device_id_1, device_id_2) == 0,
        "Deterministic IDs for the same input parameters must match");

    /* Testing with a different name */
    const char *name_2 = "astarte_device_2";
    char device_id_3[ASTARTE_DEVICE_ID_LEN + 1] = { 0 };

    res = astarte_device_id_generate_deterministic(
        namespace.val, (const uint8_t *) name_2, strlen(name_2), device_id_3);
    zassert_equal(
        ASTARTE_RESULT_OK, res, "astarte_device_id_generate_deterministic returned an error");

    /* Passing a different name must yield a different device ID */
    zassert_true(strcmp(device_id_1, device_id_3) != 0,
        "Deterministic IDs for different input names must not match");
}
