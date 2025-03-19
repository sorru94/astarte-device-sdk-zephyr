/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "property_send.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <astarte_device_sdk/data.h>
#include <astarte_device_sdk/result.h>

#include "generated_interfaces.h"
#include "utils.h"

LOG_MODULE_REGISTER(property_send, CONFIG_PROPERTY_LOG_LEVEL); // NOLINT

static const char *const paths[] = {
    "/sensor44/binaryblob_endpoint",
    "/sensor44/binaryblobarray_endpoint",
    "/sensor44/boolean_endpoint",
    "/sensor44/booleanarray_endpoint",
    "/sensor44/datetime_endpoint",
    "/sensor44/datetimearray_endpoint",
    "/sensor44/double_endpoint",
    "/sensor44/doublearray_endpoint",
    "/sensor44/integer_endpoint",
    "/sensor44/integerarray_endpoint",
    "/sensor44/longinteger_endpoint",
    "/sensor44/longintegerarray_endpoint",
    "/sensor44/string_endpoint",
    "/sensor44/stringarray_endpoint",
};

void sample_property_set_transmission(astarte_device_handle_t device)
{
    const char *interface_name = org_astarteplatform_zephyr_examples_DeviceProperty.name;
    // Wait for the predefined time

    LOG_INF("Setting some properties using the Astarte device."); // NOLINT

    astarte_data_t individuals[] = {
        astarte_data_from_binaryblob(
            (void *) utils_binary_blob_data, ARRAY_SIZE(utils_binary_blob_data)),
        astarte_data_from_binaryblob_array((const void **) utils_binary_blobs_data,
            (size_t *) utils_binary_blobs_sizes_data, ARRAY_SIZE(utils_binary_blobs_data)),
        astarte_data_from_boolean(utils_boolean_data),
        astarte_data_from_boolean_array(
            (bool *) utils_boolean_array_data, ARRAY_SIZE(utils_boolean_array_data)),
        astarte_data_from_datetime(utils_unix_time_data),
        astarte_data_from_datetime_array(
            (int64_t *) utils_unix_time_array_data, ARRAY_SIZE(utils_unix_time_array_data)),
        astarte_data_from_double(utils_double_data),
        astarte_data_from_double_array(
            (double *) utils_double_array_data, ARRAY_SIZE(utils_double_array_data)),
        astarte_data_from_integer(utils_integer_data),
        astarte_data_from_integer_array(
            (int32_t *) utils_integer_array_data, ARRAY_SIZE(utils_integer_array_data)),
        astarte_data_from_longinteger(utils_longinteger_data),
        astarte_data_from_longinteger_array(
            (int64_t *) utils_longinteger_array_data, ARRAY_SIZE(utils_longinteger_array_data)),
        astarte_data_from_string(utils_string_data),
        astarte_data_from_string_array(
            (const char **) utils_string_array_data, ARRAY_SIZE(utils_string_array_data)),
    };

    BUILD_ASSERT(ARRAY_SIZE(individuals) == ARRAY_SIZE(paths),
        "The number of paths does not match the number of individuals");
    for (size_t i = 0; i < ARRAY_SIZE(individuals); i++) {
        LOG_INF("Setting on %s:", paths[i]); // NOLINT
        utils_log_astarte_data(individuals[i]);
        astarte_result_t res
            = astarte_device_set_property(device, interface_name, paths[i], individuals[i]);
        if (res != ASTARTE_RESULT_OK) {
            LOG_INF("Astarte device transmission failure."); // NOLINT
        }
    }

    LOG_INF("Setting properties completed."); // NOLINT
}

void sample_property_unset_transmission(astarte_device_handle_t device)
{
    const char *interface_name = org_astarteplatform_zephyr_examples_DeviceProperty.name;

    LOG_INF("Unsetting some properties using the Astarte device."); // NOLINT

    for (size_t i = 0; i < ARRAY_SIZE(paths); i++) {
        LOG_INF("Unsetting %s:", paths[i]); // NOLINT
        astarte_result_t res = astarte_device_unset_property(device, interface_name, paths[i]);
        if (res != ASTARTE_RESULT_OK) {
            LOG_INF("Astarte device transmission failure."); // NOLINT
        }
    }

    LOG_INF("Unsetting properties completed."); // NOLINT
}
