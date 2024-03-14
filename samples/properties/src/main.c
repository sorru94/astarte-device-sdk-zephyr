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

#if !defined(CONFIG_SDK_DEVELOP_DISABLE_OR_IGNORE_TLS)
#include "ca_certificates.h"
#include <zephyr/net/tls_credentials.h>
#endif

#include <astarte_device_sdk/bson_types.h>
#include <astarte_device_sdk/device.h>
#include <astarte_device_sdk/interface.h>
#include <astarte_device_sdk/pairing.h>
#include <astarte_device_sdk/value.h>

#if defined(CONFIG_WIFI)
#include "wifi.h"
#else
#include "eth.h"
#endif

LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL); // NOLINT

/************************************************
 *       Checks over configuration values       *
 ***********************************************/

BUILD_ASSERT(sizeof(CONFIG_CREDENTIAL_SECRET) == ASTARTE_PAIRING_CRED_SECR_LEN + 1,
    "Missing credential secret in properties example");

/************************************************
 * Constants and defines
 ***********************************************/

#define MQTT_POLL_TIMEOUT_MS 200
#define DEVICE_OPERATIONAL_TIME_MS (60 * MSEC_PER_SEC)

const static astarte_interface_t device_property_interface = {
    .name = "org.astarteplatform.zephyr.examples.DeviceProperty",
    .major_version = 0,
    .minor_version = 1,
    .ownership = ASTARTE_INTERFACE_OWNERSHIP_DEVICE,
    .type = ASTARTE_INTERFACE_TYPE_PROPERTIES,
};

const static astarte_interface_t server_property_interface = {
    .name = "org.astarteplatform.zephyr.examples.ServerProperty",
    .major_version = 0,
    .minor_version = 1,
    .ownership = ASTARTE_INTERFACE_OWNERSHIP_SERVER,
    .type = ASTARTE_INTERFACE_TYPE_PROPERTIES,
};

#define GROUP_1_ID 40
#define GROUP_2_ID 41
#define GROUP_3_ID 42
#define GROUP_4_ID 43

/************************************************
 * Static functions declaration
 ***********************************************/

/**
 * @brief Handler for astarte connection events.
 *
 * @param event Astarte device connection event pointer.
 */
static void connection_events_handler(astarte_device_connection_event_t *event);
/**
 * @brief Handler for astarte disconnection events.
 *
 * @param event Astarte device disconnection event pointer.
 */
static void disconnection_events_handler(astarte_device_disconnection_event_t *event);
/**
 * @brief Handler for astarte data event.
 *
 * @param event Astarte device data event pointer.
 */
static void data_events_handler(astarte_device_data_event_t *event);
/**
 * @brief Set properties of group 1.
 *
 * @param event Astarte device data event pointer.
 */
static void set_properties_group_1(astarte_device_data_event_t *event);
/**
 * @brief Set properties of group 2.
 *
 * @param event Astarte device data event pointer.
 */
static void set_properties_group_2(astarte_device_data_event_t *event);
/**
 * @brief Unset properties of group 1.
 *
 * @param event Astarte device data event pointer.
 */
static void unset_properties_group_1(astarte_device_data_event_t *event);
/**
 * @brief Unset properties of group 2.
 *
 * @param event Astarte device data event pointer.
 */
static void unset_properties_group_2(astarte_device_data_event_t *event);

/************************************************
 * Global functions definition
 ***********************************************/

