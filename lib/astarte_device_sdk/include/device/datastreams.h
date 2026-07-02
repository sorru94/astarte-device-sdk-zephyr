/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DEVICE_DATASTREAMS_H
#define DEVICE_DATASTREAMS_H

/**
 * @file device/datastreams.h
 * @brief Device datastream transmission and reception header.
 */

#include "device/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Internal function to send a value, bypassing connection state checks.
 *
 * @param[in] device Handle to the device instance.
 * @param[in] interface_name Interface where to publish data.
 * @param[in] path Path where to publish data.
 * @param[in] data Astarte value to send.
 * @param[in] timestamp Timestamp of the message, ignored if set to NULL.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_device_send_individual_internal(astarte_device_handle_t device,
    const char *interface_name, const char *path, astarte_data_t data, const int64_t *timestamp);

/**
 * @brief Handles an incoming generic datastream data message.
 *
 * @details Deserializes the BSON payload and calls the appropriate handler based on the Astarte
 * interface type.
 *
 * @param[in] device Handle to the device instance.
 * @param[in] interface Interface struct for which the data has been received.
 * @param[in] path Path for which the data has been received.
 * @param[in] data Payload for the received data.
 * @param[in] data_len Length of the payload (not including NULL terminator).
 */
void astarte_device_datastreams_handle_incoming(astarte_device_handle_t device,
    const astarte_interface_t *interface, const char *path, const char *data, size_t data_len);

#ifdef __cplusplus
}
#endif

#endif // DEVICE_DATASTREAMS_H
