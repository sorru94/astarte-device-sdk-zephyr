/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "astarte_device_sdk/device.h"

#include <stdio.h>
#include <stdlib.h>

#include "astarte_device_sdk/interface.h"
#include "astarte_device_sdk/mapping.h"
#include "astarte_device_sdk/pairing.h"
#include "astarte_device_sdk/result.h"
#include "astarte_device_sdk/value.h"
#include "crypto.h"
#include "device_checks.h"
#include "interface_private.h"
#include "introspection.h"
#include "log.h"
#include "mapping_private.h"
#include "mqtt.h"
#include "pairing_private.h"
#include "value_private.h"

ASTARTE_LOG_MODULE_REGISTER(astarte_device, CONFIG_ASTARTE_DEVICE_SDK_DEVICE_LOG_LEVEL);

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

/** @brief Connection statuses for the Astarte device. */
enum connection_states
{
    /** @brief The device has never been connected or has been disconnected. */
    DEVICE_DISCONNECTED = 0U,
    /** @brief The device is connecting to Astarte. */
    DEVICE_CONNECTING,
    /** @brief The device has fully connected to Astarte. */
    DEVICE_CONNECTED,
};

/**
 * @brief Internal struct for an instance of an Astarte device.
 *
 * @warning Users should not modify the content of this struct directly
 */
struct astarte_device
{
    /** @brief Timeout for http requests. */
    int32_t http_timeout_ms;
    /** @brief Private key for the device in the PEM format. */
    char privkey_pem[ASTARTE_CRYPTO_PRIVKEY_BUFFER_SIZE];
    /** @brief Device certificate in the PEM format. */
    char crt_pem[CONFIG_ASTARTE_DEVICE_SDK_ADVANCED_CLIENT_CRT_BUFFER_SIZE];
    /** @brief Unique 128 bits, base64 URL encoded, identifier to associate to a device instance. */
    char device_id[ASTARTE_PAIRING_DEVICE_ID_LEN + 1];
    /** @brief Device's credential secret. */
    char cred_secr[ASTARTE_PAIRING_CRED_SECR_LEN + 1];
    /** @brief MQTT client handle. */
    astarte_mqtt_t astarte_mqtt;
    /** @brief Device introspection. */
    introspection_t introspection;
    /** @brief (optional) User callback for connection events. */
    astarte_device_connection_cbk_t connection_cbk;
    /** @brief (optional) User callback for disconnection events. */
    astarte_device_disconnection_cbk_t disconnection_cbk;
    /** @brief (optional) User callback for incoming datastream individual events. */
    astarte_device_datastream_individual_cbk_t datastream_individual_cbk;
    /** @brief (optional) User callback for incoming datastream object events. */
    astarte_device_datastream_object_cbk_t datastream_object_cbk;
    /** @brief (optional) User callback for incoming property set events. */
    astarte_device_property_set_cbk_t property_set_cbk;
    /** @brief (optional) User callback for incoming property unset events. */
    astarte_device_property_unset_cbk_t property_unset_cbk;
    /** @brief (optional) User data to pass to all the set callbacks. */
    void *cbk_user_data;
    /** @brief Connection state of the Astarte device. */
    enum connection_states connection_state;
};

/** @brief Generic prefix to be used for all MQTT topics. */
#define MQTT_TOPIC_PREFIX CONFIG_ASTARTE_DEVICE_SDK_REALM_NAME "/"
/** @brief Generic suffix to be used for all control MQTT topics. */
#define MQTT_CONTROL_TOPIC_SUFFIX "/control"
/** @brief Suffix to be used for the consumer properties control MQTT topic. */
#define MQTT_CONTROL_CONSUMER_PROP_TOPIC_SUFFIX MQTT_CONTROL_TOPIC_SUFFIX "/consumer/properties"
/** @brief Suffix to be used for the empty cache control MQTT topic. */
#define MQTT_CONTROL_EMPTY_CACHE_TOPIC_SUFFIX MQTT_CONTROL_TOPIC_SUFFIX "/emptyCache"

/** @brief Size in chars of the #MQTT_TOPIC_PREFIX string. */
#define MQTT_TOPIC_PREFIX_LEN (sizeof(MQTT_TOPIC_PREFIX) - 1)
/** @brief Size in chars of the generic base topic. In the form: 'REALM NAME/DEVICE ID' */
#define MQTT_BASE_TOPIC_LEN (MQTT_TOPIC_PREFIX_LEN + ASTARTE_PAIRING_DEVICE_ID_LEN)
/** @brief Size in chars of the #MQTT_CONTROL_TOPIC_SUFFIX string. */
#define MQTT_CONTROL_TOPIC_SUFFIX_LEN (sizeof(MQTT_CONTROL_TOPIC_SUFFIX) - 1)
/** @brief Size in chars of the generic control topic. */
#define MQTT_CONTROL_TOPIC_LEN (MQTT_BASE_TOPIC_LEN + MQTT_CONTROL_TOPIC_SUFFIX_LEN)
/** @brief Size in chars of the #MQTT_CONTROL_CONSUMER_PROP_TOPIC_SUFFIX string. */
#define MQTT_CONTROL_CONSUMER_PROP_TOPIC_SUFFIX_LEN                                                \
    (sizeof(MQTT_CONTROL_CONSUMER_PROP_TOPIC_SUFFIX) - 1)
/** @brief Size in chars of the consumer properties control topic. */
#define MQTT_CONTROL_CONSUMER_PROP_TOPIC_LEN                                                       \
    (MQTT_BASE_TOPIC_LEN + MQTT_CONTROL_CONSUMER_PROP_TOPIC_SUFFIX_LEN)
