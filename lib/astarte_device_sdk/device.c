/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "astarte_device_sdk/device.h"
#include "device_private.h"

#include <stdio.h>
#include <stdlib.h>

#include "astarte_device_sdk/interface.h"
#include "astarte_device_sdk/pairing.h"
#include "astarte_device_sdk/result.h"
#include "astarte_device_sdk/value.h"
#include "crypto.h"
#include "interface_private.h"
#include "introspection.h"
#include "log.h"
#include "pairing_private.h"
#include "value_private.h"

ASTARTE_LOG_MODULE_REGISTER(astarte_device, CONFIG_ASTARTE_DEVICE_SDK_DEVICE_LOG_LEVEL);

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

/** @brief Max allowed hostname characters are 253 */
#define MQTT_MAX_BROKER_HOSTNAME_LEN 253
/** @brief Max allowed port number is 65535 */
#define MQTT_MAX_BROKER_PORT_LEN 5

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
    /** @brief Device's credential secret. */
    char cred_secr[ASTARTE_PAIRING_CRED_SECR_LEN + 1];
    /** @brief MQTT broker hostname. */
    char broker_hostname[MQTT_MAX_BROKER_HOSTNAME_LEN + 1];
    /** @brief MQTT broker port. */
    char broker_port[MQTT_MAX_BROKER_PORT_LEN + 1];
    /** @brief Client ID to be used for all MQTT connections. Has to be unique for each device.
     *
     * @details To ensure the uniqueness of this ID it will be set to the common name of the client
     * TLS certificate received from Astarte. It will often be in the form "<REALM>/<DEVICE ID>".
     * This client ID will also be used as base topic for all incoming and outgoing MQTT messages
     * exchanged with Astarte.
     */
    char mqtt_client_id[ASTARTE_MQTT_MAX_CLIENT_ID_SIZE];
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
};

/************************************************
 *         Static functions declaration         *
 ***********************************************/

/**
 * @brief Calls the correct callback for the received type, also deserializes the BSON payload.
 *
 * @param[in] device Handle to the device instance.
 * @param[in] interface_name Interface name for which the data has been received.
 * @param[in] path Path for which the data has been received.
 * @param[in] data Payload for the received data.
 * @param[in] data_len Length of the payload (not including NULL terminator).
 */
static void trigger_data_callback(astarte_device_handle_t device, const char *interface_name,
    const char *path, const char *data, size_t data_len);
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

    handle->http_timeout_ms = cfg->http_timeout_ms;
    memcpy(handle->cred_secr, cfg->cred_secr, ASTARTE_PAIRING_CRED_SECR_LEN + 1);
    handle->connection_cbk = cfg->connection_cbk;
    handle->disconnection_cbk = cfg->disconnection_cbk;
    handle->datastream_individual_cbk = cfg->datastream_individual_cbk;
    handle->datastream_object_cbk = cfg->datastream_object_cbk;
    handle->property_set_cbk = cfg->property_set_cbk;
    handle->property_unset_cbk = cfg->property_unset_cbk;
    handle->cbk_user_data = cfg->cbk_user_data;

    astarte_mqtt_init(
        &handle->astarte_mqtt, cfg->mqtt_connection_timeout_ms, cfg->mqtt_connected_timeout_ms);

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
    astarte_result_t res = ASTARTE_RESULT_OK;

    // Get MQTT broker URL if not already present
    if ((strnlen(device->broker_hostname, MQTT_MAX_BROKER_HOSTNAME_LEN + 1) == 0)
        || (strnlen(device->broker_port, MQTT_MAX_BROKER_PORT_LEN + 1) == 0)) {
        char broker_url[ASTARTE_PAIRING_MAX_BROKER_URL_LEN + 1];
        res = astarte_pairing_get_broker_url(device->http_timeout_ms,
            CONFIG_ASTARTE_DEVICE_SDK_DEVICE_ID, device->cred_secr, broker_url,
            ASTARTE_PAIRING_MAX_BROKER_URL_LEN + 1);
        if (res != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Failed in obtaining the MQTT broker URL");
            return res;
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
        strncpy(device->broker_hostname, broker_url_token, MQTT_MAX_BROKER_HOSTNAME_LEN + 1);
        broker_url_token = strtok_r(NULL, "/", &saveptr);
        if (!broker_url_token) {
            ASTARTE_LOG_ERR("MQTT broker URL is malformed");
            return ASTARTE_RESULT_HTTP_REQUEST_ERROR;
        }
        strncpy(device->broker_port, broker_url_token, MQTT_MAX_BROKER_PORT_LEN + 1);
    }

    // Check if certificate is valid
    if (strlen(device->crt_pem) == 0) {
        astarte_result_t res = get_new_client_certificate(device);
        if (res != ASTARTE_RESULT_OK) {
            return res;
        }
    } else {
        astarte_result_t res = astarte_pairing_verify_client_certificate(device->http_timeout_ms,
            CONFIG_ASTARTE_DEVICE_SDK_DEVICE_ID, device->cred_secr, device->crt_pem);
        if (res == ASTARTE_RESULT_CLIENT_CERT_INVALID) {
            res = update_client_certificate(device);
            if (res != ASTARTE_RESULT_OK) {
                ASTARTE_LOG_ERR("Client crt update failed: %s.", astarte_result_to_name(res));
                return res;
            }
        }
        if (res != ASTARTE_RESULT_OK) {
            return res;
        }
    }

    return astarte_mqtt_connect(&device->astarte_mqtt, device->broker_hostname, device->broker_port,
        device->mqtt_client_id);
}

