/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "device/properties_storage.h"

#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE

#include "alloc.h"
#include "device/datastreams.h"
#include "mqtt/pubsub.h"
#include "storage/prop.h"

#include "log.h"
ASTARTE_LOG_MODULE_DECLARE(astarte_device, CONFIG_ASTARTE_DEVICE_SDK_DEVICE_LOG_LEVEL);

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

/** @brief Struct used when parsing the received purge properties string into a list. */
struct allow_node
{
    sys_snode_t node;
    const char *property;
};

/************************************************
 *         Static functions declaration         *
 ***********************************************/

static void purge_server_properties(astarte_device_handle_t device, sys_slist_t *allow_list);
static void purge_server_property(astarte_device_handle_t device,
    astarte_storage_property_iter_t *iter, char *interface_name, char *path,
    sys_slist_t *allow_list);
static void send_device_owned_property(astarte_device_handle_t device,
    astarte_storage_property_iter_t *iter, const char *interface_name, const char *path,
    uint32_t major, astarte_data_t data);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_device_get_property(astarte_device_handle_t device,
    const char *interface_name, const char *path, astarte_device_property_loader_cbk_t loader_cbk,
    void *user_data)
{
    if (!device || !interface_name || !path || !loader_cbk) {
        ASTARTE_LOG_ERR("Received a NULL reference for a required input parameter");
        return ASTARTE_RESULT_INVALID_PARAM;
    }
    astarte_result_t ares = ASTARTE_RESULT_OK;
    astarte_data_t data = { 0 };
    uint32_t out_major = 0U;
    ares = astarte_storage_property_load(&device->caching, interface_name, path, &out_major, &data);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed loading the requested property from storage");
        return ares;
    }

    astarte_device_property_loader_event_t event = { .device = device,
        .interface_name = interface_name,
        .path = path,
        .data = data,
        .user_data = user_data };
    loader_cbk(event);

    astarte_storage_property_destroy_loaded(data);
    return ares;
}

astarte_result_t astarte_device_properties_send_purge(astarte_device_handle_t device)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    char *intr_str = NULL;

    size_t intr_str_size = 0U;
    ares = astarte_storage_property_get_device_string(
        &device->caching, &device->introspection, NULL, &intr_str_size);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Error getting stored properties string: %s", astarte_result_to_name(ares));
        goto exit;
    }
    if (intr_str_size != 0) {
        intr_str = astarte_calloc(intr_str_size, sizeof(char));
        if (!intr_str) {
            ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
            ares = ASTARTE_RESULT_OUT_OF_MEMORY;
            goto exit;
        }

        ares = astarte_storage_property_get_device_string(
            &device->caching, &device->introspection, intr_str, &intr_str_size);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Can't get stored properties string: %s", astarte_result_to_name(ares));
            goto exit;
        }
    } else {
        const size_t header_size = 4U;
        intr_str = astarte_calloc(header_size, sizeof(char));
        if (!intr_str) {
            ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
            ares = ASTARTE_RESULT_OUT_OF_MEMORY;
            goto exit;
        }
        intr_str_size = header_size;
    }

    // Transmit the payload
    const char *topic = device->control_producer_prop_topic;
    const int qos = 2;
    ASTARTE_LOG_INF("Sending purge properties to: '%s', with uncompressed content: '%s'", topic,
        (intr_str) ? intr_str : "");
    astarte_mqtt_publish(&device->astarte_mqtt, topic, intr_str, intr_str_size, qos, NULL);

exit:
    astarte_free(intr_str);
    return ares;
}

