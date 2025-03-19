/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "device_rx.h"

#if defined(CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE)
#include <zlib.h>
#endif

#include "bson_deserializer.h"
#include "data_validation.h"
#if defined(CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE)
#include "device_caching.h"
#endif
#include "data_private.h"
#include "interface_private.h"
#include "object_private.h"

#include "log.h"
ASTARTE_LOG_MODULE_REGISTER(device_reception, CONFIG_ASTARTE_DEVICE_SDK_DEVICE_RX_LOG_LEVEL);

#if defined(CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE)
/** @brief Struct used when parsing the received purge properties string into a list. */
struct allow_node
{
    sys_snode_t node;
    const char *property;
};
#endif

/************************************************
 *         Static functions declaration         *
 ***********************************************/

/**
 * @brief Handles an incoming generic control message.
 *
 * @param[in] device Handle to the device instance.
 * @param[in] topic The control message topic.
 * @param[in] data Received data.
 * @param[in] data_len Length of the data (not including NULL terminator).
 */
static void on_control_message(
    astarte_device_handle_t device, const char *topic, const char *data, size_t data_len);
#if defined(CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE)
/**
 * @brief Handles an incoming purge properties control message.
 *
 * @param[in] device Handle to the device instance.
 * @param[in] data Received data.
 * @param[in] data_len Length of the data (not including NULL terminator).
 */
static void on_purge_properties(astarte_device_handle_t device, const char *data, size_t data_len);
/**
 * @brief Purge the all the stored server owned properties that are not contained in the allow list.
 *
 * @note All the properties that do not belong to an interface contained in the introspection will
 * be removed as well.
 *
 * @param[in] introspection Device's introspection to be used to check ownership of each property.
 * @param[in] allow_list Purge properties allow list.
 */
static void purge_server_properties(introspection_t *introspection, sys_slist_t *allow_list);
/**
 * @brief Purge a single stored server owned property if not contained in the allow list.
 *
 * @note If the property does not belong to an interface contained in the introspection it will be
 * removed as well.
 *
 * @param[in] introspection Device's introspection to be used to check ownership of each property.
 * @param[in] interface_name Interface name for the stored property to be removed.
 * @param[in] path Path for the stored property to be removed.
 * @param[in] allow_list Purge properties allow list.
 */
static void purge_server_property(
    introspection_t *introspection, char *interface_name, char *path, sys_slist_t *allow_list);
#endif
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
 * @param[in] base_event Data event containing information regarding the received event.
 * @param[in] data Astarte value received.
 */
static void on_set_property(
    astarte_device_handle_t device, astarte_device_data_event_t base_event, astarte_data_t data);
/**
 * @brief Handles an incoming datastream individual message.
 *
 * @param[in] device Handle to the device instance.
 * @param[in] base_event Data event containing information regarding the received event.
 * @param[in] data Astarte value received.
 */
static void on_datastream_individual(
    astarte_device_handle_t device, astarte_device_data_event_t base_event, astarte_data_t data);
/**
 * @brief Handles an incoming datastream aggregated message.
 *
 * @param[in] device Handle to the device instance.
 * @param[in] base_event Data event containing information regarding the received event.
 * @param[in] entries The object entries received, organized as an array.
 * @param[in] entries_len The number of element in the @p entries array.
 */
static void on_datastream_aggregated(astarte_device_handle_t device,
    astarte_device_data_event_t base_event, astarte_object_entry_t *entries, size_t entries_len);

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

    on_data_message(device, interface_name, path_start, data, data_len);
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static void on_control_message(
    astarte_device_handle_t device, const char *topic, const char *data, size_t data_len)
{
    if (strcmp(topic, device->control_consumer_prop_topic) == 0) {
#if defined(CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE)
        on_purge_properties(device, data, data_len);
#else
        (void) device;
        (void) data;
        (void) data_len;
#endif
    } else {
        ASTARTE_LOG_ERR("Received unrecognized control message: %s.", topic);
    }
}

