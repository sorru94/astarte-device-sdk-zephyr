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

#include <zephyr/ztest.h>

#include "astarte_device_sdk/result.h"

ZTEST_SUITE(astarte_device_sdk_result, NULL, NULL, NULL, NULL, NULL);

// Define a minimal_log function to resolve the `undefined reference to z_log_minimal_printk` error,
// because the log environment is missing in the unit_testing platform.
void z_log_minimal_printk(const char *fmt, ...) {}

ZTEST(astarte_device_sdk_result, test_astarte_result_to_name_valid)
{
    // Test a few known specific values
    zassert_str_equal(astarte_result_to_name(ASTARTE_RESULT_OK), "ASTARTE_RESULT_OK",
        "Mapping failed for ASTARTE_RESULT_OK");

    zassert_str_equal(astarte_result_to_name(ASTARTE_RESULT_OUT_OF_MEMORY),
        "ASTARTE_RESULT_OUT_OF_MEMORY", "Mapping failed for ASTARTE_RESULT_OUT_OF_MEMORY");

    zassert_str_equal(astarte_result_to_name(ASTARTE_RESULT_DEVICE_CACHING_OUTDATED_INTROSPECTION),
        "ASTARTE_RESULT_DEVICE_CACHING_OUTDATED_INTROSPECTION",
        "Mapping failed for ASTARTE_RESULT_DEVICE_CACHING_OUTDATED_INTROSPECTION");
}

ZTEST(astarte_device_sdk_result, test_astarte_result_to_name_invalid)
{
    // Test an arbitrary value that is not in the enum
    zassert_str_equal(astarte_result_to_name((astarte_result_t) 999), "UNKNOWN RESULT CODE",
        "Did not return UNKNOWN for invalid code 999");

    // Test negative value
    zassert_str_equal(astarte_result_to_name((astarte_result_t) -1), "UNKNOWN RESULT CODE",
        "Did not return UNKNOWN for invalid code -1");
}
