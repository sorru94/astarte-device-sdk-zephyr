/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "device_connection.h"

#if defined(CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE)
#include "astarte_zlib.h"
#include "device_caching.h"
#include "device_tx.h"
#endif

#include "log.h"
ASTARTE_LOG_MODULE_REGISTER(
    device_connection, CONFIG_ASTARTE_DEVICE_SDK_DEVICE_CONNECTION_LOG_LEVEL);

/************************************************
 *         Static functions declaration         *
 ***********************************************/

/**
 * @brief Setup all the MQTT subscriptions for the device.
 *
 * @param[in] device Handle to the device instance.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
static astarte_result_t setup_subscriptions(astarte_device_handle_t device);
/**
 * @brief Send the introspection for the device.
 *
 * @param[in] device Handle to the device instance.
 * @param[in] intr_str The stringified version of the introspection to transmit.
 */
static void send_introspection(astarte_device_handle_t device, char *intr_str);
/**
 * @brief Send the emptycache message to Astarte.
 *
 * @param[in] device Handle to the device instance.
 */
static void send_emptycache(astarte_device_handle_t device);
/**
 * @brief State machine runner code for the state DEVICE_START_HANDSHAKE.
 *
 * @param[in] device Handle to the device instance.
 */
static void state_machine_start_handshake_run(astarte_device_handle_t device);
/**
 * @brief State machine runner code for the state DEVICE_END_HANDSHAKE.
 *
 * @param[in] device Handle to the device instance.
 */
static void state_machine_end_handshake_run(astarte_device_handle_t device);
/**
 * @brief State machine runner code for the state DEVICE_HANDSHAKE_ERROR.
 *
 * @param[in] device Handle to the device instance.
 */
static void state_machine_handshake_error_run(astarte_device_handle_t device);
/**
 * @brief State machine runner code for the state DEVICE_CONNECTED.
 *
 * @param[in] device Handle to the device instance.
 */
static void state_machine_connected_run(astarte_device_handle_t device);
#if defined(CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE)
/**
 * @brief Send the purge properties message for the device owned properties.
 *
 * @param[in] device Handle to the device instance.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
static astarte_result_t send_purge_device_properties(astarte_device_handle_t device);
/**
 * @brief Send a single property if present in introspection and if device owned.
 *
 * @note If the property is not found in the introspection or if the major version does not match
 * the provided one then the property is deleted from the cache.
 *
 * @param[in] device Handle to the device instance.
 * @param[in] interface_name Name of the interface for the property as retreived from cache.
 * @param[in] path Path for the property as retreived from cache.
 * @param[in] major Major version for the interface of the property as retreived from cache.
 * @param[in] individual Data for the property as retreived from cache.
 */
static void send_device_owned_property(astarte_device_handle_t device, const char *interface_name,
    const char *path, uint32_t major, astarte_individual_t individual);
