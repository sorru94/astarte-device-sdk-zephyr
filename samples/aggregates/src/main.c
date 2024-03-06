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

#include <astarte_device_sdk/bson_serializer.h>
#include <astarte_device_sdk/bson_types.h>
#include <astarte_device_sdk/device.h>
#include <astarte_device_sdk/interface.h>
#include <astarte_device_sdk/pairing.h>
#include <astarte_device_sdk/type.h>

#include "wifi.h"

LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL); // NOLINT

/************************************************
 *       Checks over configuration values       *
 ***********************************************/

BUILD_ASSERT(sizeof(CONFIG_CREDENTIAL_SECRET) == ASTARTE_PAIRING_CRED_SECR_LEN + 1,
    "Missing credential secret in aggregates example");

/************************************************
 * Constants and defines
 ***********************************************/

#define MQTT_POLL_TIMEOUT_MS 200
#define DEVICE_OPERATIONAL_TIME_MS (60 * MSEC_PER_SEC)

const static astarte_interface_t device_aggregate_interface = {
    .name = "org.astarteplatform.zephyr.examples.DeviceAggregate",
    .major_version = 0,
    .minor_version = 1,
    .ownership = OWNERSHIP_DEVICE,
    .type = TYPE_DATASTREAM,
};

const static astarte_interface_t server_aggregate_interface = {
    .name = "org.astarteplatform.zephyr.examples.ServerAggregate",
    .major_version = 0,
    .minor_version = 1,
    .ownership = OWNERSHIP_SERVER,
    .type = TYPE_DATASTREAM,
};

/************************************************
 * Structs declarations
 ***********************************************/

struct rx_aggregate
{
    bool booleans[4];
    int64_t longinteger;
};

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
 * @brief Parse the received BSON data for this example.
 *
 * @param[in] bson_element The bson element to be parsed
 * @param[out] the resulting parsed data
 * @return 0 when successful, 1 otherwise
 */
static uint8_t parse_received_bson(bson_element_t bson_element, struct rx_aggregate *rx_data);

/************************************************
 * Global functions definition
 ***********************************************/

