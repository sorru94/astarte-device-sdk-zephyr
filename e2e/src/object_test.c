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

#include "e2erunner.h"
#include "e2eutilities.h"
#include "utils.h"

#include "generated_interfaces.h"

LOG_MODULE_REGISTER(object, CONFIG_OBJECT_LOG_LEVEL); // NOLINT

/************************************************
 * Constants, static variables and defines
 ***********************************************/

static const char *const binaryblob_endpoint = "binaryblob_endpoint";
static const char *const binaryblobarray_endpoint = "binaryblobarray_endpoint";
static const char *const boolean_endpoint = "boolean_endpoint";
static const char *const booleanarray_endpoint = "booleanarray_endpoint";
static const char *const datetime_endpoint = "datetime_endpoint";
static const char *const datetimearray_endpoint = "datetimearray_endpoint";
static const char *const double_endpoint = "double_endpoint";
static const char *const doublearray_endpoint = "doublearray_endpoint";
static const char *const integer_endpoint = "integer_endpoint";
static const char *const integerarray_endpoint = "integerarray_endpoint";
static const char *const longinteger_endpoint = "longinteger_endpoint";
static const char *const longintegerarray_endpoint = "longintegerarray_endpoint";
static const char *const string_endpoint = "string_endpoint";
static const char *const stringarray_endpoint = "stringarray_endpoint";

/************************************************

 * Static functions declaration
 ***********************************************/

/**
 * @brief Sets up test data
 */
static e2e_test_data_t setup_test_data();
/**
 * @brief Deallocates test data
 */
static void destroy_test_data(e2e_test_data_t data);
/**
 * @brief Function that sends data from the astarte instance connected.
 */
static void server_send_object_data(const e2e_interface_data_array *server_interfaces_data);
/**
 * @brief Helper function used to check the data the got transmitted in function
 * #transmit_datastream_data.
 */
static void check_object_data(const e2e_interface_data_array *device_interfaces_data);

/************************************************
 * Global functions definition
 ***********************************************/

e2e_device_config_t get_object_test_config()
{
    return (e2e_device_config_t){
        .device_id = CONFIG_DEVICE_ID,
        .cred_secr = CONFIG_CREDENTIAL_SECRET,
        .setup = setup_test_data,
        .server_send = server_send_object_data,
        .check_server_received = check_object_data,
        .destroy = destroy_test_data,
    };
}

/************************************************
 * Static functions definitions
 ***********************************************/

static e2e_test_data_t setup_test_data()
{
    // the pointer stored in this array (endpoint and buffers of astarte values)
    // need to be statically allocated and need be valid until the test data is deallocated with
    // function `destoy_test_data`
    e2e_object_entry_array device_pairs = DYNAMIC_ARRAY(astarte_object_entry_t,
        ((astarte_object_entry_t[]){
            { .endpoint = binaryblob_endpoint,
                .individual = astarte_individual_from_binaryblob(
                    (void *) utils_binary_blob_data, ARRAY_SIZE(utils_binary_blob_data)) },
            { .endpoint = binaryblobarray_endpoint,
                .individual
                = astarte_individual_from_binaryblob_array((const void **) utils_binary_blobs_data,
                    (size_t *) utils_binary_blobs_sizes_data,
                    ARRAY_SIZE(utils_binary_blobs_data)) },
            { .endpoint = boolean_endpoint,
                .individual = astarte_individual_from_boolean(utils_boolean_data) },
            { .endpoint = booleanarray_endpoint,
                .individual = astarte_individual_from_boolean_array(
                    (bool *) utils_boolean_array_data, ARRAY_SIZE(utils_boolean_array_data)) },
            { .endpoint = datetime_endpoint,
                .individual = astarte_individual_from_datetime(utils_unix_time_data) },
            { .endpoint = datetimearray_endpoint,
                .individual
                = astarte_individual_from_datetime_array((int64_t *) utils_unix_time_array_data,
                    ARRAY_SIZE(utils_unix_time_array_data)) },
            { .endpoint = double_endpoint,
                .individual = astarte_individual_from_double(utils_double_data) },
            { .endpoint = doublearray_endpoint,
                .individual = astarte_individual_from_double_array(
                    (double *) utils_double_array_data, ARRAY_SIZE(utils_double_array_data)) },
            { .endpoint = integer_endpoint,
                .individual = astarte_individual_from_integer(utils_integer_data) },
            { .endpoint = integerarray_endpoint,
                .individual = astarte_individual_from_integer_array(
                    (int32_t *) utils_integer_array_data, ARRAY_SIZE(utils_integer_array_data)) },
            { .endpoint = longinteger_endpoint,
                .individual = astarte_individual_from_longinteger(utils_longinteger_data) },
            { .endpoint = longintegerarray_endpoint,
                .individual = astarte_individual_from_longinteger_array(
                    (int64_t *) utils_longinteger_array_data,
                    ARRAY_SIZE(utils_longinteger_array_data)) },
            { .endpoint = string_endpoint,
                .individual = astarte_individual_from_string(utils_string_data) },
            { .endpoint = stringarray_endpoint,
                .individual
                = astarte_individual_from_string_array((const char **) utils_string_array_data,
                    ARRAY_SIZE(utils_string_array_data)) } }));

    return (e2e_test_data_t){
        .device_sent = DYNAMIC_ARRAY(e2e_interface_data_t,
            ((e2e_interface_data_t[]){
                e2e_interface_from_interface_object(
                    &org_astarte_platform_zephyr_e2etest_DeviceAggregate,
                    (e2e_object_data_t){
                        .entries = device_pairs,
                        .path = "/sensor42",
                    }),
            })),
        // TODO add interface sent by the server
        //.server_sent = DYNAMIC_ARRAY(e2e_interface_data_t, ((e2e_interface_data_t[]) {
        //    e2e_interface_from_interface_object(&org_astarte_platform_zephyr_e2etest_ServerDatastream,
        //    server_mappings),
        //}))
    };
}

static void check_object_data(const e2e_interface_data_array *interfaces_data)
{
    (void) interfaces_data;
    // TODO connect to appengine apis and check data sent in transmit_datastream_data
    LOG_WRN("E2E server check of the device sent data not implemented"); // NOLINT
}

static void destroy_test_data(e2e_test_data_t data)
{
    free_e2e_interfaces_array(data.device_sent);
    free_e2e_interfaces_array(data.server_sent);
}

static void server_send_object_data(const e2e_interface_data_array *server_interfaces_data)
{
    (void) server_interfaces_data;
    // TODO implement logic to send datastream data from astarte
    LOG_WRN("E2E server send data logic not implemented"); // NOLINT
}
