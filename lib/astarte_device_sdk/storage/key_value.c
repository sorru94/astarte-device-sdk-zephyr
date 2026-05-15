/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "storage/key_value.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/fs/fs.h>
#include <zephyr/sys/mutex.h>

#include "log.h"

ASTARTE_LOG_MODULE_REGISTER(astarte_kv_storage, CONFIG_ASTARTE_DEVICE_SDK_KV_STORAGE_LOG_LEVEL);

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

static SYS_MUTEX_DEFINE(astarte_storage_key_value_mutex);

/************************************************
 *         Static functions declaration         *
 ***********************************************/

/**
 * @brief Helper to build absolute string paths joining a base directory and a filename
 */
static char *build_path_alloc(const char *base_path, const char *key);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_storage_key_value_open(struct fs_mount_t *fs)
{
    if (!fs) {
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    int ret = fs_mount(fs);
    if (ret && ret != -EALREADY) {
        ASTARTE_LOG_ERR("File system mount error: %d.", ret);
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }

    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_storage_key_value_new(
    struct fs_mount_t *fs, const char *namespace, astarte_storage_key_value_t *kv_storage)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    char *base_path = NULL;
    char *owned_namespace = NULL;

    if (!fs || !namespace || !kv_storage) {
        ares = ASTARTE_RESULT_INVALID_PARAM;
        goto error;
    }

    // Duplicate the namespace string
    size_t owned_namespace_size = strlen(namespace) + 1;
    owned_namespace = calloc(owned_namespace_size, sizeof(char));
    if (!owned_namespace) {
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto error;
    }
    memcpy(owned_namespace, namespace, owned_namespace_size);

    // Compute the base path
    size_t base_path_size = strlen(fs->mnt_point) + 1 + strlen(namespace) + 1;
    base_path = calloc(base_path_size, sizeof(char));
    if (!base_path) {
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto error;
    }
    // TODO: evaluate the return
    snprintf(base_path, base_path_size, "%s/%s", fs->mnt_point, namespace);

    // Ensure the namespace directory exists
    struct fs_dirent entry;
    int res = fs_stat(base_path, &entry);
    if (res == -ENOENT) {
        res = fs_mkdir(base_path);
        if (res && res != -EEXIST) {
            ASTARTE_LOG_ERR("Failed to create namespace directory: %d", res);
            ares = ASTARTE_RESULT_INTERNAL_ERROR;
            goto error;
        }
    }

    // Populate the storage struct
    kv_storage->fs = fs;
    kv_storage->base_path = base_path;
    kv_storage->namespace = owned_namespace;

    return ASTARTE_RESULT_OK;

error:
    free(base_path);
    free(owned_namespace);
    return ares;
}

void astarte_storage_key_value_destroy(astarte_storage_key_value_t *kv_storage)
{
    if (kv_storage) {
        free(kv_storage->base_path);
        free(kv_storage->namespace);
        memset(kv_storage, 0, sizeof(astarte_storage_key_value_t));
    }
}

astarte_result_t astarte_storage_key_value_insert(
    astarte_storage_key_value_t *kv_storage, const char *key, const void *value, size_t value_size)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    char *path = NULL;

    sys_mutex_lock(&astarte_storage_key_value_mutex, K_FOREVER);

    if (!kv_storage || !key || !value) {
        ares = ASTARTE_RESULT_INVALID_PARAM;
        goto exit;
    }

    path = build_path_alloc(kv_storage->base_path, key);
    if (!path) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto exit;
    }

    // Unlink first to truncate the file safely before overwriting with VFS limits
    fs_unlink(path);

    struct fs_file_t file;
    fs_file_t_init(&file);

    int ret = fs_open(&file, path, FS_O_CREATE | FS_O_WRITE);
    if (ret != 0) {
        ASTARTE_LOG_ERR("Failed to open file for writing: %d", ret);
        ares = ASTARTE_RESULT_INTERNAL_ERROR;
        goto exit;
    }

    ssize_t written = fs_write(&file, value, value_size);
    fs_close(&file);

    if (written < 0 || (size_t) written != value_size) {
        ASTARTE_LOG_ERR("Write failure. Expected %zu, wrote %zd", value_size, written);
        ares = ASTARTE_RESULT_INTERNAL_ERROR;
    }

exit:
    free(path);

    sys_mutex_unlock(&astarte_storage_key_value_mutex);

    return ares;
}

astarte_result_t astarte_storage_key_value_find(
    astarte_storage_key_value_t *kv_storage, const char *key, void *value, size_t *value_size)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    char *path = NULL;

    sys_mutex_lock(&astarte_storage_key_value_mutex, K_FOREVER);

    if (!kv_storage || !key || !value) {
        ares = ASTARTE_RESULT_INVALID_PARAM;
        goto exit;
    }

    path = build_path_alloc(kv_storage->base_path, key);
    if (!path) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto exit;
    }

    struct fs_dirent entry;
    int ret = fs_stat(path, &entry);
    if (ret != 0 || entry.type != FS_DIR_ENTRY_FILE) {
        ares = ASTARTE_RESULT_NOT_FOUND;
        goto exit;
    }

    if (value == NULL) {
        *value_size = entry.size;
        goto exit;
    }

    if (*value_size < entry.size) {
        *value_size = entry.size;
        ares = ASTARTE_RESULT_INVALID_PARAM;
        goto exit;
    }

    struct fs_file_t file;
    fs_file_t_init(&file);
    ret = fs_open(&file, path, FS_O_READ);
    if (ret != 0) {
        ares = ASTARTE_RESULT_INTERNAL_ERROR;
        goto exit;
    }

    ssize_t read_bytes = fs_read(&file, value, entry.size);
    fs_close(&file);
    if (read_bytes != entry.size) {
        ASTARTE_LOG_ERR("Read failure. Expected %zu, read %zd", entry.size, read_bytes);
        ares = ASTARTE_RESULT_INTERNAL_ERROR;
        goto exit;
    }

    *value_size = entry.size;