int main(void)
{
    astarte_result_t res = ASTARTE_RESULT_OK;
    LOG_INF("MQTT Example\nBoard: %s", CONFIG_BOARD); // NOLINT

    // Initialize WiFi driver
#if defined(CONFIG_WIFI)
    LOG_INF("Initializing WiFi driver."); // NOLINT
    wifi_init();
#else
    if (eth_connect() != 0) {
        LOG_ERR("Connectivity intialization failed!"); // NOLINT
        return -1;
    }
#endif

    k_sleep(K_SECONDS(5)); // sleep for 5 seconds

#if !defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_DISABLE_OR_IGNORE_TLS)
    tls_credential_add(CONFIG_ASTARTE_DEVICE_SDK_CA_CERT_TAG, TLS_CREDENTIAL_CA_CERTIFICATE,
        ca_certificate_root, sizeof(ca_certificate_root));
#endif

    int32_t timeout_ms = 3 * MSEC_PER_SEC;
    char cred_secr[ASTARTE_PAIRING_CRED_SECR_LEN + 1] = CONFIG_CREDENTIAL_SECRET;

    const astarte_interface_t *interfaces[]
        = { &device_property_interface, &server_property_interface };

    astarte_device_config_t device_config;
    device_config.http_timeout_ms = timeout_ms;
    device_config.mqtt_connection_timeout_ms = timeout_ms;
    device_config.mqtt_connected_timeout_ms = MQTT_POLL_TIMEOUT_MS;
    device_config.connection_cbk = connection_events_handler;
    device_config.disconnection_cbk = disconnection_events_handler;
    device_config.data_cbk = data_events_handler;
    device_config.interfaces = interfaces;
    device_config.interfaces_size = ARRAY_SIZE(interfaces);
    memcpy(device_config.cred_secr, cred_secr, sizeof(cred_secr));

    astarte_device_handle_t device = NULL;
    res = astarte_device_new(&device_config, &device);
    if (res != ASTARTE_RESULT_OK) {
        return -1;
    }

    res = astarte_device_connect(device);
    if (res != ASTARTE_RESULT_OK) {
        return -1;
    }

    res = astarte_device_poll(device);
    if (res != ASTARTE_RESULT_OK) {
        LOG_ERR("First poll should not timeout as we should receive a connection ack."); // NOLINT
        return -1;
    }

    k_timepoint_t disconnect_timepoint = sys_timepoint_calc(K_MSEC(DEVICE_OPERATIONAL_TIME_MS));
    int count = 0;
    while (1) {
        k_timepoint_t timepoint = sys_timepoint_calc(K_MSEC(MQTT_POLL_TIMEOUT_MS));

        res = astarte_device_poll(device);
        if ((res != ASTARTE_RESULT_TIMEOUT) && (res != ASTARTE_RESULT_OK)) {
            return -1;
        }

        if (++count % (CONFIG_SLEEP_MS / MQTT_POLL_TIMEOUT_MS) == 0) {
            LOG_INF("Polling mqtt events... %s", CONFIG_BOARD); // NOLINT
            count = 0;
        }
        k_sleep(sys_timepoint_timeout(timepoint));

        if (K_TIMEOUT_EQ(sys_timepoint_timeout(disconnect_timepoint), K_NO_WAIT)) {
            break;
        }
    }

    LOG_INF("End of loop, disconnection imminent %s", CONFIG_BOARD); // NOLINT

    res = astarte_device_destroy(device);
    if (res != ASTARTE_RESULT_OK) {
        LOG_ERR("Failed destroying the device."); // NOLINT
        return -1;
    }

    return 0;
}

/************************************************
 * Static functions definitions
 ***********************************************/

static void connection_events_handler(astarte_device_connection_event_t *event)
{
    LOG_INF("Astarte device connected, session_present: %d", event->session_present); // NOLINT
}

static void disconnection_events_handler(astarte_device_disconnection_event_t *event)
{
    (void) event;
    LOG_INF("Astarte device disconnected"); // NOLINT
}