/** @brief Size in chars of the #MQTT_CONTROL_EMPTY_CACHE_TOPIC_SUFFIX string. */
#define MQTT_CONTROL_EMPTY_CACHE_TOPIC_SUFFIX_LEN                                                  \
    (sizeof(MQTT_CONTROL_EMPTY_CACHE_TOPIC_SUFFIX) - 1)
/** @brief Size in chars of the empty cache control topic. */
#define MQTT_CONTROL_EMPTY_CACHE_TOPIC_LEN                                                         \
    (MQTT_BASE_TOPIC_LEN + MQTT_CONTROL_EMPTY_CACHE_TOPIC_SUFFIX_LEN)

/************************************************
 *         Static functions declaration         *
 ***********************************************/

/**
 * @brief Parse an MQTT broker URL into broker hostname and broker port.
 *
 * @param[in] http_timeout_ms HTTP timeout value.
 * @param[in] device_id Device ID to use to obtain the MQTT broker URL.
 * @param[in] cred_secr Credential secret to use to obtain the MQTT broker URL.
 * @param[out] broker_hostname Static struct used to store the broker hostname.
 * @param[out] broker_port Static struct used to store the broker port.
 * @return ASTARTE_RESULT_OK if publish has been successful, an error code otherwise.
 */
static astarte_result_t get_mqtt_broker_hostname_and_port(int32_t http_timeout_ms,
    const char *device_id, const char *cred_secr,
    char broker_hostname[static ASTARTE_MQTT_MAX_BROKER_HOSTNAME_LEN + 1],
    char broker_port[static ASTARTE_MQTT_MAX_BROKER_PORT_LEN + 1]);
/**
 * @brief Handles an incoming generic data message.
 *
 * @details Deserializes the BSON payload and calls the appropriate handler based on the Astarte
 * interface type.
 *
 * @param[in] device Handle to the device instance.
 * @param[in] interface_name Interface name for which the data has been received.
 * @param[in] path Path for which the data has been received.
 * @param[in] data Payload for the received data.
 * @param[in] data_len Length of the payload (not including NULL terminator).
 */
static void on_data_message(astarte_device_handle_t device, const char *interface_name,
    const char *path, const char *data, size_t data_len);
/**
 * @brief Handles an incoming unset property message.
 *
 * @param[in] device Handle to the device instance.
 * @param[in] event Interface name for which the data has been received.
 */
static void on_unset_property(astarte_device_handle_t device, astarte_device_data_event_t event);
/**
 * @brief Handles an incoming set property message.
 *
 * @param[in] device Handle to the device instance.
 * @param[in] data_event Data event containing information regarding the received event.
 * @param[in] value Astarte value received.
 */
static void on_set_property(
    astarte_device_handle_t device, astarte_device_data_event_t data_event, astarte_value_t value);
/**
 * @brief Handles an incoming datastream individual message.
 *
 * @param[in] device Handle to the device instance.
 * @param[in] data_event Data event containing information regarding the received event.
 * @param[in] value Astarte value received.
 */
static void on_datastream_individual(
    astarte_device_handle_t device, astarte_device_data_event_t data_event, astarte_value_t value);
/**
 * @brief Handles an incoming datastream aggregated message.
 *
 * @param[in] device Handle to the device instance.
 * @param[in] data_event Data event containing information regarding the received event.
 * @param[in] values Array of Astarte value pairs received.
 * @param[in] values_length Size of the array of Astarte value pairs received.
 */
static void on_datastream_aggregated(astarte_device_handle_t device,
    astarte_device_data_event_t data_event, astarte_value_pair_t *values, size_t values_length);
/**
 * @brief Fetch a new client certificate from Astarte.
 *
 * @details This function also adds the new certificate to the device TLS credentials.
 *
 * @param[out] device Handle to the device instance where information from the new certificate will
 * be stored.
 * @return ASTARTE_RESULT_OK if publish has been successful, an error code otherwise.
 */
static astarte_result_t get_new_client_certificate(astarte_device_handle_t device);
/**
 * @brief Delete old client certificate and get a new one from Astarte.
 *
 * @param[in] device Handle to the device instance.
 * @return ASTARTE_RESULT_OK if publish has been successful, an error code otherwise.
 */
static astarte_result_t update_client_certificate(astarte_device_handle_t device);
/**
 * @brief Setup all the MQTT subscriptions for the device.
 *
 * @param[in] device Handle to the device instance.
 */
static void setup_subscriptions(astarte_device_handle_t device);
/**
 * @brief Send the emptycache message to Astarte.
 *
 * @param[in] device Handle to the device instance.
 */
static void send_emptycache(astarte_device_handle_t device);
/**
 * @brief Send the introspection for the device.
 *
 * @param[in] device Handle to the device instance.
 */
static void send_introspection(astarte_device_handle_t device);
/**
 * @brief Publish data.
 *
 * @param[in] device Handle to the device instance.
 * @param[in] interface_name Interface where to publish data.
 * @param[in] path Path where to publish data.
 * @param[in] data Data to publish.
 * @param[in] data_size Size of data to publish.
 * @param[in] qos Quality of service for MQTT publish.
 * @return ASTARTE_RESULT_OK if publish has been successful, an error code otherwise.
 */
static astarte_result_t publish_data(astarte_device_handle_t device, const char *interface_name,
    const char *path, void *data, int data_size, int qos);

/************************************************
 *       Callbacks declaration/definition       *
 ***********************************************/

