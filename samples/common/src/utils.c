/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "utils.h"

#include <stdlib.h>

#include <zephyr/logging/log.h>
#include <zephyr/sys/timeutil.h>

LOG_MODULE_REGISTER(utils, CONFIG_APP_LOG_LEVEL); // NOLINT

/************************************************
 * Constants, static variables and defines
 ***********************************************/

// Maximum size for the datetime string
#define DATETIME_MAX_STR_LEN 30

// NOLINTBEGIN(readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers)
const uint8_t utils_binary_blob_data[8] = { 0x53, 0x47, 0x56, 0x73, 0x62, 0x47, 0x38, 0x3d };
static const uint8_t binblob_1[8] = { 0x53, 0x47, 0x56, 0x73, 0x62, 0x47, 0x38, 0x3d };
static const uint8_t binblob_2[5] = { 0x64, 0x32, 0x39, 0x79, 0x62 };
const uint8_t *const utils_binary_blobs_data[2] = { binblob_1, binblob_2 };
const size_t utils_binary_blobs_sizes_data[2] = { ARRAY_SIZE(binblob_1), ARRAY_SIZE(binblob_2) };
const bool utils_boolean_data = true;
const bool utils_boolean_array_data[3] = { true, false, true };
const int64_t utils_unix_time_data = 1710940988984;
const int64_t utils_unix_time_array_data[1] = { 1710940988984 };
const double utils_double_data = 15.42;
const double utils_double_array_data[2] = { 1542.25, 88852.6 };
const int32_t utils_integer_data = 42;
const int32_t utils_integer_array_data[3] = { 4525, 0, 11 };
const int64_t utils_longinteger_data = 8589934592;
const int64_t utils_longinteger_array_data[3] = { 8589930067, 42, 8589934592 };
const char utils_string_data[] = "Hello world!";
const char *const utils_string_array_data[2] = { "Hello ", "world!" };
// NOLINTEND(readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers)

/************************************************
 * Global functions definition
 ***********************************************/

// NOLINTNEXTLINE(hicpp-function-size)
void utils_log_astarte_value(astarte_value_t value)
{
    struct tm *tm_obj = NULL;
    char tm_str[DATETIME_MAX_STR_LEN] = { 0 };

    switch (value.tag) {
        case ASTARTE_MAPPING_TYPE_BINARYBLOB:
            // NOLINTNEXTLINE
            LOG_HEXDUMP_INF(
                value.data.binaryblob.buf, value.data.binaryblob.len, "Astarte binaryblob:");
            break;
        case ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY:
            LOG_INF("Astarte binaryblobarray:"); // NOLINT
            for (size_t i = 0; i < value.data.binaryblob_array.count; i++) {
                // NOLINTNEXTLINE
                LOG_HEXDUMP_INF(
                    value.data.binaryblob_array.blobs[i], value.data.binaryblob_array.sizes[i], "");
            }
            break;
        case ASTARTE_MAPPING_TYPE_BOOLEAN:
            LOG_INF("Astarte boolean: %s", (value.data.boolean) ? "true" : "false"); // NOLINT
            break;
        case ASTARTE_MAPPING_TYPE_BOOLEANARRAY:
            LOG_INF("Astarte booleanarray:"); // NOLINT
            for (size_t i = 0; i < value.data.boolean_array.len; i++) {
                // NOLINTNEXTLINE
                LOG_INF("    %zi: %s", i, (value.data.boolean_array.buf[i]) ? "true" : "false");
            }
            break;
        case ASTARTE_MAPPING_TYPE_DATETIME:
            tm_obj = gmtime(&value.data.datetime);
            (void) strftime(tm_str, DATETIME_MAX_STR_LEN, "%Y-%m-%dT%H:%M:%S%z", tm_obj);
            LOG_INF("Astarte datetime: %s", tm_str); // NOLINT
            break;
        case ASTARTE_MAPPING_TYPE_DATETIMEARRAY:
            LOG_INF("Astarte datetimearray:"); // NOLINT
            for (size_t i = 0; i < value.data.datetime_array.len; i++) {
                tm_obj = gmtime(&value.data.datetime_array.buf[i]);
                (void) strftime(tm_str, DATETIME_MAX_STR_LEN, "%Y-%m-%dT%H:%M:%S%z", tm_obj);
                LOG_INF("    %zi: %s", i, tm_str); // NOLINT
            }
            break;
        case ASTARTE_MAPPING_TYPE_DOUBLE:
            LOG_INF("Astarte double: %f", value.data.dbl); // NOLINT
            break;
        case ASTARTE_MAPPING_TYPE_DOUBLEARRAY:
            LOG_INF("Astarte doublearray:"); // NOLINT
            for (size_t i = 0; i < value.data.double_array.len; i++) {
                LOG_INF("    %zi: %f", i, value.data.double_array.buf[i]); // NOLINT
            }
            break;
        case ASTARTE_MAPPING_TYPE_INTEGER:
            LOG_INF("Astarte integer: %i", value.data.integer); // NOLINT
            break;
        case ASTARTE_MAPPING_TYPE_INTEGERARRAY:
            LOG_INF("Astarte integerarray:"); // NOLINT
            for (size_t i = 0; i < value.data.integer_array.len; i++) {
                LOG_INF("    %zi: %i", i, value.data.integer_array.buf[i]); // NOLINT
            }
            break;
        case ASTARTE_MAPPING_TYPE_LONGINTEGER:
            LOG_INF("Astarte longinteger: %lli", value.data.longinteger); // NOLINT
            break;
        case ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY:
            LOG_INF("Astarte longintegerarray:"); // NOLINT
            for (size_t i = 0; i < value.data.longinteger_array.len; i++) {
                LOG_INF("    %zi: %lli", i, value.data.longinteger_array.buf[i]); // NOLINT
            }
            break;
        case ASTARTE_MAPPING_TYPE_STRING:
            LOG_INF("Astarte string: %s", value.data.string); // NOLINT
            break;
        case ASTARTE_MAPPING_TYPE_STRINGARRAY:
            LOG_INF("Astarte stringarray:"); // NOLINT
            for (size_t i = 0; i < value.data.string_array.len; i++) {
                LOG_INF("    %zi: %s", i, value.data.string_array.buf[i]); // NOLINT
            }
            break;
        default:
            LOG_ERR("Astarte value has invalid tag!"); // NOLINT
            break;
    }
}
