/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "astarte_device_sdk/bson_serializer.h"
#include "astarte_device_sdk/bson_types.h"

#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(bson_serializer, CONFIG_ASTARTE_DEVICE_SDK_BSON_LOG_LEVEL); // NOLINT

// When serializing a C array into a BSON array, this is the maximum allowed size of the string
// field array length. 12 chars corresponding to 999999999999 elements.
#define BSON_ARRAY_SIZE_STR_LEN 12

struct bson_serializer_t
{
    size_t capacity;
    size_t size;
    uint8_t *buf;
};

static void uint32_to_bytes(uint32_t input, uint8_t out[static sizeof(uint32_t)])
{
    uint32_t tmp = sys_cpu_to_le32(input); // NOLINT(hicpp-signed-bitwise)
    for (size_t i = 0; i < sizeof(uint32_t); i++) {
        out[i] = ((uint8_t *) &tmp)[i];
    }
}

static void int32_to_bytes(int32_t input, uint8_t out[static sizeof(int32_t)])
{
    uint32_t tmp = sys_cpu_to_le32(*((uint32_t *) &input)); // NOLINT(hicpp-signed-bitwise)
    for (size_t i = 0; i < sizeof(uint32_t); i++) {
        out[i] = ((uint8_t *) &tmp)[i];
    }
}

static void uint64_to_bytes(uint64_t input, uint8_t out[static sizeof(uint64_t)])
{
    uint64_t tmp = sys_cpu_to_le64(input); // NOLINT(hicpp-signed-bitwise)
    for (size_t i = 0; i < sizeof(uint64_t); i++) {
        out[i] = ((uint8_t *) &tmp)[i];
    }
}

static void int64_to_bytes(int64_t input, uint8_t out[static sizeof(int64_t)])
{
    uint64_t tmp = sys_cpu_to_le64(*((uint64_t *) &input)); // NOLINT(hicpp-signed-bitwise)
    for (size_t i = 0; i < sizeof(uint64_t); i++) {
        out[i] = ((uint8_t *) &tmp)[i];
    }
}

static void double_to_bytes(double input, uint8_t out[static sizeof(double)])
{
    uint64_t tmp = sys_cpu_to_le64(*((uint64_t *) &input)); // NOLINT(hicpp-signed-bitwise)
    for (size_t i = 0; i < sizeof(uint64_t); i++) {
        out[i] = ((uint8_t *) &tmp)[i];
    }
}

static astarte_err_t byte_array_init(
    bson_serializer_handle_t bson_serializer_handle, void *bytes, size_t size)
{
    bson_serializer_handle->capacity = size;
    bson_serializer_handle->size = size;
    bson_serializer_handle->buf = malloc(size);

    if (!bson_serializer_handle->buf) {
        LOG_ERR("Cannot allocate memory for BSON payload (size: %zu)!", size); // NOLINT
        return ASTARTE_ERR_OUT_OF_MEMORY;
    }

    memcpy(bson_serializer_handle->buf, bytes, size);
    return ASTARTE_OK;
}

static void byte_array_destroy(bson_serializer_handle_t bson_serializer_handle)
{
    bson_serializer_handle->capacity = 0;
    bson_serializer_handle->size = 0;
    free(bson_serializer_handle->buf);
    bson_serializer_handle->buf = NULL;
}

static void byte_array_grow(bson_serializer_handle_t bson_serializer_handle, size_t needed_size)
{
    if (bson_serializer_handle->size + needed_size >= bson_serializer_handle->capacity) {
        size_t new_capacity = bson_serializer_handle->capacity * 2;
        if (new_capacity < bson_serializer_handle->capacity + needed_size) {
            new_capacity = bson_serializer_handle->capacity + needed_size;
        }
        bson_serializer_handle->capacity = new_capacity;
        void *new_buf = malloc(new_capacity);
        if (!new_buf) {
            LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__); // NOLINT
            abort();
        }
        // NOLINTNEXTLINE(clang-analyzer-unix.cstring.NullArg)
        memcpy(new_buf, bson_serializer_handle->buf, bson_serializer_handle->size);
        free(bson_serializer_handle->buf);
        bson_serializer_handle->buf = new_buf;
    }
}

static void byte_array_append_byte(bson_serializer_handle_t bson_serializer_handle, uint8_t byte)
{
    byte_array_grow(bson_serializer_handle, sizeof(uint8_t));
    bson_serializer_handle->buf[bson_serializer_handle->size] = byte;
    bson_serializer_handle->size++;
}

