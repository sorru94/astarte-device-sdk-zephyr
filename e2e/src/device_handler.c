/*
 * (C) Copyright 2025, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "device_handler.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

#include "astarte_device_sdk/device.h"

#include "astarte_generated_interfaces.h"
#include "data.h"
#include "utilities.h"

LOG_MODULE_REGISTER(device_handler, CONFIG_DEVICE_HANDLER_LOG_LEVEL);

/************************************************
 *   Constants, static variables and defines    *
 ***********************************************/

#define GENERIC_WAIT_SLEEP_500_MS 500

static astarte_device_handle_t device_handle;

static const astarte_interface_t *device_interfaces[] = {
    &org_astarte_platform_zephyr_e2etest_DeviceAggregate,
    &org_astarte_platform_zephyr_e2etest_DeviceDatastream,
    &org_astarte_platform_zephyr_e2etest_DeviceProperty,
    &org_astarte_platform_zephyr_e2etest_ServerAggregate,
    &org_astarte_platform_zephyr_e2etest_ServerDatastream,
    &org_astarte_platform_zephyr_e2etest_ServerProperty,
};

K_THREAD_STACK_DEFINE(device_thread_stack_area, CONFIG_DEVICE_THREAD_STACK_SIZE);
static struct k_thread device_thread_data;
static atomic_t device_thread_flags;
enum device_thread_flags
{
    DEVICE_THREAD_CONNECTED_FLAG = 0,
    DEVICE_THREAD_TERMINATION_FLAG,
};

/************************************************
 *         Static functions declaration         *
 ***********************************************/

static void device_thread_entry_point(void *unused1, void *unused2, void *unused3);
static void handle_device_event(astarte_device_event_t *event);

/************************************************
 *         Global functions definition          *
 ***********************************************/

void setup_device()
{
    LOG_INF("Creating static astarte_device.");

    CHECK_HALT(device_handle != NULL, "Attempting to create a device while a device is present.");

    data_init(device_interfaces, ARRAY_SIZE(device_interfaces));

    astarte_device_config_t config = {
        .device_id = CONFIG_DEVICE_ID,
        .cred_secr = CONFIG_CREDENTIAL_SECRET,
        .interfaces = device_interfaces,
        .interfaces_size = ARRAY_SIZE(device_interfaces),
        .http_timeout_ms = CONFIG_HTTP_TIMEOUT_MS,
        .mqtt_connection_timeout_ms = CONFIG_MQTT_CONNECTION_TIMEOUT_MS,
        .mqtt_poll_timeout_ms = CONFIG_MQTT_POLL_TIMEOUT_MS,
    };

    CHECK_ASTARTE_OK_HALT(astarte_device_new(&config, &device_handle), "Device creation failure.");

    LOG_INF("Spawning a new thread to poll data from the Astarte device.");
    k_thread_create(&device_thread_data, device_thread_stack_area,
        K_THREAD_STACK_SIZEOF(device_thread_stack_area), device_thread_entry_point, NULL, NULL,
        NULL, CONFIG_DEVICE_THREAD_PRIORITY, 0, K_NO_WAIT);

    LOG_INF("Astarte device created.");
}

void free_device()
{
    atomic_set_bit(&device_thread_flags, DEVICE_THREAD_TERMINATION_FLAG);

    CHECK_HALT(k_thread_join(&device_thread_data, K_FOREVER) != 0,
        "Failed in waiting for the Astarte thread to terminate.");

    LOG_INF("Destroing the expected data structure.");
    data_free();

    LOG_INF("Destroing Astarte device and freeing resources.");
    CHECK_ASTARTE_OK_HALT(astarte_device_destroy(device_handle), "Device destruction failure.");
    device_handle = NULL;
    LOG_INF("Astarte device destroyed.");
}

void wait_for_device_connection()
{
    CHECK_HALT(device_handle == NULL, "Trying to wait on a non existent device.");
    while (!atomic_test_bit(&device_thread_flags, DEVICE_THREAD_CONNECTED_FLAG)) {
        k_sleep(K_MSEC(GENERIC_WAIT_SLEEP_500_MS));
    }
}

void disconnect_device()
{
    CHECK_HALT(device_handle == NULL, "Trying to disconnect a non existent device.");
    atomic_set_bit(&device_thread_flags, DEVICE_THREAD_TERMINATION_FLAG);
}

