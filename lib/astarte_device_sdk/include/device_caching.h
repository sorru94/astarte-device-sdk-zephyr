/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DEVICE_CACHING_H
#define DEVICE_CACHING_H

/**
 * @file device_caching.h
 * @brief Device caching utilities.
 */

#include "astarte_device_sdk/astarte.h"
#include "astarte_device_sdk/data.h"
#include "astarte_device_sdk/result.h"

#include "introspection.h"
#include "kv_storage.h"

/**
 * @brief Handle containing the persistent state for device caching.
 * @details This struct holds the context for the three NVS namespaces used by the caching system.
 */
typedef struct
{
    /** @brief NVS file system handle shared between all caching */
    struct nvs_fs nvs_fs;
    /** @brief Storage handle for synchronization state */
    astarte_kv_storage_t sync_storage;
    /** @brief Storage handle for introspection data */
    astarte_kv_storage_t intro_storage;
    /** @brief Storage handle for device properties */
    astarte_kv_storage_t prop_storage;
    /** @brief Flag to ensure we don't double-init or use uninitialized handles */
    bool initialized;
} astarte_device_caching_t;

/** @brief Iterator struct for stored properties. */
typedef struct
{
    /** @brief Key-value storage iterator. */
    astarte_kv_storage_iter_t kv_iter;
} astarte_device_caching_property_iter_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the device caching system and open all required NVS namespaces.
 * @param[in,out] handle Pointer to the handle structure to initialize.
 * @return ASTARTE_RESULT_OK if successful.
 */
astarte_result_t astarte_device_caching_init(astarte_device_caching_t *handle);

/**
 * @brief Close and clean up the device caching system.
 * * @param[in] handle Pointer to the handle structure to destroy.
 */
void astarte_device_caching_destroy(astarte_device_caching_t *handle);

/**
 * @brief Get the synchronization state.
 *
 * @param[in,out] handle Pointer to an initialized handle structure.
 * @param[out] sync Synchronization state, this will be set to true if a proper synchronization
 * has been previously achieved with Astarte.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_device_caching_synchronization_get(
    astarte_device_caching_t *handle, bool *sync);

/**
 * @brief Set the synchronization state.
 *
 * @param[in,out] handle Pointer to an initialized handle structure.
 * @param[in] sync Synchronization state, this should be set to true if a proper synchronization
 * has been achieved with Astarte.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_device_caching_synchronization_set(
    astarte_device_caching_t *handle, bool sync);

/**
 * @brief Cache in the introspection for this device.
 *
 * @param[in,out] handle Pointer to an initialized handle structure.
 * @param[in] intr Buffer containing the stringified version of the device introspection
 * @param[in] intr_size Size in chars of the @p buffer parameter.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_device_caching_introspection_store(
    astarte_device_caching_t *handle, const char *intr, size_t intr_size);

/**
 * @brief Check if the cached introspection exists and it's identical to the input one.
 *
 * @param[in,out] handle Pointer to an initialized handle structure.
 * @param[in] intr Buffer containing the stringified version of the device introspection
 * @param[in] intr_size Size in chars of the @p buffer parameter.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_device_caching_introspection_check(
    astarte_device_caching_t *handle, const char *intr, size_t intr_size);

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
astarte_result_t astarte_device_caching_property_store(astarte_device_caching_t *handle,
    const char *interface_name, const char *path, uint32_t major, astarte_data_t data);

/**
 * @brief Loads a stored property
 *
 * @warning The @p data parameter should be destroyed using
 * #astarte_device_caching_property_destroy_loaded after its usage has ended.
 *
 * @param[in,out] handle Pointer to an initialized handle structure.
 * @param[in] interface_name Interface name
 * @param[in] path Property path
 * @param[out] out_major Pointer to output major version. Might be NULL, in this case the parameter
 * is ignored.
 * @param[out] data Pointer to output Astarte data. May be NULL, in this case the parameter is
 * ignored.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_device_caching_property_load(astarte_device_caching_t *handle,
    const char *interface_name, const char *path, uint32_t *out_major, astarte_data_t *data);

/**
 * @brief Destroy data for a previously loaded property.
 *
 * @details Use this function to free the memory allocated by #astarte_device_caching_property_load.
 *
 * @param[out] data Astarte data loaded by #astarte_device_caching_property_load.
 */
void astarte_device_caching_property_destroy_loaded(astarte_data_t data);

/**
 * @brief Delete a cached property.
 *
 * @param[in,out] handle Pointer to an initialized handle structure.
 * @param[in] interface_name Interface name
 * @param[in] path Property path
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_device_caching_property_delete(
    astarte_device_caching_t *handle, const char *interface_name, const char *path);

/**
 * @brief Initialize a new iterator to be used to iterate over the stored properties.
 *
 * @param[in,out] handle Pointer to an initialized handle structure.
 * @param[out] iter Iterator instance to initialize.
 * @return ASTARTE_RESULT_OK if successful, ASTARTE_RESULT_NOT_FOUND if not found, otherwise an
 * error code.
 */
astarte_result_t astarte_device_caching_property_iterator_new(
    astarte_device_caching_t *handle, astarte_device_caching_property_iter_t *iter);

/**
 * @brief Advance the iterator over the stored properties.
 *
 * @param[inout] iter Iterator initialized with #astarte_device_caching_property_iterator_new.
 * @return ASTARTE_RESULT_OK if successful, ASTARTE_RESULT_NOT_FOUND if not found, otherwise an
 * error code.
 */
astarte_result_t astarte_device_caching_property_iterator_next(
    astarte_device_caching_property_iter_t *iter);

/**
 * @brief Get the interface name and path for the property pointed by the iterator.
 *
 * @param[in] iter Iterator initialized with #astarte_device_caching_property_iterator_new.
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
astarte_result_t astarte_device_caching_property_iterator_get(
    astarte_device_caching_property_iter_t *iter, char *interface_name, size_t *interface_name_size,
    char *path, size_t *path_size);

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
astarte_result_t astarte_device_caching_property_get_device_string(astarte_device_caching_t *handle,
    introspection_t *introspection, char *output, size_t *output_size);

#ifdef __cplusplus
}
#endif

#endif // DEVICE_CACHING_H
