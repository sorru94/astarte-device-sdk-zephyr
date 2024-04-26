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

#include "astarte_device_sdk/astarte.h"
#include "astarte_device_sdk/interface.h"
#include "astarte_device_sdk/pairing.h"
#include "astarte_device_sdk/result.h"
#include "astarte_device_sdk/value.h"

/**
 * @brief Handle for an instance of an Astarte device.
 *
 * @details Each handle is a pointer to an opaque internally allocated data struct containing
 * all the data for the Astarte device.
 */
typedef struct astarte_device *astarte_device_handle_t;

/** @brief Context for a single connection event. */
typedef struct
{
    astarte_device_handle_t device; /**< Handle to the device triggering the event */
    int session_present; /**< MQTT session present flag */
    void *user_data; /**< User data configured during device initialization */
} astarte_device_connection_event_t;

/** @brief Function pointer for connection events. */
typedef void (*astarte_device_connection_cbk_t)(astarte_device_connection_event_t event);

/** @brief Context for a single disconnection event. */
typedef struct
{
    astarte_device_handle_t device; /**< Handle to the device triggering the event */
    void *user_data; /**< User data configured during device initialization */
} astarte_device_disconnection_event_t;

/** @brief Function pointer for disconnection events. */
typedef void (*astarte_device_disconnection_cbk_t)(astarte_device_disconnection_event_t event);

/** @brief Common context for all data events. */
typedef struct
{
    astarte_device_handle_t device; /**< Handle to the device triggering the event */
    const char *interface_name; /**< Name of the interface on which the event is triggered */
    const char *path; /**< Path on which event is triggered */
    void *user_data; /**< User data configured during device initialization */
} astarte_device_data_event_t;

/** @brief Context for a single datastream individual event. */
typedef struct
{
    astarte_device_data_event_t data_event; /**< Generic data event context */
    astarte_value_t value; /**< Received data from Astarte */
} astarte_device_datastream_individual_event_t;

/** @brief Function pointer for datastream individual events. */
typedef void (*astarte_device_datastream_individual_cbk_t)(
    astarte_device_datastream_individual_event_t event);

/** @brief Context for a single datastream object event. */
typedef struct
{
    astarte_device_data_event_t data_event; /**< Generic data event context */
    astarte_value_pair_t *values; /**< Received data from Astarte, as an array of key-value pairs */
    size_t values_length; /**< Size of the array of received data */
} astarte_device_datastream_object_event_t;

/** @brief Function pointer for datastream object events. */
typedef void (*astarte_device_datastream_object_cbk_t)(
    astarte_device_datastream_object_event_t event);

/** @brief Context for a single property set event. */
typedef struct
{
    astarte_device_data_event_t data_event; /**< Generic data event context */
    astarte_value_t value; /**< Received data from Astarte */
} astarte_device_property_set_event_t;

/** @brief Function pointer for property set events. */
typedef void (*astarte_device_property_set_cbk_t)(astarte_device_property_set_event_t event);

/** @brief Function pointer for property unset events. */
typedef void (*astarte_device_property_unset_cbk_t)(astarte_device_data_event_t event);

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
#if !defined(CONFIG_ASTARTE_DEVICE_SDK_GENERATE_DEVICE_ID)
    /** @brief Unique 128 bits, base64 URL encoded, identifier to associate to a device instance. */
    char device_id[ASTARTE_PAIRING_DEVICE_ID_LEN + 1];
#endif /* !defined(CONFIG_ASTARTE_DEVICE_SDK_GENERATE_DEVICE_ID) */
    /** @brief Credential secret to be used for connecting to Astarte. */
    char cred_secr[ASTARTE_PAIRING_CRED_SECR_LEN + 1];
    /** @brief Optional callback for a connection event. */
    astarte_device_connection_cbk_t connection_cbk;
    /** @brief Optional callback for a disconnection event. */
    astarte_device_disconnection_cbk_t disconnection_cbk;
    /** @brief Optional callback for a datastream individual reception event. */
    astarte_device_datastream_individual_cbk_t datastream_individual_cbk;
    /** @brief Optional callback for a datastream object reception event. */
    astarte_device_datastream_object_cbk_t datastream_object_cbk;
    /** @brief Optional callback for a set property event. */
    astarte_device_property_set_cbk_t property_set_cbk;
    /** @brief Optional callback for a unset property event. */
    astarte_device_property_unset_cbk_t property_unset_cbk;
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
 * @param[out] device Device instance initialized.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_device_new(astarte_device_config_t *cfg, astarte_device_handle_t *device);

/**
 * @brief Destroy the Astarte device instance.
 *
 * @note The device handle will become invalid after this operation.
 *
 * @param[in] device Device instance to be destroyed.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_device_destroy(astarte_device_handle_t device);

/**
 * @brief add an interface to the device.
 *
 * @warning This function has to be called while the device is in the disconnected state,
 * before a call to #astarte_device_connect or after a call to astarte_device_disconnect.
 *
 * @note The user is responsible for making sure the interface struct remains valid for the
 * lifetime of the device. It is recommended to declare interface structs as static constants.
 *
 * @param device A valid Astarte device handle.
 * @param interface The interface to add to the device.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_device_add_interface(
    astarte_device_handle_t device, const astarte_interface_t *interface);

/**
 * @brief Connect a device to Astarte.
 *
 * @param[in] device Device instance to connect to Astarte.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_device_connect(astarte_device_handle_t device);

/**
 * @brief Disconnect the Astarte device instance.
 *
 * @note It will be possible to re-connect the device after disconnection.
 *
 * @param[in] device Device instance to be disconnected.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_device_disconnect(astarte_device_handle_t device);

/**
 * @brief Poll data from Astarte.
 *
 * @param[in] device Device instance to poll data from Astarte.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_device_poll(astarte_device_handle_t device);

/**
 * @brief Send an individual value through the device connection.
 *
 * @param[in] device Handle to the device instance.
 * @param[in] interface_name Interface where to publish data.
 * @param[in] path Path where to publish data.
 * @param[in] value Astarte value to stream.
 * @param[in] timestamp Timestamp of the message, ignored if set to NULL.
 * @param[in] qos Quality of service for MQTT publish.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_device_stream_individual(astarte_device_handle_t device,
    const char *interface_name, const char *path, astarte_value_t value, const int64_t *timestamp,
    uint8_t qos);

/**
 * @brief Send an aggregated value through the device connection.
 *
 * @param[in] device Handle to the device instance.
 * @param[in] interface_name Interface where to publish data.
 * @param[in] path Path where to publish data.
 * @param[in] values Array of aggregated values pairs.
 * @param[in] values_length Number of elements for the values array.
 * @param[in] timestamp Timestamp of the message, ignored if set to NULL.
 * @param[in] qos Quality of service for MQTT publish.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_device_stream_aggregated(astarte_device_handle_t device,
    const char *interface_name, const char *path, astarte_value_pair_t *values,
    size_t values_length, const int64_t *timestamp, uint8_t qos);

/**
 * @brief Set a device property to the provided value.
 *
 * @param[in] device Handle to the device instance.
 * @param[in] interface_name Interface of the property.
 * @param[in] path Path of the property.
 * @param[in] value New value for the property.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_device_set_property(astarte_device_handle_t device,
    const char *interface_name, const char *path, astarte_value_t value);

/**
 * @brief Unset a device property.
 *
 * @param[in] device Handle to the device instance.
 * @param[in] interface_name Interface of the property.
 * @param[in] path Path of the property.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_device_unset_property(
    astarte_device_handle_t device, const char *interface_name, const char *path);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* ASTARTE_DEVICE_SDK_DEVICE_H */