static astarte_result_t refresh_client_cert_handler(astarte_mqtt_t *astarte_mqtt)
{
    struct astarte_device *device = CONTAINER_OF(astarte_mqtt, struct astarte_device, astarte_mqtt);
    // Obtain new client certificate if no cached one exists
    if (strlen(device->crt_pem) == 0) {
        astarte_result_t res = get_new_client_certificate(device);
        if (res != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Client certificate get failed: %s.", astarte_result_to_name(res));
        }
        return res;
    }
    // Verify cached certificate
    astarte_result_t res = astarte_pairing_verify_client_certificate(
        device->http_timeout_ms, device->device_id, device->cred_secr, device->crt_pem);
    if (res == ASTARTE_RESULT_CLIENT_CERT_INVALID) {
        res = update_client_certificate(device);
        if (res != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Client crt update failed: %s.", astarte_result_to_name(res));
        }
    } else if (res != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Verify client certificate failed: %s.", astarte_result_to_name(res));
    }
    return res;
}
static void on_connected_handler(
    astarte_mqtt_t *astarte_mqtt, struct mqtt_connack_param connack_param)
{
    struct astarte_device *device = CONTAINER_OF(astarte_mqtt, struct astarte_device, astarte_mqtt);

    if (connack_param.session_present_flag != 0) {
        return;
    }

    setup_subscriptions(device);
    send_introspection(device);
    send_emptycache(device);
    // TODO: send device owned props

    device->connection_state = DEVICE_CONNECTING;
}
static void on_disconnected_handler(astarte_mqtt_t *astarte_mqtt)
{
    struct astarte_device *device = CONTAINER_OF(astarte_mqtt, struct astarte_device, astarte_mqtt);

    if (device->disconnection_cbk) {
        astarte_device_disconnection_event_t event = {
            .device = device,
            .user_data = device->cbk_user_data,
        };

        device->disconnection_cbk(event);
    }

    device->connection_state = DEVICE_DISCONNECTED;
}
static void on_incoming_handler(astarte_mqtt_t *astarte_mqtt, const char *topic, size_t topic_len,
    const char *data, size_t data_len)
{
    struct astarte_device *device = CONTAINER_OF(astarte_mqtt, struct astarte_device, astarte_mqtt);

    // Check if base topic is correct
    char base_topic[MQTT_BASE_TOPIC_LEN + 1] = { 0 };
    int snprintf_rc
        = snprintf(base_topic, MQTT_BASE_TOPIC_LEN + 1, MQTT_TOPIC_PREFIX "%s", device->device_id);
    if (snprintf_rc != MQTT_BASE_TOPIC_LEN) {
        ASTARTE_LOG_ERR("Error encoding base topic.");
        return;
    }
    if (strstr(topic, base_topic) != topic) {
        ASTARTE_LOG_ERR("Incoming message topic doesn't begin with <REALM>/<DEVICE ID>: %s", topic);
        return;
    }

    // Control message
    char control_topic[MQTT_CONTROL_TOPIC_LEN + 1] = { 0 };
    snprintf_rc = snprintf(control_topic, MQTT_CONTROL_TOPIC_LEN + 1,
        MQTT_TOPIC_PREFIX "%s" MQTT_CONTROL_TOPIC_SUFFIX, device->device_id);
    if (snprintf_rc != MQTT_CONTROL_TOPIC_LEN) {
        ASTARTE_LOG_ERR("Error encoding base control topic.");
        return;
    }
    if (strstr(topic, control_topic)) {
        const char *control_subtopic = topic + strlen(control_topic);
        ASTARTE_LOG_DBG("Received control message on control topic %s", control_subtopic);
        // TODO correctly process control messages
        (void) control_subtopic;
        // on_control_message(device, control_subtopic, data, data_len);
        return;
    }

    // Data message
    if ((topic_len < MQTT_BASE_TOPIC_LEN + strlen("/")) || (topic[MQTT_BASE_TOPIC_LEN] != '/')) {
        ASTARTE_LOG_ERR("Missing '/' after base topic, can't find interface in topic: %s", topic);
        return;
    }

    const char *interface_name_start = topic + MQTT_BASE_TOPIC_LEN + strlen("/");
    char *path_start = strchr(interface_name_start, '/');
    if (!path_start) {
        ASTARTE_LOG_ERR("Missing '/' after interface name, can't find path in topic: %s", topic);
        return;
    }

    size_t interface_name_len = path_start - interface_name_start;
    char interface_name[ASTARTE_INTERFACE_NAME_MAX_SIZE] = { 0 };
    int ret = snprintf(interface_name, ASTARTE_INTERFACE_NAME_MAX_SIZE, "%.*s", interface_name_len,
        interface_name_start);
    if ((ret < 0) || (ret >= ASTARTE_INTERFACE_NAME_MAX_SIZE)) {
        ASTARTE_LOG_ERR("Error encoding interface name");
        return;
    }

    on_data_message(device, interface_name, path_start, data, data_len);
}

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_device_new(astarte_device_config_t *cfg, astarte_device_handle_t *device)
{
    astarte_result_t res = ASTARTE_RESULT_OK;

    astarte_device_handle_t handle = calloc(1, sizeof(struct astarte_device));
    if (!handle) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        res = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto failure;
    }

    handle->http_timeout_ms = cfg->http_timeout_ms;
#if !defined(CONFIG_ASTARTE_DEVICE_SDK_GENERATE_DEVICE_ID)
    memcpy(handle->device_id, cfg->device_id, ASTARTE_PAIRING_DEVICE_ID_LEN + 1);
