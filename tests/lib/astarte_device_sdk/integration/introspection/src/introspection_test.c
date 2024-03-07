/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>

#include <string.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>

#include "astarte_device_sdk/interface.h"
#include "astarte_device_sdk/result.h"

#include "introspection.h"

LOG_MODULE_REGISTER(introspection_test, CONFIG_LOG_DEFAULT_LEVEL); // NOLINT

ZTEST_SUITE(astarte_device_sdk_introspection, NULL, NULL, NULL, NULL, NULL); // NOLINT

const static astarte_interface_t test_interface_a = {
    .name = "test.interface.a",
    .major_version = 0,
    .minor_version = 1,
    .ownership = ASTARTE_INTERFACE_OWNERSHIP_SERVER,
    .type = ASTARTE_INTERFACE_TYPE_PROPERTIES,
};

const static astarte_interface_t test_interface_b = {
    .name = "test.interface.b",
    .major_version = 0,
    .minor_version = 1,
    .ownership = ASTARTE_INTERFACE_OWNERSHIP_DEVICE,
    .type = ASTARTE_INTERFACE_TYPE_DATASTREAM,
};

const static astarte_interface_t test_interface_c = {
    .name = "test.interface.c",
    .major_version = 1,
    .minor_version = 0,
    .ownership = ASTARTE_INTERFACE_OWNERSHIP_SERVER,
    .type = ASTARTE_INTERFACE_TYPE_DATASTREAM,
};

static char *get_introspection_string(introspection_t *introspection)
{
    size_t introspection_buf_len = introspection_get_string_size(introspection);
    char *introspection_buf = calloc(introspection_buf_len, sizeof(char));
    introspection_fill_string(introspection, introspection_buf, introspection_buf_len);
    return introspection_buf;
}

static void check_add_interface(introspection_t *introspection,
    const astarte_interface_t *interface, astarte_result_t expected_res)
{
    LOG_INF("Adding interface '%s'", interface->name); // NOLINT

    zassert_equal(expected_res, introspection_add(introspection, interface),
        "Unexpected result while inserting interface '%s'", interface->name);
}

static void check_add_interface_ok(
    introspection_t *introspection, const astarte_interface_t *interface)
{
    check_add_interface(introspection, interface, ASTARTE_RESULT_OK);
}

// since ordering of the interfaces is not guardanteed we first compare the length
static void check_introspection(char *expected, char *got)
{
    size_t expected_len = strnlen(expected, ASTARTE_INTERFACE_NAME_MAX_SIZE);
    size_t got_len = strnlen(got, ASTARTE_INTERFACE_NAME_MAX_SIZE);

    zassert_equal(got_len, expected_len,
        "The introspection len does not match the expected one\nExpected: %s\nGot: %s",
        expected_len, got_len);
}

const static char expected_introspection_all[]
    = "test.interface.a:0:1;test.interface.b:0:1;test.interface.c:1:0";

ZTEST(astarte_device_sdk_introspection, test_introspection_add) // NOLINT
{
    LOG_INF("Creating introspection"); // NOLINT
    introspection_t introspection;
    introspection_init(&introspection);

    LOG_INF("Adding interfaces"); // NOLINT
    check_add_interface_ok(&introspection, &test_interface_a);
    check_add_interface_ok(&introspection, &test_interface_b);
    check_add_interface_ok(&introspection, &test_interface_c);

    char *introspection_buf = get_introspection_string(&introspection);
    LOG_INF("Introspection string '%s'", introspection_buf); // NOLINT

    check_introspection((char *) expected_introspection_all, introspection_buf);

    zassert_equal_ptr(
        &test_interface_a, introspection_get(&introspection, (char *) test_interface_a.name));
    zassert_equal_ptr(
        &test_interface_b, introspection_get(&introspection, (char *) test_interface_b.name));
    zassert_equal_ptr(
        &test_interface_c, introspection_get(&introspection, (char *) test_interface_c.name));

    LOG_INF("Freeing introspection"); // NOLINT
    introspection_free(introspection);
}

const static char expected_introspection_ab[] = "test.interface.a:0:1;test.interface.b:0:1";

static void check_remove_interface(
    introspection_t *introspection, char *interface_name, astarte_result_t expected_res)
{
    LOG_INF("Removing interface '%s'", interface_name); // NOLINT

    zassert_equal(expected_res, introspection_remove(introspection, interface_name),
        "Unexpected result while removing interface '%s'", interface_name);
}

