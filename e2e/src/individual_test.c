/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "individual_test.h"

#include <stdint.h>
#include <stdlib.h>

#include <zephyr/fatal.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/http/client.h>
#include <zephyr/net/socket.h>
#include <zephyr/posix/arpa/inet.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#include <astarte_device_sdk/individual.h>
#include <astarte_device_sdk/interface.h>
#include <astarte_device_sdk/result.h>

#include "e2edata.h"
#include "e2erunner.h"
#include "e2eutilities.h"

#include "generated_interfaces.h"

LOG_MODULE_REGISTER(individual, CONFIG_INDIVIDUAL_LOG_LEVEL); // NOLINT

/************************************************
 * Constants, static variables and defines
 ***********************************************/

#define BINARYBLOB_PATH "/binaryblob_endpoint"
#define BINARYBLOB_ARRAY_PATH "/binaryblobarray_endpoint"
#define BOOLEAN_PATH "/boolean_endpoint"
#define BOOLEAN_ARRAY_PATH "/booleanarray_endpoint"
#define DATETIME_PATH "/datetime_endpoint"
#define DATETIME_ARRAY_PATH "/datetimearray_endpoint"
#define DOUBLE_PATH "/double_endpoint"
#define DOUBLE_ARRAY_PATH "/doublearray_endpoint"
#define INTEGER_PATH "/integer_endpoint"
#define INTEGER_ARRAY_PATH "/integerarray_endpoint"
#define LONGINTEGER_PATH "/longinteger_endpoint"
#define LONGINTEGER_ARRAY_PATH "/longintegerarray_endpoint"
#define STRING_PATH "/string_endpoint"
#define STRING_ARRAY_PATH "/stringarray_endpoint"

const e2e_timestamp_option_t timestamp = { .value = 1716554316, .present = true };

