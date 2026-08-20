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

// Context to hold the device properties heap allocated variables.
typedef struct
{
    char *interface_name;
    char *path;
    astarte_data_t data;
} device_props_cleanup_ctx_t;

// Device properties cleanup function
static void cleanup_device_props(device_props_cleanup_ctx_t *ctx)
{
    if (ctx) {
        astarte_free(ctx->interface_name);
        astarte_free(ctx->path);
        astarte_storage_property_destroy_loaded(ctx->data);
        memset(ctx, 0, sizeof(device_props_cleanup_ctx_t));
    }
}

ASTARTE_SCOPE_DEFER_DEFINE(cleanup_device_props, device_props_cleanup_ctx_t *);

// Custom purge properties cleanup function.
static void cleanup_allow_list(sys_slist_t *allow_list)
{
    if (allow_list) {
        sys_snode_t *node = NULL;
        sys_snode_t *safe_node = NULL;
        SYS_SLIST_FOR_EACH_NODE_SAFE(allow_list, node, safe_node)
        {
            struct allow_node *allow_node = CONTAINER_OF(node, struct allow_node, node);
            astarte_free(allow_node);
        }
    }
}

ASTARTE_SCOPE_DEFER_DEFINE(cleanup_allow_list, sys_slist_t *);

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

    size_t intr_str_size = 0U;
    ares = astarte_storage_property_get_device_string(
        &device->caching, &device->introspection, NULL, &intr_str_size);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Error getting stored properties string: %s", astarte_result_to_name(ares));
        return ares;
    }

    // When there is no property to purge, send 4 zero bytes to indicate an empty purge message
    size_t payload_size = (intr_str_size != 0) ? intr_str_size : 4U;
    scope_var(scoped_char, payload)(payload_size);
    if (!payload) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        return ASTARTE_RESULT_OUT_OF_MEMORY;
    }

    if (intr_str_size != 0) {
        ares = astarte_storage_property_get_device_string(
            &device->caching, &device->introspection, payload, &intr_str_size);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Can't get stored properties string: %s", astarte_result_to_name(ares));
            return ares;
        }
    }

    // Transmit the payload
    const char *topic = device->control_producer_prop_topic;
    const int qos = 2;
    ASTARTE_LOG_INF(
        "Sending purge properties to: '%s', with uncompressed content: '%s'", topic, payload);
    astarte_mqtt_publish(&device->astarte_mqtt, topic, payload, payload_size, qos, NULL);

    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_device_properties_send_device_owned(astarte_device_handle_t device)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    astarte_storage_property_iter_t iter = { 0 };

    ares = astarte_storage_property_iterator_new(&device->caching, &iter);
    if ((ares != ASTARTE_RESULT_OK) && (ares != ASTARTE_RESULT_NOT_FOUND)) {
        ASTARTE_LOG_ERR("Properties iterator init failed: %s", astarte_result_to_name(ares));
        return ares;
    }

    while (ares != ASTARTE_RESULT_NOT_FOUND) {
        device_props_cleanup_ctx_t ctx = { 0 };
        scope_defer(cleanup_device_props)(&ctx);

        size_t interface_name_size = 0U;
        size_t path_size = 0U;
        ares = astarte_storage_property_iterator_get(
            &iter, NULL, &interface_name_size, NULL, &path_size);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Properties iterator get error: %s", astarte_result_to_name(ares));
            return ares;
        }

        ctx.interface_name = astarte_calloc(interface_name_size, sizeof(char));
        ctx.path = astarte_calloc(path_size, sizeof(char));
        if (!ctx.interface_name || !ctx.path) {
            ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
            return ASTARTE_RESULT_OUT_OF_MEMORY;
        }

        ares = astarte_storage_property_iterator_get(
            &iter, ctx.interface_name, &interface_name_size, ctx.path, &path_size);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Properties iterator get error: %s", astarte_result_to_name(ares));
            return ares;
        }

        uint32_t major = 0U;
        ares = astarte_storage_property_load(
            &device->caching, ctx.interface_name, ctx.path, &major, &ctx.data);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Properties load property error: %s", astarte_result_to_name(ares));
            return ares;
        }

        send_device_owned_property(device, &iter, ctx.interface_name, ctx.path, major, ctx.data);

        ares = astarte_storage_property_iterator_next(&iter);
        if ((ares != ASTARTE_RESULT_OK) && (ares != ASTARTE_RESULT_NOT_FOUND)) {
            ASTARTE_LOG_ERR("Iterator next error: %s", astarte_result_to_name(ares));
            return ares;
        }
    }

    return (ares == ASTARTE_RESULT_NOT_FOUND) ? ASTARTE_RESULT_OK : ares;
}