void wait_for_device_disconnection()
{
    while (atomic_test_bit(&device_thread_flags, DEVICE_THREAD_CONNECTED_FLAG)) {
        k_sleep(K_MSEC(GENERIC_WAIT_SLEEP_500_MS));
    }

    bool unreceived_messages = false;
    if (data_size(device_interfaces, ARRAY_SIZE(device_interfaces)) > 0) {
        unreceived_messages = true;
    }

#ifndef CONFIG_LOG_ONLY
    CHECK_HALT(unreceived_messages, "Some expected messages didn't get received");
#endif
}

// Computes a perfect hash for a device interface name
// NOTE: this depends on the names used in this case this works because we know the names of the
// interfaces.
// The konwn interfaces names are:
// - org.astarte-platform.zephyr.e2etest.ServerProperty
// - org.astarte-platform.zephyr.e2etest.DeviceProperty
// - org.astarte-platform.zephyr.e2etest.ServerAggregate
// - org.astarte-platform.zephyr.e2etest.DeviceAggregate
// - org.astarte-platform.zephyr.e2etest.ServerDatastream
// - org.astarte-platform.zephyr.e2etest.DeviceDatastream
uint64_t perfect_hash_device_interface(const char *interface_name, size_t len)
{

    const char base[] = "org.astarte-platform.zephyr.e2etest.";
    const size_t base_len = ARRAY_SIZE(base) - 1;

    CHECK_HALT(len <= 44 || strncmp(interface_name, base, base_len) != 0,
        "Received an invalid or unexpected interface name, please update the hash function");

    /* We extract:
     * Char at [36]: 'S' (Server) or 'D' (Device)
     * Char at [42]: 'P' (InterfaceTestingPropertySetTestElement), 'A' (InterfaceTestingAggregate),
     * 'D' (InterfaceDatastream) Char at [43]: 'r', 'g', 'a'... (from
     * 'InterfaceTestingPropertySetTestElement'/'InterfaceTestingAggregate'/'InterfaceDatastream')
     */
    uint32_t hash = ((uint32_t) interface_name[36] << 16) | ((uint32_t) interface_name[42] << 8)
        | ((uint32_t) interface_name[43]);

    return (uint64_t) hash;
}

int device_send_individual(
    const char *interface_name, const char *path, astarte_data_t data, optional_int64_t timestamp)
{
    astarte_result_t res = ASTARTE_RESULT_OK;
    if (timestamp.present) {
        res = astarte_device_send_individual(
            device_handle, interface_name, path, data, &timestamp.value);
    } else {
        res = astarte_device_send_individual(device_handle, interface_name, path, data, NULL);
    }
    CHECK_ASTARTE_OK_RET_1(res, "Failed to send individual to astarte");
    return 0;
}

int device_send_object(const char *interface_name, const char *path,
    astarte_object_entry_t *entries, size_t entries_len, optional_int64_t timestamp)
{
    astarte_result_t res = ASTARTE_RESULT_OK;
    if (timestamp.present) {
        res = astarte_device_send_object(
            device_handle, interface_name, path, entries, entries_len, &timestamp.value);
    } else {
        res = astarte_device_send_object(
            device_handle, interface_name, path, entries, entries_len, NULL);
    }
    CHECK_ASTARTE_OK_RET_1(res, "Failed to send object to astarte");
    return 0;
}

int device_set_property(const char *interface_name, const char *path, astarte_data_t data)
{
    CHECK_ASTARTE_OK_RET_1(astarte_device_set_property(device_handle, interface_name, path, data),
        "Failed to set property to astarte");
    return 0;
}

