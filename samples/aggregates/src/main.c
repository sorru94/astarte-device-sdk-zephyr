/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include <zephyr/data/json.h>
#include <zephyr/kernel.h>
#include <zephyr/net/http/client.h>
#include <zephyr/net/socket.h>
#include <zephyr/posix/arpa/inet.h>

#if (!defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_HTTP)                                  \
    || !defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_MQTT))
#include "ca_certificates.h"
#include <zephyr/net/tls_credentials.h>
#endif

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL); // NOLINT

#include <astarte_device_sdk/device.h>
#include <astarte_device_sdk/interface.h>
#include <astarte_device_sdk/mapping.h>
#include <astarte_device_sdk/pairing.h>
#include <astarte_device_sdk/value.h>

#if defined(CONFIG_WIFI)
#include "wifi.h"
#else
#include "eth.h"
#endif
#include "utils.h"

#include "generated_interfaces.h"

/************************************************
 *       Checks over configuration values       *
 ***********************************************/

BUILD_ASSERT(sizeof(CONFIG_DEVICE_ID) == ASTARTE_PAIRING_DEVICE_ID_LEN + 1,
    "Missing device ID in datastreams example");
BUILD_ASSERT(sizeof(CONFIG_CREDENTIAL_SECRET) == ASTARTE_PAIRING_CRED_SECR_LEN + 1,
    "Missing credential secret in aggregates example");

/************************************************
 * Constants, static variables and defines
 ***********************************************/

#define MAIN_THREAD_SLEEP_MS 500

#define DEVICE_THREAD_FLAGS_TERMINATION 1U
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
static atomic_t device_thread_flags;
K_THREAD_STACK_DEFINE(device_thread_stack_area, CONFIG_DEVICE_THREAD_STACK_SIZE);
static struct k_thread device_thread_data;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

/************************************************
 * Static functions declaration
 ***********************************************/

/**
 * @brief Entry point for the Astarte device thread.
 *
 * @param device_handle Handle to the Astarte device.
 * @param unused1 Unused parameter.
 * @param unused2 Unused parameter.
 */
static void device_thread_entry_point(void *device_handle, void *unused1, void *unused2);
/**
 * @brief Callback handler for Astarte connection events.
 *
 * @param event Astarte device connection event.
 */
static void connection_callback(astarte_device_connection_event_t event);
/**
 * @brief Callback handler for Astarte disconnection events.
 *
 * @param event Astarte device disconnection event.
 */
static void disconnection_callback(astarte_device_disconnection_event_t event);
/**
 * @brief Handler for astarte datastream object event.
 *
 * @param event Astarte device datastream object event pointer.
 */
static void datastream_object_events_handler(astarte_device_datastream_object_event_t event);
/**
 * @brief Helper function used to transmit some fixed data to Astarte.
 */
static void transmit_data(astarte_device_handle_t device);

/************************************************
 * Global functions definition
 ***********************************************/

int main(void)
{
    LOG_INF("Astarte device sample"); // NOLINT
    LOG_INF("Board: %s", CONFIG_BOARD); // NOLINT

    // Initialize WiFi/Ethernet driver
#if defined(CONFIG_WIFI)
    LOG_INF("Initializing WiFi driver."); // NOLINT
    wifi_init();
#else
    LOG_INF("Initializing Ethernet driver."); // NOLINT
    if (eth_connect() != 0) {
        LOG_ERR("Connectivity intialization failed!"); // NOLINT
        return -1;
    }
#endif

    // Add TLS certificate if required
#if (!defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_HTTP)                                  \
    || !defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_MQTT))
    tls_credential_add(CONFIG_ASTARTE_DEVICE_SDK_HTTPS_CA_CERT_TAG, TLS_CREDENTIAL_CA_CERTIFICATE,
        ca_certificate_root, sizeof(ca_certificate_root));