/**
 * @brief Send the device owned properties to Astarte.
 *
 * @param[in] device Handle to the device instance.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
static astarte_result_t send_device_owned_properties(astarte_device_handle_t device);
#endif

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_device_connection_connect(astarte_device_handle_t device)
{
    switch (device->connection_state) {
        case DEVICE_MQTT_CONNECTING:
        case DEVICE_START_HANDSHAKE:
        case DEVICE_END_HANDSHAKE:
            ASTARTE_LOG_WRN("Called connect function when device is connecting.");
            return ASTARTE_RESULT_MQTT_CLIENT_ALREADY_CONNECTING;
        case DEVICE_CONNECTED:
            ASTARTE_LOG_WRN("Called connect function when device is already connected.");
            return ASTARTE_RESULT_MQTT_CLIENT_ALREADY_CONNECTED;
        default: // Other states: (DEVICE_DISCONNECTED)
            break;
    }

    astarte_result_t ares = astarte_mqtt_connect(&device->astarte_mqtt);
    if (ares == ASTARTE_RESULT_OK) {
        ASTARTE_LOG_DBG("Device connection state -> MQTT_CONNECTING.");
        device->connection_state = DEVICE_MQTT_CONNECTING;
    }
    return ares;
}

astarte_result_t astarte_device_connection_disconnect(astarte_device_handle_t device)
{
    if (device->connection_state == DEVICE_DISCONNECTED) {
        ASTARTE_LOG_ERR("Disconnection request for a disconnected client will be ignored.");
        return ASTARTE_RESULT_DEVICE_NOT_READY;
    }

    return astarte_mqtt_disconnect(&device->astarte_mqtt);
}

void astarte_device_connection_on_connected_handler(
    astarte_mqtt_t *astarte_mqtt, struct mqtt_connack_param connack_param)
{
    struct astarte_device *device = CONTAINER_OF(astarte_mqtt, struct astarte_device, astarte_mqtt);

    ASTARTE_LOG_DBG("Device connection state -> START_HANDSHAKE.");
    device->connection_state = DEVICE_START_HANDSHAKE;

    device->mqtt_session_present_flag = connack_param.session_present_flag;
}

void astarte_device_connection_on_disconnected_handler(astarte_mqtt_t *astarte_mqtt)
{
    struct astarte_device *device = CONTAINER_OF(astarte_mqtt, struct astarte_device, astarte_mqtt);

    ASTARTE_LOG_DBG("Device connection state -> DISCONNECTED.");
    device->connection_state = DEVICE_DISCONNECTED;

    if (device->disconnection_cbk) {
        astarte_device_disconnection_event_t event = {
            .device = device,
            .user_data = device->cbk_user_data,
        };
        device->disconnection_cbk(event);
    }
}

void astarte_device_connection_on_subscribed_handler(
    astarte_mqtt_t *astarte_mqtt, uint16_t message_id, enum mqtt_suback_return_code return_code)
{
    (void) message_id;
    struct astarte_device *device = CONTAINER_OF(astarte_mqtt, struct astarte_device, astarte_mqtt);

    switch (return_code) {
        case MQTT_SUBACK_SUCCESS_QoS_0:
        case MQTT_SUBACK_SUCCESS_QoS_1:
        case MQTT_SUBACK_SUCCESS_QoS_2:
            break;
        case MQTT_SUBACK_FAILURE:
            device->subscription_failure = true;
            break;
        default:
            device->subscription_failure = true;
            ASTARTE_LOG_ERR("Invalid SUBACK return code.");
            break;
    }
}

astarte_result_t astarte_device_connection_poll(astarte_device_handle_t device)
{
    switch (device->connection_state) {
        case DEVICE_DISCONNECTED:
        case DEVICE_MQTT_CONNECTING:
            break;
        case DEVICE_START_HANDSHAKE:
            state_machine_start_handshake_run(device);
            break;
        case DEVICE_END_HANDSHAKE:
            state_machine_end_handshake_run(device);
            break;
        case DEVICE_HANDSHAKE_ERROR:
            state_machine_handshake_error_run(device);
            break;
        case DEVICE_CONNECTED:
            state_machine_connected_run(device);
            break;
        default: // nop
            break;
    }

    return astarte_mqtt_poll(&device->astarte_mqtt);
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static astarte_result_t setup_subscriptions(astarte_device_handle_t device)
{
    const char *topic = device->control_consumer_prop_topic;
    ASTARTE_LOG_DBG("Subscribing to: %s", topic);
    astarte_mqtt_subscribe(&device->astarte_mqtt, topic, 2, NULL);

    for (introspection_node_t *iterator = introspection_iter(&device->introspection);
        iterator != NULL; iterator = introspection_iter_next(&device->introspection, iterator)) {
        const astarte_interface_t *interface = iterator->interface;

        if (interface->ownership == ASTARTE_INTERFACE_OWNERSHIP_SERVER) {
            size_t topic_len = strlen(CONFIG_ASTARTE_DEVICE_SDK_REALM_NAME "///#")
                + ASTARTE_DEVICE_ID_LEN + strlen(interface->name);
            char *topic = calloc(topic_len + 1, sizeof(char));
            if (!topic) {
                ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
                return ASTARTE_RESULT_OUT_OF_MEMORY;
            }

            int ret
                = snprintf(topic, topic_len + 1, CONFIG_ASTARTE_DEVICE_SDK_REALM_NAME "/%s/%s/#",
                    device->device_id, interface->name);
            if (ret != topic_len) {
                ASTARTE_LOG_ERR("Error encoding MQTT topic.");
                free(topic);
                return ASTARTE_RESULT_INTERNAL_ERROR;
            }

            ASTARTE_LOG_DBG("Subscribing to: %s", topic);
            astarte_mqtt_subscribe(&device->astarte_mqtt, topic, 2, NULL);
            free(topic);
        }
    }
    return ASTARTE_RESULT_OK;
}

static void send_introspection(astarte_device_handle_t device, char *intr_str)
{
    const char *topic = device->base_topic;
    ASTARTE_LOG_DBG("Publishing introspection: %s", intr_str);
    astarte_mqtt_publish(&device->astarte_mqtt, topic, intr_str, strlen(intr_str), 2, NULL);
}

static void send_emptycache(astarte_device_handle_t device)
{
    const char *topic = device->control_empty_cache_topic;
    ASTARTE_LOG_DBG("Sending emptyCache to %s", topic);
    astarte_mqtt_publish(&device->astarte_mqtt, topic, "1", strlen("1"), 2, NULL);
}

static void state_machine_start_handshake_run(astarte_device_handle_t device)
{
    device->subscription_failure = false;

    char *intr_str = NULL;
    size_t intr_str_size = introspection_get_string_size(&device->introspection);

    intr_str = calloc(intr_str_size, sizeof(char));
    if (!intr_str) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        ASTARTE_LOG_DBG("Device connection state -> HANDSHAKE_ERROR.");
        device->connection_state = DEVICE_HANDSHAKE_ERROR;
        goto exit;
    }
    introspection_fill_string(&device->introspection, intr_str, intr_str_size);

#if defined(CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE)
    if ((device->mqtt_session_present_flag != 0) && device->synchronization_completed) {
        astarte_result_t ares = astarte_device_caching_introspection_check(intr_str, intr_str_size);
        if (ares == ASTARTE_RESULT_OK) {
            ASTARTE_LOG_DBG("Device connection state -> END_HANDSHAKE.");
            device->connection_state = DEVICE_END_HANDSHAKE;
            goto exit;
        }
    }
#else
    if ((device->mqtt_session_present_flag != 0) && device->synchronization_completed) {
        ASTARTE_LOG_DBG("Device connection state -> END_HANDSHAKE.");
        device->connection_state = DEVICE_END_HANDSHAKE;
        goto exit;
    }
#endif

    if (setup_subscriptions(device) != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_DBG("Device connection state -> HANDSHAKE_ERROR.");
        device->connection_state = DEVICE_HANDSHAKE_ERROR;
        goto exit;
    }
    send_introspection(device, intr_str);
    send_emptycache(device);
#if defined(CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE)
    if (send_purge_device_properties(device) != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_DBG("Device connection state -> HANDSHAKE_ERROR.");
        device->connection_state = DEVICE_HANDSHAKE_ERROR;
        goto exit;
    }
    if (send_device_owned_properties(device) != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_DBG("Device connection state -> HANDSHAKE_ERROR.");
        device->connection_state = DEVICE_HANDSHAKE_ERROR;
        goto exit;
    }
#endif
    ASTARTE_LOG_DBG("Device connection state -> END_HANDSHAKE.");
    device->connection_state = DEVICE_END_HANDSHAKE;

exit:
    free(intr_str);
}

static void state_machine_end_handshake_run(astarte_device_handle_t device)
{
    char *intr_str = NULL;
    if (device->subscription_failure) {
        ASTARTE_LOG_ERR("Subscription request has been denied.");
        ASTARTE_LOG_DBG("Device connection state -> HANDSHAKE_ERROR.");
        device->connection_state = DEVICE_HANDSHAKE_ERROR;
        goto exit;
    }
    if (!astarte_mqtt_has_pending_outgoing(&device->astarte_mqtt)) {
        ASTARTE_LOG_DBG("Device synchronization completed.");
        device->synchronization_completed = true;
        ASTARTE_LOG_DBG("Device connection state -> CONNECTED.");
        device->connection_state = DEVICE_CONNECTED;

#if defined(CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE)
        astarte_result_t ares = astarte_device_caching_synchronization_set(true);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Synchronization state set failure %s.", astarte_result_to_name(ares));
        }

        size_t intr_str_size = introspection_get_string_size(&device->introspection);
        intr_str = calloc(intr_str_size, sizeof(char));
        if (!intr_str) {
            ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
            ASTARTE_LOG_DBG("Device connection state -> HANDSHAKE_ERROR.");
            device->connection_state = DEVICE_HANDSHAKE_ERROR;
            goto exit;
        }
        introspection_fill_string(&device->introspection, intr_str, intr_str_size);

        ares = astarte_device_caching_introspection_check(intr_str, intr_str_size);
        if (ares == ASTARTE_RESULT_DEVICE_CACHING_OUTDATED_INTROSPECTION) {
            ASTARTE_LOG_DBG("Introspection requires updating.");
            ares = astarte_device_caching_introspection_store(intr_str, intr_str_size);
        }
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_DBG("Introspection update failed: %s", astarte_result_to_name(ares));
        }
#endif

        if (device->connection_cbk) {
            astarte_device_connection_event_t event = {
                .device = device,
                .user_data = device->cbk_user_data,
            };
            device->connection_cbk(event);
        }
    }

exit:
    free(intr_str);
}

static void state_machine_handshake_error_run(astarte_device_handle_t device)
{
    if (device->synchronization_completed) {
        device->synchronization_completed = false;
#if defined(CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE)
        astarte_result_t ares = astarte_device_caching_synchronization_set(false);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Synchronization state set failure %s.", astarte_result_to_name(ares));
        }
#endif
    }
    if (K_TIMEOUT_EQ(sys_timepoint_timeout(device->reconnection_timepoint), K_NO_WAIT)) {
        // Repeat the handshake procedure
        device->connection_state = DEVICE_START_HANDSHAKE;

        // Update backoff for the next attempt
        uint32_t next_backoff_ms = 0;
        backoff_get_next(&device->backoff_ctx, &next_backoff_ms);
        device->reconnection_timepoint = sys_timepoint_calc(K_MSEC(next_backoff_ms));
    }
}

static void state_machine_connected_run(astarte_device_handle_t device)
{
    backoff_context_init(&device->backoff_ctx,
        CONFIG_ASTARTE_DEVICE_SDK_RECONNECTION_ASTARTE_BACKOFF_INITIAL_MS,
        CONFIG_ASTARTE_DEVICE_SDK_RECONNECTION_ASTARTE_BACKOFF_MAX_MS, true);
}

#if defined(CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE)
static astarte_result_t send_purge_device_properties(astarte_device_handle_t device)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    char *intr_str = NULL;
    uint8_t *payload = NULL;

    size_t intr_str_size = 0U;
    ares = astarte_device_caching_property_get_device_string(
        &device->introspection, NULL, &intr_str_size);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Error getting cached properties string: %s", astarte_result_to_name(ares));
        goto exit;
    }
    if (intr_str_size != 0) {
        intr_str = calloc(intr_str_size, sizeof(char));
        if (!intr_str) {
            ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
            ares = ASTARTE_RESULT_OUT_OF_MEMORY;
            goto exit;
        }

        ares = astarte_device_caching_property_get_device_string(
            &device->introspection, intr_str, &intr_str_size);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Can't get cached properties string: %s", astarte_result_to_name(ares));
            goto exit;
        }
    }

    // Estimate compression result size and payload size
    char *compression_input = intr_str;
    size_t compression_input_len = (compression_input) ? (intr_str_size - 1) : 0;
    uLongf compressed_len = compressBound(compression_input_len);
    // Allocate enough memory for the payload
    size_t payload_size = 4 + compressed_len;
    payload = calloc(payload_size, sizeof(uint8_t));
    if (!payload) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto exit;
    }
    // Fill the first 32 bits of the payload
    uint32_t *payload_uint32 = (uint32_t *) payload;
    *payload_uint32 = __builtin_bswap32(compression_input_len);
    // Perform the compression and store result in the payload
    int compress_res = astarte_zlib_compress((char unsigned *) &payload[4], &compressed_len,
        (char unsigned *) compression_input, compression_input_len);
    if (compress_res != Z_OK) {
        ASTARTE_LOG_ERR("Error compressing the purge properties message %d.", compress_res);
        ares = ASTARTE_RESULT_INTERNAL_ERROR;
        goto exit;
    }
    // 'astarte_zlib_compress' updates 'compressed_len' to the actual size of the compressed data
    payload_size = 4 + compressed_len;
    // Check if payload is not too large for a MQTT message
    if (payload_size > INT_MAX) {
        // MQTT supports sending a maximum payload length of INT_MAX
        ASTARTE_LOG_ERR("Purge properties payload is too long for a single MQTT message.");
        ares = ASTARTE_RESULT_MQTT_ERROR;
        goto exit;
    }

    // Transmit the payload
    const char *topic = device->control_producer_prop_topic;
    const int qos = 2;
    ASTARTE_LOG_INF("Sending purge properties to: '%s', with uncompressed content: '%s'", topic,
        (compression_input) ? compression_input : "");
    astarte_mqtt_publish(&device->astarte_mqtt, topic, payload, payload_size, qos, NULL);

exit:
    free(intr_str);
    free(payload);
    return ares;
}

static void send_device_owned_property(astarte_device_handle_t device, const char *interface_name,
    const char *path, uint32_t major, astarte_individual_t individual)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    const astarte_interface_t *interface = introspection_get(
        &device->introspection, interface_name);
    if ((!interface) || (interface->major_version != major)) {
        ASTARTE_LOG_DBG("Removing property from storage: '%s%s'", interface_name, path);
        ares = astarte_device_caching_property_delete(interface_name, path);
        if ((ares != ASTARTE_RESULT_OK) && (ares != ASTARTE_RESULT_NOT_FOUND)) {
            ASTARTE_LOG_COND_ERR(ares != ASTARTE_RESULT_OK,
                "Failed deleting the cached property: %s", astarte_result_to_name(ares));
        }
        return;
    }

    if (interface->ownership == ASTARTE_INTERFACE_OWNERSHIP_DEVICE) {
        ares = astarte_device_tx_stream_individual(device, interface_name, path, individual, NULL);
        ASTARTE_LOG_COND_ERR(ares != ASTARTE_RESULT_OK, "Failed sending cached property: %s",
            astarte_result_to_name(ares));
    }
}

static astarte_result_t send_device_owned_properties(astarte_device_handle_t device)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    astarte_device_caching_property_iter_t iter = { 0 };
    char *interface_name = NULL;
    char *path = NULL;
    astarte_individual_t individual = { 0 };

    ares = astarte_device_caching_property_iterator_new(&iter);
    if ((ares != ASTARTE_RESULT_OK) && (ares != ASTARTE_RESULT_NOT_FOUND)) {
        ASTARTE_LOG_ERR("Properties iterator init failed: %s", astarte_result_to_name(ares));
        goto end;
    }

    while (ares != ASTARTE_RESULT_NOT_FOUND) {
        size_t interface_name_size = 0U;
        size_t path_size = 0U;
        ares = astarte_device_caching_property_iterator_get(
            &iter, NULL, &interface_name_size, NULL, &path_size);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Properties iterator get error: %s", astarte_result_to_name(ares));
            goto end;
        }

        // Allocate space for the name and path
        interface_name = calloc(interface_name_size, sizeof(char));
        path = calloc(path_size, sizeof(char));
        if (!interface_name || !path) {
            ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
            ares = ASTARTE_RESULT_OUT_OF_MEMORY;
            goto end;
        }

        ares = astarte_device_caching_property_iterator_get(
            &iter, interface_name, &interface_name_size, path, &path_size);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Properties iterator get error: %s", astarte_result_to_name(ares));
            goto end;
        }

        uint32_t major = 0U;
        ares = astarte_device_caching_property_load(interface_name, path, &major, &individual);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Properties load property error: %s", astarte_result_to_name(ares));
            goto end;
        }

        send_device_owned_property(device, interface_name, path, major, individual);

        free(interface_name);
        interface_name = NULL;
        free(path);
        path = NULL;
        astarte_device_caching_property_destroy_loaded(individual);
        individual = (astarte_individual_t) { 0 };

        ares = astarte_device_caching_property_iterator_next(&iter);
        if ((ares != ASTARTE_RESULT_OK) && (ares != ASTARTE_RESULT_NOT_FOUND)) {
            ASTARTE_LOG_ERR("Iterator next error: %s", astarte_result_to_name(ares));
            goto end;
        }
    }

end:
    // Free all data
    astarte_device_caching_property_iterator_destroy(iter);
    free(interface_name);
    free(path);
    astarte_device_caching_property_destroy_loaded(individual);
    return (ares == ASTARTE_RESULT_NOT_FOUND) ? ASTARTE_RESULT_OK : ares;
}
#endif
