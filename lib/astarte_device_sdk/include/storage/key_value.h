/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef STORAGE_KEY_VALUE_H
#define STORAGE_KEY_VALUE_H

/**
 * @file storage/key_value.h
 * @brief Key-value persistent storage implementation, with namespacing.
 *
 * @details Uses the file system as backend. This driver abstracts key-value pairs by mapping
 * namespaces to directories and key-value pairs to files using Zephyr's VFS.
 */

#include <zephyr/fs/fs.h>

#include "astarte_device_sdk/astarte.h"
#include "astarte_device_sdk/result.h"

/** @brief Data struct for an instance of the key-value storage driver. */
typedef struct
{
    /** @brief Namespace used for this key-value storage instance. */
    char *namespace;
    /** @brief Path representation for the namespace (e.g. "/lfs/namespace") */
    char *base_path;
    /** @brief Pointer to the Zephyr File System mount context */
    struct fs_mount_t *fs;
} astarte_storage_key_value_t;

/** @brief Iterator struct for the key-value pair storage. */
typedef struct
{
    /** @brief Reference to the storage instance used by the iterator. */
    astarte_storage_key_value_t *kv_storage;
    /** @brief Zephyr VFS directory descriptor */
    struct fs_dir_t dir;
    /** @brief Current directory entry state */
    struct fs_dirent current_dirent;
    /** @brief Flag keeping track if the iterator is pointing to a valid file */
    bool has_current;
} astarte_storage_key_value_iter_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize/mount a LittleFS partition for use with the key-value driver.
 */
astarte_result_t astarte_storage_key_value_open(struct fs_mount_t *fs);

/**
 * @brief Create a new instance of the key-value pairs storage driver for a specific namespace.
 */
astarte_result_t astarte_storage_key_value_new(
    struct fs_mount_t *fs, const char *namespace, astarte_storage_key_value_t *kv_storage);

/**
 * @brief Destroy an instance of the key-value pairs storage driver.
 */
void astarte_storage_key_value_destroy(astarte_storage_key_value_t *kv_storage);

/**
 * @brief Insert or update a new key-value pair into storage.
 */
astarte_result_t astarte_storage_key_value_insert(
    astarte_storage_key_value_t *kv_storage, const char *key, const void *value, size_t value_size);

/**
 * @brief Find an key-value pair matching the @p key in storage.
 */
astarte_result_t astarte_storage_key_value_find(
    astarte_storage_key_value_t *kv_storage, const char *key, void *value, size_t *value_size);

/**
 * @brief Delete an existing key-value pair from storage.
 */
astarte_result_t astarte_storage_key_value_delete(
    astarte_storage_key_value_t *kv_storage, const char *key);

/**
 * @brief Initialize a new iterator to be used to iterate over the keys present in storage.
 */
astarte_result_t astarte_storage_key_value_iterator_init(
    astarte_storage_key_value_t *kv_storage, astarte_storage_key_value_iter_t *iter);

/**
 * @brief Advance the iterator of one position.
 */
astarte_result_t astarte_storage_key_value_iterator_next(astarte_storage_key_value_iter_t *iter);

/**
 * @brief Get the key for the key-value pair pointed to by the iterator.
 */
astarte_result_t astarte_storage_key_value_iterator_get(
    astarte_storage_key_value_iter_t *iter, void *key, size_t *key_size);

/**
 * @brief Delete the key-value pair currently pointed to by the iterator.
 */
astarte_result_t astarte_storage_key_value_iterator_delete(astarte_storage_key_value_iter_t *iter);

#ifdef __cplusplus
}
#endif

#endif // STORAGE_KEY_VALUE_H
