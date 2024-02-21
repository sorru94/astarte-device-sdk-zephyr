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

#if !defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_DISABLE_OR_IGNORE_TLS)
#include "ca_certificates.h"
#include <zephyr/net/tls_credentials.h>
#endif

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL); // NOLINT

#include <astarte_device_sdk/device.h>
#include <astarte_device_sdk/interface.h>
#include <astarte_device_sdk/pairing.h>

#include "nvs.h"
#include "wifi.h"

/************************************************
 * Constants and defines
 ***********************************************/

#define MQTT_POLL_TIMEOUT_MS 200
#define DEVICE_OPERATIONAL_TIME_MS (60 * MSEC_PER_SEC)

const static astarte_interface_t device_datastream_interface = {
    .name = "org.astarteplatform.zephyr.examples.DeviceDatastream",
    .major_version = 0,
    .minor_version = 1,
    .ownership = OWNERSHIP_DEVICE,
    .type = TYPE_DATASTREAM,
};

const static astarte_interface_t server_datastream_interface = {
    .name = "org.astarteplatform.zephyr.examples.ServerDatastream",
    .major_version = 0,
    .minor_version = 1,
    .ownership = OWNERSHIP_SERVER,
    .type = TYPE_DATASTREAM,
};

/************************************************
 * Static functions declaration
 ***********************************************/

/**
 * @brief Handler for astarte connection events.
 *
 * @param event Astarte device connection event pointer.
 */
static void astarte_connection_events_handler(astarte_device_connection_event_t *event);
/**
 * @brief Handler for astarte disconnection events.
 *
 * @param event Astarte device disconnection event pointer.
 */
static void astarte_disconnection_events_handler(astarte_device_disconnection_event_t *event);
/**
 * @brief Handler for astarte data event.
 *
 * @param event Astarte device data event pointer.
 */
static void astarte_data_events_handler(astarte_device_data_event_t *event);

/************************************************
 * Global functions definition
 ***********************************************/

int main(void)
{
    astarte_err_t astarte_err = ASTARTE_OK;
    LOG_INF("MQTT Example\nBoard: %s", CONFIG_BOARD); // NOLINT

    // Initialize WiFi driver
#if !defined(CONFIG_BOARD_NATIVE_SIM)
    LOG_INF("Initializing WiFi driver."); // NOLINT
    wifi_init();
#endif

    // Initialize NVS driver
    if (nvs_init() != 0) {
        LOG_ERR("NVS intialization failed!"); // NOLINT
        return -1;
    }

    k_sleep(K_SECONDS(5)); // sleep for 5 seconds

#if !defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_DISABLE_OR_IGNORE_TLS)
    tls_credential_add(CONFIG_ASTARTE_DEVICE_SDK_CA_CERT_TAG, TLS_CREDENTIAL_CA_CERTIFICATE,
        ca_certificate_root, sizeof(ca_certificate_root));
#endif

    bool has_cred_secr = false;
    if (nvs_has_cred_secr(&has_cred_secr) != 0) {
        return -1;
    }

    int32_t timeout_ms = 3 * MSEC_PER_SEC;
    char cred_secr[ASTARTE_PAIRING_CRED_SECR_LEN + 1] = { 0 };
    if (has_cred_secr) {
        if (nvs_get_cred_secr(cred_secr, sizeof(cred_secr)) != 0) {
            return -1;
        }
    } else {
        astarte_err = astarte_pairing_register_device(timeout_ms, cred_secr, sizeof(cred_secr));
        if (astarte_err != ASTARTE_OK) {
            return -1;
        }
        if (nvs_store_cred_secr(cred_secr) != 0) {
            return -1;
        }
    }

    LOG_WRN("Credential secret: '%s'", cred_secr); // NOLINT

    const astarte_interface_t *interfaces[]
        = { &device_datastream_interface, &server_datastream_interface };

    astarte_device_config_t device_config;
    device_config.http_timeout_ms = timeout_ms;
    device_config.mqtt_connection_timeout_ms = timeout_ms;
    device_config.mqtt_connected_timeout_ms = MQTT_POLL_TIMEOUT_MS;
    device_config.connection_cbk = astarte_connection_events_handler;
    device_config.disconnection_cbk = astarte_disconnection_events_handler;
    device_config.data_cbk = astarte_data_events_handler;
    device_config.interfaces = interfaces;
    device_config.interfaces_size = ARRAY_SIZE(interfaces);
    memcpy(device_config.cred_secr, cred_secr, sizeof(cred_secr));

    astarte_device_handle_t device = NULL;
    astarte_err = astarte_device_new(&device_config, &device);
    if (astarte_err != ASTARTE_OK) {
        return -1;
    }

    astarte_err = astarte_device_connect(device);
    if (astarte_err != ASTARTE_OK) {
        return -1;
    }

    astarte_err = astarte_device_poll(device);
    if (astarte_err != ASTARTE_OK) {
        LOG_ERR("First poll should not timeout as we should receive a connection ack."); // NOLINT
        return -1;
    }

    k_timepoint_t disconnect_timepoint = sys_timepoint_calc(K_MSEC(DEVICE_OPERATIONAL_TIME_MS));
    int count = 0;
    while (1) {
        k_timepoint_t timepoint = sys_timepoint_calc(K_MSEC(MQTT_POLL_TIMEOUT_MS));

        astarte_err = astarte_device_poll(device);
        if ((astarte_err != ASTARTE_ERR_TIMEOUT) && (astarte_err != ASTARTE_OK)) {
            return -1;
        }

        if (++count % (CONFIG_SLEEP_MS / MQTT_POLL_TIMEOUT_MS) == 0) {
            LOG_INF("Hello world! %s", CONFIG_BOARD); // NOLINT
            count = 0;
        }
        k_sleep(sys_timepoint_timeout(timepoint));

        if (K_TIMEOUT_EQ(sys_timepoint_timeout(disconnect_timepoint), K_NO_WAIT)) {
            break;
        }
    }

    LOG_INF("End of loop, disconnection imminent %s", CONFIG_BOARD); // NOLINT

    astarte_err = astarte_device_destroy(device);
    if (astarte_err != ASTARTE_OK) {
        LOG_ERR("Failed destroying the device."); // NOLINT
        return -1;
    }

    return 0;
}

/************************************************
 * Static functions definitions
 ***********************************************/

static void astarte_connection_events_handler(astarte_device_connection_event_t *event)
{
    LOG_INF("Astarte device connected, session_present: %d", event->session_present); // NOLINT
}

static void astarte_disconnection_events_handler(astarte_device_disconnection_event_t *event)
{
    (void) event;
    LOG_INF("Astarte device disconnected"); // NOLINT
}

static void astarte_data_events_handler(astarte_device_data_event_t *event)
{
    // NOLINTNEXTLINE
    LOG_INF("Got Astarte data event, interface_name: %s, path: %s, bson_type: %d",
        event->interface_name, event->path, event->bson_element.type);
}