exit:
    sys_mutex_unlock(&astarte_storage_key_value_mutex);
    free(path);
    return ares;
}

astarte_result_t astarte_storage_key_value_delete(
    astarte_storage_key_value_t *kv_storage, const char *key)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    char *path = NULL;

    sys_mutex_lock(&astarte_storage_key_value_mutex, K_FOREVER);

    if (!kv_storage || !key) {
        ares = ASTARTE_RESULT_INVALID_PARAM;
        goto exit;
    }

    path = build_path_alloc(kv_storage->base_path, key);
    if (!path) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto exit;
    }

    int ret = fs_unlink(path);
    if ((ret != 0) && (ret != -ENOENT)) {
        ares = ASTARTE_RESULT_INTERNAL_ERROR;
    }

exit:
    free(path);
    sys_mutex_unlock(&astarte_storage_key_value_mutex);
    return ares;
}

astarte_result_t astarte_storage_key_value_iterator_init(
    astarte_storage_key_value_t *kv_storage, astarte_storage_key_value_iter_t *iter)
{
    sys_mutex_lock(&astarte_storage_key_value_mutex, K_FOREVER);

    fs_dir_t_init(&iter->dir);
    int ret = fs_opendir(&iter->dir, kv_storage->base_path);

    sys_mutex_unlock(&astarte_storage_key_value_mutex);

    if (ret != 0) {
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }

    iter->has_current = false;
    iter->kv_storage = kv_storage;

    return astarte_storage_key_value_iterator_next(iter);
}

astarte_result_t astarte_storage_key_value_iterator_next(astarte_storage_key_value_iter_t *iter)
{
    astarte_result_t ares = ASTARTE_RESULT_NOT_FOUND;

    sys_mutex_lock(&astarte_storage_key_value_mutex, K_FOREVER);

    iter->has_current = false;
    while (true) {
        int ret = fs_readdir(&iter->dir, &iter->current_dirent);
        if (ret != 0 || iter->current_dirent.name[0] == 0) {
            // End of directory or an I/O error
            break;
        }

        if (iter->current_dirent.type == FS_DIR_ENTRY_FILE) {
            iter->has_current = true;
            ares = ASTARTE_RESULT_OK;
            break;
        }
    }

    sys_mutex_unlock(&astarte_storage_key_value_mutex);

    return ares;
}

astarte_result_t astarte_storage_key_value_iterator_get(
    astarte_storage_key_value_iter_t *iter, void *key, size_t *key_size)
{
    if (!iter->has_current) {
        return ASTARTE_RESULT_NOT_FOUND;
    }

    size_t len = strlen(iter->current_dirent.name);

    // If the key ptr is NULL, simply write out the length
    if (key == NULL) {
        *key_size = len + 1;
        return ASTARTE_RESULT_OK;
    }

    if (*key_size < len + 1) {
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    memcpy(key, iter->current_dirent.name, len + 1);
    *key_size = len + 1;

    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_storage_key_value_iterator_delete(astarte_storage_key_value_iter_t *iter)
{
    if (!iter->has_current) {
        return ASTARTE_RESULT_NOT_FOUND;
    }

    char *path = build_path_alloc(iter->kv_storage->base_path, iter->current_dirent.name);
    if (!path) {
        return ASTARTE_RESULT_OUT_OF_MEMORY;
    }

    sys_mutex_lock(&astarte_storage_key_value_mutex, K_FOREVER);
    int rc = fs_unlink(path);
    sys_mutex_unlock(&astarte_storage_key_value_mutex);

    free(path);

    if (rc != 0 && rc != -ENOENT) {
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }

    iter->has_current = false;
    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_storage_key_value_iterator_destroy(astarte_storage_key_value_iter_t *iter)
{
    if (!iter) {
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    sys_mutex_lock(&astarte_storage_key_value_mutex, K_FOREVER);
    int ret = fs_closedir(&iter->dir);
    iter->has_current = false;
    sys_mutex_unlock(&astarte_storage_key_value_mutex);

    return (ret == 0) ? ASTARTE_RESULT_OK : ASTARTE_RESULT_INTERNAL_ERROR;
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static char *build_path_alloc(const char *base_path, const char *key)
{
    char *path = NULL;
    size_t path_size = strlen(base_path) + 1 + strlen(key) + 1;
    path = calloc(path_size, sizeof(char));
    if (!path) {
        return NULL;
    }
    // TODO: evaluate the return
    snprintf(path, path_size, "%s/%s", base_path, key);
    return path;
}
