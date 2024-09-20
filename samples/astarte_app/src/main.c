/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include <zephyr/data/json.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/http/client.h>
#include <zephyr/net/socket.h>
#include <zephyr/posix/arpa/inet.h>
#include <zephyr/toolchain.h>

#if (!defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_HTTP)                                  \
    || !defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_MQTT))
#include "ca_certificates.h"
#include <zephyr/net/tls_credentials.h>
#endif

#include <astarte_device_sdk/device.h>
#include <astarte_device_sdk/individual.h>
#include <astarte_device_sdk/interface.h>
#include <astarte_device_sdk/mapping.h>
#include <astarte_device_sdk/pairing.h>

#if defined(CONFIG_WIFI)
#include "wifi.h"
#else
#include "eth.h"
#endif

#include "utils.h"

#include "generated_interfaces.h"

#if defined(CONFIG_DEVICE_INDIVIDUAL_TRANSMISSION)
#include "individual_send.h"
#endif
#if defined(CONFIG_DEVICE_OBJECT_TRANSMISSION)
#include "object_send.h"
#endif
#if defined(CONFIG_DEVICE_PROPERTY_SET_TRANSMISSION)                                               \
    || defined(CONFIG_DEVICE_PROPERTY_UNSET_TRANSMISSION)
#include "property_send.h"
#endif
#if defined(CONFIG_DEVICE_REGISTRATION)
#include "register.h"
#endif

/************************************************
 *       Checks over configuration values       *
 ***********************************************/

BUILD_ASSERT(sizeof(CONFIG_DEVICE_ID) == ASTARTE_DEVICE_ID_LEN + 1,
    "Missing device ID in datastreams example");

#if !defined(CONFIG_DEVICE_REGISTRATION)
BUILD_ASSERT(sizeof(CONFIG_CREDENTIAL_SECRET) == ASTARTE_PAIRING_CRED_SECR_LEN + 1,
    "Missing credential secret in datastreams example");
#endif

/************************************************
 * Constants, static variables and defines
 ***********************************************/

LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL); // NOLINT

#define THREAD_SLEEP_MS 500

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
K_MSGQ_DEFINE(device_msgq, sizeof(astarte_device_handle_t), 1, 1);

enum thread_flags
{
    THREAD_FLAGS_CONNECTED = 1U,
    THREAD_FLAGS_TX_COMPLETE,
    THREAD_FLAGS_RX_TERMINATION,
};
static atomic_t device_thread_flags;

K_THREAD_STACK_DEFINE(device_rx_thread_stack_area, CONFIG_DEVICE_RX_THREAD_STACK_SIZE);
static struct k_thread device_rx_thread_data;

K_THREAD_STACK_DEFINE(device_tx_thread_stack_area, CONFIG_DEVICE_TX_THREAD_STACK_SIZE);
static struct k_thread device_tx_thread_data;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

/************************************************
 * Static functions declaration
 ***********************************************/

/**
 * @brief Entry point for the Astarte device reception thread.
 *
 * @param arg1 Unused argument.
 * @param arg2 Unused argument.
 * @param arg3 Unused argument.
 */
static void device_rx_thread_entry_point(void *arg1, void *arg2, void *arg3);
/**
 * @brief Entry point for the Astarte device transmission thread.
 *
 * @param arg1 Unused argument.
 * @param arg2 Unused argument.
 * @param arg3 Unused argument.
 */
static void device_tx_thread_entry_point(void *arg1, void *arg2, void *arg3);
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
 * @brief Callback handler for Astarte datastream individual event.
 *
 * @param event Astarte device datastream individual event.
 */
static void datastream_individual_callback(astarte_device_datastream_individual_event_t event);
/**
 * @brief Callback handler for Astarte datastream object event.
 *
 * @param event Astarte device datastream object event.
 */
static void datastream_object_callback(astarte_device_datastream_object_event_t event);
/**
 * @brief Callback handler for Astarte set property event.
 *
 * @param event Astarte device set property event.
 */
