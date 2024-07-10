/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MQTT_H
#define MQTT_H

/**
 * @file mqtt.h
 * @brief Wrapper for the MQTT client.
 */

#include "astarte_device_sdk/result.h"

#include <zephyr/net/mqtt.h>
#include <zephyr/sys/hash_map.h>

#include "astarte_device_sdk/pairing.h"

#include "backoff.h"

/** @brief Max allowed hostname characters are 253 */
#define ASTARTE_MQTT_MAX_BROKER_HOSTNAME_LEN 253
/** @brief Max allowed port number is 65535 */
#define ASTARTE_MQTT_MAX_BROKER_PORT_LEN 5
/** @brief Exact length in chars for the MQTT client ID */
#define ASTARTE_MQTT_CLIENT_ID_LEN                                                                 \
    (sizeof(CONFIG_ASTARTE_DEVICE_SDK_REALM_NAME "/") - 1 + ASTARTE_PAIRING_DEVICE_ID_LEN)
/** @brief Size for the MQTT transmission and reception buffers */
#define ASTARTE_MQTT_RX_TX_BUFFER_SIZE 256U

/** @brief Contains all the data related to a single MQTT client. */
typedef struct astarte_mqtt astarte_mqtt_t;

/** @brief Function pointer to be used for client certificate refresh. */
typedef astarte_result_t (*astarte_mqtt_refresh_client_cert_cbk_t)(astarte_mqtt_t *astarte_mqtt);

/** @brief Function pointer to be used for signaling a message has been delivered. */
typedef void (*astarte_mqtt_msg_delivered_cbk_t)(astarte_mqtt_t *astarte_mqtt, uint16_t message_id);

/** @brief Function pointer to notify the user that the MQTT connection has been established. */
typedef void (*astarte_mqtt_on_connected_cbk_t)(
    astarte_mqtt_t *astarte_mqtt, struct mqtt_connack_param connack_param);

/** @brief Function pointer to notify the user that the MQTT connection has been terminated. */
typedef void (*astarte_mqtt_on_disconnected_cbk_t)(astarte_mqtt_t *astarte_mqtt);

/** @brief Function pointer to notify the user that an MQTT publish message has been received. */
typedef void (*astarte_mqtt_on_incoming_cbk_t)(astarte_mqtt_t *astarte_mqtt, const char *topic,
    size_t topic_len, const char *data, size_t data_len);

/** @brief Connection statuses for the Astarte MQTT client. */
typedef enum
{
    /** @brief The MQTT client has never been connected or has been gracefully disconnected. */
    ASTARTE_MQTT_DISCONNECTED = 0U,
    /** @brief The MQTT client has established a connection and is waiting for a CONNACK message. */
    ASTARTE_MQTT_CONNECTING,
    /** @brief The MQTT client has completed the connection procedure successfully. */
    ASTARTE_MQTT_CONNECTED,
    /** @brief The MQTT client has been requested to disconnect from Astarte. */
    ASTARTE_MQTT_DISCONNECTING,
    /** @brief The MQTT client has encountred an unexpected connection error. */
    ASTARTE_MQTT_CONNECTION_ERROR,
} astarte_mqtt_connection_states_t;

/** @brief Configuration struct for the MQTT client. */
typedef struct
{
    /** @brief Clean session flag for connection. */
    bool clean_session;
    /** @brief Timeout for socket polls before connection to an MQTT broker. */
    int32_t connection_timeout_ms;
    /** @brief Timeout for socket polls of MQTT broker. */
    int32_t poll_timeout_ms;
    /** @brief Broker hostname */
    char broker_hostname[ASTARTE_MQTT_MAX_BROKER_HOSTNAME_LEN + 1];
    /** @brief Broker port */
    char broker_port[ASTARTE_MQTT_MAX_BROKER_PORT_LEN + 1];
    /** @brief Client ID */
    char client_id[ASTARTE_MQTT_CLIENT_ID_LEN + 1];
    /** @brief Callback used to check if the client certificate is valid. */
    astarte_mqtt_refresh_client_cert_cbk_t refresh_client_cert_cbk;
    /** @brief Callback used to check if transmitted messages have been delivered. */
    astarte_mqtt_msg_delivered_cbk_t msg_delivered_cbk;
    /** @brief Callback used to notify the user that MQTT connection has been established. */
    astarte_mqtt_on_connected_cbk_t on_connected_cbk;
    /** @brief Callback used to notify the user that MQTT connection has been terminated. */
    astarte_mqtt_on_disconnected_cbk_t on_disconnected_cbk;
    /** @brief Callback used to notify the user that an MQTT message has been received. */
    astarte_mqtt_on_incoming_cbk_t on_incoming_cbk;
} astarte_mqtt_config_t;

/**
 * @brief Contains all the data related to a single MQTT client.
 */
