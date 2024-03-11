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
    .ownership = ASTARTE_INTERFACE_OWNERSHIP_DEVICE,
    .type = ASTARTE_INTERFACE_TYPE_DATASTREAM,
};

const static astarte_interface_t server_aggregate_interface = {
    .name = "org.astarteplatform.zephyr.examples.ServerAggregate",
    .major_version = 0,
    .minor_version = 1,
    .ownership = ASTARTE_INTERFACE_OWNERSHIP_SERVER,
    .type = ASTARTE_INTERFACE_TYPE_DATASTREAM,
};

/************************************************
 * Structs declarations
 ***********************************************/

// All the received array types will be of this size to simplify decoding
#define MAX_RX_ARRAY_SIZE 4

struct rx_aggregate
{
    double double_endpoint;
    int32_t integer_endpoint;
    bool boolean_endpoint;
    int64_t longinteger_endpoint;
    const char *string_endpoint;
    const uint8_t *binaryblob_endpoint;
    size_t binaryblob_endpoint_size;
    int64_t datetime_endpoint;
    double doublearray_endpoint[MAX_RX_ARRAY_SIZE];
    size_t doublearray_endpoint_elements;
    bool booleanarray_endpoint[MAX_RX_ARRAY_SIZE];
    size_t booleanarray_endpoint_elements;
    const char *stringarray_endpoint[MAX_RX_ARRAY_SIZE];
    size_t stringarray_endpoint_elements;
    const uint8_t *binaryblobarray_endpoint[MAX_RX_ARRAY_SIZE];
    size_t binaryblobarray_endpoint_sizes[MAX_RX_ARRAY_SIZE];
    size_t binaryblobarray_endpoint_elements;
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
 * @param[out] rx_data the resulting parsed data (should be initialized to 0 externally).
 * @return 0 when successful, 1 otherwise
 */
static uint8_t parse_received_bson(
    astarte_bson_element_t bson_element, struct rx_aggregate *rx_data);
/**
 * @brief Parse the scalar types in the received BSON data.
 *
 * @param[in] doc The bson document to be parsed
 * @param[out] rx_data the resulting parsed data (should be initialized to 0 externally).
 * @return 0 when successful, 1 otherwise
 */
static uint8_t parse_received_bson_scalar(
    astarte_bson_document_t doc, struct rx_aggregate *rx_data);
/**
 * @brief Parse the double array endpoint in the received BSON data.
 *
 * @param[in] doc The bson document to be parsed
 * @param[out] rx_data the resulting parsed data (should be initialized to 0 externally).
 * @return 0 when successful, 1 otherwise
 */
static uint8_t parse_received_bson_doublearray(
    astarte_bson_document_t doc, struct rx_aggregate *rx_data);
/**
 * @brief Parse the boolean array endpoint in the received BSON data.
 *
 * @param[in] doc The bson document to be parsed
 * @param[out] rx_data the resulting parsed data (should be initialized to 0 externally).
 * @return 0 when successful, 1 otherwise
 */
static uint8_t parse_received_bson_booleanarray(
    astarte_bson_document_t doc, struct rx_aggregate *rx_data);
/**
 * @brief Parse the string array endpoint in the received BSON data.
 *
 * @param[in] doc The bson document to be parsed
 * @param[out] rx_data the resulting parsed data (should be initialized to 0 externally).
 * @return 0 when successful, 1 otherwise
 */
static uint8_t parse_received_bson_stringarray(
    astarte_bson_document_t doc, struct rx_aggregate *rx_data);
/**
 * @brief Parse the binaryblob array endpoint in the received BSON data.
 *
 * @param[in] doc The bson document to be parsed
 * @param[out] rx_data the resulting parsed data (should be initialized to 0 externally).
 * @return 0 when successful, 1 otherwise
 */
static uint8_t parse_received_bson_binaryblobarray(
    astarte_bson_document_t doc, struct rx_aggregate *rx_data);
/**
 * @brief Stream a predefined reply to Astarte.
 *
 * @param event Astarte device data event pointer.
 */
static void stream_reply(astarte_device_data_event_t *event);

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

