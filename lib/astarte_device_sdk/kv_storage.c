/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "kv_storage.h"

#include <stdlib.h>

#include <zephyr/sys/mutex.h>

#include "heap.h"
#include "log.h"

ASTARTE_LOG_MODULE_REGISTER(astarte_kv_storage, CONFIG_ASTARTE_DEVICE_SDK_KV_STORAGE_LOG_LEVEL);

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

/** @brief NVS ID for the number of stored key-value pairs */
#define STORED_PAIRS_NVS_ID 0U
/** @brief Number of NVS entries required to hold a key-value pair */
#define NVS_ENTRIES_FOR_PAIR 3U
/** @brief Offsets for the NVS IDs of components of an key-value pair */
#define NVS_ID_OFFSET_NAMESPACE 0U
#define NVS_ID_OFFSET_KEY 1U
#define NVS_ID_OFFSET_VALUE 2U

// This mutex will be shared by all instances of this driver.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static SYS_MUTEX_DEFINE(astarte_kv_storage_mutex);

/************************************************
 *         Static functions declaration         *
 ***********************************************/

/**
 * @brief Insert a new key-value pair using an NVS base ID.
 *
 * @param[inout] nvs_fs NVS file system to use.
 * @param[in] base_id Base NVS ID where to store the key-value pair.
 * @param[in] namespace Namespace for the key-value pair.
 * @param[in] key Key for the key-value pair.
 * @param[in] value Value for the key-value pair.
 * @param[in] value_size Size of the value array for the key-value pair.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
static astarte_result_t insert_pair_at(struct nvs_fs *nvs_fs, uint16_t base_id,
    const char *namespace, const char *key, const void *value, size_t value_size);
/**
 * @brief Find the NVS base ID for the a key-value pair.
 *
 * @param[inout] nvs_fs NVS file system to use.
 * @param[in] namespace Namespace for the key-value pair.
 * @param[in] stored_pairs Number of total stored key-value pairs.
 * @param[in] key Key to use for the search.
 * @param[out] base_id Found base ID for the key-value pair.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
static astarte_result_t find_pair_base_id(struct nvs_fs *nvs_fs, const char *namespace,
    uint16_t stored_pairs, const char *key, uint16_t *base_id);
/**
 * @brief Relocate a single key-value pair.
 *
 * @param[inout] nvs_fs NVS file system to use.
 * @param[in] dst_base_id Destination NVS base ID where the pair should be relocated.
 * @param[in] src_base_id Source NVS base ID where the pair should be relocated.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
static astarte_result_t relocate_pair(
    struct nvs_fs *nvs_fs, uint16_t dst_base_id, uint16_t src_base_id);
/**
 * @brief Get value for the NVS entry at the provided ID. Dynamically allocates the value struct.
 *
 * @param[inout] nvs_fs NVS file system from which to fetch the entry.
 * @param[in] entry_id NVS ID for the entry to read.
 * @param[out] data On success will point to a dynamically allocated buffer containing the data.
 * @param[inout] data_size The size of the allocated @p data buffer.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
static astarte_result_t get_nvs_entry_with_alloc(
    struct nvs_fs *nvs_fs, uint16_t entry_id, void **data, size_t *data_size);
/**
 * @brief Get data for the NVS entry at the provided ID.
 *
 * @param[inout] nvs_fs NVS file system from which to fetch the entry.
 * @param[in] entry_id NVS ID for the entry to read.
 * @param[out] data Buffer where to store the NVS entry data, can be NULL.
 * @param[inout] data_size When @p data is non NULL should correspond the the size of @p data.
 * Upon success it will be set to the required size to store the data.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
static astarte_result_t get_nvs_entry(
    struct nvs_fs *nvs_fs, uint16_t entry_id, void *data, size_t *data_size);
/**
 * @brief Get number of stored pairs.
 *
 * @param[inout] nvs_fs NVS file system from which to fetch the number.
 * @param[out] stored_pairs Number of pairs stored in NVS.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
static astarte_result_t get_stored_pairs(struct nvs_fs *nvs_fs, uint16_t *stored_pairs);
/**
 * @brief Update the number of stored pairs.
 *
 * @param[inout] nvs_fs NVS file system in which to store the entry.
 * @param[in] stored_pairs Updated number of pairs stored in NVS.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
static astarte_result_t update_stored_pairs(struct nvs_fs *nvs_fs, uint16_t stored_pairs);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_kv_storage_new(
    astarte_kv_storage_cfg_t config, const char *namespace, astarte_kv_storage_t *kv_storage)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    char *namespace_cpy = NULL;
    size_t namespace_cpy_size = 0U;

    namespace_cpy_size = strlen(namespace) + 1;
    namespace_cpy = astarte_calloc(namespace_cpy_size, sizeof(char));
    if (!namespace_cpy) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto error;
    }
    strncpy(namespace_cpy, namespace, namespace_cpy_size);

    kv_storage->flash_device = config.flash_device;
    kv_storage->flash_offset = config.flash_offset;
    kv_storage->flash_sector_size = config.flash_sector_size;
    kv_storage->flash_sector_count = config.flash_sector_count;
    kv_storage->namespace = namespace_cpy;

    ASTARTE_LOG_DBG("Opened key pair storage.");
    ASTARTE_LOG_DBG("    Namespace: %s", namespace);

    return ASTARTE_RESULT_OK;

error:
    astarte_free(namespace_cpy);

    return ares;
}

void astarte_kv_storage_destroy(astarte_kv_storage_t kv_storage)
{
    astarte_free(kv_storage.namespace);
    ASTARTE_LOG_DBG("Closed key pair storage.");
}

astarte_result_t astarte_kv_storage_insert(
    astarte_kv_storage_t *kv_storage, const char *key, const void *value, size_t value_size)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    struct nvs_fs nvs_fs = { 0 };
    uint16_t stored_pairs = 0;
    uint16_t base_id = 0U;
    bool append_at_end = false;

    // Lock the mutex for the key-value storage
    int mutex_rc = sys_mutex_lock(&astarte_kv_storage_mutex, K_FOREVER);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex lock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    nvs_fs.flash_device = kv_storage->flash_device;
    nvs_fs.offset = kv_storage->flash_offset;
    nvs_fs.sector_size = kv_storage->flash_sector_size;
    nvs_fs.sector_count = kv_storage->flash_sector_count;

    int nvs_rc = nvs_mount(&nvs_fs);
    if (nvs_rc) {
        ASTARTE_LOG_ERR("Mounting NVS failed: %d.", nvs_rc);
        ares = ASTARTE_RESULT_NVS_ERROR;
        goto exit;
    }

    // Get number of stored key-value pairs
    ares = get_stored_pairs(&nvs_fs, &stored_pairs);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Get total stored pairs failed %s.", astarte_result_to_name(ares));
        goto exit;
    }

    // Search if key is already present in storage
    ares = find_pair_base_id(&nvs_fs, kv_storage->namespace, stored_pairs, key, &base_id);
    if (ares == ASTARTE_RESULT_NOT_FOUND) {
        append_at_end = true;
    } else if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Check for old values failed %s.", astarte_result_to_name(ares));
        goto exit;
    }

    // If the key is not present, add it to at the end of NVS
    if (append_at_end) {
        size_t unbound_base_id = 1 + (stored_pairs * NVS_ENTRIES_FOR_PAIR);
        if (unbound_base_id + NVS_ID_OFFSET_VALUE >= UINT16_MAX) {
            ares = ASTARTE_RESULT_KV_STORAGE_FULL;
            goto exit;
        }
        base_id = (uint16_t) unbound_base_id;
    }

    ASTARTE_LOG_DBG("Inserting key-value pair with key: '%s' at base id: %d", key, base_id);
    ares = insert_pair_at(&nvs_fs, base_id, kv_storage->namespace, key, value, value_size);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Insert failed %s.", astarte_result_to_name(ares));
        goto exit;
    }

    // If the key is not present, add it to at the end of NVS
    if (append_at_end) {
        stored_pairs++;
        ares = update_stored_pairs(&nvs_fs, stored_pairs);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Update total stored pairs failed %s.", astarte_result_to_name(ares));
            goto exit;
        }
    }

exit:

    // Unlock the mutex for the key-value storage
    mutex_rc = sys_mutex_unlock(&astarte_kv_storage_mutex);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex unlock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    return ares;
}

astarte_result_t astarte_kv_storage_find(
    astarte_kv_storage_t *kv_storage, const char *key, void *value, size_t *value_size)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    struct nvs_fs nvs_fs = { 0 };
    uint16_t stored_pairs = 0;
    uint16_t base_id = 0U;

    // Lock the mutex for the key-value storage
    int mutex_rc = sys_mutex_lock(&astarte_kv_storage_mutex, K_FOREVER);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex lock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    nvs_fs.flash_device = kv_storage->flash_device;
    nvs_fs.offset = kv_storage->flash_offset;
    nvs_fs.sector_size = kv_storage->flash_sector_size;
    nvs_fs.sector_count = kv_storage->flash_sector_count;

    int nvs_rc = nvs_mount(&nvs_fs);
    if (nvs_rc) {
        ASTARTE_LOG_ERR("Mounting NVS failed: %d.", nvs_rc);
        ares = ASTARTE_RESULT_NVS_ERROR;
        goto exit;
    }

    ares = get_stored_pairs(&nvs_fs, &stored_pairs);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Get total stored pairs failed %s.", astarte_result_to_name(ares));
        goto exit;
    }

    ares = find_pair_base_id(&nvs_fs, kv_storage->namespace, stored_pairs, key, &base_id);
    if (ares != ASTARTE_RESULT_OK) {
        // Do not print errors as it could be a not found result
        goto exit;
    }

    ASTARTE_LOG_DBG("Found key-value pair with key: '%s' at base id: %d", key, base_id);

    ares = get_nvs_entry(&nvs_fs, base_id + NVS_ID_OFFSET_VALUE, value, value_size);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Get value of key-value storage failed %s.", astarte_result_to_name(ares));
    }

exit:

    // Unlock the mutex for the key-value storage
    mutex_rc = sys_mutex_unlock(&astarte_kv_storage_mutex);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex unlock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    return ares;
}

astarte_result_t astarte_kv_storage_delete(astarte_kv_storage_t *kv_storage, const char *key)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    struct nvs_fs nvs_fs = { 0 };
    uint16_t stored_pairs = 0U;
    uint16_t base_id = 0U;

    // Lock the mutex for the key-value storage
    int mutex_rc = sys_mutex_lock(&astarte_kv_storage_mutex, K_FOREVER);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex lock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    nvs_fs.flash_device = kv_storage->flash_device;
    nvs_fs.offset = kv_storage->flash_offset;
    nvs_fs.sector_size = kv_storage->flash_sector_size;
    nvs_fs.sector_count = kv_storage->flash_sector_count;

    int nvs_rc = nvs_mount(&nvs_fs);
    if (nvs_rc) {
        ASTARTE_LOG_ERR("Mounting NVS failed: %d.", nvs_rc);
        ares = ASTARTE_RESULT_NVS_ERROR;
        goto exit;
    }

    ares = get_stored_pairs(&nvs_fs, &stored_pairs);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Get total stored pairs failed %s.", astarte_result_to_name(ares));
        goto exit;
    }

    ares = find_pair_base_id(&nvs_fs, kv_storage->namespace, stored_pairs, key, &base_id);
    if (ares != ASTARTE_RESULT_OK) {
        // Do not print errors as it could be a not found result
        goto exit;
    }

    ASTARTE_LOG_DBG("Found key-valye pair for removal with key: '%s' at base id: %d", key, base_id);

    // The initial +1 accounts for the first entry used to store the number of stored entries.
    uint16_t last_base_id = 1 + ((stored_pairs - 1) * NVS_ENTRIES_FOR_PAIR);
    for (uint16_t i = base_id; i < last_base_id; i += NVS_ENTRIES_FOR_PAIR) {
        uint16_t relocation_src = i + NVS_ENTRIES_FOR_PAIR;
        uint16_t relocation_dst = i;
        ASTARTE_LOG_DBG("Relocating from %d to %d", relocation_src, relocation_dst);
        ares = relocate_pair(&nvs_fs, relocation_dst, relocation_src);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Relocation failed %s.", astarte_result_to_name(ares));
            goto exit;
        }
    }

    stored_pairs--;
    ares = update_stored_pairs(&nvs_fs, stored_pairs);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Update total stored pairs failed %s.", astarte_result_to_name(ares));
        goto exit;
    }

exit:

    // Unlock the mutex for the key-value storage
    mutex_rc = sys_mutex_unlock(&astarte_kv_storage_mutex);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex unlock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    return ares;
}

astarte_result_t astarte_kv_storage_iterator_init(
    astarte_kv_storage_t *kv_storage, astarte_kv_storage_iter_t *iter)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    struct nvs_fs nvs_fs = { 0 };
    uint16_t stored_pairs = 0;
    void *namespace = NULL;

    // Lock the mutex for the key-value storage
    int mutex_rc = sys_mutex_lock(&astarte_kv_storage_mutex, K_FOREVER);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex lock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    nvs_fs.flash_device = kv_storage->flash_device;
    nvs_fs.offset = kv_storage->flash_offset;
    nvs_fs.sector_size = kv_storage->flash_sector_size;
    nvs_fs.sector_count = kv_storage->flash_sector_count;
    int nvs_rc = nvs_mount(&nvs_fs);
    if (nvs_rc) {
        ASTARTE_LOG_ERR("Mounting NVS failed: %d.", nvs_rc);
        ares = ASTARTE_RESULT_NVS_ERROR;
        goto exit;
    }

    ares = get_stored_pairs(&nvs_fs, &stored_pairs);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Get total stored pairs failed %s.", astarte_result_to_name(ares));
        goto exit;
    }

    for (int32_t pair_base_id = stored_pairs - 1; pair_base_id >= 0; pair_base_id--) {

        uint16_t namespace_id = 1 + (pair_base_id * NVS_ENTRIES_FOR_PAIR) + NVS_ID_OFFSET_NAMESPACE;

        size_t namespace_size = 0U;
        ares = get_nvs_entry_with_alloc(&nvs_fs, namespace_id, &namespace, &namespace_size);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Failed fetching pair namespace %s.", astarte_result_to_name(ares));
            goto exit;
        }

        if (strcmp(kv_storage->namespace, namespace) == 0) {
            iter->kv_storage = kv_storage;
            iter->current_pair = pair_base_id;
            ASTARTE_LOG_DBG("Initialized iterator. Curent pair: %d", iter->current_pair);
            goto exit;
        }

        astarte_free(namespace);
        namespace = NULL;
    }

    ares = ASTARTE_RESULT_NOT_FOUND;

exit:

    // Unlock the mutex for the key-value storage
    mutex_rc = sys_mutex_unlock(&astarte_kv_storage_mutex);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex unlock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    astarte_free(namespace);
    return ares;
}

astarte_result_t astarte_kv_storage_iterator_next(astarte_kv_storage_iter_t *iter)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    struct nvs_fs nvs_fs = { 0 };
    void *namespace = NULL;

    // Lock the mutex for the key-value storage
    int mutex_rc = sys_mutex_lock(&astarte_kv_storage_mutex, K_FOREVER);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex lock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    nvs_fs.flash_device = iter->kv_storage->flash_device;
    nvs_fs.offset = iter->kv_storage->flash_offset;
    nvs_fs.sector_size = iter->kv_storage->flash_sector_size;
    nvs_fs.sector_count = iter->kv_storage->flash_sector_count;
    int nvs_rc = nvs_mount(&nvs_fs);
    if (nvs_rc) {
        ASTARTE_LOG_ERR("Mounting NVS failed: %d.", nvs_rc);
        ares = ASTARTE_RESULT_NVS_ERROR;
        goto exit;
    }

    for (int32_t pair_base_id = iter->current_pair - 1; pair_base_id >= 0; pair_base_id--) {

        uint16_t namespace_id = 1 + (pair_base_id * NVS_ENTRIES_FOR_PAIR) + NVS_ID_OFFSET_NAMESPACE;
        size_t namespace_size = 0U;
        ares = get_nvs_entry_with_alloc(&nvs_fs, namespace_id, &namespace, &namespace_size);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Failed fetching pair namespace %s.", astarte_result_to_name(ares));
            goto exit;
        }

        if (strcmp(iter->kv_storage->namespace, namespace) == 0) {
            iter->current_pair = pair_base_id;
            ASTARTE_LOG_DBG("Advanced iterator. Curent pair: %d", iter->current_pair);
            goto exit;
        }

        astarte_free(namespace);
        namespace = NULL;
    }

    ares = ASTARTE_RESULT_NOT_FOUND;

exit:

    // Unlock the mutex for the key-value storage
    mutex_rc = sys_mutex_unlock(&astarte_kv_storage_mutex);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex unlock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    astarte_free(namespace);
    return ares;
}

astarte_result_t astarte_kv_storage_iterator_get(
    astarte_kv_storage_iter_t *iter, void *key, size_t *key_size)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    // Lock the mutex for the key-value storage
    int mutex_rc = sys_mutex_lock(&astarte_kv_storage_mutex, K_FOREVER);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex lock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    struct nvs_fs nvs_fs = { 0 };
    nvs_fs.flash_device = iter->kv_storage->flash_device;
    nvs_fs.offset = iter->kv_storage->flash_offset;
    nvs_fs.sector_size = iter->kv_storage->flash_sector_size;
    nvs_fs.sector_count = iter->kv_storage->flash_sector_count;
    int nvs_rc = nvs_mount(&nvs_fs);
    if (nvs_rc) {
        ASTARTE_LOG_ERR("Mounting NVS failed: %d.", nvs_rc);
        ares = ASTARTE_RESULT_NVS_ERROR;
        goto exit;
    }

    uint16_t key_id = 1 + (iter->current_pair * NVS_ENTRIES_FOR_PAIR) + NVS_ID_OFFSET_KEY;

    nvs_rc = nvs_read(&nvs_fs, key_id, key, *key_size);
    if (nvs_rc < 0) {
        ASTARTE_LOG_ERR("nvs_read error %d.", nvs_rc);
        ares = ASTARTE_RESULT_NVS_ERROR;
        goto exit;
    }
    if (key && (nvs_rc > *key_size)) {
        ASTARTE_LOG_ERR("Key size is too large for provided buffer.");
        ares = ASTARTE_RESULT_INVALID_PARAM;
        goto exit;
    }
    *key_size = nvs_rc;

exit:

    // Unlock the mutex for the key-value storage
    mutex_rc = sys_mutex_unlock(&astarte_kv_storage_mutex);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex unlock failed with %d", mutex_rc);
    __ASSERT_NO_MSG(mutex_rc == 0);

    return ares;
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static astarte_result_t insert_pair_at(struct nvs_fs *nvs_fs, uint16_t base_id,
    const char *namespace, const char *key, const void *value, size_t value_size)
{
    uint16_t id_namespace = base_id + NVS_ID_OFFSET_NAMESPACE;
    uint16_t id_key = base_id + NVS_ID_OFFSET_KEY;
    uint16_t id_value = base_id + NVS_ID_OFFSET_VALUE;

    ssize_t nvs_rc = nvs_write(nvs_fs, id_namespace, namespace, strlen(namespace) + 1);
    if (nvs_rc < 0) {
        ASTARTE_LOG_ERR("nvs_write namespace error: %s (%d).", strerror(-nvs_rc), nvs_rc);
        return ASTARTE_RESULT_NVS_ERROR;
    }

    nvs_rc = nvs_write(nvs_fs, id_key, key, strlen(key) + 1);
    if (nvs_rc < 0) {
        ASTARTE_LOG_ERR("nvs_write key error: %s (%d).", strerror(-nvs_rc), nvs_rc);
        return ASTARTE_RESULT_NVS_ERROR;
    }

    nvs_rc = nvs_write(nvs_fs, id_value, value, value_size);
    if (nvs_rc < 0) {
        ASTARTE_LOG_ERR("nvs_write value error: %s (%d).", strerror(-nvs_rc), nvs_rc);
        return ASTARTE_RESULT_NVS_ERROR;
    }

    return ASTARTE_RESULT_OK;
}

static astarte_result_t find_pair_base_id(struct nvs_fs *nvs_fs, const char *namespace,
    uint16_t stored_pairs, const char *key, uint16_t *base_id)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    void *tmp_namespace = NULL;
    void *tmp_key = NULL;
    bool found = false;

    // Start from 1 as the first NVS slot is used by the number of elements
    uint16_t pair_number = 0U;
    for (; pair_number < stored_pairs; pair_number++) {

        size_t namespace_size = 0;
        uint16_t namespace_id = 1 + (pair_number * NVS_ENTRIES_FOR_PAIR) + NVS_ID_OFFSET_NAMESPACE;
        ares = get_nvs_entry_with_alloc(nvs_fs, namespace_id, &tmp_namespace, &namespace_size);
        if (ares != ASTARTE_RESULT_OK) {
            goto exit;
        }

        if (strcmp((char *) tmp_namespace, namespace) == 0) {

            size_t key_size = 0;
            uint16_t key_id = 1 + (pair_number * NVS_ENTRIES_FOR_PAIR) + NVS_ID_OFFSET_KEY;
            ares = get_nvs_entry_with_alloc(nvs_fs, key_id, &tmp_key, &key_size);
            if (ares != ASTARTE_RESULT_OK) {
                goto exit;
            }

            if (strcmp((char *) tmp_key, key) == 0) {
                found = true;
                break;
            }

            astarte_free(tmp_key);
            tmp_key = NULL;
        }

        astarte_free(tmp_namespace);
        tmp_namespace = NULL;
    }
    if (!found) {
        ares = ASTARTE_RESULT_NOT_FOUND;
        goto exit;
    }
    *base_id = (uint16_t) (1 + (pair_number * NVS_ENTRIES_FOR_PAIR));

exit:
    astarte_free(tmp_namespace);
    astarte_free(tmp_key);
    return ares;
}

static astarte_result_t relocate_pair(
    struct nvs_fs *nvs_fs, uint16_t dst_base_id, uint16_t src_base_id)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    void *namespace = NULL;
    void *key = NULL;
    void *value = NULL;

    size_t namespace_id = src_base_id + NVS_ID_OFFSET_NAMESPACE;
    size_t namespace_size = 0U;
    ares = get_nvs_entry_with_alloc(nvs_fs, namespace_id, &namespace, &namespace_size);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed fetching entry namespace %s.", astarte_result_to_name(ares));
        goto exit;
    }

    size_t key_id = src_base_id + NVS_ID_OFFSET_KEY;
    size_t key_size = 0U;
    ares = get_nvs_entry_with_alloc(nvs_fs, key_id, &key, &key_size);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed fetching entry key %s.", astarte_result_to_name(ares));
        goto exit;
    }

    size_t value_id = src_base_id + NVS_ID_OFFSET_VALUE;
    size_t value_size = 0U;
    ares = get_nvs_entry_with_alloc(nvs_fs, value_id, &value, &value_size);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed fetching entry value %s.", astarte_result_to_name(ares));
        goto exit;
    }

    ares = insert_pair_at(nvs_fs, dst_base_id, (char *) namespace, (char *) key, value, value_size);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed relocating entry %s.", astarte_result_to_name(ares));
    }

exit:
    astarte_free(namespace);
    astarte_free(key);
    astarte_free(value);

    return ares;
}

static astarte_result_t get_nvs_entry_with_alloc(
    struct nvs_fs *nvs_fs, uint16_t entry_id, void **data, size_t *data_size)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    void *buff = NULL;
    size_t buff_size = 0U;

    ares = get_nvs_entry(nvs_fs, entry_id, NULL, &buff_size);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed fetching entry value size %s.", astarte_result_to_name(ares));
        goto error;
    }

    buff = astarte_calloc(buff_size, sizeof(uint8_t));
    if (!buff) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto error;
    }

    ares = get_nvs_entry(nvs_fs, entry_id, buff, &buff_size);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed fetching entry value %s.", astarte_result_to_name(ares));
        goto error;
    }

    *data = buff;
    *data_size = buff_size;
    return ASTARTE_RESULT_OK;

error:
    astarte_free(buff);
    return ares;
}

static astarte_result_t get_nvs_entry(
    struct nvs_fs *nvs_fs, uint16_t entry_id, void *data, size_t *data_size)
{
    int nvs_rc = nvs_read(nvs_fs, entry_id, data, *data_size);
    if (nvs_rc < 0) {
        ASTARTE_LOG_ERR("nvs_read error %d.", nvs_rc);
        return ASTARTE_RESULT_NVS_ERROR;
    }
    if (data && (nvs_rc > *data_size)) {
        ASTARTE_LOG_ERR("Stored data is too large for provided buffer.");
        return ASTARTE_RESULT_INVALID_PARAM;
    }
    *data_size = nvs_rc;
    return ASTARTE_RESULT_OK;
}

static astarte_result_t get_stored_pairs(struct nvs_fs *nvs_fs, uint16_t *stored_pairs)
{
    uint16_t data = 0;
    int nvs_rc = nvs_read(nvs_fs, STORED_PAIRS_NVS_ID, &data, sizeof(data));
    if ((nvs_rc < 0) && (nvs_rc != -ENOENT)) {
        ASTARTE_LOG_ERR("nvs_read error: %s (%d).", strerror(-nvs_rc), nvs_rc);
        return ASTARTE_RESULT_NVS_ERROR;
    }
    *stored_pairs = data;
    return ASTARTE_RESULT_OK;
}

static astarte_result_t update_stored_pairs(struct nvs_fs *nvs_fs, uint16_t stored_pairs)
{
    ASTARTE_LOG_DBG("Updating stored pairs number to: %d", stored_pairs);
    int nvs_rc = nvs_write(nvs_fs, STORED_PAIRS_NVS_ID, &stored_pairs, sizeof(stored_pairs));
    if (nvs_rc < 0) {
        ASTARTE_LOG_ERR("nvs_write error: %s (%d).", strerror(-nvs_rc), nvs_rc);
        return ASTARTE_RESULT_NVS_ERROR;
    }
    return ASTARTE_RESULT_OK;
}
