/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ASTARTE_DEVICE_SDK_DEVICE_H
#define ASTARTE_DEVICE_SDK_DEVICE_H

/**
 * @file device.h
 * @brief Device management
 */

/**
 * @defgroup device Device management
 * @ingroup astarte_device_sdk
 * @{
 */

#include <zephyr/net/mqtt.h>

#include "astarte_device_sdk/astarte.h"
#include "astarte_device_sdk/error.h"
#include "astarte_device_sdk/interface.h"
#include "astarte_device_sdk/pairing.h"

/** @brief Max allowed hostname characters are 253 */
#define ASTARTE_MAX_MQTT_BROKER_HOSTNAME_LEN 253
/** @brief Max allowed port number is 65535 */
#define ASTARTE_MAX_MQTT_BROKER_PORT_LEN 5

/**
 * @brief Handle for an instance of an Astarte device.
 *
 * @details Each handle is a pointer to an opaque internally allocated data struct containing
 * all the data for the Astarte device.
 */
typedef struct astarte_device *astarte_device_handle_t;

/**
 * @brief Configuration struct for an Astarte device.
 *
 * @details This configuration struct might be used to create a new instance of a device
 * using the init function.
 */
typedef struct
{
    /** @brief Timeout for HTTP requests. */
    int32_t http_timeout_ms;
    /** @brief Polling timeout for MQTT connection. */
    int32_t mqtt_connection_timeout_ms;
    /** @brief Polling timeout for MQTT operation. */
    int32_t mqtt_connected_timeout_ms;
    /** @brief Credential secret to be used for connecting to Astarte. */
    char cred_secr[ASTARTE_PAIRING_CRED_SECR_LEN + 1];
    /** @brief Array of interfaces to be added to the device. */
    const astarte_interface_t **interfaces;
    /** @brief Number of elements in the interfaces array. */
    size_t interfaces_size;
} astarte_device_config_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Allocate a new instance of an Astarte device.
 *
 * @details This function has to be called to initialize the device SDK before doing anything else.
 *
 * @note A device can be instantiated and connected to Astarte only if it has been previously
 * registered on Astarte.
 *
 * @param[in] cfg Configuration struct.
 * @param[out] device Device instance inizialized.
 * @return ASTARTE_OK if successful, otherwise an error code.
 */
astarte_err_t astarte_device_new(astarte_device_config_t *cfg, astarte_device_handle_t *handle);

/**
 * @brief Destroy the Astarte device instance.
 *
 * @note The device handle will become invalid after this operation.
 *
 * @return ASTARTE_OK if successful, otherwise an error code.
 */
astarte_err_t astarte_device_destroy(astarte_device_handle_t handle);

/**
 * @brief Connect a device to Astarte.
 *
 * @param[in] device Device instance to connect to Astarte.
 * @return ASTARTE_OK if successful, otherwise an error code.
 */
astarte_err_t astarte_device_connect(astarte_device_handle_t device);

/**
 * @brief Poll data from Astarte.
 *
 * @param[in] device Device instance to poll data from Astarte.
 * @return ASTARTE_OK if successful, otherwise an error code.
 */
astarte_err_t astarte_device_poll(astarte_device_handle_t device);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* ASTARTE_DEVICE_SDK_DEVICE_H */
