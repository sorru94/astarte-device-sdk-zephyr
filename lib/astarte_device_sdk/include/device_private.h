/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DEVICE_PRIVATE_H
#define DEVICE_PRIVATE_H

/**
 * @file device_private.h
 * @brief Private methods for the Astarte device.
 */

#include "astarte_device_sdk/device.h"

#include "mqtt.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief This function should be called each time a connection event is received.
 *
 * @param[in] astarte_mqtt Handle to the Astarte MQTT client instance.
 * @param[in] connack_param CONNACK parameter, containing the session present flag.
 */
void on_connected(astarte_mqtt_t *astarte_mqtt, struct mqtt_connack_param connack_param);
/**
 * @brief This function should be called each time a disconnection event is received.
 *
 * @param[in] astarte_mqtt Handle to the Astarte MQTT client instance.
 */
void on_disconnected(astarte_mqtt_t *astarte_mqtt);
/**
 * @brief This function should be called each time an MQTT publish message is received.
 *
 * @param[in] astarte_mqtt Handle to the Astarte MQTT client instance.
 * @param[in] topic Topic on which the publish message has been received.
 * @param[in] topic_len Length of the topic string (not including NULL terminator).
 * @param[in] data Payload for the received data.
 * @param[in] data_len Length of the payload (not including NULL terminator).
 */
void on_incoming(astarte_mqtt_t *astarte_mqtt, const char *topic, size_t topic_len,
    const char *data, size_t data_len);

#ifdef __cplusplus
}
#endif

#endif // DEVICE_PRIVATE_H