static void check_remove_interface_ok(introspection_t *introspection, char *interface_name)
{
    check_remove_interface(introspection, interface_name, ASTARTE_RESULT_OK);
}

ZTEST(astarte_device_sdk_introspection, test_introspection_add_remove) // NOLINT
{
    LOG_INF("Creating introspection"); // NOLINT
    introspection_t introspection;
    introspection_init(&introspection);

    LOG_INF("Adding interfaces"); // NOLINT
    check_add_interface_ok(&introspection, &test_interface_a);
    check_add_interface_ok(&introspection, &test_interface_b);
    check_add_interface_ok(&introspection, &test_interface_c);

    char *introspection_buf = get_introspection_string(&introspection);
    LOG_INF("Complete introspection string '%s'", introspection_buf); // NOLINT

    check_introspection((char *) expected_introspection_all, introspection_buf);

    LOG_INF("Removing interface '%s'", test_interface_c.name); // NOLINT
    check_remove_interface_ok(&introspection, (char *) test_interface_c.name);

    char *introspection_buf_ab = get_introspection_string(&introspection);
    LOG_INF("Introspection string '%s'", introspection_buf); // NOLINT

    check_introspection((char *) expected_introspection_ab, introspection_buf_ab);

    zassert_equal_ptr(
        &test_interface_a, introspection_get(&introspection, (char *) test_interface_a.name));
    zassert_equal_ptr(
        &test_interface_b, introspection_get(&introspection, (char *) test_interface_b.name));
    zassert_equal_ptr(NULL, introspection_get(&introspection, (char *) test_interface_c.name));

    LOG_INF("Freeing introspection"); // NOLINT
    introspection_free(introspection);
}

ZTEST(astarte_device_sdk_introspection, test_introspection_add_twice) // NOLINT
{
    LOG_INF("Creating introspection"); // NOLINT
    introspection_t introspection;
    introspection_init(&introspection);

    LOG_INF("Adding interfaces"); // NOLINT
    check_add_interface_ok(&introspection, &test_interface_a);
    check_add_interface_ok(&introspection, &test_interface_b);
    check_add_interface_ok(&introspection, &test_interface_c);

    char *introspection_buf = get_introspection_string(&introspection);
    LOG_INF("Complete introspection string '%s'", introspection_buf); // NOLINT

    check_introspection((char *) expected_introspection_all, introspection_buf);

    check_add_interface(
        &introspection, &test_interface_a, ASTARTE_RESULT_INTERFACE_ALREADY_PRESENT);

    char *introspection_buf_abc = get_introspection_string(&introspection);
    LOG_INF("Introspection string '%s'", introspection_buf); // NOLINT

    check_introspection((char *) expected_introspection_all, introspection_buf_abc);

    LOG_INF("Freeing introspection"); // NOLINT
    introspection_free(introspection);
}

const static char expected_introspection_a[] = "test.interface.a:0:1";

ZTEST(astarte_device_sdk_introspection, test_introspection_remove_twice) // NOLINT
{
    LOG_INF("Creating introspection"); // NOLINT
    introspection_t introspection;
    introspection_init(&introspection);

    LOG_INF("Adding interface"); // NOLINT
    check_add_interface_ok(&introspection, &test_interface_a);

    char *introspection_buf = get_introspection_string(&introspection);
    LOG_INF("Complete introspection string '%s'", introspection_buf); // NOLINT
    check_introspection((char *) expected_introspection_a, introspection_buf);

    check_remove_interface_ok(&introspection, (char *) test_interface_a.name);

    char *introspection_buf_empty = get_introspection_string(&introspection);
    LOG_INF("Introspection string '%s'", introspection_buf); // NOLINT
    check_introspection("", introspection_buf_empty);

    check_remove_interface(
        &introspection, (char *) test_interface_a.name, ASTARTE_RESULT_INTERFACE_NOT_FOUND);

    char *introspection_buf_empty_2 = get_introspection_string(&introspection);
    LOG_INF("Introspection string '%s'", introspection_buf_empty_2); // NOLINT
    check_introspection("", introspection_buf_empty_2);

    LOG_INF("Freeing introspection"); // NOLINT
    introspection_free(introspection);
}

