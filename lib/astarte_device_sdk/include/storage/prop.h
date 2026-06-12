/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef STORAGE_PROPERTIES_H
#define STORAGE_PROPERTIES_H

/**
 * @file storage/prop.h
 * @brief Storage functions for the Astarte device properties.
 */

#include "astarte_device_sdk/astarte.h"
#include "astarte_device_sdk/data.h"
#include "astarte_device_sdk/result.h"

#include "introspection.h"
#include "storage/core.h"

/** @brief Iterator struct for stored properties. */
typedef struct
{
    /** @brief Key-value storage iterator. */
    astarte_key_value_iter_t kv_iter;
} astarte_storage_property_iter_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Stores a property in permanent storage.
 *
 * @param[in,out] handle Pointer to an initialized handle structure.
 * @param[in] interface_name Interface name
 * @param[in] path Property path
 * @param[in] major Major version name
 * @param[in] data Astarte data value to store
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_storage_property_store(astarte_storage_data_t *handle,
    const char *interface_name, const char *path, uint32_t major, astarte_data_t data);

/**
 * @brief Loads a stored property
 *
 * @warning The @p data parameter should be destroyed using
 * #astarte_storage_property_destroy_loaded after its usage has ended.
 *
 * @param[in,out] handle Pointer to an initialized handle structure.
 * @param[in] interface_name Interface name
 * @param[in] path Property path
 * @param[out] out_major Pointer to output major version. If NULL the parameter is ignored.
 * @param[out] data Pointer to output Astarte data. If NULL the parameter is ignored.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_storage_property_load(astarte_storage_data_t *handle,
    const char *interface_name, const char *path, uint32_t *out_major, astarte_data_t *data);

/**
 * @brief Destroy data for a previously loaded property.
 *
 * @details Use this function to free the memory allocated by #astarte_storage_property_load.
 *
 * @param[out] data Astarte data loaded by #astarte_storage_property_load.
 */
void astarte_storage_property_destroy_loaded(astarte_data_t data);

/**
 * @brief Delete a cached property.
 *
 * @param[in,out] handle Pointer to an initialized handle structure.
 * @param[in] interface_name Interface name
 * @param[in] path Property path
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_storage_property_delete(
    astarte_storage_data_t *handle, const char *interface_name, const char *path);

/**
 * @brief Initialize a new iterator to be used to iterate over the stored properties.
 *
 * @param[in,out] handle Pointer to an initialized handle structure.
 * @param[out] iter Iterator instance to initialize.
 * @return ASTARTE_RESULT_OK if successful, ASTARTE_RESULT_NOT_FOUND if not found, otherwise an
 * error code.
 */
astarte_result_t astarte_storage_property_iterator_new(
    astarte_storage_data_t *handle, astarte_storage_property_iter_t *iter);

/**
 * @brief Advance the iterator over the stored properties.
 *
 * @param[inout] iter Iterator initialized with #astarte_storage_property_iterator_new.
 * @return ASTARTE_RESULT_OK if successful, ASTARTE_RESULT_NOT_FOUND if not found, otherwise an
 * error code.
 */
astarte_result_t astarte_storage_property_iterator_next(astarte_storage_property_iter_t *iter);

/**
 * @brief Get the interface name and path for the property pointed by the iterator.
 *
 * @param[in] iter Iterator initialized with #astarte_storage_property_iterator_new.
 * @param[out] interface_name Buffer where to store the read interface name, can be NULL.
 * @param[inout] interface_name_size Size of the @p interface_name buffer. If the @p interface_name
 * parameter is NULL the input value of this parameter will be ignored.
 * The function will store in this variable the size of the read interface name (including the '\0'
 * terminating char).
 * @param[out] path Buffer where to store the read path, can be NULL.
 * @param[inout] path_size Size of the @p path buffer. If the @p path parameter is NULL the input
 * value of this parameter will be ignored.
 * The function will store in this variable the size of the read path (including the '\0'
 * terminating char).
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_storage_property_iterator_get(astarte_storage_property_iter_t *iter,
    char *interface_name, size_t *interface_name_size, char *path, size_t *path_size);

/**
 * @brief Delete the property pointed to by the iterator.
 * @warning Once a delete operation the iterator will point to the previous property. However,
 * a second delete on the same iterator without advancing it first will result in undefined
 * behaviour.
 *
 * @param[inout] iter Iterator initialized with #astarte_storage_property_iterator_new.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_storage_property_iterator_delete(astarte_storage_property_iter_t *iter);

/**
 * @brief Get the device properties string.
 *
 * @details The properties string is a comma separated list of device owned properties full paths.
 * Each property full path is composed by the interface name and path of that property.
 *
 * @param[in,out] handle Pointer to an initialized handle structure.
 * @param[in] introspection Device introspection used to verify ownership of each property.
 * @param[out] output Buffer where to strore the computed properties string, can be NULL.
 * @param[inout] output_size Size of the @p output buffer. If the @p output parameter is NULL the
 * input value of this parameter will be ignored.
 * The function will store in this variable the size of the computed properties string (including
 * the '\0' terminating char).
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_storage_property_get_device_string(astarte_storage_data_t *handle,
    introspection_t *introspection, char *output, size_t *output_size);

#ifdef __cplusplus
}
#endif

#endif // STORAGE_PROPERTIES_H
