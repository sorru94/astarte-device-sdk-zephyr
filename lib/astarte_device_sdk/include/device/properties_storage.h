/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DEVICE_PROPERTIES_STORAGE_H
#define DEVICE_PROPERTIES_STORAGE_H

/**
 * @file device/properties_storage.h
 * @brief Device properties permanent storage and synchronization header.
 */

#include "device/core.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE

/**
 * @brief Send the purge properties message for the device owned properties.
 *
 * @param[in] device Handle to the device instance.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_device_properties_send_purge(astarte_device_handle_t device);

/**
 * @brief Send the device owned properties to Astarte.
 *
 * @param[in] device Handle to the device instance.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_device_properties_send_device_owned(astarte_device_handle_t device);

/**
 * @brief Handles an incoming purge properties control message.
 *
 * @param[in] device Handle to the device instance.
 * @param[in] data Received data.
 * @param[in] data_len Length of the data (not including NULL terminator).
 */
void astarte_device_properties_handle_purge(
    astarte_device_handle_t device, const char *data, size_t data_len);

#endif /* CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE */

#ifdef __cplusplus
}
#endif

#endif // DEVICE_PROPERTIES_STORAGE_H
