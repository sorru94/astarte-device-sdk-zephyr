/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "astarte_device_sdk/device.h"

#include <stdio.h>

#include <zephyr/kernel.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/tls_credentials.h>

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
 *       Checks over configuration values       *
 ***********************************************/

#if defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_MQTT)
#warning "TLS has been disabled (unsafe)!"
#endif /* defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_MQTT) */

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

/* Buffers for MQTT client. */
#define MQTT_RX_TX_BUFFER_SIZE 256U
static uint8_t mqtt_rx_buffer[MQTT_RX_TX_BUFFER_SIZE];
static uint8_t mqtt_tx_buffer[MQTT_RX_TX_BUFFER_SIZE];

static sec_tag_t sec_tag_list[] = {
#if !defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_MQTT)
    CONFIG_ASTARTE_DEVICE_SDK_MQTTS_CA_CERT_TAG,
#endif
    CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG,
};

/* Max allowed hostname characters are 253 */
#define MAX_MQTT_BROKER_HOSTNAME_LEN 253
/** @brief Max allowed port number is 65535 */
#define MAX_MQTT_BROKER_PORT_LEN 5
/* The total MQTT topic length should never match this size. */
#define MAX_MQTT_TOPIC_SIZE 512
/* The base MQTT topic length should never match this size. */
#define MAX_MQTT_CLIENT_ID_SIZE 128
/* Size for the application message buffer, used to store incoming messages */
#define MAX_MQTT_MSG_SIZE 4096U

/**
 * @brief Internal struct for an instance of an Astarte device.
 *
 * @warning Users should not modify the content of this struct directly
 */