static void byte_array_append(
    bson_serializer_handle_t bson_serializer_handle, const void *bytes, size_t count)
{
    byte_array_grow(bson_serializer_handle, count);

    memcpy(bson_serializer_handle->buf + bson_serializer_handle->size, bytes, count);
    bson_serializer_handle->size += count;
}

static void byte_array_replace(bson_serializer_handle_t bson_serializer_handle, unsigned int pos,
    size_t count, const uint8_t *bytes)
{
    memcpy(bson_serializer_handle->buf + pos, bytes, count);
}

// NOLINTNEXTLINE(hicpp-function-size)
bson_serializer_handle_t bson_serializer_new(void)
{
    bson_serializer_handle_t bson = calloc(1, sizeof(struct bson_serializer_t));
    if (!bson) {
        LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__); // NOLINT
        return NULL;
    }

    if (byte_array_init(bson, "\0\0\0\0", 4) != ASTARTE_OK) {
        LOG_ERR("Unable to initialize byte_array"); // NOLINT
        free(bson);
        return NULL;
    }
    return bson;
}

void bson_serializer_destroy(bson_serializer_handle_t bson)
{
    byte_array_destroy(bson);
    free(bson);
}

const void *bson_serializer_get_document(bson_serializer_handle_t bson, int *size)
{
    if (size) {
        *size = (int) bson->size;
    }
    return bson->buf;
}

astarte_err_t bson_serializer_serialize_document(
    bson_serializer_handle_t bson, void *out_buf, int out_buf_size, int *out_doc_size)
{
    size_t doc_size = bson->size;
    if (out_doc_size) {
        *out_doc_size = (int) doc_size;
    }

    if (out_buf_size < bson->size) {
        return ASTARTE_ERR;
    }

    memcpy(out_buf, bson->buf, doc_size);

    return ASTARTE_OK;
}

size_t bson_serializer_document_size(bson_serializer_handle_t bson)
{
    return bson->size;
}

void bson_serializer_append_end_of_document(bson_serializer_handle_t bson)
{
    byte_array_append_byte(bson, '\0');

    uint8_t size_buf[4];
    uint32_to_bytes(bson->size, size_buf);

    byte_array_replace(bson, 0, sizeof(int32_t), size_buf);
}

void bson_serializer_append_double(bson_serializer_handle_t bson, const char *name, double value)
{
    uint8_t val_buf[sizeof(double)];
    double_to_bytes(value, val_buf);

    byte_array_append_byte(bson, BSON_TYPE_DOUBLE);
    byte_array_append(bson, name, strlen(name) + sizeof(char));
    byte_array_append(bson, val_buf, sizeof(double));
}

void bson_serializer_append_int32(bson_serializer_handle_t bson, const char *name, int32_t value)
{
    uint8_t val_buf[4];
    int32_to_bytes(value, val_buf);

    byte_array_append_byte(bson, BSON_TYPE_INT32);
    byte_array_append(bson, name, strlen(name) + sizeof(char));
    byte_array_append(bson, val_buf, sizeof(int32_t));
}

void bson_serializer_append_int64(bson_serializer_handle_t bson, const char *name, int64_t value)
{
    uint8_t val_buf[sizeof(int64_t)];
    int64_to_bytes(value, val_buf);

    byte_array_append_byte(bson, BSON_TYPE_INT64);
    byte_array_append(bson, name, strlen(name) + sizeof(char));
    byte_array_append(bson, val_buf, sizeof(int64_t));
}

void bson_serializer_append_binary(
    bson_serializer_handle_t bson, const char *name, const void *value, size_t size)
{
    uint8_t len_buf[4];
    uint32_to_bytes(size, len_buf);

    byte_array_append_byte(bson, BSON_TYPE_BINARY);
    byte_array_append(bson, name, strlen(name) + sizeof(char));
    byte_array_append(bson, len_buf, sizeof(int32_t));
    byte_array_append_byte(bson, BSON_SUBTYPE_DEFAULT_BINARY);
    byte_array_append(bson, value, size);
}

void bson_serializer_append_string(
    bson_serializer_handle_t bson, const char *name, const char *string)
{
    size_t string_len = strlen(string);

    uint8_t len_buf[4];
    uint32_to_bytes(string_len + 1, len_buf);

    byte_array_append_byte(bson, BSON_TYPE_STRING);
    byte_array_append(bson, name, strlen(name) + sizeof(char));
    byte_array_append(bson, len_buf, sizeof(int32_t));
    byte_array_append(bson, string, string_len + sizeof(char));
}