astarte_result_t astarte_device_properties_send_device_owned(astarte_device_handle_t device)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    astarte_storage_property_iter_t iter = { 0 };
    char *interface_name = NULL;
    char *path = NULL;
    astarte_data_t data = { 0 };

    ares = astarte_storage_property_iterator_new(&device->caching, &iter);
    if ((ares != ASTARTE_RESULT_OK) && (ares != ASTARTE_RESULT_NOT_FOUND)) {
        ASTARTE_LOG_ERR("Properties iterator init failed: %s", astarte_result_to_name(ares));
        goto end;
    }

    while (ares != ASTARTE_RESULT_NOT_FOUND) {
        size_t interface_name_size = 0U;
        size_t path_size = 0U;
        ares = astarte_storage_property_iterator_get(
            &iter, NULL, &interface_name_size, NULL, &path_size);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Properties iterator get error: %s", astarte_result_to_name(ares));
            goto end;
        }

        // Allocate space for the name and path
        interface_name = astarte_calloc(interface_name_size, sizeof(char));
        path = astarte_calloc(path_size, sizeof(char));
        if (!interface_name || !path) {
            ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
            ares = ASTARTE_RESULT_OUT_OF_MEMORY;
            goto end;
        }

        ares = astarte_storage_property_iterator_get(
            &iter, interface_name, &interface_name_size, path, &path_size);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Properties iterator get error: %s", astarte_result_to_name(ares));
            goto end;
        }

        uint32_t major = 0U;
        ares = astarte_storage_property_load(&device->caching, interface_name, path, &major, &data);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Properties load property error: %s", astarte_result_to_name(ares));
            goto end;
        }

        send_device_owned_property(device, &iter, interface_name, path, major, data);

        astarte_free(interface_name);
        interface_name = NULL;
        astarte_free(path);
        path = NULL;
        astarte_storage_property_destroy_loaded(data);
        data = (astarte_data_t){ 0 };

        ares = astarte_storage_property_iterator_next(&iter);
        if ((ares != ASTARTE_RESULT_OK) && (ares != ASTARTE_RESULT_NOT_FOUND)) {
            ASTARTE_LOG_ERR("Iterator next error: %s", astarte_result_to_name(ares));
            goto end;
        }
    }

end:
    // Free all data
    astarte_free(interface_name);
    astarte_free(path);
    astarte_storage_property_destroy_loaded(data);
    return (ares == ASTARTE_RESULT_NOT_FOUND) ? ASTARTE_RESULT_OK : ares;
}

void astarte_device_properties_handle_purge(
    astarte_device_handle_t device, const char *data, size_t data_len)
{
    char *data_copy = NULL;
    sys_slist_t allow_list = { 0 };
    sys_slist_init(&allow_list);

    ASTARTE_LOG_DBG("Received purge properties: '%s'", data);

    // Parse the received message and fill a list of properties
    if (data_len != 0) {
        // Allocate a mutable copy, adding 1 byte for the null terminator
        data_copy = astarte_calloc(data_len + 1, sizeof(char));
        if (!data_copy) {
            ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
            goto exit;
        }
        memcpy(data_copy, data, data_len);

        char *saveptr = NULL;

        // Tokenize the mutable copy instead of the const original
        char *property = strtok_r(data_copy, ";", &saveptr);
        if (!property) {
            ASTARTE_LOG_ERR("Error parsing the purge property message %s.", data);
            goto exit;
        }
        do {
            struct allow_node *allow_node = astarte_calloc(1, sizeof(struct allow_node));
            if (!allow_node) {
                ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
                goto exit;
            }
            allow_node->property = property;
            sys_slist_append(&allow_list, &allow_node->node);
            property = strtok_r(NULL, ";", &saveptr);
        } while (property);
    }

    // Iterate over the stored properties and purge the ones not in the allow list
    purge_server_properties(device, &allow_list);

exit:
    sys_snode_t *node = NULL;
    sys_snode_t *safe_node = NULL;
    SYS_SLIST_FOR_EACH_NODE_SAFE(&allow_list, node, safe_node)
    {
        struct allow_node *allow_node = CONTAINER_OF(node, struct allow_node, node);
        astarte_free(allow_node);
    }
    astarte_free(data_copy);
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static void purge_server_properties(astarte_device_handle_t device, sys_slist_t *allow_list)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    astarte_storage_property_iter_t iter = { 0 };
    char *interface_name = NULL;
    char *path = NULL;

    ares = astarte_storage_property_iterator_new(&device->caching, &iter);
    if ((ares != ASTARTE_RESULT_OK) && (ares != ASTARTE_RESULT_NOT_FOUND)) {
        ASTARTE_LOG_ERR("Properties iterator init failed: %s", astarte_result_to_name(ares));
        goto end;
    }

    while (ares != ASTARTE_RESULT_NOT_FOUND) {
        size_t interface_name_size = 0U;
        size_t path_size = 0U;
        ares = astarte_storage_property_iterator_get(
            &iter, NULL, &interface_name_size, NULL, &path_size);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Properties iterator get error: %s", astarte_result_to_name(ares));
            goto end;
        }

        // Allocate space for the name and path
        interface_name = astarte_calloc(interface_name_size, sizeof(char));
        path = astarte_calloc(path_size, sizeof(char));
        if (!interface_name || !path) {
            ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
            goto end;
        }

        ares = astarte_storage_property_iterator_get(
            &iter, interface_name, &interface_name_size, path, &path_size);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Properties iterator get error: %s", astarte_result_to_name(ares));
            goto end;
        }

        // Purge the property if not in the allow list
        purge_server_property(device, &iter, interface_name, path, allow_list);

        astarte_free(interface_name);
        interface_name = NULL;
        astarte_free(path);
        path = NULL;

        ares = astarte_storage_property_iterator_next(&iter);
        if ((ares != ASTARTE_RESULT_OK) && (ares != ASTARTE_RESULT_NOT_FOUND)) {
            ASTARTE_LOG_ERR("Iterator next error: %s", astarte_result_to_name(ares));
            goto end;
        }
    }