    struct rx_aggregate rx_data = { 0 };

    if ((strcmp(event->interface_name, server_aggregate_interface.name) != 0)
        || (strcmp(event->path, "/sensor11") != 0)) {
        LOG_ERR("Server aggregate incorrectly received at path %s.", event->path); // NOLINT
        return;
    }

    if (parse_received_bson(event->bson_element, &rx_data) != 0) {
        LOG_ERR("Server aggregate incorrectly received."); // NOLINT
        return;
    }

    stream_reply(event);

    LOG_INF("Device aggregate sent, using sensor_id: sensor24."); // NOLINT
}

static uint8_t parse_received_bson(
    astarte_bson_element_t bson_element, struct rx_aggregate *rx_data)
{
    if (bson_element.type != ASTARTE_BSON_TYPE_DOCUMENT) {
        LOG_ERR("Received bson is not a document."); // NOLINT
        return 1U;
    }

    astarte_bson_document_t doc = astarte_bson_deserializer_element_to_document(bson_element);

    if (parse_received_bson_scalar(doc, rx_data) != 0) {
        return 1U;
    }

    if (parse_received_bson_doublearray(doc, rx_data) != 0) {
        return 1U;
    }

    if (parse_received_bson_booleanarray(doc, rx_data) != 0) {
        return 1U;
    }

    if (parse_received_bson_stringarray(doc, rx_data) != 0) {
        return 1U;
    }

    if (parse_received_bson_binaryblobarray(doc, rx_data) != 0) {
        return 1U;
    }

    LOG_INF("Server aggregate received with the following content:"); // NOLINT
    LOG_INF("double_endpoint: %f", rx_data->double_endpoint); // NOLINT
    LOG_INF("integer_endpoint: %i", rx_data->integer_endpoint); // NOLINT
    LOG_INF("boolean_endpoint: %d", rx_data->boolean_endpoint); // NOLINT
    LOG_INF("longinteger_endpoint: %lli", rx_data->longinteger_endpoint); // NOLINT
    LOG_INF("string_endpoint: %s", rx_data->string_endpoint); // NOLINT
    // NOLINTNEXTLINE
    LOG_HEXDUMP_INF(
        rx_data->binaryblob_endpoint, rx_data->binaryblob_endpoint_size, "binaryblob_endpoint:");
    LOG_INF("datetime_endpoint: %lli", rx_data->datetime_endpoint); // NOLINT
    for (size_t i = 0; i < rx_data->doublearray_endpoint_elements; i++) {
        // NOLINTNEXTLINE
        LOG_INF("doublearray_endpoint element %i: %f", i, rx_data->doublearray_endpoint[i]);
    }
    for (size_t i = 0; i < rx_data->booleanarray_endpoint_elements; i++) {
        // NOLINTNEXTLINE
        LOG_INF("booleanarray_endpoint element %i: %s", i,
            (rx_data->booleanarray_endpoint[i] == true) ? "true" : "false");
    }
    for (size_t i = 0; i < rx_data->stringarray_endpoint_elements; i++) {
        // NOLINTNEXTLINE
        LOG_INF("stringarray_endpoint element %i: %s", i, rx_data->stringarray_endpoint[i]);
    }
    for (size_t i = 0; i < rx_data->binaryblobarray_endpoint_elements; i++) {
        // NOLINTNEXTLINE
        LOG_HEXDUMP_INF(rx_data->binaryblobarray_endpoint[i],
            rx_data->binaryblobarray_endpoint_sizes[i], "binaryblobarray_endpoint element:");
    }

    return 0U;
}

