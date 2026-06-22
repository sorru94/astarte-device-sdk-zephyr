/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "device/core.h"

#include "alloc.h"
#include "device/properties.h"
#include "device/properties_storage.h"
#include "mqtt/pubsub.h"

#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
#include "storage/introsp.h"
#include "storage/sync.h"
#endif

#include "log.h"
ASTARTE_LOG_MODULE_DECLARE(astarte_device, CONFIG_ASTARTE_DEVICE_SDK_DEVICE_LOG_LEVEL);

/************************************************
 *         Static functions declaration         *
 ***********************************************/

static void state_machine_start_handshake_run(astarte_device_handle_t device);
static void state_machine_end_handshake_run(astarte_device_handle_t device);
static void state_machine_handshake_error_run(astarte_device_handle_t device);
static void state_machine_connected_run(astarte_device_handle_t device);
static astarte_result_t setup_subscriptions(astarte_device_handle_t device);
static void send_introspection(astarte_device_handle_t device, char *intr_str);
static void send_emptycache(astarte_device_handle_t device);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_device_connect(astarte_device_handle_t device)
{
    ASTARTE_LOG_DBG("Connecting the device");

    if (!device) {
        ASTARTE_LOG_ERR("Received NULL reference for device handle");
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    switch (device->connection_state) {
        case DEVICE_MQTT_CONNECTING:
        case DEVICE_START_HANDSHAKE:
        case DEVICE_END_HANDSHAKE:
            ASTARTE_LOG_WRN("Called connect function while device is already connecting");
            return ASTARTE_RESULT_MQTT_CLIENT_ALREADY_CONNECTING;
        case DEVICE_CONNECTED:
            ASTARTE_LOG_WRN("Called connect function while device is already connected");
            return ASTARTE_RESULT_MQTT_CLIENT_ALREADY_CONNECTED;
        default: // Other states: (DEVICE_DISCONNECTED)
            break;
    }

    astarte_result_t ares = astarte_mqtt_connect(&device->astarte_mqtt);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed to connect through MQTT %s", astarte_result_to_name(ares));
    } else {
        ASTARTE_LOG_DBG("Device connection state -> MQTT_CONNECTING");
        device->connection_state = DEVICE_MQTT_CONNECTING;
    }
    return ares;
}

astarte_result_t astarte_device_disconnect(astarte_device_handle_t device, k_timeout_t timeout)
{
    ASTARTE_LOG_DBG("Disconnecting the device");
    if (!device) {
        ASTARTE_LOG_ERR("Received NULL reference for device handle");
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    if (device->connection_state == DEVICE_DISCONNECTED) {
        ASTARTE_LOG_ERR("Disconnection request for a disconnected client will be ignored");
        return ASTARTE_RESULT_DEVICE_NOT_READY;
    }

    k_timepoint_t timepoint = sys_timepoint_calc(timeout);
    while (astarte_mqtt_has_pending_outgoing(&device->astarte_mqtt)) {
        if (sys_timepoint_expired(timepoint)) {
            ASTARTE_LOG_WRN("Polling for outgoing MQTT messages timed out");
            return ASTARTE_RESULT_TIMEOUT;
        }
        astarte_result_t ares = astarte_device_poll(device);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Poll failure during disconnection %s", astarte_result_to_name(ares));
            return ares;
        }
        k_sleep(K_MSEC(100));
    }

    return astarte_mqtt_disconnect(&device->astarte_mqtt);
}

