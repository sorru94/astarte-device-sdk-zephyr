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

#include <math.h>
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

ZTEST(astarte_device_sdk_mapping, test_astarte_mapping_check_path_one_segment_no_pattern)
{
    astarte_result_t res = ASTARTE_RESULT_OK;
    astarte_mapping_t mapping = {
        .endpoint = "/binaryblob_endpoint",
        .type = ASTARTE_MAPPING_TYPE_BINARYBLOB,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = true,
        .allow_unset = false,
    };

    const char empty_path[] = "";
    res = astarte_mapping_check_path(mapping, empty_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));

    const char single_slash_path[] = "/";
    res = astarte_mapping_check_path(mapping, single_slash_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));

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
    res = astarte_mapping_check_path(mapping, prefixed_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));
}

ZTEST(astarte_device_sdk_mapping, test_astarte_mapping_check_path_multiple_segments_no_pattern)
{
    astarte_result_t res = ASTARTE_RESULT_OK;
    astarte_mapping_t mapping = {
        .endpoint = "/first_segment/second_segment/third_segment",
        .type = ASTARTE_MAPPING_TYPE_BINARYBLOB,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = true,
        .allow_unset = false,
    };

    const char empty_path[] = "";
    res = astarte_mapping_check_path(mapping, empty_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));

    const char single_slash_path[] = "/";
    res = astarte_mapping_check_path(mapping, single_slash_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));

    const char partial_path[] = "/binaryblob_endpoint/second_segment";
    res = astarte_mapping_check_path(mapping, partial_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));

    const char correct_path[] = "/first_segment/second_segment/third_segment";
    res = astarte_mapping_check_path(mapping, correct_path);
    zassert_equal(res, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(res));

    const char mispelled_path[] = "/first_segment/second_segment/third_sigment";
    res = astarte_mapping_check_path(mapping, mispelled_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));

    const char shorter_path[] = "/first_segment/second_sgment/third_segment";
    res = astarte_mapping_check_path(mapping, shorter_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));

    const char missing_first_forwardslash_path[] = "first_segment/second_segment/third_segment";
    res = astarte_mapping_check_path(mapping, missing_first_forwardslash_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));

    const char missing_second_forwardslash_path[] = "/first_segmentsecond_segment/third_segment";
    res = astarte_mapping_check_path(mapping, missing_second_forwardslash_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));

    const char missing_last_forwardslash_path[] = "/first_segment/second_segmentthird_segment";
    res = astarte_mapping_check_path(mapping, missing_last_forwardslash_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));

    const char suffixed_path[] = "/first_segment/second_segment/third_segmentt";
    res = astarte_mapping_check_path(mapping, suffixed_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));

    const char slash_suffixed_path[] = "/first_segment/second_segment/third_segment/";
    res = astarte_mapping_check_path(mapping, slash_suffixed_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));

    const char prefixed_path[] = "prefix/first_segment/second_segment/third_segment";
    res = astarte_mapping_check_path(mapping, prefixed_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));
}

ZTEST(astarte_device_sdk_mapping, test_astarte_mapping_check_path_one_segment_single_pattern)
{
    astarte_result_t res = ASTARTE_RESULT_OK;
    astarte_mapping_t mapping = {
        .endpoint = "/%{sensor_id}",
        .type = ASTARTE_MAPPING_TYPE_DOUBLE,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNIQUE,
        .explicit_timestamp = false,
        .allow_unset = true,
    };

    const char correct_path[] = "/some_sensor_name";
    res = astarte_mapping_check_path(mapping, correct_path);
    zassert_equal(res, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(res));

    const char empty_param_path[] = "/";
    res = astarte_mapping_check_path(mapping, empty_param_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));

    const char non_allowed_hash_path[] = "/some_sen#sor_name";
    res = astarte_mapping_check_path(mapping, non_allowed_hash_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));

    const char non_allowed_plus_path[] = "/some_sensor_name+";
    res = astarte_mapping_check_path(mapping, non_allowed_plus_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));

    const char non_allowed_slash_path[] = "/som/e_sensor_name";
    res = astarte_mapping_check_path(mapping, non_allowed_slash_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));
}

ZTEST(astarte_device_sdk_mapping, test_astarte_mapping_check_path_multiple_segments_single_pattern)
{
    astarte_result_t res = ASTARTE_RESULT_OK;
    astarte_mapping_t mapping = {
        .endpoint = "/first_segment/%{sensor_id}/second_segment",
        .type = ASTARTE_MAPPING_TYPE_DOUBLE,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNIQUE,
        .explicit_timestamp = false,
        .allow_unset = true,
    };

    const char correct_path[] = "/first_segment/sensor_42/second_segment";
    res = astarte_mapping_check_path(mapping, correct_path);
    zassert_equal(res, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(res));

    const char mispelled_path[] = "/first_segment/sensor_42/sepond_segment";
    res = astarte_mapping_check_path(mapping, mispelled_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));

    const char missing_param_path[] = "/first_segment/second_segment";
    res = astarte_mapping_check_path(mapping, missing_param_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));

    const char empty_param_path[] = "/first_segment//second_segment";
    res = astarte_mapping_check_path(mapping, empty_param_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));

    const char non_allowed_slash_path[] = "/first_segment/senso/r_42/second_segment";
    res = astarte_mapping_check_path(mapping, non_allowed_slash_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));

    const char non_allowed_hash_path[] = "/first_segment/#sensor_42/second_segment";
    res = astarte_mapping_check_path(mapping, non_allowed_hash_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));

    const char non_allowed_plus_path[] = "/first_segment/sensor_+42/second_segment";
    res = astarte_mapping_check_path(mapping, non_allowed_plus_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));
}

