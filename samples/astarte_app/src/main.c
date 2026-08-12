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

#include <astarte_device_sdk/data.h>
#include <astarte_device_sdk/device.h>
#include <astarte_device_sdk/interface.h>
#include <astarte_device_sdk/mapping.h>
#include <astarte_device_sdk/pairing.h>

#ifdef CONFIG_WIFI
#include "wifi.h"
#else
#include "eth.h"
#endif

#include "utils.h"

#include "generated_interfaces.h"

#ifdef CONFIG_DEVICE_INDIVIDUAL_TRANSMISSION
#include "individual_send.h"
#endif
#ifdef CONFIG_DEVICE_OBJECT_TRANSMISSION
#include "object_send.h"
#endif
#if defined(CONFIG_DEVICE_PROPERTY_SET_TRANSMISSION)                                               \
    || defined(CONFIG_DEVICE_PROPERTY_UNSET_TRANSMISSION)
#include "property_send.h"
#endif
#ifdef CONFIG_DEVICE_REGISTRATION
#include "register.h"
#endif

#include "sample_config.h"

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
 * @param device_id Device id for the Astarte device.
 * @param cred_secr/arg2 Unused argument or credential secret.
 * @param arg3 Unused argument.
 */
#if CONFIG_DEVICE_REGISTRATION
static void device_rx_thread_entry_point(void *device_id, void *arg2, void *arg3);
#else
static void device_rx_thread_entry_point(void *device_id, void *cred_secr, void *arg3);
#endif
/**
 * @brief Entry point for the Astarte device transmission thread.
 *
 * @param arg1 Unused argument.
 * @param arg2 Unused argument.
 * @param arg3 Unused argument.
 */
static void device_tx_thread_entry_point(void *arg1, void *arg2, void *arg3);
/**
 * @brief Handle an Astarte device event.
 *
 * @param event The received Astarte device event.
 */
static void handle_device_event(astarte_device_event_t *event);

/************************************************
 * Global functions definition
 ***********************************************/