#if defined(CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE)
static void on_purge_properties(astarte_device_handle_t device, const char *data, size_t data_len)
{
    char *decomp_data = NULL;
    sys_slist_t allow_list = { 0 };
    sys_slist_init(&allow_list);

    uLongf decomp_data_len = __builtin_bswap32(*(uint32_t *) data);

    decomp_data = calloc(decomp_data_len + 1, sizeof(char));
    if (!decomp_data) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        goto exit;
    }

    if (decomp_data_len != 0) {
        int uncompress_res = uncompress((char unsigned *) decomp_data, &decomp_data_len,
            (char unsigned *) data + 4, data_len - 4);
        if (uncompress_res != Z_OK) {
            ASTARTE_LOG_ERR("Decompression error %d.", uncompress_res);
            goto exit;
        }
    }

    ASTARTE_LOG_DBG("Received purge properties: '%s'", decomp_data);

    // Parse the received message and fill a list of properties
    if (decomp_data_len != 0) {
        char *property = strtok(decomp_data, ";");
        if (!property) {
            ASTARTE_LOG_ERR("Error parsing the purge property message %s.", decomp_data);
            goto exit;
        }
        do {
            struct allow_node *allow_node = calloc(1, sizeof(struct allow_node));
            if (!allow_node) {
                ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
                goto exit;
            }
            allow_node->property = property;
            sys_slist_append(&allow_list, &allow_node->node);
            property = strtok(NULL, ";");
        } while (property);
    }

    // Iterate over the stored properties and purge the ones not in the allow list
    purge_server_properties(&device->introspection, &allow_list);

exit:
    sys_snode_t *node = NULL;
    sys_snode_t *safe_node = NULL;
    SYS_SLIST_FOR_EACH_NODE_SAFE(&allow_list, node, safe_node)
    {
        struct allow_node *allow_node = CONTAINER_OF(node, struct allow_node, node);
        free(allow_node);
    }
    free(decomp_data);
}

static void purge_server_properties(introspection_t *introspection, sys_slist_t *allow_list)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    astarte_device_caching_property_iter_t iter = { 0 };
    char *interface_name = NULL;
    char *path = NULL;

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
            goto end;
        }

        ares = astarte_device_caching_property_iterator_get(
            &iter, interface_name, &interface_name_size, path, &path_size);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Properties iterator get error: %s", astarte_result_to_name(ares));
            goto end;
        }

        // Purge the property if not in the allow list
        purge_server_property(introspection, interface_name, path, allow_list);

        free(interface_name);
        interface_name = NULL;
        free(path);
        path = NULL;

        ares = astarte_device_caching_property_iterator_next(&iter);
        if ((ares != ASTARTE_RESULT_OK) && (ares != ASTARTE_RESULT_NOT_FOUND)) {
            ASTARTE_LOG_ERR("Iterator next error: %s", astarte_result_to_name(ares));
            goto end;
        }
    }

end:
    astarte_device_caching_property_iterator_destroy(iter);
    free(interface_name);
    free(path);
}

static void purge_server_property(
    introspection_t *introspection, char *interface_name, char *path, sys_slist_t *allow_list)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    char *property = NULL;

    const astarte_interface_t *interface = introspection_get(introspection, interface_name);
    if (!interface) {
        ASTARTE_LOG_DBG("Purging property from unknown interface: '%s%s'", interface_name, path);
        ares = astarte_device_caching_property_delete(interface_name, path);
        if ((ares != ASTARTE_RESULT_OK) && (ares != ASTARTE_RESULT_NOT_FOUND)) {
            ASTARTE_LOG_COND_ERR(ares != ASTARTE_RESULT_OK,
                "Failed deleting the cached property: %s", astarte_result_to_name(ares));
        }
        goto end;
    }

    if (interface->ownership != ASTARTE_INTERFACE_OWNERSHIP_SERVER) {
        goto end;
    }

    // Concatenate the interface_name and path
    property = calloc(strlen(interface_name) + strlen(path) + 1, sizeof(char));
    if (!property) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        goto end;
    }
    int snprintf_rc
        = snprintf(property, strlen(interface_name) + strlen(path), "%s%s", interface_name, path);
    if (snprintf_rc != strlen(interface_name) + strlen(path)) {
        ASTARTE_LOG_ERR("Error encoding interface name '%s' and path '%s' in a single string.",
            interface_name, path);
        goto end;
    }

    // Iterate over the allow list
    sys_snode_t *node = NULL;
    SYS_SLIST_FOR_EACH_NODE(allow_list, node)
    {
        struct allow_node *allow_node = CONTAINER_OF(node, struct allow_node, node);
        if (strcmp(allow_node->property, property) == 0) {
            goto end;
        }
    }

    ASTARTE_LOG_DBG("Purging property not in allow list: '%s%s'", interface_name, path);
    ares = astarte_device_caching_property_delete(interface_name, path);
    if ((ares != ASTARTE_RESULT_OK) && (ares != ASTARTE_RESULT_NOT_FOUND)) {
        ASTARTE_LOG_COND_ERR(ares != ASTARTE_RESULT_OK, "Failed deleting the cached property: %s",
            astarte_result_to_name(ares));
    }