static uint8_t parse_received_bson_scalar(astarte_bson_document_t doc, struct rx_aggregate *rx_data)
{
    astarte_result_t astarte_result = ASTARTE_RESULT_OK;
    astarte_bson_element_t elem_double;
    astarte_result = astarte_bson_deserializer_element_lookup(doc, "double_endpoint", &elem_double);
    if ((astarte_result != ASTARTE_RESULT_OK) || (elem_double.type != ASTARTE_BSON_TYPE_DOUBLE)) {
        LOG_ERR("Received bson is not missing double_endpoint field."); // NOLINT
        return 1U;
    }
    rx_data->double_endpoint = astarte_bson_deserializer_element_to_double(elem_double);

    astarte_bson_element_t elem_integer;
    astarte_result
        = astarte_bson_deserializer_element_lookup(doc, "integer_endpoint", &elem_integer);
    if ((astarte_result != ASTARTE_RESULT_OK) || (elem_integer.type != ASTARTE_BSON_TYPE_INT32)) {
        LOG_ERR("Received bson is not missing integer_endpoint field."); // NOLINT
        return 1U;
    }
    rx_data->integer_endpoint = astarte_bson_deserializer_element_to_int32(elem_integer);

    astarte_bson_element_t elem_boolean;
    astarte_result
        = astarte_bson_deserializer_element_lookup(doc, "boolean_endpoint", &elem_boolean);
    if ((astarte_result != ASTARTE_RESULT_OK) || (elem_boolean.type != ASTARTE_BSON_TYPE_BOOLEAN)) {
        LOG_ERR("Received bson is not missing boolean_endpoint field."); // NOLINT
        return 1U;
    }
    rx_data->boolean_endpoint = astarte_bson_deserializer_element_to_bool(elem_boolean);

    astarte_bson_element_t elem_longinteger;
    astarte_result
        = astarte_bson_deserializer_element_lookup(doc, "longinteger_endpoint", &elem_longinteger);
    if ((astarte_result != ASTARTE_RESULT_OK)
        || (elem_longinteger.type != ASTARTE_BSON_TYPE_INT64)) {
        LOG_ERR("Received bson is not missing longinteger_endpoint field."); // NOLINT
        return 1U;
    }
    rx_data->longinteger_endpoint = astarte_bson_deserializer_element_to_int64(elem_longinteger);

    astarte_bson_element_t elem_string;
    astarte_result = astarte_bson_deserializer_element_lookup(doc, "string_endpoint", &elem_string);
    if ((astarte_result != ASTARTE_RESULT_OK) || (elem_string.type != ASTARTE_BSON_TYPE_STRING)) {
        LOG_ERR("Received bson is not missing string_endpoint field."); // NOLINT
        return 1U;
    }
    rx_data->string_endpoint = astarte_bson_deserializer_element_to_string(elem_string, NULL);

    astarte_bson_element_t elem_binaryblob;
    astarte_result
        = astarte_bson_deserializer_element_lookup(doc, "binaryblob_endpoint", &elem_binaryblob);
    if ((astarte_result != ASTARTE_RESULT_OK)
        || (elem_binaryblob.type != ASTARTE_BSON_TYPE_BINARY)) {
        LOG_ERR("Received bson is not missing binaryblob_endpoint field."); // NOLINT
        return 1U;
    }
    rx_data->binaryblob_endpoint = astarte_bson_deserializer_element_to_binary(
        elem_binaryblob, &rx_data->binaryblob_endpoint_size);

    return 0U;
}

