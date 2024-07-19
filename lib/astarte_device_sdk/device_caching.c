/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "device_caching.h"

#include <stdlib.h>

#include <zephyr/drivers/flash.h>
#include <zephyr/fs/nvs.h>
#include <zephyr/kernel.h>
#include <zephyr/storage/flash_map.h>

#include "individual_private.h"

#include "log.h"
ASTARTE_LOG_MODULE_REGISTER(device_caching, CONFIG_ASTARTE_DEVICE_SDK_DEVICE_CACHING_LOG_LEVEL);

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

#define NVS_PARTITION astarte_partition
#if !FIXED_PARTITION_EXISTS(NVS_PARTITION)
#error "Permanent storage is enabled but 'astarte_partition' flash partition is missing."
#endif // FIXED_PARTITION_EXISTS(NVS_PARTITION)
#define NVS_PARTITION_DEVICE FIXED_PARTITION_DEVICE(NVS_PARTITION)
#define NVS_PARTITION_OFFSET FIXED_PARTITION_OFFSET(NVS_PARTITION)
#define NVS_PARTITION_SIZE FIXED_PARTITION_SIZE(NVS_PARTITION)

#define INTROSPECTION_NAMESPACE "introspection_namespace"
#define INTROSPECTION_KEY "introspection_string"
#define PROPERTIES_NAMESPACE "properties_namespace"

/************************************************
 *         Static functions declaration         *
 ***********************************************/

