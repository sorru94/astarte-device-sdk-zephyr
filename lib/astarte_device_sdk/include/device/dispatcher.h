/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DEVICE_DISPATCHER_H
#define DEVICE_DISPATCHER_H

/**
 * @file device/dispatcher.h
 * @brief MQTT Message Dispatcher Header
 */

#include "device/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Handler for a connection event.
 *
 * @details This function can be used as a connection event handler for the Astarte MQTT client.
 *
 * @param[in] astarte_mqtt Astarte MQTT client context.
 * @param[in] connack_param Content of the CONNACK message.
 */
void astarte_device_dispatcher_on_connected(
    astarte_mqtt_t *astarte_mqtt, struct mqtt_connack_param connack_param);

/**
 * @brief Handler for a disconnection event.
 *
 * @details This function can be used as a disconnection event handler for the Astarte MQTT client.
 *
 * @param[in] astarte_mqtt Astarte MQTT client context.
 */
void astarte_device_dispatcher_on_disconnected(astarte_mqtt_t *astarte_mqtt);

/**
 * @brief Handler for a SUBACK event.
 *
 * @details This function can be used as a SUBACK event handler for the Astarte MQTT client.
 *
 * @param[in] astarte_mqtt Astarte MQTT client context.
 * @param[in] message_id Message ID for the SUBACK message.
 * @param[in] return_code Return code of the SUBACK message.
 */
void astarte_device_dispatcher_on_subscribed(
    astarte_mqtt_t *astarte_mqtt, uint16_t message_id, enum mqtt_suback_return_code return_code);

/**
 * @brief Handler for an incoming MQTT message.
 *
 * @details Dispatches incoming messages to appropriate handlers depending on the topic.
 *
 * @param[in] astarte_mqtt Astarte MQTT client context.
 * @param[in] topic Topic for the event as a NULL terminated string.
 * @param[in] topic_len Length in chars of the topic string (without the terminating NULL char).
 * @param[in] data Data buffer for the event.
 * @param[in] data_len Number of bytes in the data buffer.
 */
void astarte_device_dispatcher_on_incoming(astarte_mqtt_t *astarte_mqtt, const char *topic,
    size_t topic_len, const char *data, size_t data_len);

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
astarte_result_t astarte_device_dispatcher_publish_data(astarte_device_handle_t device,
    const char *interface_name, const char *path, const void *data, int data_size, int qos);

#ifdef __cplusplus
}
#endif

#endif // DEVICE_DISPATCHER_H