#endif /* !defined(CONFIG_ASTARTE_DEVICE_SDK_GENERATE_DEVICE_ID) */
    memcpy(handle->cred_secr, cfg->cred_secr, ASTARTE_PAIRING_CRED_SECR_LEN + 1);
    handle->connection_cbk = cfg->connection_cbk;
    handle->disconnection_cbk = cfg->disconnection_cbk;
    handle->datastream_individual_cbk = cfg->datastream_individual_cbk;
    handle->datastream_object_cbk = cfg->datastream_object_cbk;
    handle->property_set_cbk = cfg->property_set_cbk;
    handle->property_unset_cbk = cfg->property_unset_cbk;
    handle->cbk_user_data = cfg->cbk_user_data;

    // Initializing the connection hashmap and status flags
    handle->connection_state = DEVICE_DISCONNECTED;

    ASTARTE_LOG_DBG("Initializing introspection");
    res = introspection_init(&handle->introspection);
    if (res != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Introspection initialization failure %s.", astarte_result_to_name(res));
        goto failure;
    }
    if (cfg->interfaces) {
        for (size_t i = 0; i < cfg->interfaces_size; i++) {
            res = introspection_add(&handle->introspection, cfg->interfaces[i]);
            if (res != ASTARTE_RESULT_OK) {
                ASTARTE_LOG_ERR("Introspection add failure %s.", astarte_result_to_name(res));
                introspection_free(handle->introspection);
                goto failure;
            }
        }
    }

    ASTARTE_LOG_DBG("Initializing Astarte MQTT client.");
    astarte_mqtt_config_t astarte_mqtt_config = { 0 };
    astarte_mqtt_config.connection_timeout_ms = cfg->mqtt_connection_timeout_ms;
    astarte_mqtt_config.poll_timeout_ms = cfg->mqtt_poll_timeout_ms;
    astarte_mqtt_config.refresh_client_cert_cbk = refresh_client_cert_handler;
    astarte_mqtt_config.on_connected_cbk = on_connected_handler;
    astarte_mqtt_config.on_disconnected_cbk = on_disconnected_handler;
    astarte_mqtt_config.on_incoming_cbk = on_incoming_handler;

    ASTARTE_LOG_DBG("Getting MQTT broker hostname and port");
    res = get_mqtt_broker_hostname_and_port(handle->http_timeout_ms, handle->device_id,
        handle->cred_secr, astarte_mqtt_config.broker_hostname, astarte_mqtt_config.broker_port);
    if (res != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed in parsing the MQTT broker URL");
        goto failure;
    }

    ASTARTE_LOG_DBG("Getting MQTT broker client ID");
    int snprintf_rc = snprintf(astarte_mqtt_config.client_id, sizeof(astarte_mqtt_config.client_id),
        CONFIG_ASTARTE_DEVICE_SDK_REALM_NAME "/%s", handle->device_id);
    if (snprintf_rc != ASTARTE_MQTT_CLIENT_ID_LEN) {
        ASTARTE_LOG_ERR("Error encoding MQTT client ID.");
        res = ASTARTE_RESULT_INTERNAL_ERROR;
        goto failure;
    }

    res = astarte_mqtt_init(&astarte_mqtt_config, &handle->astarte_mqtt);
    if (res != ASTARTE_RESULT_OK) {
        goto failure;
    }

    *device = handle;

    return res;

failure:
    free(handle);
    return res;
}

