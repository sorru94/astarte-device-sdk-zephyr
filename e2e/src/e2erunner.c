/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "e2erunner.h"

#include <stdlib.h>
#include <string.h>

#include <zephyr/fatal.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/bitarray.h>
#include <zephyr/sys/util.h>

#include <astarte_device_sdk/device.h>
#include <astarte_device_sdk/individual.h>
#include <astarte_device_sdk/interface.h>
#include <astarte_device_sdk/object.h>
#include <astarte_device_sdk/pairing.h>
#include <astarte_device_sdk/result.h>

#include "e2eutilities.h"
#include "individual_test.h"
#include "object_test.h"
#include "property_test.h"
#include "utils.h"

/************************************************
 * Constants, static variables and defines
 ***********************************************/

#define MAIN_THREAD_SLEEP_MS 500

LOG_MODULE_REGISTER(e2e_setup, CONFIG_E2E_SETUP_LOG_LEVEL); // NOLINT

K_THREAD_STACK_DEFINE(device_thread_stack_area, CONFIG_DEVICE_THREAD_STACK_SIZE);
static struct k_thread device_thread_data;

enum e2e_thread_flags
{
    DEVICE_CONNECTED_FLAG = 0,
    THREAD_TERMINATION_FLAG,
};
static atomic_t device_thread_flags;

ASTARTE_UTIL_DEFINE_ARRAY(astarte_interface_array_t, const astarte_interface_t *);

/************************************************
 * Static functions declaration
 ***********************************************/

static void free_interfaces(astarte_interface_array_t interfaces);
static void device_thread_entry_point(void *device_handle, void *unused1, void *unused2);
static astarte_interface_array_t alloc_interfaces_from_test_data(const e2e_test_data_t *test_data);
static astarte_device_handle_t device_setup(const e2e_device_config_t *e2e_device_config,
    const astarte_interface_array_t *interfaces, const e2e_test_data_t *test_data);
static void transmit_data(
    astarte_device_handle_t device, const e2e_interface_data_array_t *interfaces_data);
static void transmit_property_data(astarte_device_handle_t device,
    const astarte_interface_t *interface, const e2e_property_data_array_t *properties);
static void transmit_datastream_individual_data(astarte_device_handle_t device,
    const astarte_interface_t *interface, const e2e_individual_data_array_t *mappings);
static void transmit_datastream_object_data(astarte_device_handle_t device,
    const astarte_interface_t *interface, const e2e_object_data_t *object_values);
static void connection_callback(astarte_device_connection_event_t event);
static void disconnection_callback(astarte_device_disconnection_event_t event);
static void run_device(const e2e_device_config_t *config);

/************************************************
 * Global functions definition
 ***********************************************/

void run_e2e_test()
{
    const e2e_device_config_t test_device_configs[] = {
        get_individual_test_config(),
        get_object_test_config(),
        get_property_test_config(),
    };

    for (int i = 0; i < ARRAY_SIZE(test_device_configs); ++i) {
        run_device(test_device_configs + i);
    }
}

/************************************************
 * Static functions definitions
 ***********************************************/

static void free_interfaces(astarte_interface_array_t interfaces)
{
    free(interfaces.buf);
}

static astarte_interface_array_t alloc_interfaces_from_test_data(const e2e_test_data_t *test_data)
{
    size_t len = test_data->device_sent.len + test_data->server_sent.len;
    const astarte_interface_t **const test_interfaces = calloc(len, sizeof(astarte_interface_t));
    CHECK_HALT(!test_interfaces, "Could not allocate interfaces, out of memory");

    size_t index = 0;
    // copy device interfaces
    for (; index < test_data->device_sent.len; ++index) {
        test_interfaces[index] = test_data->device_sent.buf[index].interface;
    }
    // copy server interfaces
    for (; index < len; ++index) {
        test_interfaces[index]
            = test_data->server_sent.buf[index - test_data->device_sent.len].interface;
    }

    return (astarte_interface_array_t){
        .buf = test_interfaces,
        .len = len,
    };
}

