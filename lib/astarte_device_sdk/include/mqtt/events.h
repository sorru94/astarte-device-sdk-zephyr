/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef MQTT_EVENTS_H
#define MQTT_EVENTS_H

/**
 * @file mqtt/events.h
 * @brief Wrapper for the MQTT client event handlers.
 */

#include <zephyr/net/mqtt.h>

#include "mqtt/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Handle a CONNACK reception event.
 *
 * @param[in] astarte_mqtt Handle to the Astarte MQTT client instance.
 * @param[in] connack Received CONNACK data in the MQTT client format.
 */
void astarte_mqtt_handle_connack_event(
    astarte_mqtt_t *astarte_mqtt, struct mqtt_connack_param connack);
/**
 * @brief Handle a disconnection event.
 *
 * @param[in] astarte_mqtt Handle to the Astarte MQTT client instance.
 */
void astarte_mqtt_handle_disconnection_event(astarte_mqtt_t *astarte_mqtt);
/**
 * @brief Handle a PUBLISH reception event.
 *
 * @param[in] astarte_mqtt Handle to the Astarte MQTT client instance.
 * @param[in] publish Received PUBLISH data in the MQTT client format.
 */
void astarte_mqtt_handle_publish_event(
    astarte_mqtt_t *astarte_mqtt, struct mqtt_publish_param publish);
/**
 * @brief Handle a PUBREL reception event.
 *
 * @param[in] astarte_mqtt Handle to the Astarte MQTT client instance.
 * @param[in] pubrel Received PUBREL data in the MQTT client format.
 */
void astarte_mqtt_handle_pubrel_event(
    astarte_mqtt_t *astarte_mqtt, struct mqtt_pubrel_param pubrel);
/**
 * @brief Handle a PUBACK reception event.
 *
 * @param[in] astarte_mqtt Handle to the Astarte MQTT client instance.
 * @param[in] puback Received PUBACK data in the MQTT client format.
 */
void astarte_mqtt_handle_puback_event(
    astarte_mqtt_t *astarte_mqtt, struct mqtt_puback_param puback);
/**
 * @brief Handle a PUBREC reception event.
 *
 * @param[in] astarte_mqtt Handle to the Astarte MQTT client instance.
 * @param[in] pubrec Received PUBREC data in the MQTT client format.
 */
void astarte_mqtt_handle_pubrec_event(
    astarte_mqtt_t *astarte_mqtt, struct mqtt_pubrec_param pubrec);
/**
 * @brief Handle a PUBCOMP reception event.
 *
 * @param[in] astarte_mqtt Handle to the Astarte MQTT client instance.
 * @param[in] pubcomp Received PUBCOMP data in the MQTT client format.
 */
void astarte_mqtt_handle_pubcomp_event(
    astarte_mqtt_t *astarte_mqtt, struct mqtt_pubcomp_param pubcomp);
/**
 * @brief Handle a SUBACK reception event.
 *
 * @param[in] astarte_mqtt Handle to the Astarte MQTT client instance.
 * @param[in] suback Received SUBACK data in the MQTT client format.
 */
void astarte_mqtt_handle_suback_event(
    astarte_mqtt_t *astarte_mqtt, struct mqtt_suback_param suback);

#ifdef __cplusplus
}
#endif

#endif /* MQTT_EVENTS_H */
