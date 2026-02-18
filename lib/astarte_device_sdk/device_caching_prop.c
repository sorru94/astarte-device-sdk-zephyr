/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "device_caching.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "data_private.h"

#include "log.h"
ASTARTE_LOG_MODULE_DECLARE(device_caching, CONFIG_ASTARTE_DEVICE_SDK_DEVICE_CACHING_LOG_LEVEL);

/************************************************
 *         Static functions declaration         *
 ***********************************************/

/**
 * @brief Parse BSON file used to store a property
 *
 * @param[in] value BSON file
 * @param[out] out_major Pointer to output major version. Might be NULL, in this case the parameter
 * is ignored.
 * @param[out] data Pointer to output Astarte data. May be NULL, in this case
 * the parameter is ignored.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
static astarte_result_t parse_property_bson(
    const char *value, uint32_t *out_major, astarte_data_t *data);
/**
 * @brief Append a property to the end of the string.
 *
 * @note This function will append the property only if it's device owned and if it's present in
 * the introspection.
 *
 * @param[in,out] handle Pointer to an initialized handle structure.
 * @param[in] introspection Introspection to be used to check ownership for the property.
 * @param[in] interface_name Interface name for the property to append.
 * @param[in] path Path for the property to append.
 * @param[out] str_size Will be set to the size of the extended string.
 * @param[inout] str_buff Buffer containing the string to extend, if NULL it will be ignored.
 * @param[in] str_buff_size Size of the @p str_buff buffer.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
static astarte_result_t append_property_to_string(astarte_device_caching_t *handle,
    introspection_t *introspection, char *interface_name, char *path, size_t *str_size,
    char *str_buff, size_t str_buff_size);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_device_caching_property_store(astarte_device_caching_t *handle,
    const char *interface_name, const char *path, uint32_t major, astarte_data_t data)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    char *key = NULL;
    astarte_bson_serializer_t bson = { 0 };

    if (!handle || !handle->initialized) {
        ASTARTE_LOG_ERR("Device caching handle is uninitialized or NULL.");
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    ASTARTE_LOG_DBG("Caching property ('%s' - '%s').", interface_name, path);

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

    // Serialize the Astarte data
    ares = astarte_bson_serializer_init(&bson);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Could not initialize the bson serializer");
        goto exit;
    }
    ares = astarte_bson_serializer_append_int32(&bson, "major", (int32_t) major);
    if (ares != ASTARTE_RESULT_OK) {
        goto exit;
    }
    ares = astarte_bson_serializer_append_int64(&bson, "type", (int64_t) data.tag);
    if (ares != ASTARTE_RESULT_OK) {
        goto exit;
    }
    ares = astarte_data_serialize(&bson, "data", data);
    if (ares != ASTARTE_RESULT_OK) {
        goto exit;
    }
    ares = astarte_bson_serializer_append_end_of_document(&bson);
    if (ares != ASTARTE_RESULT_OK) {
        goto exit;
    }

    int data_ser_len = 0;
    void *data_ser = (void *) astarte_bson_serializer_get_serialized(&bson, &data_ser_len);
    if (!data_ser) {
        ASTARTE_LOG_ERR("Error during BSON serialization.");
        ares = ASTARTE_RESULT_BSON_SERIALIZER_ERROR;
        goto exit;
    }
    if (data_ser_len < 0) {
        ASTARTE_LOG_ERR("BSON document is too long to be cached.");
        ares = ASTARTE_RESULT_BSON_SERIALIZER_ERROR;
        goto exit;
    }

    ASTARTE_LOG_DBG("Inserting pair in storage. Key: %s", key);
    ares = astarte_kv_storage_insert(&handle->prop_storage, key, data_ser, data_ser_len);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Error caching property: %s.", astarte_result_to_name(ares));
    }

exit:
    free(key);
    astarte_bson_serializer_destroy(&bson);
    return ares;
}

astarte_result_t astarte_device_caching_property_load(astarte_device_caching_t *handle,
    const char *interface_name, const char *path, uint32_t *out_major, astarte_data_t *data)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    char *key = NULL;
    char *value = NULL;

    if (!handle || !handle->initialized) {
        ASTARTE_LOG_ERR("Device caching handle is uninitialized or NULL.");
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    ASTARTE_LOG_DBG("Loading cached property ('%s' - '%s').", interface_name, path);

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

    ASTARTE_LOG_DBG("Searching for pair in storage. Key: '%s'", key);
    size_t value_len = 0;
    ares = astarte_kv_storage_find(&handle->prop_storage, key, NULL, &value_len);
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
    ASTARTE_LOG_DBG("Searching for pair in storage. Key: '%s'", key);
    ares = astarte_kv_storage_find(&handle->prop_storage, key, value, &value_len);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Could not get property from storage: %s.", astarte_result_to_name(ares));
        goto exit;
    }

    ares = parse_property_bson(value, out_major, data);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Could not parse data from storage: %s.", astarte_result_to_name(ares));
    }

exit:
    free(key);
    free(value);
    return ares;
}

void astarte_device_caching_property_destroy_loaded(astarte_data_t data)
{
    astarte_data_destroy_deserialized(data);
}

astarte_result_t astarte_device_caching_property_delete(
    astarte_device_caching_t *handle, const char *interface_name, const char *path)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    char *key = NULL;

    if (!handle || !handle->initialized) {
        ASTARTE_LOG_ERR("Device caching handle is uninitialized or NULL.");
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    ASTARTE_LOG_DBG("Deleting cached property ('%s' - '%s').", interface_name, path);

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

    ASTARTE_LOG_DBG("Deleting pair from storage. Key: %s", key);
    ares = astarte_kv_storage_delete(&handle->prop_storage, key);
    if ((ares != ASTARTE_RESULT_OK) && (ares != ASTARTE_RESULT_NOT_FOUND)) {
        ASTARTE_LOG_ERR("Error deleting cached property: %s.", astarte_result_to_name(ares));
    }

exit:
    free(key);
    return ares;
}

astarte_result_t astarte_device_caching_property_iterator_new(
    astarte_device_caching_t *handle, astarte_device_caching_property_iter_t *iter)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    if (!handle || !handle->initialized) {
        ASTARTE_LOG_ERR("Device caching handle is uninitialized or NULL.");
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    ASTARTE_LOG_DBG("Initializing iterator for key value storage.");
    ares = astarte_kv_storage_iterator_init(&handle->prop_storage, &iter->kv_iter);
    if ((ares != ASTARTE_RESULT_OK) && (ares != ASTARTE_RESULT_NOT_FOUND)) {
        ASTARTE_LOG_ERR("Key-value storage iterator init error: %s.", astarte_result_to_name(ares));
    }

    return ares;
}

astarte_result_t astarte_device_caching_property_iterator_next(
    astarte_device_caching_property_iter_t *iter)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    ASTARTE_LOG_DBG("Advancing iterator for key value storage.");
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
    ASTARTE_LOG_DBG("Getting the key size for the pair pointer by the storage iterator.");
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

    ASTARTE_LOG_DBG("Getting the key data for the pair pointer by the storage iterator.");
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

astarte_result_t astarte_device_caching_property_get_device_string(astarte_device_caching_t *handle,
    introspection_t *introspection, char *output, size_t *output_size)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    astarte_device_caching_property_iter_t iter = { 0 };
    size_t string_size = 0U;
    char *interface_name = NULL;
    char *path = NULL;

    ares = astarte_device_caching_property_iterator_new(handle, &iter);
    if ((ares != ASTARTE_RESULT_OK) && (ares != ASTARTE_RESULT_NOT_FOUND)) {
        ASTARTE_LOG_ERR("Properties iterator init failed: %s", astarte_result_to_name(ares));
        goto error;
    }

    if (output) {
        *output = '\0';
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

        ares = append_property_to_string(
            handle, introspection, interface_name, path, &string_size, output, *output_size);
        if ((ares != ASTARTE_RESULT_OK) && (ares != ASTARTE_RESULT_NOT_FOUND)) {
            ASTARTE_LOG_COND_ERR(ares != ASTARTE_RESULT_OK,
                "Failed appending the property to the string: %s", astarte_result_to_name(ares));
            goto error;
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

static astarte_result_t parse_property_bson(
    const char *value, uint32_t *out_major, astarte_data_t *data)
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
        *out_major = (uint32_t) major;
    }

    if (data) {
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
        ares = astarte_data_deserialize(data_elem, type, data);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Failed in deserializing BSON file.");
            return ares;
        }
    }
    return ares;
}

// Function is still readable for now
// NOLINTNEXTLINE(readability-function-size)
static astarte_result_t append_property_to_string(astarte_device_caching_t *handle,
    introspection_t *introspection, char *interface_name, char *path, size_t *str_size,
    char *str_buff, size_t str_buff_size)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    // Check if property is device owned
    const astarte_interface_t *interface = introspection_get(introspection, interface_name);
    if (!interface) {
        ASTARTE_LOG_DBG("Purge property from unknown interface: '%s%s'", interface_name, path);
        ares = astarte_device_caching_property_delete(handle, interface_name, path);
        if ((ares != ASTARTE_RESULT_OK) && (ares != ASTARTE_RESULT_NOT_FOUND)) {
            ASTARTE_LOG_COND_ERR(ares != ASTARTE_RESULT_OK,
                "Failed deleting the cached property: %s", astarte_result_to_name(ares));
        }
        return ASTARTE_RESULT_NOT_FOUND;
    }

    if (interface->ownership != ASTARTE_INTERFACE_OWNERSHIP_DEVICE) {
        return ares;
    }

    // Update the formed string size
    *str_size = *str_size + strlen(interface_name) + strlen(path) + 1;

    if (!str_buff) {
        return ares;
    }

    if (str_buff_size < *str_size) {
        ASTARTE_LOG_ERR("Insufficient size to extend the string.");
        return ASTARTE_RESULT_INVALID_PARAM;
    }
    // Points to the NULL terminator char in the string
    char *str_buff_end = str_buff + strlen(str_buff);
    // Available number of chars in the buffer (including the NULL terminator)
    size_t str_buff_avail_size = str_buff_size - strlen(str_buff);
    if (strlen(str_buff) != 0) {
        int snprintf_rc = snprintf(str_buff_end, str_buff_avail_size, ";");
        if (snprintf_rc != strlen(";")) {
            ASTARTE_LOG_ERR("Couldn't append ';' to the property string. Err %d", snprintf_rc);
            ares = ASTARTE_RESULT_INTERNAL_ERROR;
        }
        str_buff_end += snprintf_rc;
        str_buff_avail_size -= snprintf_rc;
    }
    int snprintf_rc = snprintf(str_buff_end, str_buff_avail_size, "%s%s", interface_name, path);
    if (snprintf_rc != strlen(interface_name) + strlen(path)) {
        ASTARTE_LOG_ERR("Couldn't append encoding interface name '%s' and path '%s' to the "
                        "property string. Err %d",
            interface_name, path, snprintf_rc);
        ares = ASTARTE_RESULT_INTERNAL_ERROR;
    }

    return ares;
}
