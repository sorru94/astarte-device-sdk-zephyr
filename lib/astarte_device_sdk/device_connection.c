/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "device_connection.h"

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
 */
static void setup_subscriptions(astarte_device_handle_t device);
/**
 * @brief Send the introspection for the device.
 *
 * @param[in] device Handle to the device instance.
 */
static void send_introspection(astarte_device_handle_t device);
/**
 * @brief Send the emptycache message to Astarte.
 *
 * @param[in] device Handle to the device instance.
 */
static void send_emptycache(astarte_device_handle_t device);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_device_connection_connect(astarte_device_handle_t device)
{
    switch (device->connection_state) {
        case DEVICE_CONNECTING:
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
        ASTARTE_LOG_DBG("Device connection state -> CONNECTING.");
        device->connection_state = DEVICE_CONNECTING;
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

    if (connack_param.session_present_flag != 0) {
        ASTARTE_LOG_DBG("Device connection state -> CONNECTED.");
        device->connection_state = DEVICE_CONNECTED;
        return;
    }

    device->subscription_failure = false;
    setup_subscriptions(device);
    send_introspection(device);
    send_emptycache(device);
    // TODO: send device owned props

    ASTARTE_LOG_DBG("Device connection state -> CONNECTING.");
    device->connection_state = DEVICE_CONNECTING;
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
    if ((device->connection_state == DEVICE_CONNECTING)
        && astarte_mqtt_is_connected(&device->astarte_mqtt)) {
        if (device->subscription_failure) {
            ASTARTE_LOG_ERR("Subscription request has been denied, irrecoverable error.");
            return ASTARTE_RESULT_INTERNAL_ERROR;
        }
        if (!astarte_mqtt_has_pending_outgoing(&device->astarte_mqtt)) {

            ASTARTE_LOG_DBG("Device connection state -> CONNECTED.");
            device->connection_state = DEVICE_CONNECTED;

            if (device->connection_cbk) {
                astarte_device_connection_event_t event = {
                    .device = device,
                    .user_data = device->cbk_user_data,
                };
                device->connection_cbk(event);
            }
        }
    }

    return astarte_mqtt_poll(&device->astarte_mqtt);
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static void setup_subscriptions(astarte_device_handle_t device)
{
    const char *topic = device->control_consumer_prop_topic;
    ASTARTE_LOG_DBG("Subscribing to: %s", topic);
    astarte_mqtt_subscribe(&device->astarte_mqtt, topic, 2, NULL);

    for (introspection_node_t *iterator = introspection_iter(&device->introspection);
         iterator != NULL; iterator = introspection_iter_next(&device->introspection, iterator)) {
        const astarte_interface_t *interface = iterator->interface;

        if (interface->ownership == ASTARTE_INTERFACE_OWNERSHIP_SERVER) {
            char *topic = NULL;
            int ret = asprintf(&topic, CONFIG_ASTARTE_DEVICE_SDK_REALM_NAME "/%s/%s/#",
                device->device_id, interface->name);
            if (ret < 0) {
                ASTARTE_LOG_ERR("Error encoding MQTT topic");
                ASTARTE_LOG_ERR("Might be out of memory %s: %d", __FILE__, __LINE__);
                continue;
            }

            ASTARTE_LOG_DBG("Subscribing to: %s", topic);
            astarte_mqtt_subscribe(&device->astarte_mqtt, topic, 2, NULL);
            free(topic);
        }
    }
}

static void send_introspection(astarte_device_handle_t device)
{
    const char *topic = device->base_topic;

    char *intr_str = NULL;
    size_t intr_str_size = introspection_get_string_size(&device->introspection);

    // if introspection size is > 4KiB print a warning
    const size_t introspection_size_warn_level = 4096;
    if (intr_str_size > introspection_size_warn_level) {
        ASTARTE_LOG_WRN("The introspection size is > 4KiB");
    }

    intr_str = calloc(intr_str_size, sizeof(char));
    if (!intr_str) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        goto exit;
    }
    introspection_fill_string(&device->introspection, intr_str, intr_str_size);

    ASTARTE_LOG_DBG("Publishing introspection: %s", intr_str);
    astarte_mqtt_publish(&device->astarte_mqtt, topic, intr_str, strlen(intr_str), 2, NULL);

exit:
    free(intr_str);
}

static void send_emptycache(astarte_device_handle_t device)
{
    const char *topic = device->control_empty_cache_topic;
    ASTARTE_LOG_DBG("Sending emptyCache to %s", topic);
    astarte_mqtt_publish(&device->astarte_mqtt, topic, "1", strlen("1"), 2, NULL);
}