astarte_result_t astarte_device_disconnect(astarte_device_handle_t device)
{
    return astarte_mqtt_disconnect(&device->astarte_mqtt);
}

astarte_result_t astarte_device_poll(astarte_device_handle_t device)
{
    return astarte_mqtt_poll(&device->astarte_mqtt);
}

astarte_result_t astarte_device_stream_individual(astarte_device_handle_t device,
    const char *interface_name, const char *path, astarte_value_t value, const int64_t *timestamp,
    uint8_t qos)
{
    astarte_bson_serializer_handle_t bson = astarte_bson_serializer_new();
    if (!bson) {
        ASTARTE_LOG_ERR("Could not initialize the bson serializer");
        return ASTARTE_RESULT_OUT_OF_MEMORY;
    }
    astarte_result_t exit_code = astarte_value_serialize(bson, "v", value);
    if (exit_code != ASTARTE_RESULT_OK) {
        goto exit;
    }

    if (timestamp) {
        astarte_bson_serializer_append_datetime(bson, "t", *timestamp);
    }
    astarte_bson_serializer_append_end_of_document(bson);

    int len = 0;
    void *data = (void *) astarte_bson_serializer_get_document(bson, &len);
    if (!data) {
        ASTARTE_LOG_ERR("Error during BSON serialization");
        exit_code = ASTARTE_RESULT_BSON_SERIALIZER_ERROR;
        goto exit;
    }
    if (len < 0) {
        ASTARTE_LOG_ERR("BSON document is too long for MQTT publish.");
        ASTARTE_LOG_ERR("Interface: %s, path: %s", interface_name, path);

        exit_code = ASTARTE_RESULT_BSON_SERIALIZER_ERROR;
        goto exit;
    }

    exit_code = publish_data(device, interface_name, path, data, len, qos);

exit:
    astarte_bson_serializer_destroy(bson);

    return exit_code;
}

astarte_result_t astarte_device_stream_aggregated(astarte_device_handle_t device,
    const char *interface_name, const char *path, astarte_value_pair_t *values,
    size_t values_length, const int64_t *timestamp, uint8_t qos)
{
    astarte_bson_serializer_handle_t outer_bson = astarte_bson_serializer_new();
    astarte_bson_serializer_handle_t inner_bson = astarte_bson_serializer_new();
    if ((!outer_bson) || (!inner_bson)) {
        ASTARTE_LOG_ERR("Could not initialize the bson serializer");
        return ASTARTE_RESULT_BSON_SERIALIZER_ERROR;
    }
    astarte_result_t exit_code = astarte_value_pair_serialize(inner_bson, values, values_length);
    if (exit_code != ASTARTE_RESULT_OK) {
        goto exit;
    }
    astarte_bson_serializer_append_end_of_document(inner_bson);
    int inner_len = 0;
    void *inner_data = (void *) astarte_bson_serializer_get_document(inner_bson, &inner_len);
    if (!inner_data) {
        ASTARTE_LOG_ERR("Error during BSON serialization");
        exit_code = ASTARTE_RESULT_BSON_SERIALIZER_ERROR;
        goto exit;
    }
    if (inner_len < 0) {
        ASTARTE_LOG_ERR("BSON document is too long for MQTT publish.");
        ASTARTE_LOG_ERR("Interface: %s, path: %s", interface_name, path);

        exit_code = ASTARTE_RESULT_BSON_SERIALIZER_ERROR;
        goto exit;
    }

    astarte_bson_serializer_append_document(outer_bson, "v", inner_data);

    if (timestamp) {
        astarte_bson_serializer_append_datetime(outer_bson, "t", *timestamp);
    }
    astarte_bson_serializer_append_end_of_document(outer_bson);

    int len = 0;
    void *data = (void *) astarte_bson_serializer_get_document(outer_bson, &len);
    if (!data) {
        ASTARTE_LOG_ERR("Error during BSON serialization");
        exit_code = ASTARTE_RESULT_BSON_SERIALIZER_ERROR;
        goto exit;
    }
    if (len < 0) {
        ASTARTE_LOG_ERR("BSON document is too long for MQTT publish.");
        ASTARTE_LOG_ERR("Interface: %s, path: %s", interface_name, path);

        exit_code = ASTARTE_RESULT_BSON_SERIALIZER_ERROR;
        goto exit;
    }

    exit_code = publish_data(device, interface_name, path, data, len, qos);

exit:
    astarte_bson_serializer_destroy(outer_bson);
    astarte_bson_serializer_destroy(inner_bson);

    return exit_code;
}