static uint8_t parse_received_bson_doublearray(
    astarte_bson_document_t doc, struct rx_aggregate *rx_data)
{
    astarte_result_t astarte_result = ASTARTE_RESULT_OK;
    astarte_bson_element_t elem_doublearray;
    astarte_result
        = astarte_bson_deserializer_element_lookup(doc, "doublearray_endpoint", &elem_doublearray);
    if ((astarte_result != ASTARTE_RESULT_OK)
        || (elem_doublearray.type != ASTARTE_BSON_TYPE_ARRAY)) {
        LOG_ERR("Received bson is not missing doublearray_endpoint field."); // NOLINT
        return 1U;
    }

    astarte_bson_document_t arr_doublearray
        = astarte_bson_deserializer_element_to_array(elem_doublearray);

    astarte_bson_element_t elem_double;
    for (size_t i = 0; i < MAX_RX_ARRAY_SIZE; i++) {
        if (i == 0) {
            astarte_result = astarte_bson_deserializer_first_element(arr_doublearray, &elem_double);
            if ((astarte_result != ASTARTE_RESULT_OK)
                || (elem_double.type != ASTARTE_BSON_TYPE_DOUBLE)) {
                // NOLINTNEXTLINE
                LOG_ERR("Received doublearray is empty or the first element has wrong type.");
                return 1U;
            }
        } else {
            astarte_result = astarte_bson_deserializer_next_element(
                arr_doublearray, elem_double, &elem_double);
            if (astarte_result == ASTARTE_RESULT_NOT_FOUND) {
                rx_data->doublearray_endpoint_elements = i;
                break;
            }
            if ((astarte_result != ASTARTE_RESULT_OK)
                || (elem_double.type != ASTARTE_BSON_TYPE_DOUBLE)) {
                LOG_ERR("Received doublearray element has wrong type."); // NOLINT
                return 1U;
            }
        }
        rx_data->doublearray_endpoint[i] = astarte_bson_deserializer_element_to_double(elem_double);
        rx_data->doublearray_endpoint_elements = i + 1;
    }

    astarte_result
        = astarte_bson_deserializer_next_element(arr_doublearray, elem_double, &elem_double);
    if (astarte_result != ASTARTE_RESULT_NOT_FOUND) {
        LOG_ERR("Received doublearray is too large for this example."); // NOLINT
        return 1U;
    }

    return 0U;
}

static uint8_t parse_received_bson_booleanarray(
    astarte_bson_document_t doc, struct rx_aggregate *rx_data)
{
    astarte_result_t astarte_result = ASTARTE_RESULT_OK;
    astarte_bson_element_t elem_booleanarray;
    astarte_result = astarte_bson_deserializer_element_lookup(
        doc, "booleanarray_endpoint", &elem_booleanarray);
    if ((astarte_result != ASTARTE_RESULT_OK)
        || (elem_booleanarray.type != ASTARTE_BSON_TYPE_ARRAY)) {
        LOG_ERR("Received bson is not missing booleanarray_endpoint field."); // NOLINT
        return 1U;
    }

    astarte_bson_document_t arr_booleanarray
        = astarte_bson_deserializer_element_to_array(elem_booleanarray);

    astarte_bson_element_t elem_boolean;
    for (size_t i = 0; i < MAX_RX_ARRAY_SIZE; i++) {
        if (i == 0) {
            astarte_result
                = astarte_bson_deserializer_first_element(arr_booleanarray, &elem_boolean);
            if ((astarte_result != ASTARTE_RESULT_OK)
                || (elem_boolean.type != ASTARTE_BSON_TYPE_BOOLEAN)) {
                // NOLINTNEXTLINE
                LOG_ERR("Received booleanarray is empty or the first element has wrong type.");
                return 1U;
            }
        } else {
            astarte_result = astarte_bson_deserializer_next_element(
                arr_booleanarray, elem_boolean, &elem_boolean);
            if (astarte_result == ASTARTE_RESULT_NOT_FOUND) {
                rx_data->booleanarray_endpoint_elements = i;
                break;
            }
            if ((astarte_result != ASTARTE_RESULT_OK)
                || (elem_boolean.type != ASTARTE_BSON_TYPE_BOOLEAN)) {
                LOG_ERR("Received booleanarray element has wrong type."); // NOLINT
                return 1U;
            }
        }
        rx_data->booleanarray_endpoint[i] = astarte_bson_deserializer_element_to_bool(elem_boolean);
        rx_data->booleanarray_endpoint_elements = i + 1;
    }

    astarte_result
        = astarte_bson_deserializer_next_element(arr_booleanarray, elem_boolean, &elem_boolean);
    if (astarte_result != ASTARTE_RESULT_NOT_FOUND) {
        LOG_ERR("Received booleanarray is too large for this example."); // NOLINT
        return 1U;
    }

    return 0U;
}

