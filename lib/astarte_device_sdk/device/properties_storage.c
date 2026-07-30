/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "device/properties_storage.h"

#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE

#include "alloc.h"
#include "astarte_zlib.h"
#include "device/datastreams.h"
#include "mqtt/pubsub.h"
#include "storage/prop.h"
#include <zlib.h>

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
        ctx->interface_name = NULL;
        astarte_free(ctx->path);
        ctx->path = NULL;
        astarte_storage_property_destroy_loaded(ctx->data);
    }
}

ASTARTE_SCOPE_DEFER_DEFINE(cleanup_device_props, device_props_cleanup_ctx_t *);

// Context to hold the purge properties heap allocated variables.
typedef struct
{
    char *decomp_data;
    sys_slist_t *allow_list;
} purge_cleanup_ctx_t;

// Custom purge properties cleanup function.
static void cleanup_purge_ctx(purge_cleanup_ctx_t *ctx)
{
    if (ctx) {
        // free all nodes that were added to the list
        if (ctx->allow_list) {
            sys_snode_t *node = NULL;
            sys_snode_t *safe_node = NULL;
            SYS_SLIST_FOR_EACH_NODE_SAFE(ctx->allow_list, node, safe_node)
            {
                struct allow_node *allow_node = CONTAINER_OF(node, struct allow_node, node);
                astarte_free(allow_node);
                allow_node = NULL;
            }
        }
        // free the main buffer
        astarte_free(ctx->decomp_data);
        ctx->decomp_data = NULL;
    }
}

ASTARTE_SCOPE_DEFER_DEFINE(cleanup_purge_ctx, purge_cleanup_ctx_t *);

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
    scope_var(scoped_char, intr_str)(intr_str_size);
    if (intr_str_size != 0) {
        if (!intr_str) {
            ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
            return ASTARTE_RESULT_OUT_OF_MEMORY;
        }

        ares = astarte_storage_property_get_device_string(
            &device->caching, &device->introspection, intr_str, &intr_str_size);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Can't get stored properties string: %s", astarte_result_to_name(ares));
            return ares;
        }
    }

    // Estimate compression result size and payload size
    char *compression_input = intr_str;
    size_t compression_input_len = (compression_input) ? (intr_str_size - 1) : 0;
    uLongf compressed_len = compressBound(compression_input_len);
    size_t payload_size = 4 + compressed_len;

    scope_var(scoped_uint8, payload)(payload_size);
    if (!payload) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        return ASTARTE_RESULT_OUT_OF_MEMORY;
    }
    // Fill the first 32 bits of the payload
    uint32_t raw_len = __builtin_bswap32(compression_input_len);
    memcpy(payload, &raw_len, sizeof(raw_len));
    // Perform the compression and store result in the payload
    int compress_res = astarte_zlib_compress((char unsigned *) &payload[4], &compressed_len,
        (char unsigned *) compression_input, compression_input_len);
    if (compress_res != Z_OK) {
        ASTARTE_LOG_ERR("Error compressing the purge properties message %d.", compress_res);
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }
    // 'astarte_zlib_compress' updates 'compressed_len' to the actual size of the compressed data
    payload_size = 4 + compressed_len;
    // Check if payload is not too large for a MQTT message
    if (payload_size > INT_MAX) {
        ASTARTE_LOG_ERR("Purge properties payload is too long for a single MQTT message.");
        return ASTARTE_RESULT_MQTT_ERROR;
    }

    // Transmit the payload
    const char *topic = device->control_producer_prop_topic;
    const int qos = 2;
    ASTARTE_LOG_INF("Sending purge properties to: '%s', with uncompressed content: '%s'", topic,
        (compression_input) ? compression_input : "");
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
            return ares; // Safely triggers loop-scoped cleanup
        }

        ctx.interface_name = astarte_calloc(interface_name_size, sizeof(char));
        ctx.path = astarte_calloc(path_size, sizeof(char));
        if (!ctx.interface_name || !ctx.path) {
            ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
            return ASTARTE_RESULT_OUT_OF_MEMORY; // Safely triggers loop-scoped cleanup
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
    if (data_len < 4) {
        ASTARTE_LOG_WRN("Payload too small for a purge message.");
        return;
    }

    sys_slist_t allow_list = { 0 };
    sys_slist_init(&allow_list);

    purge_cleanup_ctx_t ctx = { .decomp_data = NULL, .allow_list = &allow_list };
    scope_defer(cleanup_purge_ctx)(&ctx);

    uint32_t raw_len = 0;
    memcpy(&raw_len, data, sizeof(raw_len));
    uLongf decomp_data_len = __builtin_bswap32(raw_len);

    ctx.decomp_data = astarte_calloc(decomp_data_len + 1, sizeof(char));
    if (!ctx.decomp_data) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        return;
    }

    if (decomp_data_len != 0) {
        int uncompress_res = uncompress((char unsigned *) ctx.decomp_data, &decomp_data_len,
            (char unsigned *) data + 4, data_len - 4);
        if (uncompress_res != Z_OK) {
            ASTARTE_LOG_ERR("Decompression error %d.", uncompress_res);
            return;
        }
    }

    ASTARTE_LOG_DBG("Received purge properties: '%s'", ctx.decomp_data);

    // Parse the received message and fill a list of properties
    if (decomp_data_len != 0) {
        char *saveptr = NULL;
        char *property = strtok_r(ctx.decomp_data, ";", &saveptr);
        if (!property) {
            ASTARTE_LOG_ERR("Error parsing the purge property message %s.", ctx.decomp_data);
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
        size_t interface_name_size = 0U;
        size_t path_size = 0U;
        ares = astarte_storage_property_iterator_get(
            &iter, NULL, &interface_name_size, NULL, &path_size);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Properties iterator get error: %s", astarte_result_to_name(ares));
            return;
        }

        scope_var(scoped_char, interface_name)(interface_name_size);
        scope_var(scoped_char, path)(path_size);

        if (!interface_name || !path) {
            ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
            return;
        }

        ares = astarte_storage_property_iterator_get(
            &iter, interface_name, &interface_name_size, path, &path_size);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Properties iterator get error: %s", astarte_result_to_name(ares));
            return;
        }

        // Purge the property if not in the allow list
        purge_server_property(device, &iter, interface_name, path, allow_list);

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
        ares = astarte_device_send_individual_internal(device, interface_name, path, data, NULL);
        ASTARTE_LOG_COND_ERR(ares != ASTARTE_RESULT_OK, "Failed sending cached property: %s",
            astarte_result_to_name(ares));
    }
}

#endif