static astarte_device_handle_t device_setup(const e2e_device_config_t *e2e_device_config,
    const astarte_interface_array_t *interfaces, const e2e_test_data_t *test_data)
{
    CHECK_HALT(strcmp(e2e_device_config->device_id, "") == 0
            || strcmp(e2e_device_config->cred_secr, "") == 0,
        "The device_id or cred_secret are not correctly set");

    astarte_device_config_t config = {
        .interfaces = interfaces->buf,
        .interfaces_size = interfaces->len,
        .http_timeout_ms = CONFIG_HTTP_TIMEOUT_MS,
        .mqtt_connection_timeout_ms = CONFIG_MQTT_CONNECTION_TIMEOUT_MS,
        .mqtt_poll_timeout_ms = CONFIG_MQTT_POLL_TIMEOUT_MS,
        .cbk_user_data = (void *) &test_data->server_sent,
        // TODO add the callbacks to check received properties of other tests
        .connection_cbk = connection_callback,
        .disconnection_cbk = disconnection_callback,
    };

    memcpy(config.device_id, e2e_device_config->device_id, sizeof(config.device_id));
    memcpy(config.cred_secr, e2e_device_config->cred_secr, sizeof(config.cred_secr));

    astarte_device_handle_t device = { 0 };
    LOG_INF("Creating astarte_device by calling astarte_device_new."); // NOLINT
    CHECK_ASTARTE_OK_HALT(astarte_device_new(&config, &device), "Astarte device creation failure.");

    return device;
}

static void transmit_data(
    astarte_device_handle_t device, const e2e_interface_data_array_t *interfaces_data)
{
    LOG_INF("Starting transmission of %zu interfaces", interfaces_data->len); // NOLINT

    for (size_t i = 0; i < interfaces_data->len; ++i) {
        const e2e_interface_data_t *interface = interfaces_data->buf + i;

        if (interface->interface->type == ASTARTE_INTERFACE_TYPE_PROPERTIES) {
            transmit_property_data(device, interface->interface, &interface->values.property);
        } else if (interface->interface->aggregation == ASTARTE_INTERFACE_AGGREGATION_INDIVIDUAL) {
            transmit_datastream_individual_data(
                device, interface->interface, &interface->values.individual);
        } else if (interface->interface->aggregation == ASTARTE_INTERFACE_AGGREGATION_OBJECT) {
            transmit_datastream_object_data(
                device, interface->interface, &interface->values.object);
        } else {
            CHECK_HALT(true, "Unkown interface type")
        }
    }

    LOG_INF("Ended transmission"); // NOLINT
}

static void transmit_property_data(astarte_device_handle_t device,
    const astarte_interface_t *interface, const e2e_property_data_array_t *properties)
{
    LOG_INF("Setting properties of interface '%s'", interface->name); // NOLINT

    for (size_t j = 0; j < properties->len; ++j) {
        const e2e_property_data_t *property_value = properties->buf + j;

        astarte_result_t res = ASTARTE_RESULT_OK;

        if (property_value->unset) {
            LOG_INF("Unsetting value on '%s'", property_value->path); // NOLINT
            res = astarte_device_unset_property(device, interface->name, property_value->path);
        } else {
            LOG_INF("Setting value on '%s'", property_value->path); // NOLINT
            res = astarte_device_set_property(
                device, interface->name, property_value->path, property_value->individual);
        }

        CHECK_ASTARTE_OK_HALT(
            res, "Astarte device property failure: %s", astarte_result_to_name(res));
    }

    LOG_INF("Ended transmission"); // NOLINT
}

static void transmit_datastream_individual_data(astarte_device_handle_t device,
    const astarte_interface_t *interface, const e2e_individual_data_array_t *mappings)
{
    LOG_INF("Sending values on individual interface '%s'", interface->name); // NOLINT

    // TODO create macro to loop over one of our arrays
    for (size_t j = 0; j < mappings->len; ++j) {
        const e2e_individual_data_t *mapping = mappings->buf + j;

        LOG_INF("Stream individual value on '%s'", mapping->path); // NOLINT

        const e2e_timestamp_option_t *timestamp = &mapping->timestamp;

        astarte_result_t res = ASTARTE_RESULT_OK;
        if (timestamp->present) {
            res = astarte_device_stream_individual(
                device, interface->name, mapping->path, mapping->individual, &timestamp->value);
        } else {
            res = astarte_device_stream_individual(
                device, interface->name, mapping->path, mapping->individual, NULL);
        }

        CHECK_ASTARTE_OK_HALT(res, "Astarte device individual value transmission failure: %s",
            astarte_result_to_name(res));
    }

    LOG_INF("Ended transmission"); // NOLINT
}