end:
    free(property);
}
#endif

static void on_data_message(astarte_device_handle_t device, const char *interface_name,
    const char *path, const char *data, size_t data_len)
{
    const astarte_interface_t *interface = introspection_get(
        &device->introspection, interface_name);
    if (!interface) {
        ASTARTE_LOG_ERR("Could not find interface in device introspection (%s).", interface_name);
        return;
    }

    astarte_device_data_event_t base_event = {
        .device = device,
        .interface_name = interface_name,
        .path = path,
        .user_data = device->cbk_user_data,
    };

    if ((interface->type == ASTARTE_INTERFACE_TYPE_PROPERTIES) && (data_len == 0)) {
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

    if (interface->aggregation == ASTARTE_INTERFACE_AGGREGATION_INDIVIDUAL) {
        const astarte_mapping_t *mapping = NULL;
        astarte_result_t ares = astarte_interface_get_mapping_from_path(interface, path, &mapping);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Could not find received mapping in interface %s.", interface_name);
            return;
        }
        astarte_data_t data_deserialized = { 0 };
        ares = astarte_data_deserialize(v_elem, mapping->type, &data_deserialized);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Failed in parsing the received BSON file. Interface: %s, path: %s.",
                interface_name, path);
            return;
        }

        if (interface->type == ASTARTE_INTERFACE_TYPE_PROPERTIES) {
            on_set_property(device, base_event, data_deserialized);
        } else {
            on_datastream_individual(device, base_event, data_deserialized);
        }
        astarte_data_destroy_deserialized(data_deserialized);
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
        on_datastream_aggregated(device, base_event, entries, entries_length);
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
        ASTARTE_LOG_ERR("Server property unset is invalid: %s.", astarte_result_to_name(ares));
        return;
    }

#if defined(CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE)
    ares = astarte_device_caching_property_delete(event.interface_name, event.path);
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

    astarte_result_t ares = data_validation_set_property(interface, base_event.path, data);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Server property data validation failed.");
        return;
    }

#if defined(CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE)
    ares = astarte_device_caching_property_store(
        base_event.interface_name, base_event.path, interface->major_version, data);
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

static void on_datastream_individual(
    astarte_device_handle_t device, astarte_device_data_event_t base_event, astarte_data_t data)
{
    const astarte_interface_t *interface = introspection_get(
        &device->introspection, base_event.interface_name);
    if (!interface) {
        ASTARTE_LOG_ERR(
            "Couldn't find interface in device introspection (%s).", base_event.interface_name);
        return;
    }

    astarte_result_t ares
        = data_validation_individual_datastream(interface, base_event.path, data, NULL);
    // TODO: remove this exception when the following issue is resolved:
    // https://github.com/astarte-platform/astarte/issues/938
    if (ares == ASTARTE_RESULT_MAPPING_EXPLICIT_TIMESTAMP_REQUIRED) {
        ASTARTE_LOG_WRN("Received an individual datastream with missing explicit timestamp.");
    } else if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Server individual datastream data validation failed.");
        return;
    }

    if (device->datastream_individual_cbk) {
        astarte_device_datastream_individual_event_t event = {
            .base_event = base_event,
            .data = data,
        };
        device->datastream_individual_cbk(event);
    } else {
        ASTARTE_LOG_ERR("Datastream individual received, but no callback configured.");
    }
}

static void on_datastream_aggregated(astarte_device_handle_t device,
    astarte_device_data_event_t base_event, astarte_object_entry_t *entries, size_t entries_len)
{
    const astarte_interface_t *interface = introspection_get(
        &device->introspection, base_event.interface_name);
    if (!interface) {
        ASTARTE_LOG_ERR(
            "Couldn't find interface in device introspection (%s).", base_event.interface_name);
        return;
    }

    astarte_result_t ares = data_validation_aggregated_datastream(
        interface, base_event.path, entries, entries_len, NULL);
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
            .base_event = base_event,
            .entries = entries,
            .entries_len = entries_len,
        };
        device->datastream_object_cbk(event);
    } else {
        ASTARTE_LOG_ERR("Datastream object received, but no callback configured.");
    }
}
