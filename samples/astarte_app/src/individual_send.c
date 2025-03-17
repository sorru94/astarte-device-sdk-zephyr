/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "individual_send.h"

#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <astarte_device_sdk/individual.h>

#include "astarte_device_sdk/result.h"
#include "generated_interfaces.h"
#include "utils.h"

LOG_MODULE_REGISTER(individual_send, CONFIG_INDIVIDUAL_LOG_LEVEL); // NOLINT

void sample_individual_transmission(astarte_device_handle_t device)
{
    LOG_INF("Sending some individuals using the Astarte device."); // NOLINT
    const char *interface_name = org_astarteplatform_zephyr_examples_DeviceDatastream.name;

    astarte_individual_t individuals[]
        = { astarte_individual_from_binaryblob(
                (void *) utils_binary_blob_data, ARRAY_SIZE(utils_binary_blob_data)),
              astarte_individual_from_binaryblob_array((const void **) utils_binary_blobs_data,
                  (size_t *) utils_binary_blobs_sizes_data, ARRAY_SIZE(utils_binary_blobs_data)),
              astarte_individual_from_boolean(utils_boolean_data),
              astarte_individual_from_boolean_array(
                  (bool *) utils_boolean_array_data, ARRAY_SIZE(utils_boolean_array_data)),
              astarte_individual_from_datetime(utils_unix_time_data),
              astarte_individual_from_datetime_array(
                  (int64_t *) utils_unix_time_array_data, ARRAY_SIZE(utils_unix_time_array_data)),
              astarte_individual_from_double(utils_double_data),
              astarte_individual_from_double_array(
                  (double *) utils_double_array_data, ARRAY_SIZE(utils_double_array_data)),
              astarte_individual_from_integer(utils_integer_data),
              astarte_individual_from_integer_array(
                  (int32_t *) utils_integer_array_data, ARRAY_SIZE(utils_integer_array_data)),
              astarte_individual_from_longinteger(utils_longinteger_data),
              astarte_individual_from_longinteger_array((int64_t *) utils_longinteger_array_data,
                  ARRAY_SIZE(utils_longinteger_array_data)),
              astarte_individual_from_string(utils_string_data),
              astarte_individual_from_string_array(
                  (const char **) utils_string_array_data, ARRAY_SIZE(utils_string_array_data)) };

    const char *paths[] = {
        "/binaryblob_endpoint",
        "/binaryblobarray_endpoint",
        "/boolean_endpoint",
        "/booleanarray_endpoint",
        "/datetime_endpoint",
        "/datetimearray_endpoint",
        "/double_endpoint",
        "/doublearray_endpoint",
        "/integer_endpoint",
        "/integerarray_endpoint",
        "/longinteger_endpoint",
        "/longintegerarray_endpoint",
        "/string_endpoint",
        "/stringarray_endpoint",
    };

    const int64_t tms = 1714748755;

    BUILD_ASSERT(ARRAY_SIZE(individuals) == ARRAY_SIZE(paths),
        "The number of paths does not match the number of individuals");
    for (size_t i = 0; i < ARRAY_SIZE(individuals); i++) {
        LOG_INF("Stream on %s:", paths[i]); // NOLINT
        utils_log_astarte_individual(individuals[i]);
        astarte_result_t res = astarte_device_send_individual(
            device, interface_name, paths[i], individuals[i], &tms);
        if (res != ASTARTE_RESULT_OK) {
            LOG_INF("Astarte device transmission failure."); // NOLINT
        }
    }

    LOG_INF("Individual transmission completed."); // NOLINT
}
