/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "device/datastreams.h"

#include "alloc.h"
#include "bson/deserializer.h"
#include "bson/serializer.h"
#include "data/deserialize.h"
#include "data/serialize.h"
#include "device/dispatcher.h"
#include "interface_private.h"
#include "object_private.h"
#include "validation.h"

#include "log.h"
ASTARTE_LOG_MODULE_DECLARE(astarte_device, CONFIG_ASTARTE_DEVICE_SDK_DEVICE_LOG_LEVEL);

/************************************************
 *         Static functions declaration         *
 ***********************************************/

static astarte_result_t serialize_individual_payload(
    astarte_bson_serializer_t *bson, astarte_data_t data, const int64_t *timestamp);
static astarte_result_t serialize_aggregated_payload(astarte_bson_serializer_t *outer_bson,
    astarte_object_entry_t *entries, size_t entries_len, const int64_t *timestamp);
static astarte_result_t on_datastream_individual(
    astarte_device_handle_t device, astarte_device_data_event_t base_event, astarte_data_t data);
static astarte_result_t on_datastream_aggregated(astarte_device_handle_t device,
    astarte_device_data_event_t base_event, astarte_object_entry_t *entries, size_t entries_len);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_device_send_individual(astarte_device_handle_t device,
    const char *interface_name, const char *path, astarte_data_t data, const int64_t *timestamp)
{
    astarte_bson_serializer_t bson = { 0 };
    astarte_result_t ares = ASTARTE_RESULT_OK;

    if (!device || !interface_name || !path) {
        ASTARTE_LOG_ERR("Received a NULL reference for a required input parameter");
        ares = ASTARTE_RESULT_INVALID_PARAM;
        goto exit;
    }

    const astarte_interface_t *interface = introspection_get(
        &device->introspection, interface_name);
    if (!interface) {
        ASTARTE_LOG_ERR("Couldn't find interface in device introspection (%s)", interface_name);
        ares = ASTARTE_RESULT_INTERFACE_NOT_FOUND;
        goto exit;
    }

    const astarte_mapping_t *mapping = NULL;
    ares = astarte_interface_get_mapping_from_path(interface, path, &mapping);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Could not find mapping for path %s in interface %s", path, interface_name);
        goto exit;
    }

    ares = astarte_validation_individual_datastream(interface, path, data, timestamp);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Device individual data validation failed");
        goto exit;
    }

    int qos = 0;
    ares = astarte_interface_get_qos(interface, path, &qos);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed getting QoS for individual data streaming");
        goto exit;
    }

    if (device->connection_state != DEVICE_CONNECTED) {
        if (mapping->retention == ASTARTE_MAPPING_RETENTION_DISCARD) {
            ASTARTE_LOG_WRN("Device disconnected. Discarding message for %s%s (retention: DISCARD)",
                interface_name, path);
            goto exit;
        }
    }

    ares = serialize_individual_payload(&bson, data, timestamp);
    if (ares != ASTARTE_RESULT_OK) {
        goto exit;
    }

    int data_ser_len = 0;
    const void *data_ser = astarte_bson_serializer_get_serialized(&bson, &data_ser_len);
    if (!data_ser) {
        ASTARTE_LOG_ERR("Failed getting serialized BSON");
        ares = ASTARTE_RESULT_BSON_SERIALIZER_ERROR;
        goto exit;
    }
    if (data_ser_len < 0) {
        ASTARTE_LOG_ERR("BSON document is too long for MQTT publish");
        ASTARTE_LOG_ERR("Interface: %s, path: %s", interface_name, path);
        ares = ASTARTE_RESULT_BSON_SERIALIZER_ERROR;
        goto exit;
    }

    struct astarte_device_transmission_queue_msg queue_msg = {
        .interface_name = (char *) interface_name,
        .path = (char *) path,
        .payload = (void *) data_ser,
        .payload_len = data_ser_len,
        .qos = qos,
        .retention = mapping->retention,
    };
    ares = astarte_transmission_queue_insert(&device->transmission_queue, &queue_msg);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed inserting message in transission queue");
    }

exit:
    astarte_bson_serializer_destroy(&bson);
    return ares;
}