astarte_result_t astarte_device_destroy(astarte_device_handle_t device)
{
    astarte_result_t res = astarte_device_disconnect(device);
    if (res != ASTARTE_RESULT_OK) {
        return res;
    }

    // TODO: the following two operations should only be performed if the certificate/key have
    // been added as credentials
    int tls_rc = tls_credential_delete(
        CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG, TLS_CREDENTIAL_SERVER_CERTIFICATE);
    if (tls_rc != 0) {
        ASTARTE_LOG_ERR("Failed removing the client certificate from credentials %d.", tls_rc);
        return ASTARTE_RESULT_TLS_ERROR;
    }

    tls_rc = tls_credential_delete(
        CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG, TLS_CREDENTIAL_PRIVATE_KEY);
    if (tls_rc != 0) {
        ASTARTE_LOG_ERR("Failed removing the client private key from credentials %d.", tls_rc);
        return ASTARTE_RESULT_TLS_ERROR;
    }

    free(device);
    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_device_add_interface(
    astarte_device_handle_t device, const astarte_interface_t *interface)
{
    return introspection_update(&device->introspection, interface);
}

astarte_result_t astarte_device_connect(astarte_device_handle_t device)
{
    switch (device->connection_state) {
        case DEVICE_CONNECTING:
            ASTARTE_LOG_WRN("Called connect function when device is connecting.");
            return ASTARTE_RESULT_MQTT_CLIENT_ALREADY_CONNECTING;
        case DEVICE_CONNECTED:
            ASTARTE_LOG_WRN("Called connect function when device is already connected.");
            return ASTARTE_RESULT_MQTT_CLIENT_ALREADY_CONNECTED;
        default: // Other states: (DEVICE_DISCONNECTED)
            break;
    }

    astarte_result_t astarte_rc = astarte_mqtt_connect(&device->astarte_mqtt);
    if (astarte_rc == ASTARTE_RESULT_OK) {
        device->connection_state = DEVICE_CONNECTING;
    }
    return astarte_rc;
}

astarte_result_t astarte_device_disconnect(astarte_device_handle_t device)
{
    return astarte_mqtt_disconnect(&device->astarte_mqtt);
}

astarte_result_t astarte_device_poll(astarte_device_handle_t device)
{
    if ((device->connection_state == DEVICE_CONNECTING)
        && astarte_mqtt_is_connected(&device->astarte_mqtt)
        && !astarte_mqtt_has_pending_outgoing(&device->astarte_mqtt)) {

        if (device->connection_cbk) {
            astarte_device_connection_event_t event = {
                .device = device,
                .user_data = device->cbk_user_data,
            };

            device->connection_cbk(event);
        }
        device->connection_state = DEVICE_CONNECTED;
    }

    return astarte_mqtt_poll(&device->astarte_mqtt);
}

astarte_result_t astarte_device_stream_individual(astarte_device_handle_t device,
    const char *interface_name, const char *path, astarte_value_t value, const int64_t *timestamp)
{
    astarte_bson_serializer_t bson = { 0 };
    astarte_result_t astarte_rc = ASTARTE_RESULT_OK;

    if (device->connection_state != DEVICE_CONNECTED) {
        ASTARTE_LOG_ERR("Called stream individual function when the device is not connected.");
        astarte_rc = ASTARTE_RESULT_DEVICE_NOT_READY;
        goto exit;
    }

    int qos = 0;
    astarte_rc = device_checks_individual_datastream(
        &device->introspection, interface_name, path, value, timestamp, &qos);
    if (astarte_rc != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Device individual data validation failed.");
        goto exit;
    }

    astarte_result_t exit_code = astarte_bson_serializer_init(&bson);
    if (exit_code != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Could not initialize the bson serializer");
        goto exit;
    }
    astarte_rc = astarte_value_serialize(&bson, "v", value);
    if (astarte_rc != ASTARTE_RESULT_OK) {
        goto exit;
    }

    if (timestamp) {
        astarte_bson_serializer_append_datetime(&bson, "t", *timestamp);
    }
    astarte_bson_serializer_append_end_of_document(&bson);

    int len = 0;
    void *data = (void *) astarte_bson_serializer_get_serialized(bson, &len);
    if (!data) {
        ASTARTE_LOG_ERR("Error during BSON serialization.");
        astarte_rc = ASTARTE_RESULT_BSON_SERIALIZER_ERROR;
        goto exit;
    }
    if (len < 0) {
        ASTARTE_LOG_ERR("BSON document is too long for MQTT publish.");
        ASTARTE_LOG_ERR("Interface: %s, path: %s", interface_name, path);
        astarte_rc = ASTARTE_RESULT_BSON_SERIALIZER_ERROR;
        goto exit;
    }

    astarte_rc = publish_data(device, interface_name, path, data, len, qos);

exit:
    astarte_bson_serializer_destroy(&bson);
    return astarte_rc;
}

astarte_result_t astarte_device_stream_aggregated(astarte_device_handle_t device,
    const char *interface_name, const char *path, astarte_value_pair_t *values,
    size_t values_length, const int64_t *timestamp)
{
    astarte_bson_serializer_t outer_bson = { 0 };
    astarte_bson_serializer_t inner_bson = { 0 };
    astarte_result_t astarte_rc = ASTARTE_RESULT_OK;

    if (device->connection_state != DEVICE_CONNECTED) {
        ASTARTE_LOG_ERR("Called stream aggregated function when the device is not connected.");
        astarte_rc = ASTARTE_RESULT_DEVICE_NOT_READY;
        goto exit;
    }

    int qos = 0;
    astarte_rc = device_checks_aggregated_datastream(
        &device->introspection, interface_name, path, values, values_length, timestamp, &qos);
    if (astarte_rc != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Device aggregated data validation failed.");
        return astarte_rc;
    }

    astarte_rc = astarte_bson_serializer_init(&outer_bson);
    if (astarte_rc != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Could not initialize the bson serializer");
        goto exit;
    }
    astarte_rc = astarte_bson_serializer_init(&inner_bson);
    if (astarte_rc != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Could not initialize the bson serializer");
        goto exit;
    }
    astarte_rc = astarte_value_pair_serialize(&inner_bson, values, values_length);
    if (astarte_rc != ASTARTE_RESULT_OK) {
        goto exit;
    }
    astarte_bson_serializer_append_end_of_document(&inner_bson);
    int inner_len = 0;
    const void *inner_data = astarte_bson_serializer_get_serialized(inner_bson, &inner_len);
    if (!inner_data) {
        ASTARTE_LOG_ERR("Error during BSON serialization");
        astarte_rc = ASTARTE_RESULT_BSON_SERIALIZER_ERROR;
        goto exit;
    }
    if (inner_len < 0) {
        ASTARTE_LOG_ERR("BSON document is too long for MQTT publish.");
        ASTARTE_LOG_ERR("Interface: %s, path: %s", interface_name, path);

        astarte_rc = ASTARTE_RESULT_BSON_SERIALIZER_ERROR;
        goto exit;
    }

    astarte_bson_serializer_append_document(&outer_bson, "v", inner_data);

    if (timestamp) {
        astarte_bson_serializer_append_datetime(&outer_bson, "t", *timestamp);
    }
    astarte_bson_serializer_append_end_of_document(&outer_bson);

    int len = 0;
    const void *data = astarte_bson_serializer_get_serialized(outer_bson, &len);
    if (!data) {
        ASTARTE_LOG_ERR("Error during BSON serialization");
        astarte_rc = ASTARTE_RESULT_BSON_SERIALIZER_ERROR;
        goto exit;
    }
    if (len < 0) {
        ASTARTE_LOG_ERR("BSON document is too long for MQTT publish.");
        ASTARTE_LOG_ERR("Interface: %s, path: %s", interface_name, path);

        astarte_rc = ASTARTE_RESULT_BSON_SERIALIZER_ERROR;
        goto exit;
    }

    astarte_rc = publish_data(device, interface_name, path, (void *) data, len, qos);

exit:
    astarte_bson_serializer_destroy(&outer_bson);
    astarte_bson_serializer_destroy(&inner_bson);

    return astarte_rc;
}

astarte_result_t astarte_device_set_property(astarte_device_handle_t device,
    const char *interface_name, const char *path, astarte_value_t value)
{
    if (device->connection_state != DEVICE_CONNECTED) {
        ASTARTE_LOG_ERR("Called set property function when the device is not connected.");
        return ASTARTE_RESULT_DEVICE_NOT_READY;
    }

    astarte_result_t astarte_rc
        = device_checks_set_property(&device->introspection, interface_name, path, value);
    if (astarte_rc != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Property data validation failed.");
        return astarte_rc;
    }

    return astarte_device_stream_individual(device, interface_name, path, value, NULL);
}

astarte_result_t astarte_device_unset_property(
    astarte_device_handle_t device, const char *interface_name, const char *path)
{
    if (device->connection_state != DEVICE_CONNECTED) {
        ASTARTE_LOG_ERR("Called unset property function when the device is not connected.");
        return ASTARTE_RESULT_DEVICE_NOT_READY;
    }

    astarte_result_t astarte_rc
        = device_checks_unset_property(&device->introspection, interface_name, path);
    if (astarte_rc != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Device property unset failed.");
        return astarte_rc;
    }

    return publish_data(device, interface_name, path, "", 0, 2);
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static astarte_result_t get_mqtt_broker_hostname_and_port(int32_t http_timeout_ms,
    const char *device_id, const char *cred_secr,
    char broker_hostname[static ASTARTE_MQTT_MAX_BROKER_HOSTNAME_LEN + 1],
    char broker_port[static ASTARTE_MQTT_MAX_BROKER_PORT_LEN + 1])
{
    char broker_url[ASTARTE_PAIRING_MAX_BROKER_URL_LEN + 1] = { 0 };
    astarte_result_t astarte_res = astarte_pairing_get_broker_info(
        http_timeout_ms, device_id, cred_secr, broker_url, sizeof(broker_url));
    if (astarte_res != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed in obtaining the MQTT broker URL");
        return astarte_res;
    }
    int strncmp_rc = strncmp(broker_url, "mqtts://", strlen("mqtts://"));
    if (strncmp_rc != 0) {
        ASTARTE_LOG_ERR("MQTT broker URL is malformed");
        return ASTARTE_RESULT_HTTP_REQUEST_ERROR;
    }
    char *saveptr = NULL;
    char *broker_url_token = strtok_r(&broker_url[strlen("mqtts://")], ":", &saveptr);
    if (!broker_url_token) {
        ASTARTE_LOG_ERR("MQTT broker URL is malformed");
        return ASTARTE_RESULT_HTTP_REQUEST_ERROR;
    }
    int ret = snprintf(
        broker_hostname, ASTARTE_MQTT_MAX_BROKER_HOSTNAME_LEN + 1, "%s", broker_url_token);
    if ((ret <= 0) || (ret > ASTARTE_MQTT_MAX_BROKER_HOSTNAME_LEN)) {
        ASTARTE_LOG_ERR("Error encoding broker hostname.");
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }
    broker_url_token = strtok_r(NULL, "/", &saveptr);
    if (!broker_url_token) {
        ASTARTE_LOG_ERR("MQTT broker URL is malformed");
        return ASTARTE_RESULT_HTTP_REQUEST_ERROR;
    }
    ret = snprintf(broker_port, ASTARTE_MQTT_MAX_BROKER_PORT_LEN + 1, "%s", broker_url_token);
    if ((ret <= 0) || (ret > ASTARTE_MQTT_MAX_BROKER_PORT_LEN)) {
        ASTARTE_LOG_ERR("Error encoding broker port.");
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }
    return ASTARTE_RESULT_OK;
}

static void on_data_message(astarte_device_handle_t device, const char *interface_name,
    const char *path, const char *data, size_t data_len)
{
    const astarte_interface_t *interface = introspection_get(
        &device->introspection, interface_name);
    if (!interface) {
        ASTARTE_LOG_ERR("Could not find interface in device introspection (%s).", interface_name);
        return;
    }

    astarte_device_data_event_t data_event = {
        .device = device,
        .interface_name = interface_name,
        .path = path,
        .user_data = device->cbk_user_data,
    };

    if ((interface->type == ASTARTE_INTERFACE_TYPE_PROPERTIES) && (data_len == 0)) {
        on_unset_property(device, data_event);
        return;
    }

    if (!astarte_bson_deserializer_check_validity(data, data_len)) {
        ASTARTE_LOG_ERR("Invalid BSON document in data");
        return;
    }

    astarte_bson_document_t full_document = astarte_bson_deserializer_init_doc(data);
    astarte_bson_element_t v_elem = { 0 };
    if (astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem)
        != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Cannot retrieve BSON value from data");
        return;
    }

    if (interface->aggregation == ASTARTE_INTERFACE_AGGREGATION_INDIVIDUAL) {
        const astarte_mapping_t *mapping = NULL;
        astarte_result_t res = astarte_interface_get_mapping_from_path(interface, path, &mapping);
        if (res != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Could not find received mapping in interface %s.", interface_name);
            return;
        }
        astarte_value_t value = { 0 };
        res = astarte_value_deserialize(v_elem, mapping->type, &value);
        if (res != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Failed in parsing the received BSON file. Interface: %s, path: %s.",
                interface_name, path);
            return;
        }

        if (interface->type == ASTARTE_INTERFACE_TYPE_PROPERTIES) {
            on_set_property(device, data_event, value);
        } else {
            on_datastream_individual(device, data_event, value);
        }
        astarte_value_destroy_deserialized(value);
    } else {
        astarte_value_pair_t *values = NULL;
        size_t values_length = 0;
        astarte_result_t res
            = astarte_value_pair_deserialize(v_elem, interface, path, &values, &values_length);
        if (res != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Failed in parsing the received BSON file. Interface: %s, path: %s.",
                interface_name, path);
            return;
        }
        on_datastream_aggregated(device, data_event, values, values_length);
        astarte_value_pair_destroy_deserialized(values, values_length);
    }
}