static void set_property_callback(astarte_device_property_set_event_t event);
/**
 * @brief Callback handler for Astarte unset property event.
 *
 * @param event Astarte device unset property event.
 */
static void unset_property_callback(astarte_device_data_event_t event);

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

    // Spawn new rx thread for the Astarte device
    k_thread_create(&device_rx_thread_data, device_rx_thread_stack_area,
        K_THREAD_STACK_SIZEOF(device_rx_thread_stack_area), device_rx_thread_entry_point, NULL,
        NULL, NULL, CONFIG_DEVICE_THREAD_PRIORITY, 0, K_NO_WAIT);
    // Spawn new tx thread for the Astarte device
    k_thread_create(&device_tx_thread_data, device_tx_thread_stack_area,
        K_THREAD_STACK_SIZEOF(device_tx_thread_stack_area), device_tx_thread_entry_point, NULL,
        NULL, NULL, CONFIG_DEVICE_THREAD_PRIORITY, 0, K_NO_WAIT);

    while (!atomic_test_bit(&device_thread_flags, THREAD_FLAGS_TX_COMPLETE)) {
        // Ensure the connectivity is still present
#if defined(CONFIG_WIFI)
        wifi_poll();
#else
        eth_poll();
#endif
        k_sleep(K_MSEC(THREAD_SLEEP_MS));
    }

    // Ensure the Astarte tx thread has properly terminated and no more data is being sent.
    if (k_thread_join(&device_tx_thread_data, K_FOREVER) != 0) {
        LOG_ERR("Failed in waiting for the Astarte tx thread to terminate."); // NOLINT
    }

    // Signal to the Astarte rx thread that is should terminate.
    atomic_set_bit(&device_thread_flags, THREAD_FLAGS_RX_TERMINATION);

    // Wait for the Astarte rx thread to terminate.
    if (k_thread_join(&device_rx_thread_data, K_FOREVER) != 0) {
        LOG_ERR("Failed in waiting for the Astarte rx threads to terminate."); // NOLINT
    }

    LOG_INF("Astarte device sample finished."); // NOLINT
    k_sleep(K_MSEC(MSEC_PER_SEC));

    return 0;
}

/************************************************
 * Static functions definitions
 ***********************************************/