struct astarte_mqtt
{
    /** @brief Clean session flag for connection. */
    bool clean_session;
    /** Mutex to protect access to the client instance. */
    struct sys_mutex mutex;
    /** @brief Zephyr MQTT client handle. */
    struct mqtt_client client;
    /** @brief Reception buffer to be used by the MQTT client. */
    uint8_t rx_buffer[ASTARTE_MQTT_RX_TX_BUFFER_SIZE];
    /** @brief Transmission buffer to be used by the MQTT client. */
    uint8_t tx_buffer[ASTARTE_MQTT_RX_TX_BUFFER_SIZE];
    /** @brief Timepoint to be used to check a connection timeout. */
    k_timepoint_t connection_timepoint;
    /** @brief Timeout for socket polls before connection to an MQTT broker. */
    int32_t connection_timeout_ms;
    /** @brief Timeout for socket polls of MQTT broker. */
    int32_t poll_timeout_ms;
    /** @brief Broker hostname */
    char broker_hostname[ASTARTE_MQTT_MAX_BROKER_HOSTNAME_LEN + 1];
    /** @brief Broker port */
    char broker_port[ASTARTE_MQTT_MAX_BROKER_PORT_LEN + 1];
    /** @brief Client ID */
    char client_id[ASTARTE_MQTT_CLIENT_ID_LEN + 1];
    /** @brief Backoff context for the MQTT reconnection */
    struct backoff_context backoff_ctx;
    /** @brief Reconnection timepoint. */
    k_timepoint_t reconnection_timepoint;
    /** @brief Connection state. */
    astarte_mqtt_connection_states_t connection_state;
    /** @brief Callback used to check if the client certificate is valid. */
    astarte_mqtt_refresh_client_cert_cbk_t refresh_client_cert_cbk;
    /** @brief Callback used to check if transmitted messages have been delivered. */
    astarte_mqtt_msg_delivered_cbk_t msg_delivered_cbk;
    /** @brief Configuration struct for the hashmap used to cache outgoing MQTT messages. */
    struct sys_hashmap_config out_msg_map_config;
    /** @brief Data struct for the hashmap used to cache outgoing MQTT messages. */
    struct sys_hashmap_data out_msg_map_data;
    /** @brief Main struct for the hashmap used to cache outgoing MQTT messages. */
    struct sys_hashmap out_msg_map;
    /** @brief Configuration struct for the hashmap used to cache incoming MQTT messages. */
    struct sys_hashmap_config in_msg_map_config;
    /** @brief Data struct for the hashmap used to cache incoming MQTT messages. */
    struct sys_hashmap_data in_msg_map_data;
    /** @brief Main struct for the hashmap used to cache incoming MQTT messages. */
    struct sys_hashmap in_msg_map;
    /** @brief Callback used to notify the user that MQTT connection has been established. */
    astarte_mqtt_on_connected_cbk_t on_connected_cbk;
    /** @brief Callback used to notify the user that MQTT connection has been terminated. */
    astarte_mqtt_on_disconnected_cbk_t on_disconnected_cbk;
    /** @brief Callback used to notify the user that an MQTT message has been received. */
    astarte_mqtt_on_incoming_cbk_t on_incoming_cbk;
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize an instance of the Astarte MQTT client.
 *
 * @param[in] cfg Handle to the configuration for the Astarte MQTT client instance.
 * @param[out] astarte_mqtt Handle to the Astarte MQTT client instance.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_mqtt_init(astarte_mqtt_config_t *cfg, astarte_mqtt_t *astarte_mqtt);

/**
 * @brief Start the connection procedure for the client to the MQTT broker.
 *
 * @note This function will instantiate a network connection to the broker but will not wait for
 * a connack message to be received back from the broker.
 * Connection success should be checked using the connected callback.
 *
 * @param[inout] astarte_mqtt Handle to the Astarte MQTT client instance.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_mqtt_connect(astarte_mqtt_t *astarte_mqtt);

/**
 * @brief Query the connection status of the MQTT client.
 *
 * @param[in] astarte_mqtt Handle to the Astarte MQTT client instance.
 * @return True if MQTT client is connected, false otherwise.
 */
bool astarte_mqtt_is_connected(astarte_mqtt_t *astarte_mqtt);

/**
 * @brief Disconnect the MQTT client from the broker.
 *
 * @note This function will initiate a disconnection.
 * An actual disconnection success should also be checked using the disconnected callback.
 *
 * @param[inout] astarte_mqtt Handle to the Astarte MQTT client instance.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_mqtt_disconnect(astarte_mqtt_t *astarte_mqtt);

/**
 * @brief Subscribe the client to an MQTT topic.
 *
 * @param[inout] astarte_mqtt Handle to the Astarte MQTT client instance.
 * @param[in] topic Topic to use for the subscription.
 * @param[in] max_qos Maximum QoS level at which the server can send application messages.
 * @param[out] out_message_id Stores the message ID used. Can be used in combination with the
 * message delivered callback to wait for delivery of messages.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_mqtt_subscribe(
    astarte_mqtt_t *astarte_mqtt, const char *topic, int max_qos, uint16_t *out_message_id);

/**
 * @brief Publish data to an MQTT topic.
 *
 * @param[inout] astarte_mqtt Handle to the Astarte MQTT client instance.
 * @param[in] topic Topic to use for the publish.
 * @param[in] data Buffer of data to publish.
 * @param[in] data_size Size of the buffer of data to publish in Bytes.
 * @param[in] qos QoS to be used for the publish.
 * @param[out] out_message_id Stores the message ID used. Can be used in combination with the
 * message delivered callback to wait for delivery of messages.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_mqtt_publish(astarte_mqtt_t *astarte_mqtt, const char *topic, void *data,
    size_t data_size, int qos, uint16_t *out_message_id);

/**
 * @brief Poll the MQTT client.
 *
 * @note This function will also take care of keeping the connection active in case of a long period
 * with no data transmission.
 *
 * @param[inout] astarte_mqtt Handle to the Astarte MQTT client instance.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_mqtt_poll(astarte_mqtt_t *astarte_mqtt);

/**
 * @brief Check if the MQTT client has any outgoing messages with QoS > 0 pending an acknoledgment.
 *
 * @param[in] astarte_mqtt Handle to the Astarte MQTT client instance.
 * @return True if messages are pending, false otherwise.
 */
bool astarte_mqtt_has_pending_outgoing(astarte_mqtt_t *astarte_mqtt);

#ifdef __cplusplus
}
#endif

#endif // MQTT_H