static void data_events_handler(astarte_device_data_event_t *event)
{
    // NOLINTNEXTLINE
    LOG_INF("Got Astarte data event, interface_name: %s, path: %s, bson_type: %d",
        event->interface_name, event->path, event->bson_element.type);

    if ((strcmp(event->interface_name, server_property_interface.name) == 0)
        && (strcmp(event->path, "/sensor11/integer_endpoint") == 0)
        && (astarte_bson_deserializer_element_to_int32(event->bson_element) == GROUP_1_ID)) {
        // NOLINTNEXTLINE
        LOG_INF("Received set property event on integer endpoint with content '%d'.", GROUP_1_ID);
        set_properties_group_1(event);
    }
    if ((strcmp(event->interface_name, server_property_interface.name) == 0)
        && (strcmp(event->path, "/sensor11/integer_endpoint") == 0)
        && (astarte_bson_deserializer_element_to_int32(event->bson_element) == GROUP_2_ID)) {
        // NOLINTNEXTLINE
        LOG_INF("Received set property event on integer endpoint with content '%d'.", GROUP_2_ID);
        set_properties_group_2(event);
    }
    if ((strcmp(event->interface_name, server_property_interface.name) == 0)
        && (strcmp(event->path, "/sensor11/integer_endpoint") == 0)
        && (astarte_bson_deserializer_element_to_int32(event->bson_element) == GROUP_3_ID)) {
        // NOLINTNEXTLINE
        LOG_INF("Received set property event on integer endpoint with content '%d'.", GROUP_3_ID);
        unset_properties_group_1(event);
    }
    if ((strcmp(event->interface_name, server_property_interface.name) == 0)
        && (strcmp(event->path, "/sensor11/integer_endpoint") == 0)
        && (astarte_bson_deserializer_element_to_int32(event->bson_element) == GROUP_4_ID)) {
        // NOLINTNEXTLINE
        LOG_INF("Received set property event on integer endpoint with content '%d'.", GROUP_4_ID);
        unset_properties_group_2(event);
    }
}

// The values sent are magic numbers.
// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
static void set_properties_group_1(astarte_device_data_event_t *event)
{
    LOG_INF("Setting group 1 device properties."); // NOLINT

    astarte_result_t res = ASTARTE_RESULT_OK;
    res = astarte_device_set_property(event->device, device_property_interface.name,
        "/sensor44/double_endpoint", astarte_value_from_double(32.4));
    if (res != ASTARTE_RESULT_OK) {
        LOG_ERR("Setting operation for double_endpoint failed."); // NOLINT
        return;
    }
    res = astarte_device_set_property(event->device, device_property_interface.name,
        "/sensor44/string_endpoint", astarte_value_from_string("hello world"));
    if (res != ASTARTE_RESULT_OK) {
        LOG_ERR("Setting operation for string_endpoint failed."); // NOLINT
        return;
    }
    res = astarte_device_set_property(event->device, device_property_interface.name,
        "/sensor44/datetime_endpoint", astarte_value_from_datetime(1710237968800));
    if (res != ASTARTE_RESULT_OK) {
        LOG_ERR("Setting operation for datetime_endpoint failed."); // NOLINT
        return;
    }
    res = astarte_device_set_property(event->device, device_property_interface.name,
        "/sensor44/longinteger_endpoint", astarte_value_from_longinteger(34359738368));
    if (res != ASTARTE_RESULT_OK) {
        LOG_ERR("Setting operation for longinteger_endpoint failed."); // NOLINT
        return;
    }
    res = astarte_device_set_property(event->device, device_property_interface.name,
        "/sensor44/boolean_endpoint", astarte_value_from_boolean(true));
    if (res != ASTARTE_RESULT_OK) {
        LOG_ERR("Setting operation for boolean_endpoint failed."); // NOLINT
        return;
    }
}
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