/**
 * @brief Open a key-value storage namespace.
 *
 * @param[in] namespace Namespace to open.
 * @param[out] kv_storage Uninitialized key-value storage handle.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t open_kv_storage(const char *namespace, astarte_kv_storage_t *kv_storage);
/**
 * @brief Parse BSON file used to store a property
 *
 * @param[in] value BSON file
 * @param[out] out_major Pointer to output major version. Might be NULL, in this case the parameter
 * is ignored.
 * @param[out] individual Pointer to output Astarte individual. May be NULL, in this case
 * the parameter is ignored.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
static astarte_result_t parse_property_bson(
    const char *value, uint32_t *out_major, astarte_individual_t *individual);
/**
 * @brief Append a property to the end of a string.
 *
 * @param[inout] string String to extend.
 * @param[inout] string_size Size of the @p string buffer.
 * @param[in] interface_name Interface name for the property to append.
 * @param[in] path Path for the property to append.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
static astarte_result_t append_property_to_string(
    char *string, size_t string_size, const char *interface_name, const char *path);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_device_caching_introspection_store(const char *intr, size_t intr_size)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    astarte_kv_storage_t kv_storage = { 0 };

    ASTARTE_LOG_DBG("Storing introspection in key-value storage: '%s' (%d).", intr, intr_size);

    ares = open_kv_storage(INTROSPECTION_NAMESPACE, &kv_storage);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Init error for introspection cache: %s.", astarte_result_to_name(ares));
        return ares;
    }

    ares = astarte_kv_storage_insert(&kv_storage, INTROSPECTION_KEY, intr, intr_size);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Error caching introspection: %s.", astarte_result_to_name(ares));
    }

    return ares;
}

astarte_result_t astarte_device_caching_introspection_check(const char *intr, size_t intr_size)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    char *read_intr = NULL;
    size_t read_intr_size = 0;
    astarte_kv_storage_t kv_storage = { 0 };

    ASTARTE_LOG_DBG("Checking stored introspection against new one: '%s' (%d).", intr, intr_size);

    ares = open_kv_storage(INTROSPECTION_NAMESPACE, &kv_storage);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Init error for introspection cache: %s.", astarte_result_to_name(ares));
        goto exit;
    }

    ares = astarte_kv_storage_find(&kv_storage, INTROSPECTION_KEY, NULL, &read_intr_size);
    if (ares == ASTARTE_RESULT_NOT_FOUND) {
        ares = ASTARTE_RESULT_DEVICE_CACHING_OUTDATED_INTROSPECTION;
        goto exit;
    }
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Fetch error for cached introspection: %s.", astarte_result_to_name(ares));
        goto exit;
    }

    if (read_intr_size != intr_size) {
        ares = ASTARTE_RESULT_DEVICE_CACHING_OUTDATED_INTROSPECTION;
        goto exit;
    }

    read_intr = calloc(read_intr_size, sizeof(char));
    if (!read_intr) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto exit;
    }

    ares = astarte_kv_storage_find(&kv_storage, INTROSPECTION_KEY, read_intr, &read_intr_size);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Fetch error for cached introspection: %s.", astarte_result_to_name(ares));
        goto exit;
    }

    if (memcmp(intr, read_intr, MIN(read_intr_size, intr_size)) != 0) {
        ASTARTE_LOG_INF("Found outdated introspection: '%s' (%d).", read_intr, read_intr_size);
        ares = ASTARTE_RESULT_DEVICE_CACHING_OUTDATED_INTROSPECTION;
        goto exit;
    }

exit:
    free(read_intr);

    return ares;
}

astarte_result_t astarte_device_caching_property_store(
    const char *interface_name, const char *path, uint32_t major, astarte_individual_t individual)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    char *key = NULL;
    astarte_bson_serializer_t bson = { 0 };

    astarte_kv_storage_t kv_storage = { 0 };

    ASTARTE_LOG_DBG("Caching property ('%s' - '%s').", interface_name, path);

    ares = open_kv_storage(PROPERTIES_NAMESPACE, &kv_storage);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Init error for property cache: %s.", astarte_result_to_name(ares));
        goto exit;
    }

    // Get the full key interface_name + ';' + path
    size_t key_len = strlen(interface_name) + 1 + strlen(path) + 1;
    key = calloc(key_len, sizeof(char));
    if (!key) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto exit;
    }
    int snprintf_rc = snprintf(key, key_len, "%s;%s", interface_name, path);
    if (snprintf_rc != key_len - 1) {
        ASTARTE_LOG_ERR("Could not create the property key-value storage key.");
        ares = ASTARTE_RESULT_INTERNAL_ERROR;
        goto exit;
    }

    // Serialize the Astarte individual
    ares = astarte_bson_serializer_init(&bson);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Could not initialize the bson serializer");
        goto exit;
    }
    astarte_bson_serializer_append_int32(&bson, "major", *(int32_t *) &major);
    astarte_bson_serializer_append_int64(&bson, "type", (int64_t) individual.tag);
    ares = astarte_individual_serialize(&bson, "data", individual);
    if (ares != ASTARTE_RESULT_OK) {
        goto exit;
    }
    astarte_bson_serializer_append_end_of_document(&bson);

    int data_len = 0;
    void *data = (void *) astarte_bson_serializer_get_serialized(bson, &data_len);
    if (!data) {
        ASTARTE_LOG_ERR("Error during BSON serialization.");
        ares = ASTARTE_RESULT_BSON_SERIALIZER_ERROR;
        goto exit;
    }
    if (data_len < 0) {
        ASTARTE_LOG_ERR("BSON document is too long to be cached.");
        ares = ASTARTE_RESULT_BSON_SERIALIZER_ERROR;
        goto exit;
    }

    ares = astarte_kv_storage_insert(&kv_storage, key, data, data_len);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Error caching property: %s.", astarte_result_to_name(ares));
    }

exit:
    free(key);
    astarte_bson_serializer_destroy(&bson);
    return ares;
}

astarte_result_t astarte_device_caching_property_load(const char *interface_name, const char *path,
    uint32_t *out_major, astarte_individual_t *individual)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    char *key = NULL;
    char *value = NULL;

    ASTARTE_LOG_DBG("Loading cached property ('%s' - '%s').", interface_name, path);

    astarte_kv_storage_t kv_storage = { 0 };
    ares = open_kv_storage(PROPERTIES_NAMESPACE, &kv_storage);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Init error for property cache: %s.", astarte_result_to_name(ares));
        goto exit;
    }

    // Get the full key interface_name + ';' + path
    size_t key_len = strlen(interface_name) + 1 + strlen(path) + 1;
    key = calloc(key_len, sizeof(char));
    if (!key) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto exit;
    }
    int snprintf_rc = snprintf(key, key_len, "%s;%s", interface_name, path);
    if (snprintf_rc != key_len - 1) {
        ASTARTE_LOG_ERR("Could not create the property key-value storage key.");
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto exit;
    }

    size_t value_len = 0;
    ares = astarte_kv_storage_find(&kv_storage, key, NULL, &value_len);
    if (ares != ASTARTE_RESULT_OK) {
        goto exit;
    }

    // Allocate memory for BSON file to read
    value = calloc(value_len, sizeof(char));
    if (!value) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto exit;
    }

    // Get the data from NVS
    ares = astarte_kv_storage_find(&kv_storage, key, value, &value_len);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Could not get property from storage: %s.", astarte_result_to_name(ares));
        goto exit;
    }

    ares = parse_property_bson(value, out_major, individual);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Could not parse data from stroage: %s.", astarte_result_to_name(ares));
    }

exit:
    free(key);
    free(value);
    return ares;
}

void astarte_device_caching_property_destroy_loaded(astarte_individual_t individual)
{
    astarte_individual_destroy_deserialized(individual);
}

astarte_result_t astarte_device_caching_property_delete(
    const char *interface_name, const char *path)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    char *key = NULL;

    ASTARTE_LOG_DBG("Deleting cached property ('%s' - '%s').", interface_name, path);

    astarte_kv_storage_t kv_storage = { 0 };
    ares = open_kv_storage(PROPERTIES_NAMESPACE, &kv_storage);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Init error for property cache: %s.", astarte_result_to_name(ares));
        goto exit;
    }

    // Get the full key interface_name + path
    size_t key_len = strlen(interface_name) + 1 + strlen(path) + 1;
    key = calloc(key_len, sizeof(char));
    if (!key) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto exit;
    }
    int snprintf_rc = snprintf(key, key_len, "%s;%s", interface_name, path);
    if (snprintf_rc != key_len - 1) {
        ASTARTE_LOG_ERR("Could not create the property key-value storage key.");
        ares = ASTARTE_RESULT_INTERNAL_ERROR;
        goto exit;
    }

    ares = astarte_kv_storage_delete(&kv_storage, key);
    if ((ares != ASTARTE_RESULT_OK) && (ares != ASTARTE_RESULT_NOT_FOUND)) {
        ASTARTE_LOG_ERR("Error deleting cached property: %s.", astarte_result_to_name(ares));
    }

exit:
    free(key);
    return ares;
}

astarte_result_t astarte_device_caching_property_iterator_init(
    astarte_device_caching_property_iter_t *iter)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    ares = open_kv_storage(PROPERTIES_NAMESPACE, &iter->kv_storage);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Init error for property cache: %s.", astarte_result_to_name(ares));
        return ares;
    }

    ares = astarte_kv_storage_iterator_init(&iter->kv_storage, &iter->kv_iter);
    if ((ares != ASTARTE_RESULT_OK) && (ares != ASTARTE_RESULT_NOT_FOUND)) {
        ASTARTE_LOG_ERR("Key-value storage iterator init error: %s.", astarte_result_to_name(ares));
    }

    return ares;
}

astarte_result_t astarte_device_caching_property_iterator_next(
    astarte_device_caching_property_iter_t *iter)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    ares = astarte_kv_storage_iterator_next(&iter->kv_iter);
    if ((ares != ASTARTE_RESULT_OK) && (ares != ASTARTE_RESULT_NOT_FOUND)) {
        ASTARTE_LOG_ERR("Key-value storage iterator error: %s.", astarte_result_to_name(ares));
    }

    return ares;
}

astarte_result_t astarte_device_caching_property_iterator_get(
    astarte_device_caching_property_iter_t *iter, char *interface_name, size_t *interface_name_size,
    char *path, size_t *path_size)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    char *key = NULL;

    // Check input parameters
    if (!interface_name_size || !path_size) {
        ASTARTE_LOG_ERR("Parameters interface_name_size and path_size can't be NULL.");
        ares = ASTARTE_RESULT_INVALID_PARAM;
        goto exit;
    }
    if ((!interface_name && path) || (interface_name && !path)) {
        ASTARTE_LOG_ERR("Parameters interface_name and path can only be NULL at the same time.");
        ares = ASTARTE_RESULT_INVALID_PARAM;
        goto exit;
    }

    // Get size of item key
    size_t key_size = 0U;
    ares = astarte_kv_storage_iterator_get(&iter->kv_iter, NULL, &key_size);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Key-value storage iterator error: %s.", astarte_result_to_name(ares));
        goto exit;
    }

    key = calloc(key_size, sizeof(char));
    if (!key) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto exit;
    }

    ares = astarte_kv_storage_iterator_get(&iter->kv_iter, key, &key_size);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Key-value storage iterator error: %s.", astarte_result_to_name(ares));
        goto exit;
    }

    // Split interface name and path
    char *read_interface_name = strtok(key, ";");
    size_t read_interface_name_size = strlen(read_interface_name) + 1;
    char *read_path = strtok(NULL, "\0");
    size_t read_path_size = strlen(read_path) + 1;

    if (!interface_name && !path) {
        *interface_name_size = read_interface_name_size;
        *path_size = read_path_size;
        goto exit;
    }

    if ((*interface_name_size < read_interface_name_size) || (*path_size < read_path_size)) {
        ASTARTE_LOG_ERR("Insufficient buff size in astarte_device_caching_property_iterator_get.");
        ares = ASTARTE_RESULT_INVALID_PARAM;
        goto exit;
    }

    *interface_name_size = read_interface_name_size;
    *path_size = read_path_size;

    int snprintf_rc = snprintf(interface_name, *interface_name_size, "%s", read_interface_name);
    if (snprintf_rc != read_interface_name_size - 1) {
        ASTARTE_LOG_ERR("Could not create the property interface name.");
        ares = ASTARTE_RESULT_INTERNAL_ERROR;
        goto exit;
    }

    snprintf_rc = snprintf(path, *path_size, "%s", read_path);
    if (snprintf_rc != read_path_size - 1) {
        ASTARTE_LOG_ERR("Could not create the property path.");
        ares = ASTARTE_RESULT_INTERNAL_ERROR;
        goto exit;
    }

exit:
    free(key);
    return ares;
}

astarte_result_t astarte_device_caching_property_get_string(char *output, size_t *output_size)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    size_t string_size = 0U;
    char *interface_name = NULL;
    char *path = NULL;

    if (output) {
        *output = '\0';
    }

    astarte_device_caching_property_iter_t iter = { 0 };
    ares = astarte_device_caching_property_iterator_init(&iter);
    if ((ares != ASTARTE_RESULT_OK) && (ares != ASTARTE_RESULT_NOT_FOUND)) {
        ASTARTE_LOG_ERR("Properties iterator init failed: %s", astarte_result_to_name(ares));
        goto error;
    }

    while (ares != ASTARTE_RESULT_NOT_FOUND) {
        size_t interface_name_size = 0U;
        size_t path_size = 0U;
        ares = astarte_device_caching_property_iterator_get(
            &iter, NULL, &interface_name_size, NULL, &path_size);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Properties iterator get error: %s", astarte_result_to_name(ares));
            goto error;
        }

        interface_name = calloc(interface_name_size, sizeof(char));
        path = calloc(path_size, sizeof(char));
        if (!interface_name || !path) {
            ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
            goto error;
        }

        ares = astarte_device_caching_property_iterator_get(
            &iter, interface_name, &interface_name_size, path, &path_size);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Properties iterator get error: %s", astarte_result_to_name(ares));
            goto error;
        }

        // Update the formed string size
        string_size += strlen(interface_name) + strlen(path) + 1;

        // Update the formed string
        if (output) {
            ares = append_property_to_string(output, *output_size, interface_name, path);
            if (ares != ASTARTE_RESULT_OK) {
                ASTARTE_LOG_ERR("Failed extending the string: %s", astarte_result_to_name(ares));
                goto error;
            }
        }

        free(interface_name);
        interface_name = NULL;
        free(path);
        path = NULL;

        ares = astarte_device_caching_property_iterator_next(&iter);
        if ((ares != ASTARTE_RESULT_OK) && (ares != ASTARTE_RESULT_NOT_FOUND)) {
            ASTARTE_LOG_ERR("Iterator next error: %s", astarte_result_to_name(ares));
            goto error;
        }
    }

    *output_size = string_size;
    return ASTARTE_RESULT_OK;

error:
    free(interface_name);
    free(path);
    return ares;
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

astarte_result_t open_kv_storage(const char *namespace, astarte_kv_storage_t *kv_storage)
{
    int flash_rc = 0;
    struct flash_pages_info fp_info = { 0 };

    const struct device *flash_device = NVS_PARTITION_DEVICE;
    if (!device_is_ready(flash_device)) {
        ASTARTE_LOG_ERR("Flash device %s not ready.", flash_device->name);
        return ASTARTE_RESULT_DEVICE_NOT_READY;
    }
    off_t flash_offset = NVS_PARTITION_OFFSET;
    flash_rc = flash_get_page_info_by_offs(flash_device, flash_offset, &fp_info);
    if (flash_rc) {
        ASTARTE_LOG_ERR("Unable to get page info: %d.", flash_rc);
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }

    astarte_kv_storage_cfg_t kv_storage_cfg = {
        .flash_device = flash_device,
        .flash_offset = flash_offset,
        .flash_sector_count = NVS_PARTITION_SIZE / fp_info.size,
        .flash_sector_size = fp_info.size,
    };
    astarte_result_t ares = astarte_kv_storage_init(kv_storage_cfg, namespace, kv_storage);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Error initialize introspection cache: %s.", astarte_result_to_name(ares));
        return ares;
    }

    return ASTARTE_RESULT_OK;
}

static astarte_result_t parse_property_bson(
    const char *value, uint32_t *out_major, astarte_individual_t *individual)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    astarte_bson_document_t full_document = astarte_bson_deserializer_init_doc(value);
    if (out_major) {
        astarte_bson_element_t major_elem = { 0 };
        ares = astarte_bson_deserializer_element_lookup(full_document, "major", &major_elem);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Cannot parse BSON element for major version.");
            return ares;
        }
        int32_t major = astarte_bson_deserializer_element_to_int32(major_elem);
        *out_major = *(uint32_t *) &major;
    }
    if (individual) {
        astarte_bson_element_t type_elem = { 0 };
        ares = astarte_bson_deserializer_element_lookup(full_document, "type", &type_elem);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Cannot parse BSON element for type.");
            return ares;
        }
        astarte_mapping_type_t type
            = (astarte_mapping_type_t) astarte_bson_deserializer_element_to_int64(type_elem);

        astarte_bson_element_t data_elem = { 0 };
        ares = astarte_bson_deserializer_element_lookup(full_document, "data", &data_elem);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Cannot parse BSON element for data.");
            return ares;
        }
        ares = astarte_individual_deserialize(data_elem, type, individual);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Failed in deserializing BSON file.");
            return ares;
        }
    }
    return ares;
}

static astarte_result_t append_property_to_string(
    char *string, size_t string_size, const char *interface_name, const char *path)
{
    size_t string_len = strlen(string);
    if (string_size <= string_len + strlen(";") + strlen(interface_name) + strlen(path)) {
        ASTARTE_LOG_ERR("Insufficient size to extend the string.");
        return ASTARTE_RESULT_INVALID_PARAM;
    }
    if (string_len != 0) {
        strncat(string, ";", string_size - string_len - 1);
        string_len = strlen(string);
    }
    strncat(string, interface_name, string_size - string_len - 1);
    string_len = strlen(string);
    strncat(string, path, string_size - string_len - 1);
    return ASTARTE_RESULT_OK;
}