int main(void)
{
    astarte_err_t astarte_err = ASTARTE_OK;
    LOG_INF("MQTT Example\nBoard: %s", CONFIG_BOARD); // NOLINT

    // Initialize WiFi driver
#if defined(CONFIG_WIFI)
    LOG_INF("Initializing WiFi driver."); // NOLINT
    wifi_init();
#endif

    k_sleep(K_SECONDS(5)); // sleep for 5 seconds

#if !defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_DISABLE_OR_IGNORE_TLS)
    tls_credential_add(CONFIG_ASTARTE_DEVICE_SDK_CA_CERT_TAG, TLS_CREDENTIAL_CA_CERTIFICATE,
        ca_certificate_root, sizeof(ca_certificate_root));
#endif

    int32_t timeout_ms = 3 * MSEC_PER_SEC;
    char cred_secr[ASTARTE_PAIRING_CRED_SECR_LEN + 1] = CONFIG_CREDENTIAL_SECRET;

    const astarte_interface_t *interfaces[]
        = { &device_aggregate_interface, &server_aggregate_interface };

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
            LOG_INF("Polling mqtt events... %s", CONFIG_BOARD); // NOLINT
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

    struct rx_aggregate rx_data;

    if ((strcmp(event->interface_name, server_aggregate_interface.name) != 0)
        || (strcmp(event->path, "/11") != 0)) {
        LOG_ERR("Server aggregate incorrectly received at path %s.", event->path); // NOLINT
        return;
    }

    if (parse_received_bson(event->bson_element, &rx_data) != 0) {
        LOG_ERR("Server aggregate incorrectly received."); // NOLINT
        return;
    }

    LOG_INF("Server aggregate received with the following content:"); // NOLINT
    LOG_INF("longinteger_endpoint: %lli", rx_data.longinteger); // NOLINT
    // NOLINTNEXTLINE
    LOG_INF("booleanarray_endpoint: {%d, %d, %d, %d}", rx_data.booleans[0], rx_data.booleans[1],
        rx_data.booleans[2], rx_data.booleans[3]);

    // TODO enable the device response once we implement stream aggregate
    // const double double_endpoint = 43.2;
    // const int32_t integer_endpoint = 54;
    // bool boolean_endpoint = true;
    // const double doubleanarray_endpoint[] = { 11.2, 2.2, 99.9, 421.1 };
    // LOG_INF("Sending device aggregate with the following content:");
    // LOG_INF("double_endpoint: %lf", double_endpoint);
    // LOG_INF("integer_endpoint: %" PRIu32, integer_endpoint);
    // LOG_INF("boolean_endpoint: %d", boolean_endpoint);
    // LOG_INF("doubleanarray_endpoint: {%lf, %lf, %lf, %lf}", doubleanarray_endpoint[0],
    //    doubleanarray_endpoint[1], doubleanarray_endpoint[2], doubleanarray_endpoint[3]);

    // bson_serializer_handle_t aggregate_bson = bson_serializer_new();
    // bson_serializer_append_double(aggregate_bson, "double_endpoint", double_endpoint);
    // bson_serializer_append_int32(aggregate_bson, "integer_endpoint", integer_endpoint);
    // bson_serializer_append_boolean(
    //     aggregate_bson, "boolean_endpoint", boolean_endpoint);
    // bson_serializer_append_double_array(
    //     aggregate_bson, "doublearray_endpoint", doubleanarray_endpoint, 4);
    // bson_serializer_append_end_of_document(aggregate_bson);

    // const void *doc = bson_serializer_get_document(aggregate_bson, NULL);
    // err_t res = device_stream_aggregate(
    //     event->device, device_aggregate_interface.name, "/24", doc, 0);
    // if (res != ASTARTE_OK) {
    //     LOG_ERR("Error streaming the aggregate");
    // }

    // bson_serializer_destroy(aggregate_bson);
    // LOG_INF("Device aggregate sent, using sensor_id: 24.");
}

static uint8_t parse_received_bson(bson_element_t bson_element, struct rx_aggregate *rx_data)
{
    if (bson_element.type != BSON_TYPE_DOCUMENT) {
        return 1U;
    }

    bson_document_t doc = bson_deserializer_element_to_document(bson_element);
    const uint32_t expected_doc_size = 0x4F;
    if (doc.size != expected_doc_size) {
        return 1U;
    }

    bson_element_t elem_longinteger_endpoint;
    if ((bson_deserializer_element_lookup(doc, "longinteger_endpoint", &elem_longinteger_endpoint)
            != ASTARTE_OK)
        || (elem_longinteger_endpoint.type != BSON_TYPE_INT64)) {
        return 1U;
    }
    rx_data->longinteger = bson_deserializer_element_to_int64(elem_longinteger_endpoint);

    bson_element_t elem_boolean_array;
    if ((bson_deserializer_element_lookup(doc, "booleanarray_endpoint", &elem_boolean_array)
            != ASTARTE_OK)
        || (elem_boolean_array.type != BSON_TYPE_ARRAY)) {
        return 1U;
    }

    bson_document_t arr = bson_deserializer_element_to_array(elem_boolean_array);
    const uint32_t expected_arr_size = 0x15;
    if (arr.size != expected_arr_size) {
        return 1U;
    }

    bson_element_t elem_boolean;
    if ((bson_deserializer_first_element(arr, &elem_boolean) != ASTARTE_OK)
        || (elem_boolean.type != BSON_TYPE_BOOLEAN)) {
        return 1U;
    }
    rx_data->booleans[0] = bson_deserializer_element_to_bool(elem_boolean);

    for (size_t i = 1; i < 4; i++) {
        if ((bson_deserializer_next_element(arr, elem_boolean, &elem_boolean) != ASTARTE_OK)
            || (elem_boolean.type != BSON_TYPE_BOOLEAN)) {
            return 1U;
        }
        rx_data->booleans[i] = bson_deserializer_element_to_bool(elem_boolean);
    }

    return 0U;
}