void astarte_device_properties_handle_purge(
    astarte_device_handle_t device, const char *data, size_t data_len)
{
    sys_slist_t allow_list = { 0 };
    sys_slist_init(&allow_list);
    scope_defer(cleanup_allow_list)(&allow_list);

    // The data received through MQTT is not null-terminated and could be null
    ASTARTE_LOG_DBG("Received purge properties: '%.*s'", (int) data_len, data ? data : "");

    // Allocate a mutable copy, adding 1 byte for the null terminator
    scope_var(scoped_char, data_copy)(data_len + 1);
    if (!data_copy) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        return;
    }

    // Parse the received message and fill a list of properties
    if (data_len != 0) {
        memcpy(data_copy, data, data_len);

        char *saveptr = NULL;
        char *property = strtok_r(data_copy, ";", &saveptr);
        if (!property) {
            ASTARTE_LOG_ERR("Error parsing the purge property message %s.", data_copy);
            return;
        }
        do {
            struct allow_node *allow_node = astarte_calloc(1, sizeof(struct allow_node));
            if (!allow_node) {
                ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
                return;
            }
            allow_node->property = property;
            sys_slist_append(&allow_list, &allow_node->node);
            property = strtok_r(NULL, ";", &saveptr);
        } while (property);
    }

    // Iterate over the stored properties and purge the ones not in the allow list
    purge_server_properties(device, &allow_list);
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static void purge_server_properties(astarte_device_handle_t device, sys_slist_t *allow_list)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    astarte_storage_property_iter_t iter = { 0 };

    ares = astarte_storage_property_iterator_new(&device->caching, &iter);
    if ((ares != ASTARTE_RESULT_OK) && (ares != ASTARTE_RESULT_NOT_FOUND)) {
        ASTARTE_LOG_ERR("Properties iterator init failed: %s", astarte_result_to_name(ares));
        return;
    }

    while (ares != ASTARTE_RESULT_NOT_FOUND) {
        device_props_cleanup_ctx_t ctx = { 0 };
        scope_defer(cleanup_device_props)(&ctx);

        size_t interface_name_size = 0U;
        size_t path_size = 0U;
        ares = astarte_storage_property_iterator_get(
            &iter, NULL, &interface_name_size, NULL, &path_size);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Properties iterator get error: %s", astarte_result_to_name(ares));
            return;
        }

        // Allocate on the heap using 1-byte minimum safeguard
        ctx.interface_name
            = astarte_calloc(interface_name_size > 0 ? interface_name_size : 1, sizeof(char));
        ctx.path = astarte_calloc(path_size > 0 ? path_size : 1, sizeof(char));
        if (!ctx.interface_name || !ctx.path) {
            ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
            return;
        }

        ares = astarte_storage_property_iterator_get(
            &iter, ctx.interface_name, &interface_name_size, ctx.path, &path_size);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Properties iterator get error: %s", astarte_result_to_name(ares));
            return;
        }

        // Purge the property if not in the allow list
        purge_server_property(device, &iter, ctx.interface_name, ctx.path, allow_list);

        ares = astarte_storage_property_iterator_next(&iter);
        if ((ares != ASTARTE_RESULT_OK) && (ares != ASTARTE_RESULT_NOT_FOUND)) {
            ASTARTE_LOG_ERR("Iterator next error: %s", astarte_result_to_name(ares));
            return;
        }
    }
}

static void purge_server_property(astarte_device_handle_t device,
    astarte_storage_property_iter_t *iter, char *interface_name, char *path,
    sys_slist_t *allow_list)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    const astarte_interface_t *interface = introspection_get(
        &device->introspection, interface_name);
    if (!interface) {
        ASTARTE_LOG_DBG("Purging property from unknown interface: '%s%s'", interface_name, path);
        ares = astarte_storage_property_iterator_delete(iter);
        if ((ares != ASTARTE_RESULT_OK) && (ares != ASTARTE_RESULT_NOT_FOUND)) {
            ASTARTE_LOG_COND_ERR(ares != ASTARTE_RESULT_OK,
                "Failed deleting the cached property: %s", astarte_result_to_name(ares));
        }
        return;
    }

    if (interface->ownership != ASTARTE_INTERFACE_OWNERSHIP_SERVER) {
        return;
    }

    // Concatenate the interface_name and path
    scope_var(scoped_char, property)(strlen(interface_name) + strlen(path) + 1);
    if (!property) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        return;
    }
    int snprintf_rc = snprintf(
        property, strlen(interface_name) + strlen(path) + 1, "%s%s", interface_name, path);
    if (snprintf_rc != strlen(interface_name) + strlen(path)) {
        ASTARTE_LOG_ERR("Error encoding interface name '%s' and path '%s' in a single string.",
            interface_name, path);
        return;
    }

    // Iterate over the allow list
    sys_snode_t *node = NULL;
    SYS_SLIST_FOR_EACH_NODE(allow_list, node)
    {
        struct allow_node *allow_node = CONTAINER_OF(node, struct allow_node, node);
        if (strcmp(allow_node->property, property) == 0) {
            return;
        }
    }

    ASTARTE_LOG_DBG("Purging property not in allow list: '%s%s'", interface_name, path);

    ares = astarte_storage_property_iterator_delete(iter);
    if ((ares != ASTARTE_RESULT_OK) && (ares != ASTARTE_RESULT_NOT_FOUND)) {
        ASTARTE_LOG_COND_ERR(ares != ASTARTE_RESULT_OK, "Failed deleting the cached property: %s",
            astarte_result_to_name(ares));
    }
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
        ares = astarte_device_set_property(device, interface_name, path, data);
        ASTARTE_LOG_COND_ERR(ares != ASTARTE_RESULT_OK, "Failed sending cached property: %s",
            astarte_result_to_name(ares));
    }
}

#endif
