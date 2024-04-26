/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef E2ERUNNER_H
#define E2ERUNNER_H

#include <stdint.h>

#include <zephyr/sys/atomic_types.h>
#include <zephyr/sys/bitarray.h>

#include <astarte_device_sdk/device.h>
#include <astarte_device_sdk/individual.h>
#include <astarte_device_sdk/interface.h>
#include <astarte_device_sdk/object.h>
#include <astarte_device_sdk/util.h>

// Can be used to hold a timestamp and if it should be sent or not
typedef struct
{
    int64_t value;
    bool present;
} e2e_timestamp_option_t;

// Can be used to hold test data for an individual mapping
typedef struct
{
    astarte_individual_t individual;
    e2e_timestamp_option_t timestamp;
    const char *endpoint;
} e2e_individual_data_t;

ASTARTE_UTIL_DEFINE_ARRAY(e2e_mapping_data_array, e2e_individual_data_t);

// Property data used to test astarte individual properties interfaces
typedef struct
{
    const char *path;
    astarte_individual_t value;
    bool unset;
} e2e_property_data_t;

ASTARTE_UTIL_DEFINE_ARRAY(e2e_property_data_array, e2e_property_data_t);

ASTARTE_UTIL_DEFINE_ARRAY(e2e_object_entry_array, astarte_object_entry_t);

// Object data used to test astarte object aggregated interfaces
typedef struct
{
    e2e_timestamp_option_t timestamp;
    e2e_object_entry_array entries;
    const char *path;
} e2e_object_data_t;

// Interface definition used in the e2e test that contains the values and the definition of a
// specific interface. It also contains a `received` field that is used to verify that all the
// expected data got received.
typedef struct
{
    const astarte_interface_t *interface;
    // union that follows the value of the interface->aggregation tag
    union
    {
        e2e_mapping_data_array individual;
        e2e_property_data_array property;
        e2e_object_data_t object;
    } values;
    sys_bitarray_t received;
} e2e_interface_data_t;

ASTARTE_UTIL_DEFINE_ARRAY(e2e_interface_data_array, e2e_interface_data_t);

// Complete test data contains:
// - the interfaces that will be sent by the server and verified by the client
// - the interfaces that will be sent by the device and verified by the server
typedef struct
{
    e2e_interface_data_array device_sent;
    e2e_interface_data_array server_sent;
} e2e_test_data_t;

// function that sets up the test data and returns it
typedef e2e_test_data_t (*setup_test_data_fn_t)();
// function that cleans up the test data
typedef void (*destroy_test_data_fn_t)(e2e_test_data_t);
// function that sends the passed data to the other end of the test
typedef void (*sent_data_fn_t)(const e2e_interface_data_array *);

// complete device configuration for an instance of the test
typedef struct
{
    char device_id[ASTARTE_PAIRING_DEVICE_ID_LEN + 1];
    char cred_secr[ASTARTE_PAIRING_CRED_SECR_LEN + 1];
    setup_test_data_fn_t setup;
    sent_data_fn_t check_server_received;
    sent_data_fn_t server_send;
    destroy_test_data_fn_t destroy;
} e2e_device_config_t;

// run the e2e test on all test devices
void run_e2e_test();

#endif /* E2ERUNNER_H */
