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