ZTEST(astarte_device_sdk_introspection, test_introspection_iter) // NOLINT
{
    LOG_INF("Creating introspection"); // NOLINT
    introspection_t introspection;
    introspection_init(&introspection);

    LOG_INF("Adding interfaces"); // NOLINT
    check_add_interface_ok(&introspection, &test_interface_a);
    check_add_interface_ok(&introspection, &test_interface_b);
    check_add_interface_ok(&introspection, &test_interface_c);

    LOG_INF("Creating introspection iterator"); // NOLINT
    const introspection_node_t *introspection_iterator = introspection_iter(&introspection);
    LOG_INF("First interface '%s'", introspection_iterator->interface); // NOLINT
    zassert_equal_ptr(&test_interface_a, introspection_iterator->interface);

    LOG_INF("Advancing iterator"); // NOLINT
    introspection_iterator = introspection_iter_next(&introspection, introspection_iterator);
    LOG_INF("Second interface '%s'", introspection_iterator->interface); // NOLINT
    zassert_equal_ptr(&test_interface_b, introspection_iterator->interface);

    LOG_INF("Advancing iterator"); // NOLINT
    introspection_iterator = introspection_iter_next(&introspection, introspection_iterator);
    LOG_INF("Third interface '%s'", introspection_iterator->interface); // NOLINT
    zassert_equal_ptr(&test_interface_c, introspection_iterator->interface);

    LOG_INF("Advancing iterator"); // NOLINT
    introspection_iterator = introspection_iter_next(&introspection, introspection_iterator);
    LOG_INF("No fourth interface %p", introspection_iterator); // NOLINT
    zassert_equal_ptr(NULL, introspection_iterator);

    LOG_INF("Advancing iterator"); // NOLINT
    introspection_iterator = introspection_iter_next(&introspection, introspection_iterator);
    LOG_INF("No fourth interface %p", introspection_iterator); // NOLINT
    zassert_equal_ptr(NULL, introspection_iterator);

    LOG_INF("Freeing introspection"); // NOLINT
    introspection_free(introspection);
}

static void check_update_interface(introspection_t *introspection,
    const astarte_interface_t *interface, astarte_result_t expected_res)
{
    LOG_INF("Updating interface '%s'", interface->name); // NOLINT

    zassert_equal(expected_res, introspection_update(introspection, interface),
        "Unexpected result while updating interface '%s'", interface->name);
}

static void check_update_interface_ok(
    introspection_t *introspection, const astarte_interface_t *interface)
{
    check_update_interface(introspection, interface, ASTARTE_RESULT_OK);
}

const astarte_interface_t test_interface_a_v2_valid = {
    .name = "test.interface.a",
    .major_version = 0,
    .minor_version = 2,
    .ownership = ASTARTE_INTERFACE_OWNERSHIP_SERVER,
    .type = ASTARTE_INTERFACE_TYPE_PROPERTIES,
};

ZTEST(astarte_device_sdk_introspection, test_introspection_update_ok) // NOLINT
{
    LOG_INF("Creating introspection"); // NOLINT
    introspection_t introspection;
    introspection_init(&introspection);

    LOG_INF("Adding interfaces"); // NOLINT
    check_add_interface_ok(&introspection, &test_interface_a);
    check_add_interface_ok(&introspection, &test_interface_b);
    check_add_interface_ok(&introspection, &test_interface_c);

    LOG_INF("Updating the interface '%s'", test_interface_a_v2_valid.name); // NOLINT
    check_update_interface_ok(&introspection, &test_interface_a_v2_valid);

    LOG_INF("Freeing introspection"); // NOLINT
    introspection_free(introspection);
}

ZTEST(astarte_device_sdk_introspection, test_introspection_update_invalid_version) // NOLINT
{
    LOG_INF("Creating introspection"); // NOLINT
    introspection_t introspection;
    introspection_init(&introspection);

    LOG_INF("Adding interfaces"); // NOLINT
    check_add_interface_ok(&introspection, &test_interface_a);
    check_add_interface_ok(&introspection, &test_interface_b);
    check_add_interface_ok(&introspection, &test_interface_c);

    LOG_INF("Updating the interface '%s' with the same struct", test_interface_a.name); // NOLINT
    check_update_interface(&introspection, &test_interface_a, ASTARTE_RESULT_INTERFACE_CONFLICTING);

    LOG_INF("Freeing introspection"); // NOLINT
    introspection_free(introspection);
}
