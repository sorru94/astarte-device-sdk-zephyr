/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DEVICE_RX_H
#define DEVICE_RX_H

/**
 * @file device_rx.h
 * @brief Device reception header.
 */

#include "astarte_device_sdk/astarte.h"
#include "astarte_device_sdk/device.h"
#include "astarte_device_sdk/result.h"

#include "device_private.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Handler for a reception event.
 *
 * @details This function can be used as a reception event handler for the Astarte MQTT client.
 *
 * @param[in] astarte_mqtt Astarte MQTT client context.
 * @param[in] topic Topic for the event as a NULL terminated string.
 * @param[in] topic_len Length in chars of the topic string (without the terminating NULL char).
 * @param[in] data Data buffer for the event.
 * @param[in] data_len Number of bytes in the data buffer.
 */
void astarte_device_rx_on_incoming_handler(astarte_mqtt_t *astarte_mqtt, const char *topic,
    size_t topic_len, const char *data, size_t data_len);

#ifdef __cplusplus
}
#endif

#endif // DEVICE_RX_H
