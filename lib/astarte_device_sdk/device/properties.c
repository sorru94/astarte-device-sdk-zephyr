/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "device/properties.h"

#include "alloc.h"
#include "bson/deserializer.h"
#include "data/deserialize.h"
#include "device/datastreams.h"
#include "device/dispatcher.h"
#include "mqtt/pubsub.h"
#include "validation.h"

#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
#include "storage/prop.h"
#endif

#include "log.h"
ASTARTE_LOG_MODULE_DECLARE(astarte_device, CONFIG_ASTARTE_DEVICE_SDK_DEVICE_LOG_LEVEL);

/************************************************
 *         Static functions declaration         *
 ***********************************************/

static void on_unset_property(astarte_device_handle_t device, astarte_device_data_event_t event);
static void on_set_property(
    astarte_device_handle_t device, astarte_device_data_event_t base_event, astarte_data_t data);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_device_set_property(astarte_device_handle_t device,
    const char *interface_name, const char *path, astarte_data_t data)
{
    if (!device || !interface_name || !path) {
        ASTARTE_LOG_ERR("Received a NULL reference for a required input parameter");
        return ASTARTE_RESULT_INVALID_PARAM;
    }
    if (device->connection_state != DEVICE_CONNECTED) {
        ASTARTE_LOG_ERR("Called set property function when the device is not connected");
        return ASTARTE_RESULT_DEVICE_NOT_READY;
    }

    const astarte_interface_t *interface = introspection_get(
        &device->introspection, interface_name);
    if (!interface) {
        ASTARTE_LOG_ERR("Couldn't find interface in device introspection (%s)", interface_name);
        return ASTARTE_RESULT_INTERFACE_NOT_FOUND;
    }

    astarte_result_t ares = astarte_validation_set_property(interface, path, data);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Property data validation failed");
        return ares;
    }

#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
    ares = astarte_storage_property_store(
        &device->caching, interface_name, path, interface->major_version, data);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed storing the property");
    }
#endif

    return astarte_device_send_individual(device, interface_name, path, data, NULL);
}

astarte_result_t astarte_device_unset_property(
    astarte_device_handle_t device, const char *interface_name, const char *path)
{
    if (!device || !interface_name || !path) {
        ASTARTE_LOG_ERR("Received a NULL reference for a required input parameter");
        return ASTARTE_RESULT_INVALID_PARAM;
    }
    if (device->connection_state != DEVICE_CONNECTED) {
        ASTARTE_LOG_ERR("Called unset property function when the device is not connected");
        return ASTARTE_RESULT_DEVICE_NOT_READY;
    }

    const astarte_interface_t *interface = introspection_get(
        &device->introspection, interface_name);
    if (!interface) {
        ASTARTE_LOG_ERR("Couldn't find interface in device introspection (%s).", interface_name);
        return ASTARTE_RESULT_INTERFACE_NOT_FOUND;
    }

    astarte_result_t ares = astarte_validation_unset_property(interface, path);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Device property unset failed");
        return ares;
    }

#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
    ares = astarte_storage_property_delete(&device->caching, interface_name, path);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed deleting the stored property");
    }
#endif

    return astarte_device_dispatcher_publish_data(device, interface_name, path, "", 0, 2);
}

void astarte_device_properties_handle_incoming(astarte_device_handle_t device,
    const astarte_interface_t *interface, const char *path, const char *data, size_t data_len)
{
    astarte_device_data_event_t base_event = {
        .device = device,
        .interface_name = interface->name,
        .path = path,
        .user_data = device->cbk_user_data,
    };

    if (data_len == 0) {
        on_unset_property(device, base_event);
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

    const astarte_mapping_t *mapping = NULL;
    astarte_result_t ares = astarte_interface_get_mapping_from_path(interface, path, &mapping);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Could not find received mapping in interface %s", interface->name);
        return;
    }
    astarte_data_t data_deserialized = { 0 };
    ares = astarte_data_deserialize(v_elem, mapping->type, &data_deserialized);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed in parsing the received BSON file. Interface: %s, path: %s.",
            interface->name, path);
        return;
    }

    on_set_property(device, base_event, data_deserialized);
    astarte_data_destroy_deserialized(data_deserialized);
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static void on_unset_property(astarte_device_handle_t device, astarte_device_data_event_t event)
{
    const astarte_interface_t *interface = introspection_get(
        &device->introspection, event.interface_name);
    if (!interface) {
        ASTARTE_LOG_ERR(
            "Could not find interface in device introspection (%s).", event.interface_name);
        return;
    }

    astarte_result_t ares = astarte_validation_unset_property(interface, event.path);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Server property unset is invalid: %s.", astarte_result_to_name(ares));
        return;
    }

#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
    ares = astarte_storage_property_delete(&device->caching, event.interface_name, event.path);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed deleting the stored server property.");
    }
#endif

    if (device->property_unset_cbk) {
        device->property_unset_cbk(event);
    } else {
        ASTARTE_LOG_ERR("Unset property received, but no callback configured.");
    }
}

static void on_set_property(
    astarte_device_handle_t device, astarte_device_data_event_t base_event, astarte_data_t data)
{
    const astarte_interface_t *interface = introspection_get(
        &device->introspection, base_event.interface_name);
    if (!interface) {
        ASTARTE_LOG_ERR(
            "Could not find interface in device introspection (%s).", base_event.interface_name);
        return;
    }

    astarte_result_t ares = astarte_validation_set_property(interface, base_event.path, data);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Server property data validation failed.");
        return;
    }

#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
    ares = astarte_storage_property_store(&device->caching, base_event.interface_name,
        base_event.path, interface->major_version, data);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed storing the server property.");
    }
#endif

    if (device->property_set_cbk) {
        astarte_device_property_set_event_t set_event = {
            .base_event = base_event,
            .data = data,
        };
        device->property_set_cbk(set_event);
    } else {
        ASTARTE_LOG_ERR("Set property received, but no callback configured.");
    }
}