static uint8_t parse_received_bson_stringarray(
    astarte_bson_document_t doc, struct rx_aggregate *rx_data)
{
    astarte_result_t astarte_result = ASTARTE_RESULT_OK;
    astarte_bson_element_t elem_stringarray;
    astarte_result
        = astarte_bson_deserializer_element_lookup(doc, "stringarray_endpoint", &elem_stringarray);
    if ((astarte_result != ASTARTE_RESULT_OK)
        || (elem_stringarray.type != ASTARTE_BSON_TYPE_ARRAY)) {
        LOG_ERR("Received bson is not missing stringarray_endpoint field."); // NOLINT
        return 1U;
    }

    astarte_bson_document_t arr_stringarray
        = astarte_bson_deserializer_element_to_array(elem_stringarray);

    astarte_bson_element_t elem_string;
    for (size_t i = 0; i < MAX_RX_ARRAY_SIZE; i++) {
        if (i == 0) {
            astarte_result = astarte_bson_deserializer_first_element(arr_stringarray, &elem_string);
            if ((astarte_result != ASTARTE_RESULT_OK)
                || (elem_string.type != ASTARTE_BSON_TYPE_STRING)) {
                // NOLINTNEXTLINE
                LOG_ERR("Received stringarray is empty or the first element has wrong type.");
                return 1U;
            }
        } else {
            astarte_result = astarte_bson_deserializer_next_element(
                arr_stringarray, elem_string, &elem_string);
            if (astarte_result == ASTARTE_RESULT_NOT_FOUND) {
                rx_data->stringarray_endpoint_elements = i;
                break;
            }
            if ((astarte_result != ASTARTE_RESULT_OK)
                || (elem_string.type != ASTARTE_BSON_TYPE_STRING)) {
                LOG_ERR("Received stringarray element has wrong type."); // NOLINT
                return 1U;
            }
        }
        rx_data->stringarray_endpoint[i]
            = astarte_bson_deserializer_element_to_string(elem_string, NULL);
        rx_data->stringarray_endpoint_elements = i + 1;
    }

    astarte_result
        = astarte_bson_deserializer_next_element(arr_stringarray, elem_string, &elem_string);
    if (astarte_result != ASTARTE_RESULT_NOT_FOUND) {
        LOG_ERR("Received stringarray is too large for this example."); // NOLINT
        return 1U;
    }

    return 0U;
}

static uint8_t parse_received_bson_binaryblobarray(
    astarte_bson_document_t doc, struct rx_aggregate *rx_data)
{
    astarte_result_t astarte_result = ASTARTE_RESULT_OK;
    astarte_bson_element_t elem_binaryblobarray;
    astarte_result = astarte_bson_deserializer_element_lookup(
        doc, "binaryblobarray_endpoint", &elem_binaryblobarray);
    if ((astarte_result != ASTARTE_RESULT_OK)
        || (elem_binaryblobarray.type != ASTARTE_BSON_TYPE_ARRAY)) {
        LOG_ERR("Received bson is not missing binaryblobarray_endpoint field."); // NOLINT
        return 1U;
    }

    astarte_bson_document_t arr_binaryblobarray
        = astarte_bson_deserializer_element_to_array(elem_binaryblobarray);

    astarte_bson_element_t elem_binaryblob;
    for (size_t i = 0; i < MAX_RX_ARRAY_SIZE; i++) {
        if (i == 0) {
            astarte_result
                = astarte_bson_deserializer_first_element(arr_binaryblobarray, &elem_binaryblob);
            if ((astarte_result != ASTARTE_RESULT_OK)
                || (elem_binaryblob.type != ASTARTE_BSON_TYPE_BINARY)) {
                // NOLINTNEXTLINE
                LOG_ERR("Received binaryblobarray is empty or the first element has wrong type.");
                return 1U;
            }
        } else {
            astarte_result = astarte_bson_deserializer_next_element(
                arr_binaryblobarray, elem_binaryblob, &elem_binaryblob);
            if (astarte_result == ASTARTE_RESULT_NOT_FOUND) {
                rx_data->binaryblobarray_endpoint_elements = i;
                break;
            }
            if ((astarte_result != ASTARTE_RESULT_OK)
                || (elem_binaryblob.type != ASTARTE_BSON_TYPE_BINARY)) {
                LOG_ERR("Received binaryblobarray element has wrong type."); // NOLINT
                return 1U;
            }
        }
        rx_data->binaryblobarray_endpoint[i] = astarte_bson_deserializer_element_to_binary(
            elem_binaryblob, &rx_data->binaryblobarray_endpoint_sizes[i]);
        rx_data->binaryblobarray_endpoint_elements = i + 1;
    }

    astarte_result = astarte_bson_deserializer_next_element(
        arr_binaryblobarray, elem_binaryblob, &elem_binaryblob);
    if (astarte_result != ASTARTE_RESULT_NOT_FOUND) {
        LOG_ERR("Received binaryblobarray is too large for this example."); // NOLINT
        return 1U;
    }

    return 0U;
}