astarte_result_t astarte_device_set_property(astarte_device_handle_t device,
    const char *interface_name, const char *path, astarte_value_t value)
{
    return astarte_device_stream_individual(device, interface_name, path, value, NULL, 2);
}

astarte_result_t astarte_device_unset_property(
    astarte_device_handle_t device, const char *interface_name, const char *path)
{
    return publish_data(device, interface_name, path, "", 0, 2);
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

void on_connected(astarte_mqtt_t *astarte_mqtt, struct mqtt_connack_param connack_param)
{
    struct astarte_device *device = CONTAINER_OF(astarte_mqtt, struct astarte_device, astarte_mqtt);

    if (device->connection_cbk) {
        astarte_device_connection_event_t event = {
            .device = device,
            .session_present = connack_param.session_present_flag,
            .user_data = device->cbk_user_data,
        };

        device->connection_cbk(event);
    }

    if (connack_param.session_present_flag != 0) {
        return;
    }

    setup_subscriptions(device);
    send_introspection(device);
    send_emptycache(device);
    // TODO: send device owned props
}

void on_disconnected(astarte_mqtt_t *astarte_mqtt)
{
    struct astarte_device *device = CONTAINER_OF(astarte_mqtt, struct astarte_device, astarte_mqtt);

    if (device->disconnection_cbk) {
        astarte_device_disconnection_event_t event = {
            .device = device,
            .user_data = device->cbk_user_data,
        };

        device->disconnection_cbk(event);
    }
}

void on_incoming(astarte_mqtt_t *astarte_mqtt, const char *topic, size_t topic_len,
    const char *data, size_t data_len)
{
    struct astarte_device *device = CONTAINER_OF(astarte_mqtt, struct astarte_device, astarte_mqtt);
    if (strstr(topic, device->mqtt_client_id) != topic) {
        ASTARTE_LOG_ERR("Incoming message topic doesn't begin with MQTT client ID: %s", topic);
        return;
    }

    char control_prefix[ASTARTE_MQTT_MAX_TOPIC_SIZE] = { 0 };
    int ret = snprintf(
        control_prefix, ASTARTE_MQTT_MAX_TOPIC_SIZE, "%s/control", device->mqtt_client_id);
    if ((ret < 0) || (ret >= ASTARTE_MQTT_MAX_TOPIC_SIZE)) {
        ASTARTE_LOG_ERR("Error encoding control prefix");
        return;
    }

    // Control message
    size_t control_prefix_len = strlen(control_prefix);
    if (strstr(topic, control_prefix)) {
        const char *control_topic = topic + control_prefix_len;
        ASTARTE_LOG_DBG("Received control message on control topic %s", control_topic);
        // TODO correctly process control messages
        (void) control_topic; // Remove when this variable will be used
        // on_control_message(device, control_topic, data, data_len);
        return;
    }

    // Data message
    if (topic_len < strlen(device->mqtt_client_id) + strlen("/")
        || topic[strlen(device->mqtt_client_id)] != '/') {
        ASTARTE_LOG_ERR("Missing '/' after MQTT device ID, can't find interface: %s", topic);
        return;
    }

    const char *interface_name_begin = topic + strlen(device->mqtt_client_id) + strlen("/");
    char *path_begin = strchr(interface_name_begin, '/');
    if (!path_begin) {
        ASTARTE_LOG_ERR("No / after interface_name, can't find path: %s", topic);
        return;
    }

    int interface_name_len = path_begin - interface_name_begin;
    char interface_name[ASTARTE_INTERFACE_NAME_MAX_SIZE] = { 0 };
    ret = snprintf(interface_name, ASTARTE_INTERFACE_NAME_MAX_SIZE, "%.*s", interface_name_len,
        interface_name_begin);
    if ((ret < 0) || (ret >= ASTARTE_INTERFACE_NAME_MAX_SIZE)) {
        ASTARTE_LOG_ERR("Error encoding interface name");
        return;
    }

    size_t path_len = topic_len - strlen(device->mqtt_client_id) - strlen("/") - interface_name_len;
    char path[ASTARTE_MQTT_MAX_TOPIC_SIZE] = { 0 };
    ret = snprintf(path, ASTARTE_MQTT_MAX_TOPIC_SIZE, "%.*s", path_len, path_begin);
    if ((ret < 0) || (ret >= ASTARTE_MQTT_MAX_TOPIC_SIZE)) {
        ASTARTE_LOG_ERR("Error encoding path");
        return;
    }

    trigger_data_callback(device, interface_name, path, data, data_len);
}

// This function is borderline hard to read. However splitting it would create duplicated code.
// NOLINTNEXTLINE (readability-function-cognitive-complexity)
static void trigger_data_callback(astarte_device_handle_t device, const char *interface_name,
    const char *path, const char *data, size_t data_len)
{
    const astarte_interface_t *interface = introspection_get(
        &device->introspection, interface_name);

    astarte_device_data_event_t data_event = {
        .device = device,
        .interface_name = interface_name,
        .path = path,
        .user_data = device->cbk_user_data,
    };

    if ((interface->type == ASTARTE_INTERFACE_TYPE_PROPERTIES) && (!data && data_len == 0)) {
        if (device->property_unset_cbk) {
            device->property_unset_cbk(data_event);
        } else {
            ASTARTE_LOG_ERR("Unset received, but no callback (%s/%s).", interface_name, path);
        }
        return;
    }

    if (!astarte_bson_deserializer_check_validity(data, data_len)) {
        ASTARTE_LOG_ERR("Invalid BSON document in data");
        return;
    }

    astarte_bson_document_t full_document = astarte_bson_deserializer_init_doc(data);
    astarte_bson_element_t v_elem;
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
            ASTARTE_LOG_ERR("Failed in parsing the received BSON file.");
            return;
        }

        if (interface->type == ASTARTE_INTERFACE_TYPE_PROPERTIES) {
            if (device->property_set_cbk) {
                astarte_device_property_set_event_t event = {
                    .data_event = data_event,
                    .value = value,
                };
                device->property_set_cbk(event);
            } else {
                ASTARTE_LOG_ERR("Set received, but no callback (%s/%s).", interface_name, path);
            }
        } else {
            if (device->datastream_individual_cbk) {
                astarte_device_datastream_individual_event_t event = {
                    .data_event = data_event,
                    .value = value,
                };
                device->datastream_individual_cbk(event);
            } else {
                ASTARTE_LOG_ERR("Datastream individual received, but no callback (%s/%s).",
                    interface_name, path);
            }
        }
        astarte_value_destroy_deserialized(value);
    } else {
        astarte_value_pair_t *values = NULL;
        size_t values_length = 0;
        astarte_result_t res
            = astarte_value_pair_deserialize(v_elem, interface, path, &values, &values_length);
        if (res != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Failed in parsing the received BSON file.");
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
            ASTARTE_LOG_ERR(
                "Datastream object received, but no callback (%s/%s).", interface_name, path);
        }
        astarte_value_pair_destroy_deserialized(values, values_length);
    }
}