ZTEST(astarte_device_sdk_mapping, test_astarte_mapping_check_path_multiple_segments_three_patterns)
{
    astarte_result_t res = ASTARTE_RESULT_OK;
    astarte_mapping_t mapping = {
        .endpoint = "/%{first_param}/first_segment/%{second_param}/second_segment/%{third_param}",
        .type = ASTARTE_MAPPING_TYPE_DOUBLE,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNIQUE,
        .explicit_timestamp = false,
        .allow_unset = true,
    };

    const char correct_path[] = "/sensor_42/first_segment/sens.or_11/second_segment/sensor_54";
    res = astarte_mapping_check_path(mapping, correct_path);
    zassert_equal(res, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(res));

    const char mispelled_path[] = "/sensor_42/first_egment/sens.or_11/second_segment/sensor_54";
    res = astarte_mapping_check_path(mapping, mispelled_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));

    const char missing_first_param_path[] = "/first_segment/sens.or_11/second_segment/sensor_54";
    res = astarte_mapping_check_path(mapping, missing_first_param_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));

    const char missing_second_param_path[] = "/sensor_42/first_segment/second_segment/sensor_54";
    res = astarte_mapping_check_path(mapping, missing_second_param_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));

    const char missing_third_param_path[] = "/sensor_42/first_segment/sens.or_11/second_segment";
    res = astarte_mapping_check_path(mapping, missing_third_param_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));

    const char empty_third_param_path[] = "/sensor_42/first_segment/sens.or_11/second_segment/";
    res = astarte_mapping_check_path(mapping, empty_third_param_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));

    const char non_allowed_char_first_param_path[] = "/+s42/first_segment/s11/second_segment/s54";
    res = astarte_mapping_check_path(mapping, non_allowed_char_first_param_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));

    const char non_allowed_char_second_param_path[] = "/s42/first_segment/#s11/second_segment/s54";
    res = astarte_mapping_check_path(mapping, non_allowed_char_second_param_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));

    const char non_allowed_char_third_param_path[] = "/s42/first_segment/s11/second_segment/s54#";
    res = astarte_mapping_check_path(mapping, non_allowed_char_third_param_path);
    zassert_equal(res, ASTARTE_RESULT_MAPPING_PATH_MISMATCH, "Res:%s", astarte_result_to_name(res));
}

ZTEST(astarte_device_sdk_mapping, test_astarte_mapping_check_data_double)
{
    astarte_result_t res = ASTARTE_RESULT_OK;
    astarte_mapping_t mapping = {
        .endpoint = "/%{sensor_id}/double_endpoint",
        .type = ASTARTE_MAPPING_TYPE_DOUBLE,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNIQUE,
        .explicit_timestamp = false,
        .allow_unset = true,
    };

    astarte_data_t double_data_ok = astarte_data_from_double(42.3);
    res = astarte_mapping_check_data(&mapping, double_data_ok);
    zassert_equal(res, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(res));

    astarte_data_t double_data_nan = astarte_data_from_double(NAN);
    res = astarte_mapping_check_data(&mapping, double_data_nan);
    zassert_equal(
        res, ASTARTE_RESULT_MAPPING_DATA_INCOMPATIBLE, "Res:%s", astarte_result_to_name(res));

    astarte_data_t double_data_inf = astarte_data_from_double(INFINITY);
    res = astarte_mapping_check_data(&mapping, double_data_inf);
    zassert_equal(
        res, ASTARTE_RESULT_MAPPING_DATA_INCOMPATIBLE, "Res:%s", astarte_result_to_name(res));

    astarte_data_t integer_data = astarte_data_from_integer(42);
    res = astarte_mapping_check_data(&mapping, integer_data);
    zassert_equal(
        res, ASTARTE_RESULT_MAPPING_DATA_INCOMPATIBLE, "Res:%s", astarte_result_to_name(res));
}

ZTEST(astarte_device_sdk_mapping, test_astarte_mapping_check_data_doublearray)
{
    astarte_result_t res = ASTARTE_RESULT_OK;
    astarte_mapping_t mapping = {
        .endpoint = "/%{sensor_id}/doublearray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_DOUBLEARRAY,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNIQUE,
        .explicit_timestamp = false,
        .allow_unset = true,
    };

    double doublearray_ok[] = { 12.4, 23.4 };
    astarte_data_t doublearray_data_ok
        = astarte_data_from_double_array(doublearray_ok, ARRAY_SIZE(doublearray_ok));
    res = astarte_mapping_check_data(&mapping, doublearray_data_ok);
    zassert_equal(res, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(res));

    double doublearray_nan[] = { 12.4, NAN, 23.4 };
    astarte_data_t doublearray_data_nan
        = astarte_data_from_double_array(doublearray_nan, ARRAY_SIZE(doublearray_nan));
    res = astarte_mapping_check_data(&mapping, doublearray_data_nan);
    zassert_equal(
        res, ASTARTE_RESULT_MAPPING_DATA_INCOMPATIBLE, "Res:%s", astarte_result_to_name(res));

    double doublearray_inf[] = { 12.4, INFINITY, 23.4 };
    astarte_data_t doublearray_data_inf
        = astarte_data_from_double_array(doublearray_inf, ARRAY_SIZE(doublearray_inf));
    res = astarte_mapping_check_data(&mapping, doublearray_data_inf);
    zassert_equal(
        res, ASTARTE_RESULT_MAPPING_DATA_INCOMPATIBLE, "Res:%s", astarte_result_to_name(res));
}