static void stream_reply(astarte_device_data_event_t *event)
{

    LOG_INF("Sending device aggregate with the following content:"); // NOLINT
    // NOLINTBEGIN(readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers)
    double double_endpoint = 43.2;
    int32_t integer_endpoint = 54;
    bool boolean_endpoint = true;
    int64_t longinteger_endpoint = 45993543534;
    char string_endpoint[] = "hello world";
    uint8_t binaryblob_endpoint[] = { 0x12, 0x11, 0x32 };
    int64_t datetime_endpoint = 1710157717000; // Needs to be in ms
    // NOLINTEND(readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers)

    LOG_INF("double_endpoint: %lf", double_endpoint); // NOLINT
    LOG_INF("integer_endpoint: %" PRIu32, integer_endpoint); // NOLINT
    LOG_INF("boolean_endpoint: %d", boolean_endpoint); // NOLINT
    LOG_INF("longinteger_endpoint: %lld", longinteger_endpoint); // NOLINT
    LOG_INF("string_endpoint: %s", string_endpoint); // NOLINT
    for (size_t i = 0; i < ARRAY_SIZE(binaryblob_endpoint); i++) {
        LOG_INF("binaryblob_endpoint element %i: %d", i, binaryblob_endpoint[i]); // NOLINT
    }
    LOG_INF("datetime_endpoint: %lld", datetime_endpoint); // NOLINT

    // NOLINTBEGIN(readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers)
    double doublearray_endpoint[] = { 11.2, 2.2, 99.9, 421.1 };
    int32_t integerarray_endpoint[] = { 11, 2, 21 };
    bool booleanarray_endpoint[] = { true, false, true };
    int64_t longintegerarray_endpoint[] = { 45993543534, 5 };
    const char *const stringarray_endpoint[] = { "hello", "world" };
    uint8_t binaryblob_array_1[] = { 0x46, 0x23, 0x11 };
    uint8_t binaryblob_array_2[] = { 0x43, 0x00, 0xEE };
    size_t binaryblob_array_sizes[]
        = { ARRAY_SIZE(binaryblob_array_1), ARRAY_SIZE(binaryblob_array_2) };
    uint8_t *binaryblobarray_endpoint[] = { binaryblob_array_1, binaryblob_array_2 };
    int64_t datetimearray_endpoint[] = { 1710157717000 };
    // NOLINTEND(readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers)

    for (size_t i = 0; i < ARRAY_SIZE(doublearray_endpoint); i++) {
        LOG_INF("doublearray_endpoint element %i: %f", i, doublearray_endpoint[i]); // NOLINT
    }
    for (size_t i = 0; i < ARRAY_SIZE(integerarray_endpoint); i++) {
        LOG_INF("integerarray_endpoint element %i: %d", i, integerarray_endpoint[i]); // NOLINT
    }
    for (size_t i = 0; i < ARRAY_SIZE(booleanarray_endpoint); i++) {
        LOG_INF("booleanarray_endpoint element %i: %d", i, booleanarray_endpoint[i]); // NOLINT
    }
    for (size_t i = 0; i < ARRAY_SIZE(longintegerarray_endpoint); i++) {
        // NOLINTNEXTLINE
        LOG_INF("longintegerarray_endpoint element %i: %lld", i, longintegerarray_endpoint[i]);
    }
    for (size_t i = 0; i < ARRAY_SIZE(stringarray_endpoint); i++) {
        LOG_INF("stringarray_endpoint element %i: %s", i, stringarray_endpoint[i]); // NOLINT
    }
    for (size_t i = 0; i < ARRAY_SIZE(binaryblob_array_1); i++) {
        LOG_INF("binaryblob_array_1 element %i: %d", i, binaryblob_array_1[i]); // NOLINT
    }
    for (size_t i = 0; i < ARRAY_SIZE(binaryblob_array_2); i++) {
        LOG_INF("binaryblob_array_2 element %i: %d", i, binaryblob_array_2[i]); // NOLINT
    }
    for (size_t i = 0; i < ARRAY_SIZE(datetimearray_endpoint); i++) {
        LOG_INF("datetimearray_endpoint element %i: %lld", i, datetimearray_endpoint[i]); // NOLINT
    }

    astarte_value_pair_t value_pairs[] = {
        { .endpoint = "double_endpoint", .value = astarte_value_from_double(double_endpoint) },
        { .endpoint = "integer_endpoint", .value = astarte_value_from_integer(integer_endpoint) },
        { .endpoint = "boolean_endpoint", .value = astarte_value_from_boolean(boolean_endpoint) },
        { .endpoint = "binaryblob_endpoint",
            .value
            = astarte_value_from_binaryblob(binaryblob_endpoint, ARRAY_SIZE(binaryblob_endpoint)) },
        { .endpoint = "longinteger_endpoint",
            .value = astarte_value_from_longinteger(longinteger_endpoint) },
        { .endpoint = "string_endpoint", .value = astarte_value_from_string(string_endpoint) },
        { .endpoint = "datetime_endpoint",
            .value = astarte_value_from_datetime(datetime_endpoint) },
        { .endpoint = "doublearray_endpoint",
            .value = astarte_value_from_double_array(
                doublearray_endpoint, ARRAY_SIZE(doublearray_endpoint)) },
        { .endpoint = "integerarray_endpoint",
            .value = astarte_value_from_integer_array(
                integerarray_endpoint, ARRAY_SIZE(integerarray_endpoint)) },
        { .endpoint = "booleanarray_endpoint",
            .value = astarte_value_from_boolean_array(
                booleanarray_endpoint, ARRAY_SIZE(booleanarray_endpoint)) },
        { .endpoint = "longintegerarray_endpoint",
            .value = astarte_value_from_longinteger_array(
                longintegerarray_endpoint, ARRAY_SIZE(longintegerarray_endpoint)) },
        { .endpoint = "stringarray_endpoint",
            .value = astarte_value_from_string_array(
                stringarray_endpoint, ARRAY_SIZE(stringarray_endpoint)) },
        { .endpoint = "binaryblobarray_endpoint",
            .value
            = astarte_value_from_binaryblob_array((const void *const) binaryblobarray_endpoint,
                binaryblob_array_sizes, ARRAY_SIZE(binaryblobarray_endpoint)) },
        { .endpoint = "datetimearray_endpoint",
            .value = astarte_value_from_datetime_array(
                datetimearray_endpoint, ARRAY_SIZE(datetimearray_endpoint)) },
    };

    astarte_result_t res
        = astarte_device_stream_aggregated(event->device, device_aggregate_interface.name,
            "/sensor24", value_pairs, ARRAY_SIZE(value_pairs), NULL, 0);
    if (res != ASTARTE_RESULT_OK) {
        LOG_ERR("Error streaming the aggregate"); // NOLINT
    }
}