int main(void)
{
    LOG_INF("Astarte device sample"); // NOLINT
    LOG_INF("Board: %s", CONFIG_BOARD); // NOLINT

    struct sample_config cfg_from_file = { 0 };
    sample_config_get(&cfg_from_file);
    LOG_INF("Configured device ID: %s", cfg_from_file.device_id); // NOLINT
#ifndef CONFIG_DEVICE_REGISTRATION
    LOG_INF("Configured credential secret: %s", cfg_from_file.credential_secret); // NOLINT
#endif
#ifdef CONFIG_WIFI
    LOG_INF("Configured WiFi SSID: %s", cfg_from_file.wifi_ssid); // NOLINT
    LOG_INF("Configured WiFi password: %s", cfg_from_file.wifi_pwd); // NOLINT
#endif

#ifdef CONFIG_WIFI
    LOG_INF("Initializing WiFi driver."); // NOLINT
    app_wifi_init();
    k_sleep(K_SECONDS(5));
    enum wifi_security_type sec = WIFI_SECURITY_TYPE_PSK;
    if (app_wifi_connect(cfg_from_file.wifi_ssid, sec, cfg_from_file.wifi_pwd) != 0) {
        LOG_ERR("Connectivity intialization failed!"); // NOLINT
        return -1;
    }
#else
    // Initialize Ethernet driver
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
#if CONFIG_DEVICE_REGISTRATION
    k_thread_create(&device_rx_thread_data, device_rx_thread_stack_area,
        K_THREAD_STACK_SIZEOF(device_rx_thread_stack_area), device_rx_thread_entry_point,
        cfg_from_file.device_id, NULL, NULL, CONFIG_DEVICE_THREAD_PRIORITY, 0, K_NO_WAIT);
#else
    k_thread_create(&device_rx_thread_data, device_rx_thread_stack_area,
        K_THREAD_STACK_SIZEOF(device_rx_thread_stack_area), device_rx_thread_entry_point,
        cfg_from_file.device_id, cfg_from_file.credential_secret, NULL,
        CONFIG_DEVICE_THREAD_PRIORITY, 0, K_NO_WAIT);
#endif
    // Spawn new tx thread for the Astarte device
    k_thread_create(&device_tx_thread_data, device_tx_thread_stack_area,
        K_THREAD_STACK_SIZEOF(device_tx_thread_stack_area), device_tx_thread_entry_point, NULL,
        NULL, NULL, CONFIG_DEVICE_THREAD_PRIORITY, 0, K_NO_WAIT);

    while (!atomic_test_bit(&device_thread_flags, THREAD_FLAGS_TX_COMPLETE)) {
#ifndef CONFIG_WIFI
        // Ensure the connectivity is still present
        if (eth_poll() != 0) {
            LOG_ERR("Failed polling Ethernet."); // NOLINT
            return -1;
        }
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

#if CONFIG_DEVICE_REGISTRATION
static void device_rx_thread_entry_point(void *device_id, void *arg2, void *arg3)
{
    ARG_UNUSED(arg2);
#else
static void device_rx_thread_entry_point(void *device_id, void *cred_secr, void *arg3)
{
#endif
    ARG_UNUSED(arg3);

    astarte_device_handle_t device = NULL;
    astarte_result_t res = ASTARTE_RESULT_OK;

    // Create a new instance of an Astarte device
#if CONFIG_DEVICE_REGISTRATION
    char cred_secr[ASTARTE_PAIRING_CRED_SECR_LEN + 1] = { 0 };
    if (register_device((char *) device_id, cred_secr) != 0) {
        LOG_ERR("Device registration failed, stopping rx thread"); // NOLINT
        k_msgq_put(&device_msgq, (void *) &device, K_FOREVER);
        return;
    }
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
    device_config.interfaces = interfaces;
    device_config.interfaces_size = ARRAY_SIZE(interfaces);
    memcpy(device_config.device_id, device_id, ASTARTE_DEVICE_ID_LEN + 1);
    memcpy(device_config.cred_secr, (const char *) cred_secr, ASTARTE_PAIRING_CRED_SECR_LEN + 1);

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
        astarte_device_event_t event = { 0 };

        astarte_result_t get_res
            = astarte_device_get_event(device, &event, K_MSEC(CONFIG_DEVICE_POLL_PERIOD_MS));

        if ((get_res != ASTARTE_RESULT_OK) && (get_res != ASTARTE_RESULT_TIMEOUT)) {
            // NOLINTNEXTLINE
            LOG_ERR("Error event retrieval failure: %s", astarte_result_to_name(get_res));
            continue;
        }

        if (get_res == ASTARTE_RESULT_OK) {
            handle_device_event(&event);
            astarte_device_event_cleanup(&event);
        }
    }

    LOG_INF("End of loop, disconnection imminent."); // NOLINT

    res = astarte_device_disconnect(device, K_SECONDS(10));
    if (res != ASTARTE_RESULT_OK) {
        LOG_ERR("Astarte device disconnection failure %s.", astarte_result_to_name(res)); // NOLINT
        return;
    }

    // we wait for a complete disconnection to avoid loosing some messages
    while (atomic_test_bit(&device_thread_flags, THREAD_FLAGS_CONNECTED)) {
        astarte_device_event_t event = { 0 };
        astarte_result_t get_res
            = astarte_device_get_event(device, &event, K_MSEC(CONFIG_DEVICE_POLL_PERIOD_MS));

        if ((get_res != ASTARTE_RESULT_OK) && (get_res != ASTARTE_RESULT_TIMEOUT)) {
            LOG_ERR("Error event retrieval failure: %s", astarte_result_to_name(get_res)); // NOLINT
            continue;
        }

        if (get_res == ASTARTE_RESULT_OK) {
            handle_device_event(&event);
            astarte_device_event_cleanup(&event);
        }
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

#ifdef CONFIG_DEVICE_INDIVIDUAL_TRANSMISSION
    // NOLINTNEXTLINE
    LOG_INF("Waiting %d seconds to send individuals.",
        CONFIG_DEVICE_INDIVIDUAL_TRANSMISSION_DELAY_SECONDS);
    k_sleep(K_SECONDS(CONFIG_DEVICE_INDIVIDUAL_TRANSMISSION_DELAY_SECONDS));
    sample_individual_transmission(device);
#endif
#ifdef CONFIG_DEVICE_OBJECT_TRANSMISSION
    // NOLINTNEXTLINE
    LOG_INF("Waiting %d seconds to send objects.", CONFIG_DEVICE_OBJECT_TRANSMISSION_DELAY_SECONDS);
    k_sleep(K_SECONDS(CONFIG_DEVICE_OBJECT_TRANSMISSION_DELAY_SECONDS));
    sample_object_transmission(device);
#endif
#ifdef CONFIG_DEVICE_PROPERTY_SET_TRANSMISSION
    // NOLINTNEXTLINE
    LOG_INF("Waiting %d seconds to set properties.",
        CONFIG_DEVICE_PROPERTY_SET_TRANSMISSION_DELAY_SECONDS);
    k_sleep(K_SECONDS(CONFIG_DEVICE_PROPERTY_SET_TRANSMISSION_DELAY_SECONDS));
    sample_property_set_transmission(device);
#endif
#ifdef CONFIG_DEVICE_PROPERTY_UNSET_TRANSMISSION
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
            atomic_set_bit(&device_thread_flags, THREAD_FLAGS_CONNECTED);
            break;
        }
        case ASTARTE_DEVICE_EVENT_DISCONNECTED: {
            LOG_INF("Astarte device disconnected."); // NOLINT
            atomic_clear_bit(&device_thread_flags, THREAD_FLAGS_CONNECTED);
            break;
        }
        case ASTARTE_DEVICE_EVENT_DATASTREAM_INDIVIDUAL: {
            const char *interface_name
                = event->data.datastream_individual.base_event.interface_name;
            const char *path = event->data.datastream_individual.base_event.path;
            astarte_data_t individual = event->data.datastream_individual.data;
            // NOLINTNEXTLINE
            LOG_INF("Datastream individual event, interface: %s, path: %s", interface_name, path);
            utils_log_astarte_data(individual);
            break;
        }
        case ASTARTE_DEVICE_EVENT_DATASTREAM_OBJECT: {
            const char *interface_name = event->data.datastream_object.base_event.interface_name;
            const char *path = event->data.datastream_object.base_event.path;
            astarte_object_entry_t *entries = event->data.datastream_object.entries;
            size_t entries_length = event->data.datastream_object.entries_len;
            // NOLINTNEXTLINE
            LOG_INF("Datastream object event, interface: %s, path: %s", interface_name, path);
            utils_log_astarte_object(entries, entries_length);
            break;
        }
        case ASTARTE_DEVICE_EVENT_PROPERTY_SET: {
            const char *interface_name = event->data.property_set.base_event.interface_name;
            const char *path = event->data.property_set.base_event.path;
            astarte_data_t individual = event->data.property_set.data;
            // NOLINTNEXTLINE
            LOG_INF("Property set event, interface: %s, path: %s", interface_name, path);
            utils_log_astarte_data(individual);
            break;
        }
        case ASTARTE_DEVICE_EVENT_PROPERTY_UNSET: {
            const char *interface_name = event->data.property_unset.interface_name;
            const char *path = event->data.property_unset.path;
            // NOLINTNEXTLINE
            LOG_INF("Property unset event, interface: %s, path: %s", interface_name, path);
            break;
        }
        default: {
            break;
        }
    }
}
