/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef MQTT_PUBSUB_H
#define MQTT_PUBSUB_H

/**
 * @file mqtt/pubsub.h
 * @brief Wrapper for the MQTT client pubblish and subscribe operations.
 */

#include <zephyr/net/mqtt.h>

#include "mqtt/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Subscribe the client to an MQTT topic.
 *
 * @param[inout] astarte_mqtt Handle to the Astarte MQTT client instance.
 * @param[in] topic Topic to use for the subscription.
 * @param[in] max_qos Maximum QoS level at which the server can send application messages.
 * @param[out] out_message_id Stores the message ID used. Can be used in combination with the
 * message delivered callback to wait for delivery of messages.
 */
void astarte_mqtt_subscribe(
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
 */
void astarte_mqtt_publish(astarte_mqtt_t *astarte_mqtt, const char *topic, void *data,
    size_t data_size, int qos, uint16_t *out_message_id);

/**
 * @brief Check if the MQTT client has any outgoing messages with QoS > 0 pending an acknoledgment.
 *
 * @param[in] astarte_mqtt Handle to the Astarte MQTT client instance.
 * @return True if messages are pending, false otherwise.
 */
bool astarte_mqtt_has_pending_outgoing(astarte_mqtt_t *astarte_mqtt);

/**
 * @brief Clear all MQTT messages that are waiting to be acknoledged.
 *
 * @param[in] astarte_mqtt Handle to the Astarte MQTT client instance.
 */
void astarte_mqtt_clear_all_pending(astarte_mqtt_t *astarte_mqtt);

#ifdef __cplusplus
}
#endif

#endif /* MQTT_PUBSUB_H */