astarte_result_t astarte_device_send_object(astarte_device_handle_t device,
    const char *interface_name, const char *path, astarte_object_entry_t *entries,
    size_t entries_len, const int64_t *timestamp)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    astarte_bson_serializer_t outer_bson = { 0 };

    if (!device || !interface_name || !path || !entries) {
        ASTARTE_LOG_ERR("Received a NULL reference for a required input parameter.");
        ares = ASTARTE_RESULT_INVALID_PARAM;
        goto exit;
    }

    const astarte_interface_t *interface = introspection_get(
        &device->introspection, interface_name);
    if (!interface) {
        ASTARTE_LOG_ERR("Couldn't find interface in device introspection (%s).", interface_name);
        ares = ASTARTE_RESULT_INTERFACE_NOT_FOUND;
        goto exit;
    }

    if (interface->mappings_length != entries_len) {
        ASTARTE_LOG_ERR("Incomplete aggregated datastream (%s/%s).", interface->name, path);
        ares = ASTARTE_RESULT_INCOMPLETE_AGGREGATION_OBJECT;
        goto exit;
    }

    // All mappings are required to have the same retention policy, so we can check the first one.
    const astarte_mapping_t *mapping = NULL;
    astarte_object_entry_t *entry = &entries[0];
    ares = astarte_interface_get_mapping_from_paths(interface, path, entry->path, &mapping);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Can't find mapping in interface %s for path %s/%s.", interface->name, path,
            entry->path);
        goto exit;
    }

    ares = astarte_validation_aggregated_datastream(
        interface, path, entries, entries_len, timestamp);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Device aggregated data validation failed.");
        goto exit;
    }

    if (device->connection_state != DEVICE_CONNECTED) {
        if (mapping->retention == ASTARTE_MAPPING_RETENTION_DISCARD) {
            ASTARTE_LOG_WRN("Device disconnected. Discarding message for %s%s (retention: DISCARD)",
                interface_name, path);
            goto exit;
        }
    }

    int qos = 0;
    ares = astarte_interface_get_qos(interface, NULL, &qos);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed getting QoS for aggregated data streaming.");
        goto exit;
    }

    ares = serialize_aggregated_payload(&outer_bson, entries, entries_len, timestamp);
    if (ares != ASTARTE_RESULT_OK) {
        goto exit;
    }

    int len = 0;
    const void *data = astarte_bson_serializer_get_serialized(&outer_bson, &len);
    if (!data || len < 0) {
        ASTARTE_LOG_ERR("BSON document is too long or invalid for MQTT publish.");
        ASTARTE_LOG_ERR("Interface: %s, path: %s", interface_name, path);
        ares = ASTARTE_RESULT_BSON_SERIALIZER_ERROR;
        goto exit;
    }

    struct astarte_device_transmission_queue_msg queue_msg = {
        .interface_name = (char *) interface_name,
        .path = (char *) path,
        .payload = (void *) data,
        .payload_len = len,
        .qos = qos,
        .retention = mapping->retention,
    };
    ares = astarte_transmission_queue_insert(&device->transmission_queue, &queue_msg);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed inserting message in transmission queue");
    }

exit:
    astarte_bson_serializer_destroy(&outer_bson);

    return ares;
}

void astarte_device_datastreams_handle_incoming(astarte_device_handle_t device,
    const astarte_interface_t *interface, const char *path, const char *data, size_t data_len)
{
    astarte_device_data_event_t base_event = {
        .device = device,
        .interface_name = interface->name,
        .path = path,
    };

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
            ASTARTE_LOG_ERR("Could not find received mapping in interface %s.", interface->name);
            return;
        }
        astarte_data_t data_deserialized = { 0 };
        ares = astarte_data_deserialize(v_elem, mapping->type, &data_deserialized);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Failed in parsing the received BSON file. Interface: %s, path: %s.",
                interface->name, path);
            return;
        }

        ares = on_datastream_individual(device, base_event, data_deserialized);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR(
                "Failed in handling the received individual datastream. Interface: %s, path: %s.",
                interface->name, path);
            astarte_data_destroy_deserialized(data_deserialized);
        }
    } else {
        astarte_object_entries_ctx_t cleanup_ctx = { .entries = NULL, .length = 0 };
        scope_defer(astarte_cleanup_object_entries)(&cleanup_ctx);

        astarte_result_t ares = astarte_object_entries_deserialize(
            v_elem, interface, path, &cleanup_ctx.entries, &cleanup_ctx.length);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Failed in parsing the received BSON file. Interface: %s, path: %s.",
                interface->name, path);
            return;
        }

        ares
            = on_datastream_aggregated(device, base_event, cleanup_ctx.entries, cleanup_ctx.length);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR(
                "Failed in handling the received aggregated datastream. Interface: %s, path: %s.",
                interface->name, path);
            return;
        }
        // Disarm automatic cleanup
        cleanup_ctx.entries = NULL;
        cleanup_ctx.length = 0;
    }
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static astarte_result_t serialize_individual_payload(
    astarte_bson_serializer_t *bson, astarte_data_t data, const int64_t *timestamp)
{
    astarte_result_t ares = astarte_bson_serializer_init(bson);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Could not initialize the BSON serializer");
        return ares;
    }

    ares = astarte_data_serialize(bson, "v", data);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed serializing data in BSON");
        return ares;
    }

    if (timestamp) {
        ares = astarte_bson_serializer_append_datetime(bson, "t", *timestamp);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Failed adding datetime to the BSON");
            return ares;
        }
    }

    ares = astarte_bson_serializer_append_end_of_document(bson);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed appending end of document to BSON");
    }

    return ares;
}