void bson_serializer_append_datetime(
    bson_serializer_handle_t bson, const char *name, uint64_t epoch_millis)
{
    uint8_t val_buf[sizeof(uint64_t)];
    uint64_to_bytes(epoch_millis, val_buf);

    byte_array_append_byte(bson, BSON_TYPE_DATETIME);
    byte_array_append(bson, name, strlen(name) + sizeof(char));
    byte_array_append(bson, val_buf, sizeof(uint64_t));
}

void bson_serializer_append_boolean(bson_serializer_handle_t bson, const char *name, bool value)
{
    byte_array_append_byte(bson, BSON_TYPE_BOOLEAN);
    byte_array_append(bson, name, strlen(name) + sizeof(char));
    byte_array_append_byte(bson, value ? '\1' : '\0');
}

void bson_serializer_append_document(
    bson_serializer_handle_t bson, const char *name, const void *document)
{
    uint32_t size = 0U;
    memcpy(&size, document, sizeof(uint32_t));
    size = sys_le32_to_cpu(size);

    byte_array_append_byte(bson, BSON_TYPE_DOCUMENT);
    byte_array_append(bson, name, strlen(name) + sizeof(char));

    byte_array_append(bson, document, size);
}

#define IMPLEMENT_ASTARTE_BSON_SERIALIZER_APPEND_TYPE_ARRAY(TYPE, TYPE_NAME)                       \
    astarte_err_t bson_serializer_append_##TYPE_NAME##_array(                                      \
        bson_serializer_handle_t bson, const char *name, TYPE arr, int count)                      \
    {                                                                                              \
        astarte_err_t result = ASTARTE_OK;                                                         \
        bson_serializer_handle_t array_ser = bson_serializer_new();                                \
        for (int i = 0; i < count; i++) {                                                          \
            char key[BSON_ARRAY_SIZE_STR_LEN] = { 0 };                                             \
            int ret = snprintf(key, BSON_ARRAY_SIZE_STR_LEN, "%i", i);                             \
            if ((ret < 0) || (ret >= BSON_ARRAY_SIZE_STR_LEN)) {                                   \
                result = ASTARTE_ERR;                                                              \
            }                                                                                      \
            bson_serializer_append_##TYPE_NAME(array_ser, key, arr[i]);                            \
        }                                                                                          \
        bson_serializer_append_end_of_document(array_ser);                                         \
                                                                                                   \
        int size;                                                                                  \
        const void *document = bson_serializer_get_document(array_ser, &size);                     \
                                                                                                   \
        byte_array_append_byte(bson, BSON_TYPE_ARRAY);                                             \
        byte_array_append(bson, name, strlen(name) + sizeof(char));                                \
                                                                                                   \
        byte_array_append(bson, document, size);                                                   \
                                                                                                   \
        bson_serializer_destroy(array_ser);                                                        \
                                                                                                   \
        return result;                                                                             \
    }

IMPLEMENT_ASTARTE_BSON_SERIALIZER_APPEND_TYPE_ARRAY(const double *, double)
IMPLEMENT_ASTARTE_BSON_SERIALIZER_APPEND_TYPE_ARRAY(const int32_t *, int32)
IMPLEMENT_ASTARTE_BSON_SERIALIZER_APPEND_TYPE_ARRAY(const int64_t *, int64)
IMPLEMENT_ASTARTE_BSON_SERIALIZER_APPEND_TYPE_ARRAY(const char *const *, string)
IMPLEMENT_ASTARTE_BSON_SERIALIZER_APPEND_TYPE_ARRAY(const int64_t *, datetime)
IMPLEMENT_ASTARTE_BSON_SERIALIZER_APPEND_TYPE_ARRAY(const bool *, boolean)

astarte_err_t bson_serializer_append_binary_array(bson_serializer_handle_t bson, const char *name,
    const void *const *arr, const int *sizes, int count)
{
    astarte_err_t result = ASTARTE_OK;
    bson_serializer_handle_t array_ser = bson_serializer_new();
    for (int i = 0; i < count; i++) {
        char key[BSON_ARRAY_SIZE_STR_LEN] = { 0 };
        int ret = snprintf(key, BSON_ARRAY_SIZE_STR_LEN, "%i", i);
        if ((ret < 0) || (ret >= BSON_ARRAY_SIZE_STR_LEN)) {
            result = ASTARTE_ERR;
        }
        bson_serializer_append_binary(array_ser, key, arr[i], sizes[i]);
    }
    bson_serializer_append_end_of_document(array_ser);

    int size = 0;
    const void *document = bson_serializer_get_document(array_ser, &size);

    byte_array_append_byte(bson, BSON_TYPE_ARRAY);
    byte_array_append(bson, name, strlen(name) + sizeof(char));
    byte_array_append(bson, document, size);

    bson_serializer_destroy(array_ser);

    return result;
}