static void transmit_datastream_object_data(astarte_device_handle_t device,
    const astarte_interface_t *interface, const e2e_object_data_t *object)
{
    LOG_INF("Sending values on object interface '%s'", interface->name); // NOLINT

    astarte_result_t res = ASTARTE_RESULT_OK;
    if (object->timestamp.present) {
        res = astarte_device_stream_aggregated(device, interface->name, object->path,
            (astarte_object_entry_t *) object->entries.buf, object->entries.len,
            &object->timestamp.value);
    } else {
        res = astarte_device_stream_aggregated(device, interface->name, object->path,
            (astarte_object_entry_t *) object->entries.buf, object->entries.len, NULL);
    }

    CHECK_ASTARTE_OK_HALT(
        res, "Astarte device object transmission failure: %s", astarte_result_to_name(res));

    LOG_INF("Ended transmission"); // NOLINT
}

static void run_device(const e2e_device_config_t *config)
{
    // create resources
    LOG_INF("Setting up device and getting test_data."); // NOLINT
    e2e_test_data_t test_data = config->setup();
    const astarte_interface_array_t test_device_interfaces
        = alloc_interfaces_from_test_data(&test_data);
    astarte_device_handle_t device_handle
        = device_setup(config, &test_device_interfaces, &test_data);

    // clear previously set flags
    atomic_clear(&device_thread_flags);

    LOG_INF("Spawning a new thread to poll data from the Astarte device."); // NOLINT
    k_thread_create(&device_thread_data, device_thread_stack_area,
        K_THREAD_STACK_SIZEOF(device_thread_stack_area), device_thread_entry_point, device_handle,
        NULL, NULL, CONFIG_DEVICE_THREAD_PRIORITY, 0, K_NO_WAIT);

    // wait while device connects
    // TODO could use a semaphore here to avoid the while
    while (!atomic_test_bit(&device_thread_flags, DEVICE_CONNECTED_FLAG)) {
        k_sleep(K_MSEC(MAIN_THREAD_SLEEP_MS));
    }

    // transmit all device data declared
    transmit_data(device_handle, &test_data.device_sent);

    // after the server sent its data we check that all the expected data got received
    // TODO perform the check for received data

    LOG_INF("Stopping and joining the astarte device polling thread."); // NOLINT
    atomic_set_bit(&device_thread_flags, THREAD_TERMINATION_FLAG);
    CHECK_HALT(k_thread_join(&device_thread_data, K_FOREVER) != 0,
        "Failed in waiting for the Astarte thread to terminate.");

    LOG_INF("Destroing Astarte device and freeing resources."); // NOLINT
    astarte_device_destroy(device_handle);

    // wait until we are disconnected
    while (atomic_test_bit(&device_thread_flags, DEVICE_CONNECTED_FLAG)) {
        k_sleep(K_MSEC(MAIN_THREAD_SLEEP_MS));
    }

    free_interfaces(test_device_interfaces);
}

static void device_thread_entry_point(void *device_handle, void *unused1, void *unused2)
{
    (void) unused1;
    (void) unused2;

    astarte_device_handle_t device = (astarte_device_handle_t) device_handle;

    LOG_INF("Starting e2e device thread."); // NOLINT

    CHECK_ASTARTE_OK_HALT(astarte_device_connect(device), "Astarte device connection failure.");

    while (!atomic_test_bit(&device_thread_flags, THREAD_TERMINATION_FLAG)) {
        k_timepoint_t timepoint = sys_timepoint_calc(K_MSEC(CONFIG_DEVICE_POLL_PERIOD_MS));

        astarte_result_t res = astarte_device_poll(device);
        CHECK_HALT(res != ASTARTE_RESULT_TIMEOUT && res != ASTARTE_RESULT_OK,
            "Astarte device poll failure.");

        k_sleep(sys_timepoint_timeout(timepoint));
    }

    LOG_INF("Exiting from the polling thread."); // NOLINT
}

static void connection_callback(astarte_device_connection_event_t event)
{
    (void) event;
    LOG_INF("Astarte device connected"); // NOLINT
    atomic_set_bit(&device_thread_flags, DEVICE_CONNECTED_FLAG);
}

static void disconnection_callback(astarte_device_disconnection_event_t event)
{
    (void) event;
    LOG_INF("Astarte device disconnected"); // NOLINT
    atomic_clear_bit(&device_thread_flags, DEVICE_CONNECTED_FLAG);
}