end:
    astarte_free(interface_name);
    astarte_free(path);
}

static void purge_server_property(astarte_device_handle_t device,
    astarte_storage_property_iter_t *iter, char *interface_name, char *path,
    sys_slist_t *allow_list)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    char *property = NULL;

    const astarte_interface_t *interface = introspection_get(
        &device->introspection, interface_name);
    if (!interface) {
        ASTARTE_LOG_DBG("Purging property from unknown interface: '%s%s'", interface_name, path);
        ares = astarte_storage_property_iterator_delete(iter);
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
    property = astarte_calloc(strlen(interface_name) + strlen(path) + 1, sizeof(char));
    if (!property) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        goto end;
    }
    int snprintf_rc = snprintf(
        property, strlen(interface_name) + strlen(path) + 1, "%s%s", interface_name, path);
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

    ares = astarte_storage_property_iterator_delete(iter);
    if ((ares != ASTARTE_RESULT_OK) && (ares != ASTARTE_RESULT_NOT_FOUND)) {
        ASTARTE_LOG_COND_ERR(ares != ASTARTE_RESULT_OK, "Failed deleting the cached property: %s",
            astarte_result_to_name(ares));
    }

end:
    astarte_free(property);
}

static void send_device_owned_property(astarte_device_handle_t device,
    astarte_storage_property_iter_t *iter, const char *interface_name, const char *path,
    uint32_t major, astarte_data_t data)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    const astarte_interface_t *interface = introspection_get(
        &device->introspection, interface_name);
    if ((!interface) || (interface->major_version != major)) {
        ASTARTE_LOG_DBG("Removing property from storage: '%s%s'", interface_name, path);
        ares = astarte_storage_property_iterator_delete(iter);
        if ((ares != ASTARTE_RESULT_OK) && (ares != ASTARTE_RESULT_NOT_FOUND)) {
            ASTARTE_LOG_COND_ERR(ares != ASTARTE_RESULT_OK,
                "Failed deleting the cached property: %s", astarte_result_to_name(ares));
        }
        return;
    }

    if (interface->ownership == ASTARTE_INTERFACE_OWNERSHIP_DEVICE) {
        ares = astarte_device_send_individual(device, interface_name, path, data, NULL);
        ASTARTE_LOG_COND_ERR(ares != ASTARTE_RESULT_OK, "Failed sending cached property: %s",
            astarte_result_to_name(ares));
    }
}

#endif