static void on_unset_property(astarte_device_handle_t device, astarte_device_data_event_t event)
{
    astarte_result_t astarte_rc
        = device_checks_unset_property(&device->introspection, event.interface_name, event.path);
    if (astarte_rc != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Server property unset failed: %s.", astarte_result_to_name(astarte_rc));
        return;
    }

    if (device->property_unset_cbk) {
        device->property_unset_cbk(event);
    } else {
        ASTARTE_LOG_ERR("Unset property received, but no callback configured.");
    }
}

static void on_set_property(
    astarte_device_handle_t device, astarte_device_data_event_t data_event, astarte_value_t value)
{
    astarte_result_t astarte_rc = device_checks_set_property(
        &device->introspection, data_event.interface_name, data_event.path, value);
    if (astarte_rc != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Server property data validation failed.");
        return;
    }

    if (device->property_set_cbk) {
        astarte_device_property_set_event_t set_event = {
            .data_event = data_event,
            .value = value,
        };
        device->property_set_cbk(set_event);
    } else {
        ASTARTE_LOG_ERR("Set property received, but no callback configured.");
    }
}

static void on_datastream_individual(
    astarte_device_handle_t device, astarte_device_data_event_t data_event, astarte_value_t value)
{
    astarte_result_t astarte_rc = device_checks_individual_datastream(
        &device->introspection, data_event.interface_name, data_event.path, value, NULL, NULL);
    // TODO: remove this exception when the following issue is resolved:
    // https://github.com/astarte-platform/astarte/issues/938
    if (astarte_rc == ASTARTE_RESULT_MAPPING_EXPLICIT_TIMESTAMP_REQUIRED) {
        ASTARTE_LOG_WRN("Received an individual datastream with missing explicit timestamp.");
    } else if (astarte_rc != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Server individual data validation failed.");
        return;
    }

    if (device->datastream_individual_cbk) {
        astarte_device_datastream_individual_event_t event = {
            .data_event = data_event,
            .value = value,
        };
        device->datastream_individual_cbk(event);
    } else {
        ASTARTE_LOG_ERR("Datastream individual received, but no callback configured.");
    }
}