struct astarte_device
{
    /** @brief Timeout for http requests. */
    int32_t http_timeout_ms;
    /** @brief Timeout for socket polls before connection to an MQTT broker. */
    int32_t mqtt_connection_timeout_ms;
    /** @brief Timeout for socket polls on an already connected MQTT broker. */
    int32_t mqtt_connected_timeout_ms;
    /** @brief Private key for the device in the PEM format. */
    char privkey_pem[ASTARTE_CRYPTO_PRIVKEY_BUFFER_SIZE];
    /** @brief Device certificate in the PEM format. */
    char crt_pem[CONFIG_ASTARTE_DEVICE_SDK_ADVANCED_CLIENT_CRT_BUFFER_SIZE];
    /** @brief Device's credential secret. */
    char cred_secr[ASTARTE_PAIRING_CRED_SECR_LEN + 1];
    /** @brief MQTT broker hostname. */
    char broker_hostname[MAX_MQTT_BROKER_HOSTNAME_LEN + 1];
    /** @brief MQTT broker port. */
    char broker_port[MAX_MQTT_BROKER_PORT_LEN + 1];
    /** @brief Client ID to be used for all MQTT connections. Has to be unique for each device.
     *
     * @details To ensure the uniqueness of this ID it will be set to the common name of the client
     * TLS certificate received from Astarte. It will often be in the form "<REALM>/<DEVICE ID>".
     * This client ID will also be used as base topic for all incoming and outgoing MQTT messages
     * exchanged with Astarte.
     */
    char mqtt_client_id[MAX_MQTT_CLIENT_ID_SIZE];
    /** @brief MQTT client handle. */
    struct mqtt_client mqtt_client;
    /** @brief Last transmitted message ID. */
    uint16_t mqtt_message_id;
    /** @brief Device introspection. */
    introspection_t introspection;
    /** @brief Flag representing if the device is connected to the MQTT broker. */
    bool mqtt_is_connected;
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
 * @brief This function should be called each time a connection event is received.
 *
 * @param[in] device Handle to the device instance.
 * @param[in] connack_param CONNACK parameter, containing the session present flag.
 */
static void on_connected(astarte_device_handle_t device, struct mqtt_connack_param connack_param);
/**
 * @brief This function should be called each time a disconnection event is received.
 *
 * @param[in] device Handle to the device instance.
 */
static void on_disconnected(astarte_device_handle_t device);
/**
 * @brief Helper function to parse an incoming published message.
 *
 * @param[in] device Handle to the device instance.
 * @param[in] pub Received published data in the MQTT client format.
 * @return The number of bytes received upon success, a negative error (errno.h) otherwise.
 */
static ssize_t handle_published_message(
    astarte_device_handle_t device, const struct mqtt_publish_param *pub);
/**
 * @brief This function should be called each time an MQTT publish message is received.
 *
 * @param[in] device Handle to the device instance.
 * @param[in] topic Topic on which the publish message has been received.
 * @param[in] topic_len Length of the topic string (not including NULL terminator).
 * @param[in] data Payload for the received data.
 * @param[in] data_len Length of the payload (not including NULL terminator).
 */
static void on_incoming(astarte_device_handle_t device, const char *topic, size_t topic_len,
    const char *data, size_t data_len);
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
 *       Callbacks declaration/definition       *
 ***********************************************/

static void mqtt_evt_handler(struct mqtt_client *const client, const struct mqtt_evt *evt)
{
    int res = 0;
    struct astarte_device *device = CONTAINER_OF(client, struct astarte_device, mqtt_client);

    switch (evt->type) {
        case MQTT_EVT_CONNACK:
            if (evt->result != 0) {
                ASTARTE_LOG_ERR("MQTT connect failed %d", evt->result);
                break;
            }
            ASTARTE_LOG_DBG("MQTT client connected");
            on_connected(device, evt->param.connack);
            break;

        case MQTT_EVT_DISCONNECT:
            ASTARTE_LOG_DBG("MQTT client disconnected %d", evt->result);
            on_disconnected(device);
            break;

        case MQTT_EVT_PUBLISH:
            if (evt->result != 0) {
                ASTARTE_LOG_ERR("MQTT publish reception failed %d", evt->result);
                break;
            }

            const struct mqtt_publish_param *pub = &evt->param.publish;
            res = handle_published_message(device, pub);
            if ((res < 0) || (res != pub->message.payload.len)) {
                ASTARTE_LOG_ERR("MQTT published incoming data parsing error %d", res);
                break;
            }

            break;

        case MQTT_EVT_PUBREL:
            if (evt->result != 0) {
                ASTARTE_LOG_ERR("MQTT PUBREL error %d", evt->result);
                break;
            }
            ASTARTE_LOG_DBG("PUBREL packet id: %u", evt->param.pubrel.message_id);

            struct mqtt_pubcomp_param pubcomp = { .message_id = evt->param.pubrel.message_id };
            res = mqtt_publish_qos2_complete(&device->mqtt_client, &pubcomp);
            if (res != 0) {
                ASTARTE_LOG_ERR("MQTT PUBCOMP transmission error %d", res);
            }

            break;

        case MQTT_EVT_PUBACK:
            if (evt->result != 0) {
                ASTARTE_LOG_ERR("MQTT PUBACK error %d", evt->result);
                break;
            }
            ASTARTE_LOG_DBG("PUBACK packet id: %u", evt->param.puback.message_id);
            break;

        case MQTT_EVT_PUBREC:
            if (evt->result != 0) {
                ASTARTE_LOG_ERR("MQTT PUBREC error %d", evt->result);
                break;
            }
            ASTARTE_LOG_DBG("PUBREC packet id: %u", evt->param.pubrec.message_id);
            const struct mqtt_pubrel_param rel_param
                = { .message_id = evt->param.pubrec.message_id };
            int err = mqtt_publish_qos2_release(client, &rel_param);
            if (err != 0) {
                ASTARTE_LOG_ERR("Failed to send MQTT PUBREL: %d", err);
            }
            break;

        case MQTT_EVT_PUBCOMP:
            if (evt->result != 0) {
                ASTARTE_LOG_ERR("MQTT PUBCOMP error %d", evt->result);
                break;
            }
            ASTARTE_LOG_DBG("PUBCOMP packet id: %u", evt->param.pubcomp.message_id);
            break;

        case MQTT_EVT_SUBACK:
            if (evt->result != 0) {
                ASTARTE_LOG_ERR("MQTT SUBACK error %d", evt->result);
                break;
            }
            ASTARTE_LOG_DBG("SUBACK packet id: %u", evt->param.suback.message_id);
            break;

        case MQTT_EVT_PINGRESP:
            ASTARTE_LOG_DBG("PINGRESP packet");
            break;

        default:
            ASTARTE_LOG_WRN("Unhandled MQTT event: %d", evt->type);
            break;
    }
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
    handle->mqtt_connection_timeout_ms = cfg->mqtt_connection_timeout_ms;
    handle->mqtt_connected_timeout_ms = cfg->mqtt_connected_timeout_ms;
    memcpy(handle->cred_secr, cfg->cred_secr, ASTARTE_PAIRING_CRED_SECR_LEN + 1);
    handle->connection_cbk = cfg->connection_cbk;
    handle->disconnection_cbk = cfg->disconnection_cbk;
    handle->datastream_individual_cbk = cfg->datastream_individual_cbk;
    handle->datastream_object_cbk = cfg->datastream_object_cbk;
    handle->property_set_cbk = cfg->property_set_cbk;
    handle->property_unset_cbk = cfg->property_unset_cbk;
    handle->cbk_user_data = cfg->cbk_user_data;
    handle->mqtt_is_connected = false;
    handle->mqtt_message_id = 1U;

    *device = handle;

    return res;

failure:
    free(handle);
    return res;
}

astarte_result_t astarte_device_destroy(astarte_device_handle_t device)
{
    if (device->mqtt_is_connected) {
        int res = mqtt_disconnect(&device->mqtt_client);
        if (res < 0) {
            ASTARTE_LOG_ERR("Device disconnection failure %d", res);
            return ASTARTE_RESULT_MQTT_ERROR;
        }
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
    if ((strnlen(device->broker_hostname, MAX_MQTT_BROKER_HOSTNAME_LEN + 1) == 0)
        || (strnlen(device->broker_port, MAX_MQTT_BROKER_PORT_LEN + 1) == 0)) {
        char broker_url[ASTARTE_PAIRING_MAX_BROKER_URL_LEN + 1];
        res = astarte_pairing_get_broker_url(device->http_timeout_ms, device->cred_secr, broker_url,
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
        strncpy(device->broker_hostname, broker_url_token, MAX_MQTT_BROKER_HOSTNAME_LEN + 1);
        broker_url_token = strtok_r(NULL, "/", &saveptr);
        if (!broker_url_token) {
            ASTARTE_LOG_ERR("MQTT broker URL is malformed");
            return ASTARTE_RESULT_HTTP_REQUEST_ERROR;
        }
        strncpy(device->broker_port, broker_url_token, MAX_MQTT_BROKER_PORT_LEN + 1);
    }

    // Check if certificate is valid
    if (strlen(device->crt_pem) == 0) {
        astarte_result_t res = get_new_client_certificate(device);
        if (res != ASTARTE_RESULT_OK) {
            return res;
        }
    } else {
        astarte_result_t res = astarte_pairing_verify_client_certificate(
            device->http_timeout_ms, device->cred_secr, device->crt_pem);
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

    // Get broker address info
    struct zsock_addrinfo *broker_addrinfo = NULL;
    struct zsock_addrinfo hints;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    int sock_rc
        = zsock_getaddrinfo(device->broker_hostname, device->broker_port, &hints, &broker_addrinfo);
    if (sock_rc != 0) {
        ASTARTE_LOG_ERR("Unable to resolve broker address %d", sock_rc);
        ASTARTE_LOG_ERR("Errno: %s", strerror(errno));
        return ASTARTE_RESULT_SOCKET_ERROR;
    }

    // MQTT client configuration
    mqtt_client_init(&device->mqtt_client);
    device->mqtt_client.broker = broker_addrinfo->ai_addr;
    device->mqtt_client.evt_cb = mqtt_evt_handler;
    device->mqtt_client.client_id.utf8 = (uint8_t *) device->mqtt_client_id;
    device->mqtt_client.client_id.size = strnlen(device->mqtt_client_id, MAX_MQTT_CLIENT_ID_SIZE);
    device->mqtt_client.password = NULL;
    device->mqtt_client.user_name = NULL;
    device->mqtt_client.protocol_version = MQTT_VERSION_3_1_1;
    device->mqtt_client.transport.type = MQTT_TRANSPORT_SECURE;

    // MQTT TLS configuration
    struct mqtt_sec_config *tls_config = &(device->mqtt_client.transport.tls.config);
#if !defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_MQTT)
    tls_config->peer_verify = TLS_PEER_VERIFY_REQUIRED;
#else
    tls_config->peer_verify = TLS_PEER_VERIFY_NONE;
#endif
    tls_config->cipher_list = NULL;
    tls_config->sec_tag_list = sec_tag_list;
    tls_config->sec_tag_count = ARRAY_SIZE(sec_tag_list);
    tls_config->hostname = device->broker_hostname;

    // MQTT buffers configuration
    device->mqtt_client.rx_buf = mqtt_rx_buffer;
    device->mqtt_client.rx_buf_size = sizeof(mqtt_rx_buffer);
    device->mqtt_client.tx_buf = mqtt_tx_buffer;
    device->mqtt_client.tx_buf_size = sizeof(mqtt_tx_buffer);

    // Request connection to broker
    int mqtt_rc = mqtt_connect(&device->mqtt_client);
    if (mqtt_rc != 0) {
        ASTARTE_LOG_ERR("MQTT connection error (%d)", mqtt_rc);
        return ASTARTE_RESULT_MQTT_ERROR;
    }

    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_device_disconnect(astarte_device_handle_t device)
{
    if (device->mqtt_is_connected) {
        int res = mqtt_disconnect(&device->mqtt_client);
        if (res < 0) {
            ASTARTE_LOG_ERR("Device disconnection failure %d", res);
            return ASTARTE_RESULT_MQTT_ERROR;
        }
    }
    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_device_poll(astarte_device_handle_t device)
{
    // Poll the socket
    struct zsock_pollfd socket_fds[1];
    int socket_nfds = 1;
    socket_fds[0].fd = device->mqtt_client.transport.tls.sock;
    socket_fds[0].events = ZSOCK_POLLIN;
    int32_t timeout = (device->mqtt_is_connected) ? device->mqtt_connected_timeout_ms
                                                  : device->mqtt_connection_timeout_ms;
    int32_t keepalive = mqtt_keepalive_time_left(&device->mqtt_client);
    int socket_rc = zsock_poll(socket_fds, socket_nfds, MIN(timeout, keepalive));
    if (socket_rc < 0) {
        ASTARTE_LOG_ERR("Poll error: %d", errno);
        return ASTARTE_RESULT_SOCKET_ERROR;
    }
    if (socket_rc != 0) {
        // Process the MQTT response
        int mqtt_rc = mqtt_input(&device->mqtt_client);
        if (mqtt_rc != 0) {
            ASTARTE_LOG_ERR("MQTT input failed (%d)", mqtt_rc);
            return ASTARTE_RESULT_MQTT_ERROR;
        }
    }
    // Keep alive the connection
    int mqtt_rc = mqtt_live(&device->mqtt_client);
    if ((mqtt_rc != 0) && (mqtt_rc != -EAGAIN)) {
        ASTARTE_LOG_ERR("Failed to keep alive MQTT: %d", mqtt_rc);
        return ASTARTE_RESULT_MQTT_ERROR;
    }
    return (socket_rc == 0) ? ASTARTE_RESULT_TIMEOUT : ASTARTE_RESULT_OK;
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

static void on_connected(astarte_device_handle_t device, struct mqtt_connack_param connack_param)
{
    device->mqtt_is_connected = true;

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

static void on_disconnected(astarte_device_handle_t device)
{
    device->mqtt_is_connected = false;

    if (device->disconnection_cbk) {
        astarte_device_disconnection_event_t event = {
            .device = device,
            .user_data = device->cbk_user_data,
        };

        device->disconnection_cbk(event);
    }
}

static ssize_t handle_published_message(
    astarte_device_handle_t device, const struct mqtt_publish_param *pub)
{
    int ret = 0U;
    size_t received = 0U;
    uint32_t message_size = pub->message.payload.len;
    char msg_buffer[MAX_MQTT_MSG_SIZE] = { 0 };
    const bool discarded = message_size > MAX_MQTT_MSG_SIZE;

    ASTARTE_LOG_DBG("RECEIVED on topic \"%.*s\" [ id: %u qos: %u ] payload: %u / %u B",
        pub->message.topic.topic.size, (const char *) pub->message.topic.topic.utf8,
        pub->message_id, pub->message.topic.qos, message_size, MAX_MQTT_MSG_SIZE);

    while (received < message_size) {
        char *pkt = discarded ? msg_buffer : &msg_buffer[received];

        ret = mqtt_read_publish_payload_blocking(&device->mqtt_client, pkt, MAX_MQTT_MSG_SIZE);
        if (ret < 0) {
            return ret;
        }

        received += ret;
    }

    if (pub->message.topic.qos == MQTT_QOS_1_AT_LEAST_ONCE) {
        struct mqtt_puback_param puback = { .message_id = pub->message_id };
        ret = mqtt_publish_qos1_ack(&device->mqtt_client, &puback);
        if (ret != 0) {
            ASTARTE_LOG_ERR("MQTT PUBACK transmission error %d", ret);
        }
    }
    if (pub->message.topic.qos == MQTT_QOS_2_EXACTLY_ONCE) {
        struct mqtt_pubrec_param pubrec = { .message_id = pub->message_id };
        ret = mqtt_publish_qos2_receive(&device->mqtt_client, &pubrec);
        if (ret != 0) {
            ASTARTE_LOG_ERR("MQTT PUBREC transmission error %d", ret);
        }
    }

    if (!discarded) {
        ASTARTE_LOG_HEXDUMP_DBG(msg_buffer, MIN(message_size, 256U), "Received payload:");

        size_t topic_len = pub->message.topic.topic.size;
        if (topic_len >= MAX_MQTT_TOPIC_SIZE) {
            ASTARTE_LOG_ERR(
                "MQTT topics longer than %d chars are not supported.", MAX_MQTT_TOPIC_SIZE - 1);
            return -ENOTSUP;
        }
        char topic[MAX_MQTT_TOPIC_SIZE] = { 0 };
        strncpy(topic, pub->message.topic.topic.utf8, MAX_MQTT_TOPIC_SIZE - 1);
        on_incoming(device, topic, topic_len, msg_buffer, message_size);
    }

    return discarded ? -ENOMEM : (ssize_t) received;
}

static void on_incoming(astarte_device_handle_t device, const char *topic, size_t topic_len,
    const char *data, size_t data_len)
{
    if (strstr(topic, device->mqtt_client_id) != topic) {
        ASTARTE_LOG_ERR("Incoming message topic doesn't begin with MQTT client ID: %s", topic);
        return;
    }

    char control_prefix[MAX_MQTT_TOPIC_SIZE] = { 0 };
    int ret = snprintf(control_prefix, MAX_MQTT_TOPIC_SIZE, "%s/control", device->mqtt_client_id);
    if ((ret < 0) || (ret >= MAX_MQTT_TOPIC_SIZE)) {
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
    char path[MAX_MQTT_TOPIC_SIZE] = { 0 };
    ret = snprintf(path, MAX_MQTT_TOPIC_SIZE, "%.*s", path_len, path_begin);
    if ((ret < 0) || (ret >= MAX_MQTT_TOPIC_SIZE)) {
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
        device->cred_secr, device->privkey_pem, sizeof(device->privkey_pem), device->crt_pem,
        sizeof(device->crt_pem));
    if (res != ASTARTE_RESULT_OK) {
        return res;
    }

    // The base topic for this device is returned by Astarte in the common name of the certificate
    // It will be usually be in the format: <REALM>/<DEVICE ID>
    res = astarte_crypto_get_certificate_info(
        device->crt_pem, device->mqtt_client_id, MAX_MQTT_CLIENT_ID_SIZE);
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
    char topic_str[MAX_MQTT_TOPIC_SIZE] = { 0 };
    int ret = snprintf(
        topic_str, MAX_MQTT_TOPIC_SIZE, "%s/control/consumer/properties", device->mqtt_client_id);
    if ((ret < 0) || (ret >= MAX_MQTT_TOPIC_SIZE)) {
        ASTARTE_LOG_ERR("Error encoding MQTT topic");
        return;
    }

    struct mqtt_topic ctrl_topics[] = { {
        .topic = { .utf8 = topic_str, .size = strlen(topic_str) },
        .qos = 2,
    } };
    const struct mqtt_subscription_list ctrl_sub_list = {
        .list = ctrl_topics,
        .list_count = ARRAY_SIZE(ctrl_topics),
        .message_id = device->mqtt_message_id++,
    };

    ASTARTE_LOG_DBG("Subscribing to %s", topic_str);

    ret = mqtt_subscribe(&device->mqtt_client, &ctrl_sub_list);
    if (ret != 0) {
        ASTARTE_LOG_ERR("Failed to subscribe to control topic: %d", ret);
        return;
    }

    for (introspection_node_t *iterator = introspection_iter(&device->introspection);
         iterator != NULL; iterator = introspection_iter_next(&device->introspection, iterator)) {
        const astarte_interface_t *interface = iterator->interface;

        if (interface->ownership == ASTARTE_INTERFACE_OWNERSHIP_SERVER) {
            // Subscribe to server interface subtopics
            ret = snprintf(
                topic_str, MAX_MQTT_TOPIC_SIZE, "%s/%s/#", device->mqtt_client_id, interface->name);
            if ((ret < 0) || (ret >= MAX_MQTT_TOPIC_SIZE)) {
                ASTARTE_LOG_ERR("Error encoding MQTT topic");
                continue;
            }

            struct mqtt_topic topics[] = { {
                .topic = { .utf8 = topic_str, .size = strlen(topic_str) },
                .qos = 2,
            } };
            const struct mqtt_subscription_list sub_list = {
                .list = topics,
                .list_count = ARRAY_SIZE(topics),
                .message_id = device->mqtt_message_id++,
            };

            ASTARTE_LOG_DBG("Subscribing to %s", topic_str);

            ret = mqtt_subscribe(&device->mqtt_client, &sub_list);
            if (ret != 0) {
                ASTARTE_LOG_ERR("Failed to subscribe to %s: %d", topic_str, ret);
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

    struct mqtt_publish_param msg;
    msg.retain_flag = 0U;
    msg.message.topic.topic.utf8 = device->mqtt_client_id;
    msg.message.topic.topic.size = strlen(device->mqtt_client_id);
    msg.message.topic.qos = 2;
    msg.message.payload.data = introspection_str;
    msg.message.payload.len = strlen(introspection_str);
    msg.message_id = device->mqtt_message_id++;
    int res = mqtt_publish(&device->mqtt_client, &msg);
    if (res != 0) {
        ASTARTE_LOG_ERR("MQTT publish failed during send_introspection.");
    }

    free(introspection_str);
}

static void send_emptycache(astarte_device_handle_t device)
{
    char topic[MAX_MQTT_TOPIC_SIZE] = { 0 };
    int ret = snprintf(topic, MAX_MQTT_TOPIC_SIZE, "%s/control/emptyCache", device->mqtt_client_id);
    if ((ret < 0) || (ret >= MAX_MQTT_TOPIC_SIZE)) {
        ASTARTE_LOG_ERR("Error encoding topic");
        return;
    }

    ASTARTE_LOG_DBG("Sending emptyCache to %s", topic);

    struct mqtt_publish_param msg;
    msg.retain_flag = 0U;
    msg.message.topic.topic.utf8 = topic;
    msg.message.topic.topic.size = strlen(topic);
    msg.message.topic.qos = 2;
    msg.message.payload.data = "1";
    msg.message.payload.len = strlen("1");
    msg.message_id = device->mqtt_message_id++;
    int res = mqtt_publish(&device->mqtt_client, &msg);
    if (res != 0) {
        ASTARTE_LOG_ERR("MQTT publish failed during send_introspection.");
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

    char topic[MAX_MQTT_TOPIC_SIZE] = { 0 };
    int print_ret = snprintf(
        topic, MAX_MQTT_TOPIC_SIZE, "%s/%s%s", device->mqtt_client_id, interface_name, path);
    if ((print_ret < 0) || (print_ret >= MAX_MQTT_TOPIC_SIZE)) {
        ASTARTE_LOG_ERR("Error encoding topic");
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }

    struct mqtt_publish_param msg;
    msg.retain_flag = 0U;
    msg.message.topic.topic.utf8 = topic;
    msg.message.topic.topic.size = strlen(topic);
    msg.message.topic.qos = qos;
    msg.message.payload.data = data;
    msg.message.payload.len = data_size;
    msg.message_id = device->mqtt_message_id++;

    int ret = mqtt_publish(&device->mqtt_client, &msg);
    if (ret != 0) {
        ASTARTE_LOG_ERR("Failed to publish message: %d", ret);
        return ASTARTE_RESULT_MQTT_ERROR;
    }

    ASTARTE_LOG_INF("PUBLISHED on topic \"%s\" [ id: %u qos: %u ], payload: %u B", topic,
        msg.message_id, msg.message.topic.qos, data_size);
    ASTARTE_LOG_HEXDUMP_DBG(data, data_size, "Published payload:");

    return ASTARTE_RESULT_OK;
}