int device_unset_property(const char *interface_name, const char *path)
{
    CHECK_ASTARTE_OK_RET_1(astarte_device_unset_property(device_handle, interface_name, path),
        "Failed to unset property to astarte");
    return 0;
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static void device_thread_entry_point(void *unused1, void *unused2, void *unused3)
{
    ARG_UNUSED(unused1);
    ARG_UNUSED(unused2);
    ARG_UNUSED(unused3);

    LOG_INF("Started Astarte device thread.");

    CHECK_ASTARTE_OK_HALT(astarte_device_connect(device_handle), "Device connection failure.");

    while (!atomic_test_bit(&device_thread_flags, DEVICE_THREAD_TERMINATION_FLAG)) {
        astarte_device_event_t event = { 0 };
        astarte_result_t get_res = astarte_device_get_event(device_handle, &event, K_MSEC(100));
        if ((get_res != ASTARTE_RESULT_OK) && (get_res != ASTARTE_RESULT_TIMEOUT)) {
            LOG_ERR("Error event retrieval failure: %s", astarte_result_to_name(get_res));
            continue;
        }

        if (get_res == ASTARTE_RESULT_OK) {
            handle_device_event(&event);
            astarte_device_event_cleanup(&event);
        }
    }

    LOG_INF("Disconnecting Astarte device...");
    astarte_result_t res = astarte_device_disconnect(device_handle, K_SECONDS(10));
    if (res != ASTARTE_RESULT_OK) {
        LOG_ERR("Device disconnection failure: %d", res);
    }

    while (atomic_test_bit(&device_thread_flags, DEVICE_THREAD_CONNECTED_FLAG)) {
        astarte_device_event_t event = { 0 };
        astarte_result_t get_res = astarte_device_get_event(device_handle, &event, K_MSEC(100));
        if ((get_res != ASTARTE_RESULT_OK) && (get_res != ASTARTE_RESULT_TIMEOUT)) {
            LOG_ERR("Error event retrieval failure: %s", astarte_result_to_name(get_res));
            continue;
        }

        if (get_res == ASTARTE_RESULT_OK) {
            handle_device_event(&event);
            astarte_device_event_cleanup(&event);
        }
    }

    LOG_INF("Exiting from the polling thread.");
}

static void handle_device_event(astarte_device_event_t *event)
{
    switch (event->type) {
        case ASTARTE_DEVICE_EVENT_ERROR: {
            // NOLINTNEXTLINE
            LOG_ERR("Astarte internal device error: %s (Context: %s)",
                astarte_result_to_name(event->data.error.result),
                event->data.error.context ? event->data.error.context : "Unknown");
            break;
        }
        case ASTARTE_DEVICE_EVENT_CONNECTED: {
            LOG_INF("Astarte device connected."); // NOLINT
            atomic_set_bit(&device_thread_flags, DEVICE_THREAD_CONNECTED_FLAG);
            break;
        }
        case ASTARTE_DEVICE_EVENT_DISCONNECTED: {
            LOG_INF("Astarte device disconnected."); // NOLINT
            atomic_clear_bit(&device_thread_flags, DEVICE_THREAD_CONNECTED_FLAG);
            break;
        }
        case ASTARTE_DEVICE_EVENT_DATASTREAM_INDIVIDUAL: {
            const char *interface_name
                = event->data.datastream_individual.base_event.interface_name;
            const char *path = event->data.datastream_individual.base_event.path;
            astarte_data_t individual = event->data.datastream_individual.data;
            // NOLINTNEXTLINE
            LOG_INF("Datastream individual event, interface: %s, path: %s", interface_name, path);
#ifndef CONFIG_LOG_ONLY
            CHECK_HALT(data_expected_individual(interface_name, path, &individual) != 0,
                "Received individual datastream does not match any expected one");
#endif
            break;
        }
        case ASTARTE_DEVICE_EVENT_DATASTREAM_OBJECT: {
            const char *interface_name = event->data.datastream_object.base_event.interface_name;
            const char *path = event->data.datastream_object.base_event.path;
            astarte_object_entry_t *entries = event->data.datastream_object.entries;
            size_t entries_length = event->data.datastream_object.entries_len;
            // NOLINTNEXTLINE
            LOG_INF("Datastream object event, interface: %s, path: %s", interface_name, path);
#ifndef CONFIG_LOG_ONLY
            CHECK_HALT(data_expected_object(interface_name, path, entries, entries_length) != 0,
                "Received object datastream does not match any expected one");
#endif
            break;
        }
        case ASTARTE_DEVICE_EVENT_PROPERTY_SET: {
            const char *interface_name = event->data.property_set.base_event.interface_name;
            const char *path = event->data.property_set.base_event.path;
            astarte_data_t individual = event->data.property_set.data;
            // NOLINTNEXTLINE
            LOG_INF("Property set event, interface: %s, path: %s", interface_name, path);
#ifndef CONFIG_LOG_ONLY
            CHECK_HALT(data_expected_set_property(interface_name, path, &individual) != 0,
                "Received property set event does not match any expected one");
#endif
            break;
        }
        case ASTARTE_DEVICE_EVENT_PROPERTY_UNSET: {
            const char *interface_name = event->data.property_unset.interface_name;
            const char *path = event->data.property_unset.path;
            // NOLINTNEXTLINE
            LOG_INF("Property unset event, interface: %s, path: %s", interface_name, path);
#ifndef CONFIG_LOG_ONLY
            CHECK_HALT(data_expected_unset_property(interface_name, path) != 0,
                "Received property unset event does not match any expected one");
#endif
            break;
        }
        default: {
            LOG_ERR("Unreacheable code reached, unknown event type: %d", event->type);
            break;
        }
    }
}