static astarte_result_t get_new_client_certificate(astarte_device_handle_t device)
{
    astarte_result_t res = astarte_pairing_get_client_certificate(device->http_timeout_ms,
        CONFIG_ASTARTE_DEVICE_SDK_DEVICE_ID, device->cred_secr, device->privkey_pem,
        sizeof(device->privkey_pem), device->crt_pem, sizeof(device->crt_pem));
    if (res != ASTARTE_RESULT_OK) {
        return res;
    }

    // The base topic for this device is returned by Astarte in the common name of the certificate
    // It will be usually be in the format: <REALM>/<DEVICE ID>
    res = astarte_crypto_get_certificate_info(
        device->crt_pem, device->mqtt_client_id, ASTARTE_MQTT_MAX_CLIENT_ID_SIZE);
    if ((res != ASTARTE_RESULT_OK) || (strlen(device->mqtt_client_id) == 0)) {
        ASTARTE_LOG_ERR("Error in certificate common name extraction.");
        return res;
    }

    int tls_rc = tls_credential_add(CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG,
        TLS_CREDENTIAL_SERVER_CERTIFICATE, device->crt_pem, strlen(device->crt_pem) + 1);
    if (tls_rc != 0) {
        ASTARTE_LOG_ERR("Failed adding client crt to credentials %d.", tls_rc);
        return ASTARTE_RESULT_TLS_ERROR;
    }

    tls_rc = tls_credential_add(CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG,
        TLS_CREDENTIAL_PRIVATE_KEY, device->privkey_pem, strlen(device->privkey_pem) + 1);
    if (tls_rc != 0) {
        ASTARTE_LOG_ERR("Failed adding client private key to credentials %d.", tls_rc);
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
    char topic[ASTARTE_MQTT_MAX_TOPIC_SIZE] = { 0 };
    int ret = snprintf(topic, ASTARTE_MQTT_MAX_TOPIC_SIZE, "%s/control/consumer/properties",
        device->mqtt_client_id);
    if ((ret < 0) || (ret >= ASTARTE_MQTT_MAX_TOPIC_SIZE)) {
        ASTARTE_LOG_ERR("Error encoding MQTT topic");
        return;
    }

    astarte_result_t mqtt_res = astarte_mqtt_subscribe(&device->astarte_mqtt, topic);
    if (mqtt_res != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Error in MQTT subscription to topic %s.", topic);
        return;
    }

    for (introspection_node_t *iterator = introspection_iter(&device->introspection);
         iterator != NULL; iterator = introspection_iter_next(&device->introspection, iterator)) {
        const astarte_interface_t *interface = iterator->interface;

        if (interface->ownership == ASTARTE_INTERFACE_OWNERSHIP_SERVER) {
            // Subscribe to server interface subtopics
            ret = snprintf(topic, ASTARTE_MQTT_MAX_TOPIC_SIZE, "%s/%s/#", device->mqtt_client_id,
                interface->name);
            if ((ret < 0) || (ret >= ASTARTE_MQTT_MAX_TOPIC_SIZE)) {
                ASTARTE_LOG_ERR("Error encoding MQTT topic");
                continue;
            }

            astarte_result_t mqtt_res = astarte_mqtt_subscribe(&device->astarte_mqtt, topic);
            if (mqtt_res != ASTARTE_RESULT_OK) {
                ASTARTE_LOG_ERR("Error in MQTT subscription to topic %s.", topic);
                return;
            }
        }
    }
}

static void send_introspection(astarte_device_handle_t device)
{
    size_t introspection_str_size = introspection_get_string_size(&device->introspection);

    // if introspection size is > 4KiB print a warning
    const size_t introspection_size_warn_level = 4096;
    if (introspection_str_size > introspection_size_warn_level) {
        ASTARTE_LOG_WRN("The introspection size is > 4KiB");
    }

    char *introspection_str = calloc(introspection_str_size, sizeof(char));
    if (!introspection_str) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        return;
    }
    introspection_fill_string(&device->introspection, introspection_str, introspection_str_size);

    ASTARTE_LOG_DBG("Publishing introspection: %s", introspection_str);

    astarte_result_t mqtt_res = astarte_mqtt_publish(&device->astarte_mqtt, device->mqtt_client_id,
        introspection_str, strlen(introspection_str), 2);
    if (mqtt_res != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Error publishing introspection.");
        return;
    }

    free(introspection_str);
}

