/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "device/dispatcher.h"

#include "alloc.h"
#include "device/datastreams.h"
#include "device/properties.h"
#include "device/properties_storage.h"
#include "mqtt/pubsub.h"

#include "log.h"
ASTARTE_LOG_MODULE_DECLARE(astarte_device, CONFIG_ASTARTE_DEVICE_SDK_DEVICE_LOG_LEVEL);

/************************************************
 *         Static functions declaration         *
 ***********************************************/

static void on_control_message(
    astarte_device_handle_t device, const char *topic, const char *data, size_t data_len);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

void astarte_device_dispatcher_on_connected(
    astarte_mqtt_t *astarte_mqtt, struct mqtt_connack_param connack_param)
{
    struct astarte_device *device = CONTAINER_OF(astarte_mqtt, struct astarte_device, astarte_mqtt);

    ASTARTE_LOG_DBG("Device connection state -> START_HANDSHAKE");
    device->connection_state = DEVICE_START_HANDSHAKE;

    device->mqtt_session_present_flag = connack_param.session_present_flag;
}

void astarte_device_dispatcher_on_disconnected(astarte_mqtt_t *astarte_mqtt)
{
    struct astarte_device *device = CONTAINER_OF(astarte_mqtt, struct astarte_device, astarte_mqtt);

    ASTARTE_LOG_DBG("Device connection state -> DISCONNECTED");
    device->connection_state = DEVICE_DISCONNECTED;

    if (device->disconnection_cbk) {
        astarte_device_disconnection_event_t event = {
            .device = device,
            .user_data = device->cbk_user_data,
        };
        device->disconnection_cbk(event);
    }
}

void astarte_device_dispatcher_on_subscribed(
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
            ASTARTE_LOG_ERR("Subscription failure");
            break;
        default:
            device->subscription_failure = true;
            ASTARTE_LOG_ERR("Invalid SUBACK return code");
            break;
    }
}

void astarte_device_dispatcher_on_incoming(astarte_mqtt_t *astarte_mqtt, const char *topic,
    size_t topic_len, const char *data, size_t data_len)
{
    struct astarte_device *device = CONTAINER_OF(astarte_mqtt, struct astarte_device, astarte_mqtt);

    // Check if base topic is correct
    if (strstr(topic, device->base_topic) != topic) {
        ASTARTE_LOG_ERR("Incoming message topic doesn't begin with <REALM>/<DEVICE ID>: %s", topic);
        return;
    }

    // Control message
    if (strstr(topic, device->control_topic)) {
        ASTARTE_LOG_DBG("Received control message on control topic %s", topic);
        on_control_message(device, topic, data, data_len);
        return;
    }

    // Data message
    if ((topic_len < MQTT_BASE_TOPIC_LEN + strlen("/")) || (topic[MQTT_BASE_TOPIC_LEN] != '/')) {
        ASTARTE_LOG_ERR("Missing '/' after base topic, can't find interface in topic: %s", topic);
        return;
    }

    const char *interface_name_start = topic + MQTT_BASE_TOPIC_LEN + strlen("/");
    char *path_start = strchr(interface_name_start, '/');
    if (!path_start) {
        ASTARTE_LOG_ERR("Missing '/' after interface name, can't find path in topic: %s", topic);
        return;
    }

    size_t interface_name_len = path_start - interface_name_start;
    char interface_name[ASTARTE_INTERFACE_NAME_MAX_SIZE] = { 0 };
    int ret = snprintf(interface_name, ASTARTE_INTERFACE_NAME_MAX_SIZE, "%.*s", interface_name_len,
        interface_name_start);
    if ((ret < 0) || (ret >= ASTARTE_INTERFACE_NAME_MAX_SIZE)) {
        ASTARTE_LOG_ERR("Error encoding interface name");
        return;
    }

    const astarte_interface_t *interface = introspection_get(
        &device->introspection, interface_name);
    if (!interface) {
        ASTARTE_LOG_ERR("Could not find interface in device introspection (%s)", interface_name);
        return;
    }

    if (interface->type == ASTARTE_INTERFACE_TYPE_PROPERTIES) {
        astarte_device_properties_handle_incoming(device, interface, path_start, data, data_len);
    } else {
        astarte_device_datastreams_handle_incoming(device, interface, path_start, data, data_len);
    }
}

astarte_result_t astarte_device_dispatcher_publish_data(astarte_device_handle_t device,
    const char *interface_name, const char *path, const void *data, int data_size, int qos)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    char *topic = NULL;

    if (path[0] != '/') {
        ASTARTE_LOG_ERR("Invalid path: %s (must be start with /)", path);
        ares = ASTARTE_RESULT_INVALID_PARAM;
        goto exit;
    }

    if (qos < 0 || qos > 2) {
        ASTARTE_LOG_ERR("Invalid QoS: %d (must be 0, 1 or 2)", qos);
        ares = ASTARTE_RESULT_INVALID_PARAM;
        goto exit;
    }

    size_t topic_len = strlen(CONFIG_ASTARTE_DEVICE_SDK_REALM_NAME "//") + ASTARTE_DEVICE_ID_LEN
        + strlen(interface_name) + strlen(path);
    topic = astarte_calloc(topic_len + 1, sizeof(char));
    if (!topic) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto exit;
    }

    int ret = snprintf(topic, topic_len + 1, CONFIG_ASTARTE_DEVICE_SDK_REALM_NAME "/%s/%s%s",
        device->device_id, interface_name, path);
    if (ret != topic_len) {
        ASTARTE_LOG_ERR("Error encoding MQTT topic");
        ares = ASTARTE_RESULT_INTERNAL_ERROR;
        goto exit;
    }

    astarte_mqtt_publish(&device->astarte_mqtt, topic, (void *) data, data_size, qos, NULL);

exit:
    astarte_free(topic);
    return ares;
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static void on_control_message(
    astarte_device_handle_t device, const char *topic, const char *data, size_t data_len)
{
    if (strcmp(topic, device->control_consumer_prop_topic) == 0) {
#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
        astarte_device_properties_handle_purge(device, data, data_len);
#else
        (void) device;
        (void) data;
        (void) data_len;
#endif
    } else {
        ASTARTE_LOG_ERR("Received unrecognized control message: %s", topic);
    }
}
