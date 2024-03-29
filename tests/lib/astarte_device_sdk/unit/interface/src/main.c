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

#include "astarte_device_sdk/interface.h"
#include "interface_private.h"
#include "lib/astarte_device_sdk/interface.c"

ZTEST_SUITE(astarte_device_sdk_interface, NULL, NULL, NULL, NULL, NULL);

// Define a minimal_log function to resolve the `undefined reference to z_log_minimal_printk` error,
// because the log environment is missing in the unit_testing platform.
void z_log_minimal_printk(const char *fmt, ...) {}

ZTEST(astarte_device_sdk_interface, test_astarte_interface_get_mapping)
{
    astarte_result_t res = ASTARTE_RESULT_OK;
    astarte_mapping_t *mapping = NULL;

    const astarte_mapping_t mappings[3]
        = { {
                .endpoint = "/binaryblob_endpoint",
                .regex_endpoint = "/binaryblob_endpoint",
                .type = ASTARTE_MAPPING_TYPE_BINARYBLOB,
                .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
                .explicit_timestamp = true,
                .allow_unset = false,
            },
              {
                  .endpoint = "/binaryblobarray_endpoint",
                  .regex_endpoint = "/binaryblobarray_endpoint",
                  .type = ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY,
                  .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
                  .explicit_timestamp = true,
                  .allow_unset = false,
              },
              {
                  .endpoint = "/boolean_endpoint",
                  .regex_endpoint = "/boolean_endpoint",
                  .type = ASTARTE_MAPPING_TYPE_BOOLEAN,
                  .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
                  .explicit_timestamp = true,
                  .allow_unset = false,
              } };

    const astarte_interface_t interface = {
        .name = "org.astarteplatform.zephyr.test",
        .major_version = 0,
        .minor_version = 1,
        .type = ASTARTE_INTERFACE_TYPE_DATASTREAM,
        .ownership = ASTARTE_INTERFACE_OWNERSHIP_DEVICE,
        .aggregation = ASTARTE_INTERFACE_AGGREGATION_INDIVIDUAL,
        .mappings = mappings,
        .mappings_length = 3U,
    };

    const char path_first_endpoint[] = "/binaryblob_endpoint";
    mapping = NULL;
    res = astarte_interface_get_mapping(&interface, path_first_endpoint, &mapping);
    zassert_equal(res, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(res));
    zassert_equal_ptr(mapping, &mappings[0]);

    const char path_second_endpoint[] = "/binaryblobarray_endpoint";
    mapping = NULL;
    res = astarte_interface_get_mapping(&interface, path_second_endpoint, &mapping);
    zassert_equal(res, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(res));
    zassert_equal_ptr(mapping, &mappings[1]);

    const char path_third_endpoint[] = "/boolean_endpoint";
    mapping = NULL;
    res = astarte_interface_get_mapping(&interface, path_third_endpoint, &mapping);
    zassert_equal(res, ASTARTE_RESULT_OK, "Res:%s", astarte_result_to_name(res));
    zassert_equal_ptr(mapping, &mappings[2]);

    const char path_missing_endpoint[] = "/missing_endpoint";
    mapping = NULL;
    res = astarte_interface_get_mapping(&interface, path_missing_endpoint, &mapping);
    zassert_equal(
        res, ASTARTE_RESULT_MAPPING_NOT_IN_INTERFACE, "Res: %s", astarte_result_to_name(res));
}