static void send_emptycache(astarte_device_handle_t device)
{
    char topic[ASTARTE_MQTT_MAX_TOPIC_SIZE] = { 0 };
    int ret = snprintf(
        topic, ASTARTE_MQTT_MAX_TOPIC_SIZE, "%s/control/emptyCache", device->mqtt_client_id);
    if ((ret < 0) || (ret >= ASTARTE_MQTT_MAX_TOPIC_SIZE)) {
        ASTARTE_LOG_ERR("Error encoding topic");
        return;
    }

    ASTARTE_LOG_DBG("Sending emptyCache to %s", topic);

    astarte_result_t mqtt_res
        = astarte_mqtt_publish(&device->astarte_mqtt, topic, "1", strlen("1"), 2);
    if (mqtt_res != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Error publishing empty cache.");
        return;
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

    char topic[ASTARTE_MQTT_MAX_TOPIC_SIZE] = { 0 };
    int print_ret = snprintf(topic, ASTARTE_MQTT_MAX_TOPIC_SIZE, "%s/%s%s", device->mqtt_client_id,
        interface_name, path);
    if ((print_ret < 0) || (print_ret >= ASTARTE_MQTT_MAX_TOPIC_SIZE)) {
        ASTARTE_LOG_ERR("Error encoding topic");
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }

    astarte_result_t mqtt_res
        = astarte_mqtt_publish(&device->astarte_mqtt, topic, data, data_size, qos);
    if (mqtt_res != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Error publishing empty cache.");
        return mqtt_res;
    }

    return ASTARTE_RESULT_OK;
}