astarte_result_t astarte_device_force_disconnect(astarte_device_handle_t device)
{
    ASTARTE_LOG_DBG("Forcing device disconnection");
    if (!device) {
        ASTARTE_LOG_ERR("Received NULL reference for device handle");
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    if (device->connection_state == DEVICE_DISCONNECTED) {
        ASTARTE_LOG_ERR("Disconnection request for a disconnected client will be ignored");
        return ASTARTE_RESULT_DEVICE_NOT_READY;
    }

    return astarte_mqtt_disconnect(&device->astarte_mqtt);
}

astarte_result_t astarte_device_poll(astarte_device_handle_t device)
{
    if (!device) {
        ASTARTE_LOG_ERR("Received NULL reference for device handle");
        return ASTARTE_RESULT_INVALID_PARAM;
    }

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

static void state_machine_start_handshake_run(astarte_device_handle_t device)
{
    device->subscription_failure = false;

    char *intr_str = NULL;
    size_t intr_str_size = introspection_get_string_size(&device->introspection);

    intr_str = astarte_calloc(intr_str_size, sizeof(char));
    if (!intr_str) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        ASTARTE_LOG_DBG("Device connection state -> HANDSHAKE_ERROR");
        device->connection_state = DEVICE_HANDSHAKE_ERROR;
        goto exit;
    }
    introspection_fill_string(&device->introspection, intr_str, intr_str_size);

#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
    if ((device->mqtt_session_present_flag != 0) && device->synchronization_completed) {
        astarte_result_t ares
            = astarte_storage_introspection_check(&device->caching, intr_str, intr_str_size);
        if (ares == ASTARTE_RESULT_OK) {
            ASTARTE_LOG_DBG("Device connection state -> END_HANDSHAKE");
            device->connection_state = DEVICE_END_HANDSHAKE;
            goto exit;
        }
    }
#else
    if ((device->mqtt_session_present_flag != 0) && device->synchronization_completed) {
        ASTARTE_LOG_DBG("Device connection state -> END_HANDSHAKE");
        device->connection_state = DEVICE_END_HANDSHAKE;
        goto exit;
    }
#endif

    if (setup_subscriptions(device) != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_DBG("Device connection state -> HANDSHAKE_ERROR");
        device->connection_state = DEVICE_HANDSHAKE_ERROR;
        goto exit;
    }
    send_introspection(device, intr_str);
    send_emptycache(device);
#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
    if (astarte_device_properties_send_purge(device) != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_DBG("Device connection state -> HANDSHAKE_ERROR");
        device->connection_state = DEVICE_HANDSHAKE_ERROR;
        goto exit;
    }
    if (astarte_device_properties_send_device_owned(device) != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_DBG("Device connection state -> HANDSHAKE_ERROR");
        device->connection_state = DEVICE_HANDSHAKE_ERROR;
        goto exit;
    }
#endif
    ASTARTE_LOG_DBG("Device connection state -> END_HANDSHAKE");
    device->connection_state = DEVICE_END_HANDSHAKE;

exit:
    astarte_free(intr_str);
}

static void state_machine_end_handshake_run(astarte_device_handle_t device)
{
    char *intr_str = NULL;
    if (device->subscription_failure) {
        ASTARTE_LOG_ERR("Subscription request has been denied");
        ASTARTE_LOG_DBG("Device connection state -> HANDSHAKE_ERROR");
        device->connection_state = DEVICE_HANDSHAKE_ERROR;
        goto exit;
    }
    if (!astarte_mqtt_has_pending_outgoing(&device->astarte_mqtt)) {
        ASTARTE_LOG_DBG("Device synchronization completed");
        device->synchronization_completed = true;
        ASTARTE_LOG_DBG("Device connection state -> CONNECTED");
        device->connection_state = DEVICE_CONNECTED;

#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
        astarte_result_t ares = astarte_storage_synchronization_set(&device->caching, true);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Synchronization state set failure %s", astarte_result_to_name(ares));
        }

        size_t intr_str_size = introspection_get_string_size(&device->introspection);
        intr_str = astarte_calloc(intr_str_size, sizeof(char));
        if (!intr_str) {
            ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
            ASTARTE_LOG_DBG("Device connection state -> HANDSHAKE_ERROR");
            device->connection_state = DEVICE_HANDSHAKE_ERROR;
            goto exit;
        }
        introspection_fill_string(&device->introspection, intr_str, intr_str_size);

        ares = astarte_storage_introspection_check(&device->caching, intr_str, intr_str_size);
        if (ares == ASTARTE_RESULT_DEVICE_CACHING_OUTDATED_INTROSPECTION) {
            ASTARTE_LOG_DBG("Introspection requires updating");
            ares = astarte_storage_introspection_store(&device->caching, intr_str, intr_str_size);
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
    astarte_free(intr_str);
}

static void state_machine_handshake_error_run(astarte_device_handle_t device)
{
    if (device->synchronization_completed) {
        device->synchronization_completed = false;
#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
        astarte_result_t ares = astarte_storage_synchronization_set(&device->caching, false);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Synchronization state set failure %s", astarte_result_to_name(ares));
        }
#endif
    }
    if (K_TIMEOUT_EQ(sys_timepoint_timeout(device->reconnection_timepoint), K_NO_WAIT)) {
        // Repeat the handshake procedure
        ASTARTE_LOG_DBG("Device connection state -> START_HANDSHAKE");
        device->connection_state = DEVICE_START_HANDSHAKE;

        // Update backoff for the next attempt
        uint64_t next_backoff_ms = backoff_get_next_delay(&device->backoff_ctx);
        device->reconnection_timepoint = sys_timepoint_calc(K_MSEC(next_backoff_ms));
    }
}

static void state_machine_connected_run(astarte_device_handle_t device)
{
    backoff_init(&device->backoff_ctx, CONFIG_ASTARTE_DEVICE_SDK_RECONNECTION_BACKOFF_MULT_COEFF_MS,
        CONFIG_ASTARTE_DEVICE_SDK_RECONNECTION_BACKOFF_CUTOFF_COEFF_MS);
}

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
            char *topic = astarte_calloc(topic_len + 1, sizeof(char));
            if (!topic) {
                ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
                return ASTARTE_RESULT_OUT_OF_MEMORY;
            }

            int ret
                = snprintf(topic, topic_len + 1, CONFIG_ASTARTE_DEVICE_SDK_REALM_NAME "/%s/%s/#",
                    device->device_id, interface->name);
            if (ret != topic_len) {
                ASTARTE_LOG_ERR("Error encoding MQTT topic");
                astarte_free(topic);
                return ASTARTE_RESULT_INTERNAL_ERROR;
            }

            ASTARTE_LOG_DBG("Subscribing to: %s", topic);
            astarte_mqtt_subscribe(&device->astarte_mqtt, topic, 2, NULL);
            astarte_free(topic);
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