#endif

    // Create a new instance of an Astarte device
    char device_id[ASTARTE_PAIRING_DEVICE_ID_LEN + 1] = CONFIG_DEVICE_ID;
    char cred_secr[ASTARTE_PAIRING_CRED_SECR_LEN + 1] = CONFIG_CREDENTIAL_SECRET;

    const astarte_interface_t *interfaces[]
        = { &org_astarteplatform_zephyr_examples_DeviceAggregate,
              &org_astarteplatform_zephyr_examples_ServerAggregate };

    astarte_device_config_t device_config = { 0 };
    device_config.http_timeout_ms = CONFIG_HTTP_TIMEOUT_MS;
    device_config.mqtt_connection_timeout_ms = CONFIG_MQTT_CONNECTION_TIMEOUT_MS;
    device_config.mqtt_poll_timeout_ms = CONFIG_MQTT_POLL_TIMEOUT_MS;
    device_config.connection_cbk = connection_callback;
    device_config.disconnection_cbk = disconnection_callback;
    device_config.datastream_object_cbk = datastream_object_events_handler;
    device_config.interfaces = interfaces;
    device_config.interfaces_size = ARRAY_SIZE(interfaces);
    memcpy(device_config.device_id, device_id, sizeof(device_id));
    memcpy(device_config.cred_secr, cred_secr, sizeof(cred_secr));

    astarte_device_handle_t device = NULL;
    astarte_result_t res = astarte_device_new(&device_config, &device);
    if (res != ASTARTE_RESULT_OK) {
        LOG_ERR("Astarte device creation failure."); // NOLINT
        return -1;
    }

    // Spawn a new thread for the Astarte device
    k_thread_create(&device_thread_data, device_thread_stack_area,
        K_THREAD_STACK_SIZEOF(device_thread_stack_area), device_thread_entry_point, (void *) device,
        NULL, NULL, CONFIG_DEVICE_THREAD_PRIORITY, 0, K_NO_WAIT);

    // Wait for a predefined operational time.
    k_timepoint_t disconnect_timepoint
        = sys_timepoint_calc(K_SECONDS(CONFIG_DEVICE_OPERATIONAL_TIME_SECONDS));
    k_timepoint_t transmit_timepoint
        = sys_timepoint_calc(K_SECONDS(CONFIG_DEVICE_TRANSMISSION_DELAY_SECONDS));
    bool transmission_performed = false;
    while (!K_TIMEOUT_EQ(sys_timepoint_timeout(disconnect_timepoint), K_NO_WAIT)) {
        if ((!transmission_performed)
            && (K_TIMEOUT_EQ(sys_timepoint_timeout(transmit_timepoint), K_NO_WAIT))) {
            LOG_INF("Transmitting some data using the Astarte device."); // NOLINT
            transmit_data(device);
            transmission_performed = true;
        }

// Ensure the connectivity is still present
#if defined(CONFIG_WIFI)
        wifi_poll();
#else
        eth_poll();
#endif

        k_sleep(K_MSEC(MAIN_THREAD_SLEEP_MS));
    }

    // Signal to the Astarte thread that is should terminate.
    atomic_set_bit(&device_thread_flags, DEVICE_THREAD_FLAGS_TERMINATION);

    // Wait for the Astarte thread to terminate.
    if (k_thread_join(&device_thread_data, K_FOREVER) != 0) {
        LOG_ERR("Failed in waiting for the Astarte thread to terminate."); // NOLINT
    }

    LOG_INF("Astarte device sample finished."); // NOLINT
    k_sleep(K_MSEC(MSEC_PER_SEC));

    return 0;
}

/************************************************
 * Static functions definitions
 ***********************************************/

static void device_thread_entry_point(void *device_handle, void *unused1, void *unused2)
{
    (void) unused1;
    (void) unused2;
    astarte_result_t res = ASTARTE_RESULT_OK;

    astarte_device_handle_t device = (astarte_device_handle_t) device_handle;
    res = astarte_device_connect(device);
    if (res != ASTARTE_RESULT_OK) {
        LOG_ERR("Astarte device connection failure."); // NOLINT
        return;
    }

    res = astarte_device_poll(device);
    if (res != ASTARTE_RESULT_OK) {
        // First poll should not timeout as we should receive a connection ack.
        LOG_ERR("Astarte device first poll failure."); // NOLINT
        return;
    }

    while (!atomic_test_bit(&device_thread_flags, DEVICE_THREAD_FLAGS_TERMINATION)) {
        k_timepoint_t timepoint = sys_timepoint_calc(K_MSEC(CONFIG_DEVICE_POLL_PERIOD_MS));

        res = astarte_device_poll(device);
        if (res != ASTARTE_RESULT_OK) {
            LOG_ERR("Astarte device poll failure."); // NOLINT
            return;
        }

        k_sleep(sys_timepoint_timeout(timepoint));
    }

    LOG_INF("End of loop, disconnection imminent."); // NOLINT

    res = astarte_device_disconnect(device);
    if (res != ASTARTE_RESULT_OK) {
        LOG_ERR("Astarte device disconnection failure."); // NOLINT
        return;
    }

    LOG_INF("Astarte thread will now be terminated."); // NOLINT

    k_sleep(K_MSEC(MSEC_PER_SEC));
}

