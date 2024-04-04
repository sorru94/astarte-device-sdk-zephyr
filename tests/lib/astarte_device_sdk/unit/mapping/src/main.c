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

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <zephyr/ztest.h>

#include "astarte_device_sdk/mapping.h"
#include "lib/astarte_device_sdk/mapping.c"
#include "mapping_private.h"

ZTEST_SUITE(astarte_device_sdk_mapping, NULL, NULL, NULL, NULL, NULL);

// Define a minimal_log function to resolve the `undefined reference to z_log_minimal_printk` error,
// because the log environment is missing in the unit_testing platform.
void z_log_minimal_printk(const char *fmt, ...) {}

ZTEST(astarte_device_sdk_mapping, test_astarte_mapping_check_path_no_pattern)
{
    astarte_result_t res = ASTARTE_RESULT_OK;
    astarte_mapping_t mapping = {
        .endpoint = "/binaryblob_endpoint",
        .regex_endpoint = "/binaryblob_endpoint$",
        .type = ASTARTE_MAPPING_TYPE_BINARYBLOB,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = true,
        .allow_unset = false,
    };

    const char correct_path[] = "/binaryblob_endpoint";
    res = astarte_mapping_check_path(mapping, correct_path);
    zassert_equal(res, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(res));

    const char incorrect_path[] = "/binary_endpoint";
    res = astarte_mapping_check_path(mapping, incorrect_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));

    const char missing_forwardslash_path[] = "binaryblob_endpoint";
    res = astarte_mapping_check_path(mapping, missing_forwardslash_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));

    const char suffixed_path[] = "/binaryblob_endpointtttt";
    res = astarte_mapping_check_path(mapping, suffixed_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));

    const char prefixed_path[] = "prefix/binaryblob_endpoint";
    res = astarte_mapping_check_path(mapping, suffixed_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));
}

ZTEST(astarte_device_sdk_mapping, test_astarte_mapping_check_path_single_pattern)
{
    astarte_result_t res = ASTARTE_RESULT_OK;
    astarte_mapping_t mapping = {
        .endpoint = "/%{sensor_id}/double_endpoint",
        .regex_endpoint = "/[a-zA-Z_]+[a-zA-Z0-9_]*/double_endpoint",
        .type = ASTARTE_MAPPING_TYPE_DOUBLE,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNIQUE,
        .explicit_timestamp = false,
        .allow_unset = true,
    };

    const char correct_path[] = "/sensor42/double_endpoint";
    res = astarte_mapping_check_path(mapping, correct_path);
    zassert_equal(res, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(res));

    const char incorrect_path[] = "/sensor42/dbl_endpoint";
    res = astarte_mapping_check_path(mapping, incorrect_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));

    const char missing_param_path[] = "/double_endpoint";
    res = astarte_mapping_check_path(mapping, missing_param_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));

    const char incorrect_param_path[] = "/12sensor12/double_endpoint";
    res = astarte_mapping_check_path(mapping, incorrect_param_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));
}

ZTEST(astarte_device_sdk_mapping, test_astarte_mapping_check_path_three_patterns)
{
    astarte_result_t res = ASTARTE_RESULT_OK;
    astarte_mapping_t mapping = {
        .endpoint = "/%{sensor_1_id}/double/%{sensor_2_id}/endpoint/%{sensor_3_id}",
        .regex_endpoint = "/[a-zA-Z_]+[a-zA-Z0-9_]*/double/[a-zA-Z_]+[a-zA-Z0-9_]*/endpoint/"
                          "[a-zA-Z_]+[a-zA-Z0-9_]*",
        .type = ASTARTE_MAPPING_TYPE_DOUBLE,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNIQUE,
        .explicit_timestamp = false,
        .allow_unset = true,
    };

    const char correct_path[] = "/sensor_42/double/subsensor_11/endpoint/subsensor_54";
    res = astarte_mapping_check_path(mapping, correct_path);
    zassert_equal(res, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(res));

    const char incorrect_path[] = "/sensor_42/dbl/subsensor_11/endpoint/subsensor_54";
    res = astarte_mapping_check_path(mapping, incorrect_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));

    const char missing_first_param_path[] = "/double/subsensor_11/endpoint/subsensor_54";
    res = astarte_mapping_check_path(mapping, missing_first_param_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));

    const char missing_second_param_path[] = "/sensor_42/double/endpoint/subsensor_54";
    res = astarte_mapping_check_path(mapping, missing_second_param_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));

    const char missing_third_param_path[] = "/sensor_42/double/subsensor_11/endpoint";
    res = astarte_mapping_check_path(mapping, missing_third_param_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));

    const char incorrect_second_param_path[] = "/sensor_42/double/11/endpoint/subsensor_54";
    res = astarte_mapping_check_path(mapping, incorrect_second_param_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));
}