static void device_rx_thread_entry_point(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    astarte_device_handle_t device = NULL;
    astarte_result_t res = ASTARTE_RESULT_OK;

    // Create a new instance of an Astarte device
#if CONFIG_DEVICE_REGISTRATION
    char device_id[ASTARTE_DEVICE_ID_LEN + 1] = CONFIG_DEVICE_ID;
    char cred_secr[ASTARTE_PAIRING_CRED_SECR_LEN + 1] = { 0 };

    if (register_device(device_id, cred_secr) != 0) {
        LOG_ERR("Device registration failed, stopping rx thread"); // NOLINT
        k_msgq_put(&device_msgq, (void *) &device, K_FOREVER);
        return;
    }
#else
    char device_id[ASTARTE_DEVICE_ID_LEN + 1] = CONFIG_DEVICE_ID;
    char cred_secr[ASTARTE_PAIRING_CRED_SECR_LEN + 1] = CONFIG_CREDENTIAL_SECRET;
#endif

    const astarte_interface_t *interfaces[] = {
        &org_astarteplatform_zephyr_examples_DeviceDatastream,
        &org_astarteplatform_zephyr_examples_ServerDatastream,
        &org_astarteplatform_zephyr_examples_DeviceAggregate,
        &org_astarteplatform_zephyr_examples_ServerAggregate,
        &org_astarteplatform_zephyr_examples_DeviceProperty,
        &org_astarteplatform_zephyr_examples_ServerProperty,
    };

    astarte_device_config_t device_config = { 0 };
    device_config.http_timeout_ms = CONFIG_HTTP_TIMEOUT_MS;
    device_config.mqtt_connection_timeout_ms = CONFIG_MQTT_CONNECTION_TIMEOUT_MS;
    device_config.mqtt_poll_timeout_ms = CONFIG_MQTT_POLL_TIMEOUT_MS;
    device_config.connection_cbk = connection_callback;
    device_config.disconnection_cbk = disconnection_callback;
    device_config.datastream_individual_cbk = datastream_individual_callback;
    device_config.datastream_object_cbk = datastream_object_callback;
    device_config.property_set_cbk = set_property_callback;
    device_config.property_unset_cbk = unset_property_callback;
    device_config.interfaces = interfaces;
    device_config.interfaces_size = ARRAY_SIZE(interfaces);
    memcpy(device_config.device_id, device_id, sizeof(device_id));
    memcpy(device_config.cred_secr, cred_secr, sizeof(cred_secr));

    res = astarte_device_new(&device_config, &device);
    if (res != ASTARTE_RESULT_OK) {
        LOG_ERR("Astarte device creation failure."); // NOLINT
        k_msgq_put(&device_msgq, (void *) &device, K_FOREVER);
        return;
    }

    res = astarte_device_connect(device);
    if (res != ASTARTE_RESULT_OK) {
        LOG_ERR("Astarte device connection failure."); // NOLINT
        k_msgq_put(&device_msgq, (void *) &device, K_FOREVER);
        return;
    }

    // Add the message to the queue for the transmit thread
    // NO_WAIT is used since we know we are the only therad writing
    k_msgq_put(&device_msgq, (void *) &device, K_NO_WAIT);

    while (!atomic_test_bit(&device_thread_flags, THREAD_FLAGS_RX_TERMINATION)) {
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

    // we wait for a complete disconnection to avoid loosing some messages
    while (atomic_test_bit(&device_thread_flags, THREAD_FLAGS_CONNECTED)) {
        k_sleep(K_MSEC(THREAD_SLEEP_MS));
    }

    LOG_INF("Astarte device will now be destroyed."); // NOLINT
    res = astarte_device_destroy(device);
    if (res != ASTARTE_RESULT_OK) {
        LOG_ERR("Astarte device destroy failure."); // NOLINT
        return;
    }

    LOG_INF("Astarte thread will now be terminated."); // NOLINT

    k_sleep(K_MSEC(MSEC_PER_SEC));
}

static void device_tx_thread_entry_point(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    astarte_device_handle_t device = NULL;
    k_msgq_get(&device_msgq, (void *) &device, K_FOREVER);
    if (!device) {
        LOG_ERR("Received a failed device initialization, stopping transmission thread"); // NOLINT
        goto stop_transmission;
    }

    // wait for the device to be connected before sending data
    while (!atomic_test_bit(&device_thread_flags, THREAD_FLAGS_CONNECTED)) {
        k_sleep(K_MSEC(THREAD_SLEEP_MS));
    }

#if defined(CONFIG_DEVICE_INDIVIDUAL_TRANSMISSION)
    // NOLINTNEXTLINE
    LOG_INF("Waiting %d seconds to send individuals.",
        CONFIG_DEVICE_INDIVIDUAL_TRANSMISSION_DELAY_SECONDS);
    k_sleep(K_SECONDS(CONFIG_DEVICE_INDIVIDUAL_TRANSMISSION_DELAY_SECONDS));
    sample_individual_transmission(device);
#endif
#if defined(CONFIG_DEVICE_OBJECT_TRANSMISSION)
    // NOLINTNEXTLINE
    LOG_INF("Waiting %d seconds to send objects.", CONFIG_DEVICE_OBJECT_TRANSMISSION_DELAY_SECONDS);
    k_sleep(K_SECONDS(CONFIG_DEVICE_OBJECT_TRANSMISSION_DELAY_SECONDS));
    sample_object_transmission(device);
#endif
#if defined(CONFIG_DEVICE_PROPERTY_SET_TRANSMISSION)
    // NOLINTNEXTLINE
    LOG_INF("Waiting %d seconds to set properties.",
        CONFIG_DEVICE_PROPERTY_SET_TRANSMISSION_DELAY_SECONDS);
    k_sleep(K_SECONDS(CONFIG_DEVICE_PROPERTY_SET_TRANSMISSION_DELAY_SECONDS));
    sample_property_set_transmission(device);
#endif
#if defined(CONFIG_DEVICE_PROPERTY_UNSET_TRANSMISSION)
    // NOLINTNEXTLINE
    LOG_INF("Waiting %d seconds to unset properties.",
        CONFIG_DEVICE_PROPERTY_UNSET_TRANSMISSION_DELAY_SECONDS);
    k_sleep(K_SECONDS(CONFIG_DEVICE_PROPERTY_UNSET_TRANSMISSION_DELAY_SECONDS));
    sample_property_unset_transmission(device);
#endif

#if defined(CONFIG_DEVICE_INDIVIDUAL_TRANSMISSION) || defined(CONFIG_DEVICE_OBJECT_TRANSMISSION)   \
    || defined(CONFIG_DEVICE_PROPERTY_SET_TRANSMISSION)                                            \
    || defined(CONFIG_DEVICE_PROPERTY_UNSET_TRANSMISSION)
    LOG_INF("Transmission completed."); // NOLINT
#else
    // avoid unused device warnings
    (void) device;

    // NOLINTNEXTLINE
    LOG_INF("No transmission to perform. Keeping the device connected for %d seconds",
        CONFIG_DEVICE_OPERATIONAL_TIMEOUT);
    k_sleep(K_SECONDS(CONFIG_DEVICE_OPERATIONAL_TIMEOUT));
#endif

stop_transmission:
    // Signal to the main thread that the transmission is complete and the thread can be joined.
    atomic_set_bit(&device_thread_flags, THREAD_FLAGS_TX_COMPLETE);
}

static void connection_callback(astarte_device_connection_event_t event)
{
    ARG_UNUSED(event);
    LOG_INF("Astarte device connected."); // NOLINT
    atomic_set_bit(&device_thread_flags, THREAD_FLAGS_CONNECTED);
}

static void disconnection_callback(astarte_device_disconnection_event_t event)
{
    ARG_UNUSED(event);
    LOG_INF("Astarte device disconnected"); // NOLINT
    atomic_clear_bit(&device_thread_flags, THREAD_FLAGS_CONNECTED);
}

static void datastream_individual_callback(astarte_device_datastream_individual_event_t event)
{
    const char *interface_name = event.data_event.interface_name;
    const char *path = event.data_event.path;
    astarte_individual_t individual = event.individual;

    LOG_INF("Datastream individual event, interface: %s, path: %s", interface_name, path); // NOLINT

    utils_log_astarte_individual(individual);
}

static void datastream_object_callback(astarte_device_datastream_object_event_t event)
{
    const char *interface_name = event.data_event.interface_name;
    const char *path = event.data_event.path;
    astarte_object_entry_t *entries = event.entries;
    size_t entries_length = event.entries_len;

    LOG_INF("Datastream object event, interface: %s, path: %s", interface_name, path); // NOLINT

    utils_log_astarte_object(entries, entries_length);
}

static void set_property_callback(astarte_device_property_set_event_t event)
{
    const char *interface_name = event.data_event.interface_name;
    const char *path = event.data_event.path;
    astarte_individual_t individual = event.individual;

    LOG_INF("Property set event, interface: %s, path: %s", interface_name, path); // NOLINT

    utils_log_astarte_individual(individual);
}

static void unset_property_callback(astarte_device_data_event_t event)
{
    const char *interface_name = event.interface_name;
    const char *path = event.path;
    LOG_INF("Property unset event, interface: %s, path: %s", interface_name, path); // NOLINT
}
