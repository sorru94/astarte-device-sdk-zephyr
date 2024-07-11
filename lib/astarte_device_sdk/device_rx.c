/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "device_rx.h"

#include "bson_deserializer.h"
#include "data_validation.h"
#include "individual_private.h"
#include "interface_private.h"
#include "object_private.h"

#include "log.h"
ASTARTE_LOG_MODULE_REGISTER(device_reception, CONFIG_ASTARTE_DEVICE_SDK_DEVICE_RX_LOG_LEVEL);

/************************************************
 *         Static functions declaration         *
 ***********************************************/

/**
 * @brief Handles an incoming generic data message.
 *
 * @details Deserializes the BSON payload and calls the appropriate handler based on the Astarte
 * interface type.
 *
 * @param[in] device Handle to the device instance.
 * @param[in] interface_name Interface name for which the data has been received.
 * @param[in] path Path for which the data has been received.
 * @param[in] data Payload for the received data.
 * @param[in] data_len Length of the payload (not including NULL terminator).
 */
static void on_data_message(astarte_device_handle_t device, const char *interface_name,
    const char *path, const char *data, size_t data_len);
/**
 * @brief Handles an incoming unset property message.
 *
 * @param[in] device Handle to the device instance.
 * @param[in] event Interface name for which the data has been received.
 */
static void on_unset_property(astarte_device_handle_t device, astarte_device_data_event_t event);
/**
 * @brief Handles an incoming set property message.
 *
 * @param[in] device Handle to the device instance.
 * @param[in] data_event Data event containing information regarding the received event.
 * @param[in] individual Astarte individual value received.
 */
static void on_set_property(astarte_device_handle_t device, astarte_device_data_event_t data_event,
    astarte_individual_t individual);
/**
 * @brief Handles an incoming datastream individual message.
 *
 * @param[in] device Handle to the device instance.
 * @param[in] data_event Data event containing information regarding the received event.
 * @param[in] individual Astarte individual value received.
 */
static void on_datastream_individual(astarte_device_handle_t device,
    astarte_device_data_event_t data_event, astarte_individual_t individual);
/**
 * @brief Handles an incoming datastream aggregated message.
 *
 * @param[in] device Handle to the device instance.
 * @param[in] data_event Data event containing information regarding the received event.
 * @param[in] entries The object entries received, organized as an array.
 * @param[in] entries_len The number of element in the @p entries array.
 */
static void on_datastream_aggregated(astarte_device_handle_t device,
    astarte_device_data_event_t data_event, astarte_object_entry_t *entries, size_t entries_len);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

void astarte_device_rx_on_incoming_handler(astarte_mqtt_t *astarte_mqtt, const char *topic,
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
        const char *control_subtopic = topic + strlen(device->control_topic);
        ASTARTE_LOG_DBG("Received control message on control topic %s", control_subtopic);
        // TODO correctly process control messages
        (void) control_subtopic;
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

    on_data_message(device, interface_name, path_start, data, data_len);
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static void on_data_message(astarte_device_handle_t device, const char *interface_name,
    const char *path, const char *data, size_t data_len)
{
    const astarte_interface_t *interface = introspection_get(
        &device->introspection, interface_name);
    if (!interface) {
        ASTARTE_LOG_ERR("Could not find interface in device introspection (%s).", interface_name);
        return;
    }

    astarte_device_data_event_t data_event = {
        .device = device,
        .interface_name = interface_name,
        .path = path,
        .user_data = device->cbk_user_data,
    };

    if ((interface->type == ASTARTE_INTERFACE_TYPE_PROPERTIES) && (data_len == 0)) {
        on_unset_property(device, data_event);
        return;
    }

    if (!astarte_bson_deserializer_check_validity(data, data_len)) {
        ASTARTE_LOG_ERR("Invalid BSON document in data");
        return;
    }

    astarte_bson_document_t full_document = astarte_bson_deserializer_init_doc(data);
    astarte_bson_element_t v_elem = { 0 };
    if (astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem)
        != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Cannot retrieve BSON value from data");
        return;
    }

    if (interface->aggregation == ASTARTE_INTERFACE_AGGREGATION_INDIVIDUAL) {
        const astarte_mapping_t *mapping = NULL;
        astarte_result_t ares = astarte_interface_get_mapping_from_path(interface, path, &mapping);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Could not find received mapping in interface %s.", interface_name);
            return;
        }
        astarte_individual_t individual = { 0 };
        ares = astarte_individual_deserialize(v_elem, mapping->type, &individual);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Failed in parsing the received BSON file. Interface: %s, path: %s.",
                interface_name, path);
            return;
        }

        if (interface->type == ASTARTE_INTERFACE_TYPE_PROPERTIES) {
            on_set_property(device, data_event, individual);
        } else {
            on_datastream_individual(device, data_event, individual);
        }
        astarte_individual_destroy_deserialized(individual);
    } else {
        astarte_object_entry_t *entries = NULL;
        size_t entries_length = 0;
        astarte_result_t ares = astarte_object_entries_deserialize(
            v_elem, interface, path, &entries, &entries_length);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Failed in parsing the received BSON file. Interface: %s, path: %s.",
                interface_name, path);
            return;
        }
        on_datastream_aggregated(device, data_event, entries, entries_length);
        astarte_object_entries_destroy_deserialized(entries, entries_length);
    }
}

