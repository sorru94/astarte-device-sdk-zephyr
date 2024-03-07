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
#include "astarte_device_sdk/bson_deserializer.h"
#include "astarte_device_sdk/bson_serializer.h"
#include "astarte_device_sdk/error.h"
#include "astarte_device_sdk/interface.h"
#include "astarte_device_sdk/pairing.h"
#include "astarte_device_sdk/value.h"

/**
 * @brief Handle for an instance of an Astarte device.
 *
 * @details Each handle is a pointer to an opaque internally allocated data struct containing
 * all the data for the Astarte device.
 */
typedef struct astarte_device *astarte_device_handle_t;

/**
 * @brief Typedefined struct containing data for a single connection event.
 */
typedef struct
{
    /** @brief Handle to the device triggering the event */
    astarte_device_handle_t device;
    /** @brief MQTT session present flag */
    int session_present;
    /** @brief User data configured during device initialization */
    void *user_data;
} astarte_device_connection_event_t;

/**
 * @brief Typedefined function pointer for connection events.
 */
typedef void (*astarte_device_connection_cbk_t)(astarte_device_connection_event_t *event);

/**
 * @brief Typedefined struct containing data for a single disconnection event.
 */
typedef struct
{
    /** @brief Handle to the device triggering the event */
    astarte_device_handle_t device;
    /** @brief User data configured during device initialization */
    void *user_data;
} astarte_device_disconnection_event_t;

/**
 * @brief Typedefined function pointer for disconnection events.
 */
typedef void (*astarte_device_disconnection_cbk_t)(astarte_device_disconnection_event_t *event);

/**
 * @brief Typedefined struct containing data for a data reception event.
 */
typedef struct
{
    /** @brief Handle to the device triggering the event */
    astarte_device_handle_t device;
    /** @brief Name of the interface for which the data event has been triggered */
    const char *interface_name;
    /** @brief Path for which the data event has been triggered */
    const char *path;
    /** @brief BSON element contained in the data event */
    astarte_bson_element_t bson_element;
    /** @brief User data configured during device initialization */
    void *user_data;
} astarte_device_data_event_t;

/**
 * @brief Typedefined function pointer for data reception events.
 */
typedef void (*astarte_device_data_cbk_t)(astarte_device_data_event_t *event);

/**
 * @brief Typedefined struct containing data for a property unset reception event.
 */
typedef struct
{
    /** @brief Handle to the device triggering the event */
    astarte_device_handle_t device;
    /** @brief Name of the interface for which the data event has been triggered */
    const char *interface_name;
    /** @brief Path for which the data event has been triggered */
    const char *path;
    /** @brief User data configured during device initialization */
    void *user_data;
} astarte_device_unset_event_t;

/**
 * @brief Typedefined function pointer for property unset reception events.
 */
typedef void (*astarte_device_unset_cbk_t)(astarte_device_unset_event_t *event);

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
    /** @brief Optional callback for a connection event. */
    astarte_device_connection_cbk_t connection_cbk;
    /** @brief Optional callback for a disconnection event. */
    astarte_device_disconnection_cbk_t disconnection_cbk;
    /** @brief Optional callback for a data reception event. */
    astarte_device_data_cbk_t data_cbk;
    /** @brief Optional callback for a unset property event. */
    astarte_device_unset_cbk_t unset_cbk;
    /** @brief User data passed to each callback function. */
    void *cbk_user_data;
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
 * If an error code is returned the astarte_device_free function must not be called.
 *
 * @note A device can be instantiated and connected to Astarte only if it has been previously
 * registered on Astarte.
 *
 * @param[in] cfg Configuration struct.
 * @param[out] handle Device instance initialized.
 * @return ASTARTE_OK if successful, otherwise an error code.
 */
astarte_error_t astarte_device_new(astarte_device_config_t *cfg, astarte_device_handle_t *handle);

/**
 * @brief Disconnect the Astarte device instance.
 *
 * @note It will be possible to re-connect the device after disconnection.
 *
 * @param[in] handle Device instance to be disconnected.
 * @return ASTARTE_OK if successful, otherwise an error code.
 */
astarte_error_t astarte_device_disconnect(astarte_device_handle_t handle);

/**
 * @brief Destroy the Astarte device instance.
 *
 * @note The device handle will become invalid after this operation.
 *
 * @param[in] handle Device instance to be destroyed.
 * @return ASTARTE_OK if successful, otherwise an error code.
 */
astarte_error_t astarte_device_destroy(astarte_device_handle_t handle);

/**
 * @brief Connect a device to Astarte.
 *
 * @param[in] device Device instance to connect to Astarte.
 * @return ASTARTE_OK if successful, otherwise an error code.
 */
astarte_error_t astarte_device_connect(astarte_device_handle_t device);

/**
 * @brief Poll data from Astarte.
 *
 * @param[in] device Device instance to poll data from Astarte.
 * @return ASTARTE_OK if successful, otherwise an error code.
 */
astarte_error_t astarte_device_poll(astarte_device_handle_t device);

/**
 * @brief Send an individual value through the device connection.
 *
 * @param[in] device Handle to the device instance.
 * @param[in] interface_name Interface where to publish data.
 * @param[in] path Path where to publish data.
 * @param[in] value Correctly initialized astarte value.
 * @param[in] timestamp Nullable Timestamp of the message.
 * @param[in] qos Quality of service for MQTT publish.
 * @return ASTARTE_OK if successful, otherwise an error code.
 */
astarte_error_t astarte_device_stream_individual(astarte_device_handle_t device,
    char *interface_name, char *path, astarte_value_t value, const int64_t *timestamp, uint8_t qos);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* ASTARTE_DEVICE_SDK_DEVICE_H */