// clang-format off
static const e2e_individual_data_t device_individual_data[] = {
    {
        .path = BINARYBLOB_PATH,
        .timestamp = timestamp,
        .individual = MAKE_ASTARTE_INDIVIDUAL_ARRAY(ASTARTE_MAPPING_TYPE_BINARYBLOB, binaryblob,
            (void *) binary_blob_data, ARRAY_SIZE(binary_blob_data))
    },
    {
        .path = BINARYBLOB_ARRAY_PATH,
        .timestamp = timestamp,
        .individual = MAKE_ASTARTE_INDIVIDUAL_BINARYBLOB_ARRAY(
            (const void **) binary_blob_array_data, (size_t *) binary_blob_array_sizes_data,
            ARRAY_SIZE(binary_blob_array_data))
    },
    {
        .path = BOOLEAN_PATH,
        .timestamp = timestamp,
        .individual = MAKE_ASTARTE_INDIVIDUAL_SCALAR(ASTARTE_MAPPING_TYPE_BOOLEAN, boolean,
            BOOLEAN_DATA)
    },
    {
        .path = BOOLEAN_ARRAY_PATH,
        .timestamp = timestamp,
        .individual =  MAKE_ASTARTE_INDIVIDUAL_ARRAY(ASTARTE_MAPPING_TYPE_BOOLEANARRAY,
            boolean_array, (bool *) boolean_array_data, ARRAY_SIZE(boolean_array_data))
    },
    {
        .path = DATETIME_PATH,
        .timestamp = timestamp,
        .individual = MAKE_ASTARTE_INDIVIDUAL_SCALAR(ASTARTE_MAPPING_TYPE_DATETIME, datetime,
            DATE_TIME_DATA)
    },
    {
        .path = DATETIME_ARRAY_PATH,
        .timestamp = timestamp,
        .individual = MAKE_ASTARTE_INDIVIDUAL_ARRAY(ASTARTE_MAPPING_TYPE_DATETIMEARRAY,
            datetime_array, (int64_t *) date_time_array_data,
            ARRAY_SIZE(date_time_array_data))
    },
    {
        .path = DOUBLE_PATH,
        .timestamp = timestamp,
        .individual = MAKE_ASTARTE_INDIVIDUAL_SCALAR(ASTARTE_MAPPING_TYPE_DOUBLE, dbl, DOUBLE_DATA)
    },
    {
        .path = DOUBLE_ARRAY_PATH,
        .timestamp = timestamp,
        .individual = MAKE_ASTARTE_INDIVIDUAL_ARRAY(ASTARTE_MAPPING_TYPE_DOUBLEARRAY,
            double_array, (double *) double_array_data, ARRAY_SIZE(double_array_data))
    },
    {
        .path = INTEGER_PATH,
        .timestamp = timestamp,
        .individual = MAKE_ASTARTE_INDIVIDUAL_SCALAR(ASTARTE_MAPPING_TYPE_INTEGER, integer,
            INTEGER_DATA)
    },
    {
        .path = INTEGER_ARRAY_PATH,
        .timestamp = timestamp,
        .individual = MAKE_ASTARTE_INDIVIDUAL_ARRAY(ASTARTE_MAPPING_TYPE_INTEGERARRAY,
            integer_array, (int32_t *) integer_array_data, ARRAY_SIZE(integer_array_data))
    },
    {
        .path = LONGINTEGER_PATH,
        .timestamp = timestamp,
        .individual = MAKE_ASTARTE_INDIVIDUAL_SCALAR(ASTARTE_MAPPING_TYPE_LONGINTEGER,
            longinteger, LONGINTEGER_DATA)
    },
    {
        .path = LONGINTEGER_ARRAY_PATH,
        .timestamp = timestamp,
        .individual = MAKE_ASTARTE_INDIVIDUAL_ARRAY(ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY,
            longinteger_array, (int64_t *) longinteger_array_data,
            ARRAY_SIZE(longinteger_array_data))
    },
    {
        .path = STRING_PATH,
        .timestamp = timestamp,
        .individual = MAKE_ASTARTE_INDIVIDUAL_SCALAR(ASTARTE_MAPPING_TYPE_STRING, string,
            string_data)
    },
    {
        .path = STRING_ARRAY_PATH,
        .timestamp = timestamp,
        .individual = MAKE_ASTARTE_INDIVIDUAL_ARRAY(ASTARTE_MAPPING_TYPE_STRINGARRAY,
            string_array, (const char **) string_array_data, ARRAY_SIZE(string_array_data))
    }
};
static e2e_interface_data_t device_interface_data[] = {
    {
        .interface = &org_astarte_platform_zephyr_e2etest_DeviceDatastream,
        .values = {
            .individual = {
                .buf = device_individual_data,
                .len = ARRAY_SIZE(device_individual_data)
            },
        },
    }
};
static const e2e_individual_data_t server_individual_data[] = {
    {
        .path = BINARYBLOB_PATH,
        .individual = MAKE_ASTARTE_INDIVIDUAL_ARRAY(ASTARTE_MAPPING_TYPE_BINARYBLOB, binaryblob,
            (void *) binary_blob_data, ARRAY_SIZE(binary_blob_data))
    },
    {
        .path = BINARYBLOB_ARRAY_PATH,
        .individual = MAKE_ASTARTE_INDIVIDUAL_BINARYBLOB_ARRAY(
            (const void **) binary_blob_array_data, (size_t *) binary_blob_array_sizes_data,
            ARRAY_SIZE(binary_blob_array_data))
    },
    {
        .path = BOOLEAN_PATH,
        .individual = MAKE_ASTARTE_INDIVIDUAL_SCALAR(ASTARTE_MAPPING_TYPE_BOOLEAN, boolean,
            BOOLEAN_DATA)
    },
    {
        .path = BOOLEAN_ARRAY_PATH,
        .individual =  MAKE_ASTARTE_INDIVIDUAL_ARRAY(ASTARTE_MAPPING_TYPE_BOOLEANARRAY,
            boolean_array, (bool *) boolean_array_data, ARRAY_SIZE(boolean_array_data))
    },
    {
        .path = DATETIME_PATH,
        .individual = MAKE_ASTARTE_INDIVIDUAL_SCALAR(ASTARTE_MAPPING_TYPE_DATETIME, datetime,
            DATE_TIME_DATA)
    },
    {
        .path = DATETIME_ARRAY_PATH,
        .individual = MAKE_ASTARTE_INDIVIDUAL_ARRAY(ASTARTE_MAPPING_TYPE_DATETIMEARRAY,
            datetime_array, (int64_t *) date_time_array_data,
            ARRAY_SIZE(date_time_array_data))
    },
    {
        .path = DOUBLE_PATH,
        .individual = MAKE_ASTARTE_INDIVIDUAL_SCALAR(ASTARTE_MAPPING_TYPE_DOUBLE, dbl, DOUBLE_DATA)
    },
    {
        .path = DOUBLE_ARRAY_PATH,
        .individual = MAKE_ASTARTE_INDIVIDUAL_ARRAY(ASTARTE_MAPPING_TYPE_DOUBLEARRAY,
            double_array, (double *) double_array_data, ARRAY_SIZE(double_array_data))
    },
    {
        .path = INTEGER_PATH,
        .individual = MAKE_ASTARTE_INDIVIDUAL_SCALAR(ASTARTE_MAPPING_TYPE_INTEGER, integer,
            INTEGER_DATA)
    },
    {
        .path = INTEGER_ARRAY_PATH,
        .individual = MAKE_ASTARTE_INDIVIDUAL_ARRAY(ASTARTE_MAPPING_TYPE_INTEGERARRAY,
            integer_array, (int32_t *) integer_array_data, ARRAY_SIZE(integer_array_data))
    },
    {
        .path = LONGINTEGER_PATH,
        .individual = MAKE_ASTARTE_INDIVIDUAL_SCALAR(ASTARTE_MAPPING_TYPE_LONGINTEGER,
            longinteger, LONGINTEGER_DATA)
    },
    {
        .path = LONGINTEGER_ARRAY_PATH,
        .individual = MAKE_ASTARTE_INDIVIDUAL_ARRAY(ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY,
            longinteger_array, (int64_t *) longinteger_array_data,
            ARRAY_SIZE(longinteger_array_data))
    },
    {
        .path = STRING_PATH,
        .individual = MAKE_ASTARTE_INDIVIDUAL_SCALAR(ASTARTE_MAPPING_TYPE_STRING, string,
            string_data)
    },
    {
        .path = STRING_ARRAY_PATH,
        .individual = MAKE_ASTARTE_INDIVIDUAL_ARRAY(ASTARTE_MAPPING_TYPE_STRINGARRAY,
            string_array, (const char **) string_array_data, ARRAY_SIZE(string_array_data))
    }
};
static e2e_interface_data_t server_interface_data[] = {
    {
        .interface = &org_astarte_platform_zephyr_e2etest_ServerDatastream,
        // check that the type of the interface matches the passed union
        .values = {
            .individual = {
                .buf = server_individual_data,
                .len = ARRAY_SIZE(server_individual_data)
            },
        },
    },
};
// clang-format on

/************************************************
 * Static functions declaration
 ***********************************************/

static e2e_test_data_t setup_test_data();

/************************************************
 * Global functions definition
 ***********************************************/

e2e_device_config_t get_individual_test_config()
{
    return (e2e_device_config_t){
        .device_id = CONFIG_DEVICE_ID,
        .cred_secr = CONFIG_CREDENTIAL_SECRET,
        .setup = setup_test_data,
    };
}

/************************************************
 * Static functions definitions
 ***********************************************/

static e2e_test_data_t setup_test_data()
{
    return (e2e_test_data_t) {
        .device_sent = {
            .buf = device_interface_data,
            .len = ARRAY_SIZE(device_interface_data),
        },
        .server_sent = {
            .buf = server_interface_data,
            .len = ARRAY_SIZE(server_interface_data),
        }
    };
}