static void on_unset_property(astarte_device_handle_t device, astarte_device_data_event_t event)
{
    const astarte_interface_t *interface = introspection_get(
        &device->introspection, event.interface_name);
    if (!interface) {
        ASTARTE_LOG_ERR(
            "Could not find interface in device introspection (%s).", event.interface_name);
        return;
    }

    astarte_result_t ares = data_validation_unset_property(interface, event.path);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Server property unset failed: %s.", astarte_result_to_name(ares));
        return;
    }

    if (device->property_unset_cbk) {
        device->property_unset_cbk(event);
    } else {
        ASTARTE_LOG_ERR("Unset property received, but no callback configured.");
    }
}

static void on_set_property(astarte_device_handle_t device, astarte_device_data_event_t data_event,
    astarte_individual_t individual)
{
    const astarte_interface_t *interface = introspection_get(
        &device->introspection, data_event.interface_name);
    if (!interface) {
        ASTARTE_LOG_ERR(
            "Could not find interface in device introspection (%s).", data_event.interface_name);
        return;
    }

    astarte_result_t ares = data_validation_set_property(interface, data_event.path, individual);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Server property data validation failed.");
        return;
    }

    if (device->property_set_cbk) {
        astarte_device_property_set_event_t set_event = {
            .data_event = data_event,
            .individual = individual,
        };
        device->property_set_cbk(set_event);
    } else {
        ASTARTE_LOG_ERR("Set property received, but no callback configured.");
    }
}

static void on_datastream_individual(astarte_device_handle_t device,
    astarte_device_data_event_t data_event, astarte_individual_t individual)
{
    const astarte_interface_t *interface = introspection_get(
        &device->introspection, data_event.interface_name);
    if (!interface) {
        ASTARTE_LOG_ERR(
            "Couldn't find interface in device introspection (%s).", data_event.interface_name);
        return;
    }

    astarte_result_t ares
        = data_validation_individual_datastream(interface, data_event.path, individual, NULL);
    // TODO: remove this exception when the following issue is resolved:
    // https://github.com/astarte-platform/astarte/issues/938
    if (ares == ASTARTE_RESULT_MAPPING_EXPLICIT_TIMESTAMP_REQUIRED) {
        ASTARTE_LOG_WRN("Received an individual datastream with missing explicit timestamp.");
    } else if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Server individual data validation failed.");
        return;
    }

    if (device->datastream_individual_cbk) {
        astarte_device_datastream_individual_event_t event = {
            .data_event = data_event,
            .individual = individual,
        };
        device->datastream_individual_cbk(event);
    } else {
        ASTARTE_LOG_ERR("Datastream individual received, but no callback configured.");
    }
}

static void on_datastream_aggregated(astarte_device_handle_t device,
    astarte_device_data_event_t data_event, astarte_object_entry_t *entries, size_t entries_len)
{
    const astarte_interface_t *interface = introspection_get(
        &device->introspection, data_event.interface_name);
    if (!interface) {
        ASTARTE_LOG_ERR(
            "Couldn't find interface in device introspection (%s).", data_event.interface_name);
        return;
    }

    astarte_result_t ares = data_validation_aggregated_datastream(
        interface, data_event.path, entries, entries_len, NULL);
    // TODO: remove this exception when the following issue is resolved:
    // https://github.com/astarte-platform/astarte/issues/938
    if (ares == ASTARTE_RESULT_MAPPING_EXPLICIT_TIMESTAMP_REQUIRED) {
        ASTARTE_LOG_WRN("Received an aggregated datastream with missing explicit timestamp.");
    } else if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Server aggregated data validation failed.");
        return;
    }

    if (device->datastream_object_cbk) {
        astarte_device_datastream_object_event_t event = {
            .data_event = data_event,
            .entries = entries,
            .entries_len = entries_len,
        };
        device->datastream_object_cbk(event);
    } else {
        ASTARTE_LOG_ERR("Datastream object received, but no callback configured.");
    }
}