static astarte_result_t serialize_aggregated_payload(astarte_bson_serializer_t *outer_bson,
    astarte_object_entry_t *entries, size_t entries_len, const int64_t *timestamp)
{
    astarte_bson_serializer_t inner_bson = { 0 };
    astarte_result_t ares = ASTARTE_RESULT_OK;

    ares = astarte_bson_serializer_init(outer_bson);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Could not initialize the outer bson serializer");
        goto exit;
    }

    ares = astarte_bson_serializer_init(&inner_bson);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Could not initialize the inner bson serializer");
        goto exit;
    }

    ares = astarte_object_entries_serialize(&inner_bson, entries, entries_len);
    if (ares != ASTARTE_RESULT_OK) {
        goto exit;
    }

    ares = astarte_bson_serializer_append_end_of_document(&inner_bson);
    if (ares != ASTARTE_RESULT_OK) {
        goto exit;
    }

    int inner_len = 0;
    const void *inner_data = astarte_bson_serializer_get_serialized(&inner_bson, &inner_len);
    if (!inner_data || inner_len < 0) {
        ASTARTE_LOG_ERR("Error during BSON serialization of inner document");
        ares = ASTARTE_RESULT_BSON_SERIALIZER_ERROR;
        goto exit;
    }

    ares = astarte_bson_serializer_append_document(outer_bson, "v", inner_data);
    if (ares != ASTARTE_RESULT_OK) {
        goto exit;
    }

    if (timestamp) {
        ares = astarte_bson_serializer_append_datetime(outer_bson, "t", *timestamp);
        if (ares != ASTARTE_RESULT_OK) {
            goto exit;
        }
    }

    ares = astarte_bson_serializer_append_end_of_document(outer_bson);

exit:
    astarte_bson_serializer_destroy(&inner_bson);
    return ares;
}

static astarte_result_t on_datastream_individual(
    astarte_device_handle_t device, astarte_device_data_event_t base_event, astarte_data_t data)
{
    const astarte_interface_t *interface = introspection_get(
        &device->introspection, base_event.interface_name);
    if (!interface) {
        ASTARTE_LOG_ERR(
            "Couldn't find interface in device introspection (%s).", base_event.interface_name);
        return ASTARTE_RESULT_INTERFACE_NOT_FOUND;
    }

    astarte_result_t ares
        = astarte_validation_individual_datastream(interface, base_event.path, data, NULL);
    // TODO: remove this exception when the following issue is resolved:
    // https://github.com/astarte-platform/astarte/issues/938
    if (ares == ASTARTE_RESULT_MAPPING_EXPLICIT_TIMESTAMP_REQUIRED) {
        ASTARTE_LOG_WRN("Received an individual datastream with missing explicit timestamp.");
    } else if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Server individual datastream data validation failed.");
        return ares;
    }

    astarte_device_event_t dev_event = { .type = ASTARTE_DEVICE_EVENT_DATASTREAM_INDIVIDUAL,
        .data.datastream_individual = {
            .base_event = base_event,
            .data = data,
        } };

    if (k_msgq_put(&device->event_queue, &dev_event, K_NO_WAIT) != 0) {
        ASTARTE_LOG_ERR("Event queue full. Dropping incoming individual datastream.");
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }
    return ASTARTE_RESULT_OK;
}

static astarte_result_t on_datastream_aggregated(astarte_device_handle_t device,
    astarte_device_data_event_t base_event, astarte_object_entry_t *entries, size_t entries_len)
{
    const astarte_interface_t *interface = introspection_get(
        &device->introspection, base_event.interface_name);
    if (!interface) {
        ASTARTE_LOG_ERR(
            "Couldn't find interface in device introspection (%s).", base_event.interface_name);
        return ASTARTE_RESULT_INTERFACE_NOT_FOUND;
    }

    astarte_result_t ares = astarte_validation_aggregated_datastream(
        interface, base_event.path, entries, entries_len, NULL);
    // TODO: remove this exception when the following issue is resolved:
    // https://github.com/astarte-platform/astarte/issues/938
    if (ares == ASTARTE_RESULT_MAPPING_EXPLICIT_TIMESTAMP_REQUIRED) {
        ASTARTE_LOG_WRN("Received an aggregated datastream with missing explicit timestamp.");
    } else if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Server aggregated data validation failed.");
        return ares;
    }

    astarte_device_event_t dev_event = { .type = ASTARTE_DEVICE_EVENT_DATASTREAM_OBJECT,
        .data.datastream_object = {
            .base_event = base_event,
            .entries = entries,
            .entries_len = entries_len,
        } };

    if (k_msgq_put(&device->event_queue, &dev_event, K_NO_WAIT) != 0) {
        ASTARTE_LOG_ERR("Event queue full. Dropping incoming object datastream.");
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }
    return ASTARTE_RESULT_OK;
}