static void on_datastream_aggregated(astarte_device_handle_t device,
    astarte_device_data_event_t data_event, astarte_value_pair_t *values, size_t values_length)
{
    astarte_result_t astarte_rc = device_checks_aggregated_datastream(&device->introspection,
        data_event.interface_name, data_event.path, values, values_length, NULL, NULL);
    // TODO: remove this exception when the following issue is resolved:
    // https://github.com/astarte-platform/astarte/issues/938
    if (astarte_rc == ASTARTE_RESULT_MAPPING_EXPLICIT_TIMESTAMP_REQUIRED) {
        ASTARTE_LOG_WRN("Received an aggregated datastream with missing explicit timestamp.");
    } else if (astarte_rc != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Server aggregated data validation failed.");
        return;
    }

    if (device->datastream_object_cbk) {
        astarte_device_datastream_object_event_t event = {
            .data_event = data_event,
            .values = values,
            .values_length = values_length,
        };
        device->datastream_object_cbk(event);
    } else {
        ASTARTE_LOG_ERR("Datastream object received, but no callback configured.");
    }
}

static astarte_result_t get_new_client_certificate(astarte_device_handle_t device)
{
    astarte_result_t res = astarte_pairing_get_client_certificate(device->http_timeout_ms,
        device->device_id, device->cred_secr, device->privkey_pem, sizeof(device->privkey_pem),
        device->crt_pem, sizeof(device->crt_pem));
    if (res != ASTARTE_RESULT_OK) {
        return res;
    }

    int tls_rc = tls_credential_add(CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG,
        TLS_CREDENTIAL_SERVER_CERTIFICATE, device->crt_pem, strlen(device->crt_pem) + 1);
    if (tls_rc != 0) {
        ASTARTE_LOG_ERR("Failed adding client crt to credentials %d.", tls_rc);
        memset(device->privkey_pem, 0, sizeof(device->privkey_pem));
        memset(device->crt_pem, 0, sizeof(device->crt_pem));
        return ASTARTE_RESULT_TLS_ERROR;
    }

    tls_rc = tls_credential_add(CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG,
        TLS_CREDENTIAL_PRIVATE_KEY, device->privkey_pem, strlen(device->privkey_pem) + 1);
    if (tls_rc != 0) {
        ASTARTE_LOG_ERR("Failed adding client private key to credentials %d.", tls_rc);
        tls_credential_delete(
            CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG, TLS_CREDENTIAL_SERVER_CERTIFICATE);
        memset(device->privkey_pem, 0, sizeof(device->privkey_pem));
        memset(device->crt_pem, 0, sizeof(device->crt_pem));
        return ASTARTE_RESULT_TLS_ERROR;
    }

    return res;
}

static astarte_result_t update_client_certificate(astarte_device_handle_t device)
{
    int tls_rc = tls_credential_delete(
        CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG, TLS_CREDENTIAL_SERVER_CERTIFICATE);
    if (tls_rc != 0) {
        ASTARTE_LOG_ERR("Failed removing the client certificate from credentials %d.", tls_rc);
        return ASTARTE_RESULT_TLS_ERROR;
    }

    tls_rc = tls_credential_delete(
        CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG, TLS_CREDENTIAL_PRIVATE_KEY);
    if (tls_rc != 0) {
        ASTARTE_LOG_ERR("Failed removing the client private key from credentials %d.", tls_rc);
        return ASTARTE_RESULT_TLS_ERROR;
    }

    return get_new_client_certificate(device);
}