static void connection_callback(astarte_device_connection_event_t event)
{
    (void) event;
    LOG_INF("Astarte device connected."); // NOLINT
}

static void disconnection_callback(astarte_device_disconnection_event_t event)
{
    (void) event;
    LOG_INF("Astarte device disconnected"); // NOLINT
}

static void datastream_object_events_handler(astarte_device_datastream_object_event_t event)
{
    const char *interface_name = event.data_event.interface_name;
    const char *path = event.data_event.path;

    astarte_value_pair_t *value_pairs = NULL;
    size_t value_pairs_length = 0;

    astarte_result_t astarte_rc = astarte_value_pair_array_to_value_pairs(
        event.value_pair_array, &value_pairs, &value_pairs_length);
    if (astarte_rc != ASTARTE_RESULT_OK) {
        LOG_INF("Value pair array error: %s.", astarte_result_to_name(astarte_rc)); // NOLINT
        return;
    }

    LOG_INF("Datastream object event, interface: %s, path: %s", interface_name, path); // NOLINT

    LOG_INF("Astarte object:"); // NOLINT
    for (size_t i = 0; i < value_pairs_length; i++) {
        const char *endpoint = NULL;
        astarte_value_t value = { 0 };
        astarte_result_t astarte_rc
            = astarte_value_pair_to_endpoint_and_value(value_pairs[i], &endpoint, &value);
        if (astarte_rc == ASTARTE_RESULT_OK) {
            LOG_INF("Partial endpoint: %s", endpoint); // NOLINT
            utils_log_astarte_value(value);
        }
    }
}

static void transmit_data(astarte_device_handle_t device)
{
    astarte_value_pair_t value_pairs[UTILS_DATA_ELEMENTS] = {
        astarte_value_pair_new("binaryblob_endpoint",
            astarte_value_from_binaryblob(
                (void *) utils_binary_blob_data, ARRAY_SIZE(utils_binary_blob_data))),
        astarte_value_pair_new("binaryblobarray_endpoint",
            astarte_value_from_binaryblob_array((const void **) utils_binary_blobs_data,
                (size_t *) utils_binary_blobs_sizes_data, ARRAY_SIZE(utils_binary_blobs_data))),
        astarte_value_pair_new("boolean_endpoint", astarte_value_from_boolean(utils_boolean_data)),
        astarte_value_pair_new("booleanarray_endpoint",
            astarte_value_from_boolean_array(
                (bool *) utils_boolean_array_data, ARRAY_SIZE(utils_boolean_array_data))),
        astarte_value_pair_new(
            "datetime_endpoint", astarte_value_from_datetime(utils_unix_time_data)),
        astarte_value_pair_new("datetimearray_endpoint",
            astarte_value_from_datetime_array(
                (int64_t *) utils_unix_time_array_data, ARRAY_SIZE(utils_unix_time_array_data))),
        astarte_value_pair_new("double_endpoint", astarte_value_from_double(utils_double_data)),
        astarte_value_pair_new("doublearray_endpoint",
            astarte_value_from_double_array(
                (double *) utils_double_array_data, ARRAY_SIZE(utils_double_array_data))),
        astarte_value_pair_new("integer_endpoint", astarte_value_from_integer(utils_integer_data)),
        astarte_value_pair_new("integerarray_endpoint",
            astarte_value_from_integer_array(
                (int32_t *) utils_integer_array_data, ARRAY_SIZE(utils_integer_array_data))),
        astarte_value_pair_new(
            "longinteger_endpoint", astarte_value_from_longinteger(utils_longinteger_data)),
        astarte_value_pair_new("longintegerarray_endpoint",
            astarte_value_from_longinteger_array((int64_t *) utils_longinteger_array_data,
                ARRAY_SIZE(utils_longinteger_array_data))),
        astarte_value_pair_new("string_endpoint", astarte_value_from_string(utils_string_data)),
        astarte_value_pair_new("stringarray_endpoint",
            astarte_value_from_string_array(
                (const char **) utils_string_array_data, ARRAY_SIZE(utils_string_array_data))),
    };

    astarte_value_pair_array_t value_pair_array
        = astarte_value_pair_array_new(value_pairs, ARRAY_SIZE(value_pairs));

    astarte_result_t res = astarte_device_stream_aggregated(device,
        org_astarteplatform_zephyr_examples_DeviceAggregate.name, "/sensor24", value_pair_array,
        NULL);
    if (res != ASTARTE_RESULT_OK) {
        LOG_ERR("Error streaming the aggregate"); // NOLINT
    }
}
