/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "object_test.h"

#include <stdint.h>
#include <stdlib.h>

#include <zephyr/fatal.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/http/client.h>
#include <zephyr/net/socket.h>
#include <zephyr/posix/arpa/inet.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#include <astarte_device_sdk/interface.h>
#include <astarte_device_sdk/object.h>
#include <astarte_device_sdk/result.h>

#include "e2edata.h"
#include "e2erunner.h"
#include "e2eutilities.h"

#include "generated_interfaces.h"

LOG_MODULE_REGISTER(object, CONFIG_OBJECT_LOG_LEVEL); // NOLINT

/************************************************
 * Constants, static variables and defines
 ***********************************************/

#define BINARYBLOB_PATH "binaryblob_endpoint"
#define BINARYBLOB_ARRAY_PATH "binaryblobarray_endpoint"
#define BOOLEAN_PATH "boolean_endpoint"
#define BOOLEAN_ARRAY_PATH "booleanarray_endpoint"
#define DATETIME_PATH "datetime_endpoint"
#define DATETIME_ARRAY_PATH "datetimearray_endpoint"
#define DOUBLE_PATH "double_endpoint"
#define DOUBLE_ARRAY_PATH "doublearray_endpoint"
#define INTEGER_PATH "integer_endpoint"
#define INTEGER_ARRAY_PATH "integerarray_endpoint"
#define LONGINTEGER_PATH "longinteger_endpoint"
#define LONGINTEGER_ARRAY_PATH "longintegerarray_endpoint"
#define STRING_PATH "string_endpoint"
#define STRING_ARRAY_PATH "stringarray_endpoint"

static const astarte_object_entry_t entries[] = {
    { .endpoint = BINARYBLOB_PATH,
        .individual = MAKE_ASTARTE_INDIVIDUAL_ARRAY(ASTARTE_MAPPING_TYPE_BINARYBLOB, binaryblob,
            (void *) binary_blob_data, ARRAY_SIZE(binary_blob_data)) },
    { .endpoint = BINARYBLOB_ARRAY_PATH,
        .individual
        = MAKE_ASTARTE_INDIVIDUAL_BINARYBLOB_ARRAY((const void **) binary_blob_array_data,
            (size_t *) binary_blob_array_sizes_data, ARRAY_SIZE(binary_blob_array_data)) },
    { .endpoint = BOOLEAN_PATH,
        .individual
        = MAKE_ASTARTE_INDIVIDUAL_SCALAR(ASTARTE_MAPPING_TYPE_BOOLEAN, boolean, BOOLEAN_DATA) },
    { .endpoint = BOOLEAN_ARRAY_PATH,
        .individual = MAKE_ASTARTE_INDIVIDUAL_ARRAY(ASTARTE_MAPPING_TYPE_BOOLEANARRAY,
            boolean_array, (bool *) boolean_array_data, ARRAY_SIZE(boolean_array_data)) },
    { .endpoint = DATETIME_PATH,
        .individual
        = MAKE_ASTARTE_INDIVIDUAL_SCALAR(ASTARTE_MAPPING_TYPE_DATETIME, datetime, DATE_TIME_DATA) },
    { .endpoint = DATETIME_ARRAY_PATH,
        .individual = MAKE_ASTARTE_INDIVIDUAL_ARRAY(ASTARTE_MAPPING_TYPE_DATETIMEARRAY,
            datetime_array, (int64_t *) date_time_array_data, ARRAY_SIZE(date_time_array_data)) },
    { .endpoint = DOUBLE_PATH,
        .individual
        = MAKE_ASTARTE_INDIVIDUAL_SCALAR(ASTARTE_MAPPING_TYPE_DOUBLE, dbl, DOUBLE_DATA) },
    { .endpoint = DOUBLE_ARRAY_PATH,
        .individual = MAKE_ASTARTE_INDIVIDUAL_ARRAY(ASTARTE_MAPPING_TYPE_DOUBLEARRAY, double_array,
            (double *) double_array_data, ARRAY_SIZE(double_array_data)) },
    { .endpoint = INTEGER_PATH,
        .individual
        = MAKE_ASTARTE_INDIVIDUAL_SCALAR(ASTARTE_MAPPING_TYPE_INTEGER, integer, INTEGER_DATA) },
    { .endpoint = INTEGER_ARRAY_PATH,
        .individual = MAKE_ASTARTE_INDIVIDUAL_ARRAY(ASTARTE_MAPPING_TYPE_INTEGERARRAY,
            integer_array, (int32_t *) integer_array_data, ARRAY_SIZE(integer_array_data)) },
    { .endpoint = LONGINTEGER_PATH,
        .individual = MAKE_ASTARTE_INDIVIDUAL_SCALAR(
            ASTARTE_MAPPING_TYPE_LONGINTEGER, longinteger, LONGINTEGER_DATA) },
    { .endpoint = LONGINTEGER_ARRAY_PATH,
        .individual
        = MAKE_ASTARTE_INDIVIDUAL_ARRAY(ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY, longinteger_array,
            (int64_t *) longinteger_array_data, ARRAY_SIZE(longinteger_array_data)) },
    { .endpoint = STRING_PATH,
        .individual
        = MAKE_ASTARTE_INDIVIDUAL_SCALAR(ASTARTE_MAPPING_TYPE_STRING, string, string_data) },
    { .endpoint = STRING_ARRAY_PATH,
        .individual = MAKE_ASTARTE_INDIVIDUAL_ARRAY(ASTARTE_MAPPING_TYPE_STRINGARRAY, string_array,
            (const char **) string_array_data, ARRAY_SIZE(string_array_data)) }
};

static e2e_interface_data_t device_interface_data[] = {
    {
        .interface = &org_astarte_platform_zephyr_e2etest_DeviceAggregate,
        // check that the type of the interface matches the passed union
        .values = {
            .object = {
                .path = "/sensor42",
                .entries = {
                    .buf = entries,
                    .len = ARRAY_SIZE(entries),
                }
            },
        },
    }
};

/************************************************
 * Static functions declaration
 ***********************************************/

static e2e_test_data_t setup_test_data();

/************************************************
 * Global functions definition
 ***********************************************/

e2e_device_config_t get_object_test_config()
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
    return (e2e_test_data_t){ .device_sent = {
                                  .buf = device_interface_data,
                                  .len = ARRAY_SIZE(device_interface_data),
                              } };
}