// The values sent are magic numbers.
// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
static void set_properties_group_2(astarte_device_data_event_t *event)
{

    LOG_INF("Setting group 2 device properties."); // NOLINT

    astarte_result_t res = ASTARTE_RESULT_OK;
    res = astarte_device_set_property(event->device, device_property_interface.name,
        "/sensor44/integer_endpoint", astarte_value_from_integer(11));
    if (res != ASTARTE_RESULT_OK) {
        LOG_ERR("Setting operation for integer_endpoint failed."); // NOLINT
        return;
    }
    bool bool_arr[] = { true, false };
    res = astarte_device_set_property(event->device, device_property_interface.name,
        "/sensor44/booleanarray_endpoint",
        astarte_value_from_boolean_array(bool_arr, ARRAY_SIZE(bool_arr)));
    if (res != ASTARTE_RESULT_OK) {
        LOG_ERR("Setting operation for booleanarray_endpoint failed."); // NOLINT
        return;
    }
    double double_array[] = { 0.1, 32.4, 0.0, 23.4 };
    res = astarte_device_set_property(event->device, device_property_interface.name,
        "/sensor44/doublearray_endpoint",
        astarte_value_from_double_array(double_array, ARRAY_SIZE(double_array)));
    if (res != ASTARTE_RESULT_OK) {
        LOG_ERR("Setting operation for doublearray_endpoint failed."); // NOLINT
        return;
    }
}
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

// The values sent are magic numbers.
// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
static void unset_properties_group_1(astarte_device_data_event_t *event)
{
    LOG_INF("Unsetting group 1 device properties."); // NOLINT

    astarte_result_t res = ASTARTE_RESULT_OK;
    res = astarte_device_unset_property(
        event->device, device_property_interface.name, "/sensor44/double_endpoint");
    if (res != ASTARTE_RESULT_OK) {
        LOG_ERR("Unsetting operation for double_endpoint failed."); // NOLINT
        return;
    }
    res = astarte_device_unset_property(
        event->device, device_property_interface.name, "/sensor44/string_endpoint");
    if (res != ASTARTE_RESULT_OK) {
        LOG_ERR("Unsetting operation for string_endpoint failed."); // NOLINT
        return;
    }
    res = astarte_device_unset_property(
        event->device, device_property_interface.name, "/sensor44/datetime_endpoint");
    if (res != ASTARTE_RESULT_OK) {
        LOG_ERR("Unsetting operation for datetime_endpoint failed."); // NOLINT
        return;
    }
    res = astarte_device_unset_property(
        event->device, device_property_interface.name, "/sensor44/longinteger_endpoint");
    if (res != ASTARTE_RESULT_OK) {
        LOG_ERR("Unsetting operation for longinteger_endpoint failed."); // NOLINT
        return;
    }
    res = astarte_device_unset_property(
        event->device, device_property_interface.name, "/sensor44/boolean_endpoint");
    if (res != ASTARTE_RESULT_OK) {
        LOG_ERR("Unsetting operation for boolean_endpoint failed."); // NOLINT
        return;
    }
    res = astarte_device_unset_property(
        event->device, device_property_interface.name, "/sensor44/booleanarray_endpoint");
    if (res != ASTARTE_RESULT_OK) {
        LOG_ERR("Unsetting operation for booleanarray_endpoint failed."); // NOLINT
        return;
    }
}
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

// The values sent are magic numbers.
// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
static void unset_properties_group_2(astarte_device_data_event_t *event)
{
    LOG_INF("Unsetting group 2 device properties."); // NOLINT

    astarte_result_t res = ASTARTE_RESULT_OK;
    res = astarte_device_unset_property(
        event->device, device_property_interface.name, "/sensor44/integer_endpoint");
    if (res != ASTARTE_RESULT_OK) {
        LOG_ERR("Unsetting operation for integer_endpoint failed."); // NOLINT
        return;
    }
    res = astarte_device_unset_property(
        event->device, device_property_interface.name, "/sensor44/booleanarray_endpoint");
    if (res != ASTARTE_RESULT_OK) {
        LOG_ERR("Unsetting operation for booleanarray_endpoint failed."); // NOLINT
        return;
    }
    res = astarte_device_unset_property(
        event->device, device_property_interface.name, "/sensor44/doublearray_endpoint");
    if (res != ASTARTE_RESULT_OK) {
        LOG_ERR("Unsetting operation for doublearray_endpoint failed."); // NOLINT
        return;
    }
}
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
