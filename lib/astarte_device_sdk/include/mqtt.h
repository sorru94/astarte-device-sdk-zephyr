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

#include "astarte_device_sdk/pairing.h"

#include "backoff.h"

/** @brief Max allowed hostname characters are 253 */
#define ASTARTE_MQTT_MAX_BROKER_HOSTNAME_LEN 253
/** @brief Max allowed port number is 65535 */
#define ASTARTE_MQTT_MAX_BROKER_PORT_LEN 5
/** @brief Exact length in chars for the MQTT client ID */
#define ASTARTE_MQTT_CLIENT_ID_LEN                                                                 \
    (sizeof(CONFIG_ASTARTE_DEVICE_SDK_REALM_NAME "/") - sizeof(char)                               \
        + ASTARTE_PAIRING_DEVICE_ID_LEN)

/**
 * @brief Contains all the data related to a single MQTT client.
 */
typedef struct astarte_mqtt astarte_mqtt_t;

/** @brief Function pointer to be used for client certificate refresh. */
typedef astarte_result_t (*astarte_mqtt_refresh_client_cert_cbk_t)(astarte_mqtt_t *astarte_mqtt);

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
} astarte_mqtt_config_t;

/**
 * @brief Contains all the data related to a single MQTT client.
 */
struct astarte_mqtt
{
    /** Mutex to protect access to the client instance. */
    struct sys_mutex mutex;
    /** @brief Zephyr MQTT client handle. */
    struct mqtt_client client;
    /** @brief Last transmitted message ID. */
    uint16_t message_id;
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
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize an instance of the Astarte MQTT client.
 *
 * @param[in] cfg Handle to the configuration for the Astarte MQTT client instance.
 * @param[out] astarte_mqtt Handle to the Astarte MQTT client instance.
 */
void astarte_mqtt_init(astarte_mqtt_config_t *cfg, astarte_mqtt_t *astarte_mqtt);

/**
 * @brief Start the connection procedure for the client to the MQTT broker.
 *
 * @note This function will instantiate a network connection to the broker but will not wait for
 * a connack message to be received back from the broker.
 * Connection success should also be checked using the #on_connected callback.
 *
 * @param[inout] astarte_mqtt Handle to the Astarte MQTT client instance.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_mqtt_connect(astarte_mqtt_t *astarte_mqtt);

/**
 * @brief Disconnect the MQTT client from the broker.
 *
 * @note This function will initiate a disconnection.
 * An actual disconnection success should also be checked using the #on_disconnected callback.
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
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_mqtt_subscribe(astarte_mqtt_t *astarte_mqtt, const char *topic);

/**
 * @brief Publish data to an MQTT topic.
 *
 * @param[inout] astarte_mqtt Handle to the Astarte MQTT client instance.
 * @param[in] topic Topic to use for the publish.
 * @param[in] data Buffer of data to publish.
 * @param[in] data_size Size of the buffer of data to publish in Bytes.
 * @param[in] qos QoS to be used for the publish.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_mqtt_publish(
    astarte_mqtt_t *astarte_mqtt, const char *topic, void *data, size_t data_size, int qos);

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

#ifdef __cplusplus
}
#endif

#endif // MQTT_H