static void setup_subscriptions(astarte_device_handle_t device)
{
    uint16_t message_id = 0;
    char prop_topic[MQTT_CONTROL_CONSUMER_PROP_TOPIC_LEN + 1] = { 0 };
    int snprintf_rc = snprintf(prop_topic, MQTT_CONTROL_CONSUMER_PROP_TOPIC_LEN + 1,
        MQTT_TOPIC_PREFIX "%s" MQTT_CONTROL_CONSUMER_PROP_TOPIC_SUFFIX, device->device_id);
    if (snprintf_rc != MQTT_CONTROL_CONSUMER_PROP_TOPIC_LEN) {
        ASTARTE_LOG_ERR("Error encoding properties subscription topic.");
        return;
    }
    astarte_result_t mqtt_res
        = astarte_mqtt_subscribe(&device->astarte_mqtt, prop_topic, 2, &message_id);
    if (mqtt_res != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Error in MQTT subscription to topic %s.", prop_topic);
        return;
    }

    for (introspection_node_t *iterator = introspection_iter(&device->introspection);
         iterator != NULL; iterator = introspection_iter_next(&device->introspection, iterator)) {
        const astarte_interface_t *interface = iterator->interface;

        if (interface->ownership == ASTARTE_INTERFACE_OWNERSHIP_SERVER) {
            char *topic = NULL;
            int ret = asprintf(&topic, CONFIG_ASTARTE_DEVICE_SDK_REALM_NAME "/%s/%s/#",
                device->device_id, interface->name);
            if (ret < 0) {
                ASTARTE_LOG_ERR("Error encoding MQTT topic");
                ASTARTE_LOG_ERR("Might be out of memory %s: %d", __FILE__, __LINE__);
                continue;
            }

            astarte_result_t mqtt_res
                = astarte_mqtt_subscribe(&device->astarte_mqtt, topic, 2, &message_id);
            free(topic);
            if (mqtt_res != ASTARTE_RESULT_OK) {
                ASTARTE_LOG_ERR("Error in MQTT subscription to topic %s.", topic);
                return;
            }
        }
    }
}

static void send_introspection(astarte_device_handle_t device)
{
    char *introspection_str = NULL;
    size_t introspection_str_size = introspection_get_string_size(&device->introspection);

    // if introspection size is > 4KiB print a warning
    const size_t introspection_size_warn_level = 4096;
    if (introspection_str_size > introspection_size_warn_level) {
        ASTARTE_LOG_WRN("The introspection size is > 4KiB");
    }

    introspection_str = calloc(introspection_str_size, sizeof(char));
    if (!introspection_str) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        goto exit;
    }
    introspection_fill_string(&device->introspection, introspection_str, introspection_str_size);

    char topic[MQTT_BASE_TOPIC_LEN + 1] = { 0 };
    int snprintf_rc
        = snprintf(topic, MQTT_BASE_TOPIC_LEN + 1, MQTT_TOPIC_PREFIX "%s", device->device_id);
    if (snprintf_rc != MQTT_BASE_TOPIC_LEN) {
        ASTARTE_LOG_ERR("Error encoding introspection topic.");
        goto exit;
    }

    ASTARTE_LOG_DBG("Publishing introspection: %s", introspection_str);

    uint16_t message_id = 0;
    astarte_result_t mqtt_res = astarte_mqtt_publish(
        &device->astarte_mqtt, topic, introspection_str, strlen(introspection_str), 2, &message_id);
    if (mqtt_res != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Error publishing introspection.");
    }

exit:
    free(introspection_str);
}

static void send_emptycache(astarte_device_handle_t device)
{
    char topic[MQTT_CONTROL_EMPTY_CACHE_TOPIC_LEN + 1] = { 0 };
    int snprintf_rc = snprintf(topic, MQTT_CONTROL_EMPTY_CACHE_TOPIC_LEN + 1,
        MQTT_TOPIC_PREFIX "%s" MQTT_CONTROL_EMPTY_CACHE_TOPIC_SUFFIX, device->device_id);
    if (snprintf_rc != MQTT_CONTROL_EMPTY_CACHE_TOPIC_LEN) {
        ASTARTE_LOG_ERR("Error encoding empty cache.");
        return;
    }
    ASTARTE_LOG_DBG("Sending emptyCache to %s", topic);

    uint16_t message_id = 0;
    astarte_result_t mqtt_res
        = astarte_mqtt_publish(&device->astarte_mqtt, topic, "1", strlen("1"), 2, &message_id);
    if (mqtt_res != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Error publishing empty cache.");
    }
}

static astarte_result_t publish_data(astarte_device_handle_t device, const char *interface_name,
    const char *path, void *data, int data_size, int qos)
{
    if (path[0] != '/') {
        ASTARTE_LOG_ERR("Invalid path: %s (must be start with /)", path);
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    if (qos < 0 || qos > 2) {
        ASTARTE_LOG_ERR("Invalid QoS: %d (must be 0, 1 or 2)", qos);
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    char *topic = NULL;
    int ret = asprintf(&topic, CONFIG_ASTARTE_DEVICE_SDK_REALM_NAME "/%s/%s%s", device->device_id,
        interface_name, path);
    if (ret < 0) {
        ASTARTE_LOG_ERR("Error encoding MQTT topic.");
        ASTARTE_LOG_ERR("Might be out of memory %s: %d", __FILE__, __LINE__);
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }

    astarte_result_t res
        = astarte_mqtt_publish(&device->astarte_mqtt, topic, data, data_size, qos, NULL);
    free(topic);
    if (res != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Error publishing data.");
        return res;
    }

    return ASTARTE_RESULT_OK;
}
