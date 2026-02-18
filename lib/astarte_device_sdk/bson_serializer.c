/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bson_serializer.h"

#include <stdio.h>
#include <stdlib.h>

#include <zephyr/sys/byteorder.h>

#include "bson_types.h"
#include "log.h"

ASTARTE_LOG_MODULE_REGISTER(bson_serializer, CONFIG_ASTARTE_DEVICE_SDK_BSON_LOG_LEVEL);

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

// When serializing a C array into a BSON array, this is the maximum allowed size of the string
// field array length. 12 chars corresponding to 999999999999 elements.
#define BSON_ARRAY_SIZE_STR_LEN 12

/************************************************
 *         Static functions declaration         *
 ***********************************************/

/**
 * @brief Serialize a input into an array of bytes in little endian.
 *
 * @param[in] input Input data.
 * @param[out] out Output serialized data.
 */
static void uint32_to_bytes(uint32_t input, uint8_t out[static sizeof(uint32_t)]);
/**
 * @brief Serialize a input into an array of bytes in little endian.
 *
 * @param[in] input Input data.
 * @param[out] out Output serialized data.
 */
static void int32_to_bytes(int32_t input, uint8_t out[static sizeof(int32_t)]);
/**
 * @brief Serialize a input into an array of bytes in little endian.
 *
 * @param[in] input Input data.
 * @param[out] out Output serialized data.
 */
static void uint64_to_bytes(uint64_t input, uint8_t out[static sizeof(uint64_t)]);
/**
 * @brief Serialize a input into an array of bytes in little endian.
 *
 * @param[in] input Input data.
 * @param[out] out Output serialized data.
 */
static void int64_to_bytes(int64_t input, uint8_t out[static sizeof(int64_t)]);
/**
 * @brief Serialize a input into an array of bytes in little endian.
 *
 * @param[in] input Input data.
 * @param[out] out Output serialized data.
 */
static void double_to_bytes(double input, uint8_t out[static sizeof(double)]);
/**
 * @brief Initialize a byte array.
 *
 * @param[out] bson The bson struct where to store the byte array.
 * @param[in] bytes The initial data in the byte array.
 * @param[in] size Size of @p bytes in bytes.
 * @return #ASTARTE_RESULT_OK on success, an error otherwise.
 */
static astarte_result_t byte_array_init(astarte_bson_serializer_t *bson, void *bytes, size_t size);
/**
 * @brief Destroy (free) a byte array.
 *
 * @param[in] bson The bson struct where the byte array is stored.
 */
static void byte_array_destroy(astarte_bson_serializer_t *bson);
/**
 * @brief Expand (realloc) a byte array.
 *
 * @param[in,out] bson The bson struct where the byte array is stored.
 * @param[in] needed_size New required size for the array.
 * @return #ASTARTE_RESULT_OK on success, an error otherwise.
 */
static astarte_result_t byte_array_grow(astarte_bson_serializer_t *bson, size_t needed_size);
/**
 * @brief Append a byte in the byte array.
 *
 * @param[in,out] bson The bson struct where the byte array is stored.
 * @param[in] byte Byte to append.
 * @return #ASTARTE_RESULT_OK on success, an error otherwise.
 */
static astarte_result_t byte_array_append_byte(astarte_bson_serializer_t *bson, uint8_t byte);
/**
 * @brief Append a byte array to the byte array.
 *
 * @param[in,out] bson The bson struct where the byte array is stored.
 * @param[in] bytes The byte array to append.
 * @param[in] count The size of @p bytes in bytes.
 * @return #ASTARTE_RESULT_OK on success, an error otherwise.
 */
static astarte_result_t byte_array_append(
    astarte_bson_serializer_t *bson, const void *bytes, size_t count);
/**
 * @brief Replace a portion of data in a byte array.
 *
 * @param[in,out] bson The bson struct where the byte array is stored.
 * @param[in] pos The position within the byte array where the replacing should start.
 * @param[in] bytes The new content to use.
 * @param[in] count The size of @p bytes in bytes.
 * @return #ASTARTE_RESULT_OK on success, an error otherwise.
 */
static void byte_array_replace(
    astarte_bson_serializer_t *bson, unsigned int pos, const uint8_t *bytes, size_t count);

/************************************************
 * Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_bson_serializer_init(astarte_bson_serializer_t *bson)
{
    astarte_result_t ares = byte_array_init(bson, "\0\0\0\0", 4);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Unable to initialize byte_array");
    }
    return ares;
}

void astarte_bson_serializer_destroy(astarte_bson_serializer_t *bson)
{
    byte_array_destroy(bson);
}

const void *astarte_bson_serializer_get_serialized(const astarte_bson_serializer_t *bson, int *size)
{
    if (size) {
        *size = (int) bson->size;
    }
    return bson->buf;
}

astarte_result_t astarte_bson_serializer_append_end_of_document(astarte_bson_serializer_t *bson)
{
    astarte_result_t res = byte_array_append_byte(bson, '\0');
    if (res != ASTARTE_RESULT_OK) {
        return res;
    }

    uint8_t size_buf[4] = { 0 };
    uint32_to_bytes(bson->size, size_buf);

    byte_array_replace(bson, 0, size_buf, sizeof(int32_t));
    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_bson_serializer_append_double(
    astarte_bson_serializer_t *bson, const char *name, double value)
{
    uint8_t val_buf[sizeof(double)] = { 0 };
    double_to_bytes(value, val_buf);

    astarte_result_t res = byte_array_append_byte(bson, ASTARTE_BSON_TYPE_DOUBLE);
    if (res == ASTARTE_RESULT_OK) {
        res = byte_array_append(bson, name, strlen(name) + 1);
    }
    if (res == ASTARTE_RESULT_OK) {
        res = byte_array_append(bson, val_buf, sizeof(double));
    }
    return res;
}

astarte_result_t astarte_bson_serializer_append_int32(
    astarte_bson_serializer_t *bson, const char *name, int32_t value)
{
    uint8_t val_buf[4] = { 0 };
    int32_to_bytes(value, val_buf);

    astarte_result_t res = byte_array_append_byte(bson, ASTARTE_BSON_TYPE_INT32);
    if (res == ASTARTE_RESULT_OK) {
        res = byte_array_append(bson, name, strlen(name) + 1);
    }
    if (res == ASTARTE_RESULT_OK) {
        res = byte_array_append(bson, val_buf, sizeof(int32_t));
    }
    return res;
}

astarte_result_t astarte_bson_serializer_append_int64(
    astarte_bson_serializer_t *bson, const char *name, int64_t value)
{
    uint8_t val_buf[sizeof(int64_t)] = { 0 };
    int64_to_bytes(value, val_buf);

    astarte_result_t res = byte_array_append_byte(bson, ASTARTE_BSON_TYPE_INT64);
    if (res == ASTARTE_RESULT_OK) {
        res = byte_array_append(bson, name, strlen(name) + 1);
    }
    if (res == ASTARTE_RESULT_OK) {
        res = byte_array_append(bson, val_buf, sizeof(int64_t));
    }
    return res;
}

astarte_result_t astarte_bson_serializer_append_binary(
    astarte_bson_serializer_t *bson, const char *name, const void *value, size_t size)
{
    uint8_t len_buf[4] = { 0 };
    uint32_to_bytes(size, len_buf);

    astarte_result_t res = byte_array_append_byte(bson, ASTARTE_BSON_TYPE_BINARY);
    if (res == ASTARTE_RESULT_OK) {
        res = byte_array_append(bson, name, strlen(name) + 1);
    }
    if (res == ASTARTE_RESULT_OK) {
        res = byte_array_append(bson, len_buf, sizeof(int32_t));
    }
    if (res == ASTARTE_RESULT_OK) {
        res = byte_array_append_byte(bson, ASTARTE_BSON_SUBTYPE_DEFAULT_BINARY);
    }
    if (res == ASTARTE_RESULT_OK) {
        res = byte_array_append(bson, value, size);
    }
    return res;
}

astarte_result_t astarte_bson_serializer_append_string(
    astarte_bson_serializer_t *bson, const char *name, const char *string)
{
    size_t string_len = strlen(string);

    uint8_t len_buf[4] = { 0 };
    uint32_to_bytes(string_len + 1, len_buf);

    astarte_result_t res = byte_array_append_byte(bson, ASTARTE_BSON_TYPE_STRING);
    if (res == ASTARTE_RESULT_OK) {
        res = byte_array_append(bson, name, strlen(name) + 1);
    }
    if (res == ASTARTE_RESULT_OK) {
        res = byte_array_append(bson, len_buf, sizeof(int32_t));
    }
    if (res == ASTARTE_RESULT_OK) {
        res = byte_array_append(bson, string, string_len + 1);
    }
    return res;
}

astarte_result_t astarte_bson_serializer_append_datetime(
    astarte_bson_serializer_t *bson, const char *name, uint64_t epoch_millis)
{
    uint8_t val_buf[sizeof(uint64_t)] = { 0 };
    uint64_to_bytes(epoch_millis, val_buf);

    astarte_result_t res = byte_array_append_byte(bson, ASTARTE_BSON_TYPE_DATETIME);
    if (res == ASTARTE_RESULT_OK) {
        res = byte_array_append(bson, name, strlen(name) + 1);
    }
    if (res == ASTARTE_RESULT_OK) {
        res = byte_array_append(bson, val_buf, sizeof(uint64_t));
    }
    return res;
}

astarte_result_t astarte_bson_serializer_append_boolean(
    astarte_bson_serializer_t *bson, const char *name, bool value)
{
    astarte_result_t res = byte_array_append_byte(bson, ASTARTE_BSON_TYPE_BOOLEAN);
    if (res == ASTARTE_RESULT_OK) {
        res = byte_array_append(bson, name, strlen(name) + 1);
    }
    if (res == ASTARTE_RESULT_OK) {
        res = byte_array_append_byte(bson, value ? '\1' : '\0');
    }
    return res;
}

astarte_result_t astarte_bson_serializer_append_document(
    astarte_bson_serializer_t *bson, const char *name, const void *document)
{
    uint32_t size = 0U;
    memcpy(&size, document, sizeof(uint32_t));
    size = sys_le32_to_cpu(size);

    astarte_result_t res = byte_array_append_byte(bson, ASTARTE_BSON_TYPE_DOCUMENT);
    if (res == ASTARTE_RESULT_OK) {
        res = byte_array_append(bson, name, strlen(name) + 1);
    }
    if (res == ASTARTE_RESULT_OK) {
        res = byte_array_append(bson, document, size);
    }
    return res;
}

#define IMPLEMENT_ASTARTE_BSON_SERIALIZER_APPEND_TYPE_ARRAY(TYPE, TYPE_NAME)                       \
    astarte_result_t astarte_bson_serializer_append_##TYPE_NAME##_array(                           \
        astarte_bson_serializer_t *bson, const char *name, TYPE arr, int count)                    \
    {                                                                                              \
        astarte_bson_serializer_t array_ser = { 0 };                                               \
        astarte_result_t ares = astarte_bson_serializer_init(&array_ser);                          \
        if (ares != ASTARTE_RESULT_OK) {                                                           \
            return ares;                                                                           \
        }                                                                                          \
        for (int i = 0; i < count; i++) {                                                          \
            char key[BSON_ARRAY_SIZE_STR_LEN] = { 0 };                                             \
            int ret = snprintf(key, BSON_ARRAY_SIZE_STR_LEN, "%i", i);                             \
            if ((ret < 0) || (ret >= BSON_ARRAY_SIZE_STR_LEN)) {                                   \
                ares = ASTARTE_RESULT_INTERNAL_ERROR;                                              \
            }                                                                                      \
            if (ares == ASTARTE_RESULT_OK) {                                                       \
                ares = astarte_bson_serializer_append_##TYPE_NAME(&array_ser, key, arr[i]);        \
            }                                                                                      \
            if (ares != ASTARTE_RESULT_OK) {                                                       \
                astarte_bson_serializer_destroy(&array_ser);                                       \
                return ares;                                                                       \
            }                                                                                      \
        }                                                                                          \
        ares = astarte_bson_serializer_append_end_of_document(&array_ser);                         \
        if (ares != ASTARTE_RESULT_OK) {                                                           \
            astarte_bson_serializer_destroy(&array_ser);                                           \
            return ares;                                                                           \
        }                                                                                          \
                                                                                                   \
        int size;                                                                                  \
        const void *document = astarte_bson_serializer_get_serialized(&array_ser, &size);          \
                                                                                                   \
        ares = byte_array_append_byte(bson, ASTARTE_BSON_TYPE_ARRAY);                              \
        if (ares == ASTARTE_RESULT_OK) {                                                           \
            ares = byte_array_append(bson, name, strlen(name) + 1);                                \
        }                                                                                          \
        if (ares == ASTARTE_RESULT_OK) {                                                           \
            ares = byte_array_append(bson, document, size);                                        \
        }                                                                                          \
                                                                                                   \
        astarte_bson_serializer_destroy(&array_ser);                                               \
                                                                                                   \
        return ares;                                                                               \
    }

IMPLEMENT_ASTARTE_BSON_SERIALIZER_APPEND_TYPE_ARRAY(const double *, double)
IMPLEMENT_ASTARTE_BSON_SERIALIZER_APPEND_TYPE_ARRAY(const int32_t *, int32)
IMPLEMENT_ASTARTE_BSON_SERIALIZER_APPEND_TYPE_ARRAY(const int64_t *, int64)
IMPLEMENT_ASTARTE_BSON_SERIALIZER_APPEND_TYPE_ARRAY(const char *const *, string)
IMPLEMENT_ASTARTE_BSON_SERIALIZER_APPEND_TYPE_ARRAY(const int64_t *, datetime)
IMPLEMENT_ASTARTE_BSON_SERIALIZER_APPEND_TYPE_ARRAY(const bool *, boolean)

astarte_result_t astarte_bson_serializer_append_binary_array(astarte_bson_serializer_t *bson,
    const char *name, const void *const *arr, const size_t *sizes, int count)
{
    astarte_bson_serializer_t array_ser = { 0 };
    astarte_result_t ares = astarte_bson_serializer_init(&array_ser);
    if (ares != ASTARTE_RESULT_OK) {
        return ares;
    }
    for (int i = 0; i < count; i++) {
        char key[BSON_ARRAY_SIZE_STR_LEN] = { 0 };
        int ret = snprintf(key, BSON_ARRAY_SIZE_STR_LEN, "%i", i);
        if ((ret < 0) || (ret >= BSON_ARRAY_SIZE_STR_LEN)) {
            ares = ASTARTE_RESULT_INTERNAL_ERROR;
        }
        if (ares == ASTARTE_RESULT_OK) {
            ares = astarte_bson_serializer_append_binary(&array_ser, key, arr[i], sizes[i]);
        }
        if (ares != ASTARTE_RESULT_OK) {
            astarte_bson_serializer_destroy(&array_ser);
            return ares;
        }
    }
    ares = astarte_bson_serializer_append_end_of_document(&array_ser);
    if (ares != ASTARTE_RESULT_OK) {
        astarte_bson_serializer_destroy(&array_ser);
        return ares;
    }

    int size = 0;
    const void *document = astarte_bson_serializer_get_serialized(&array_ser, &size);

    ares = byte_array_append_byte(bson, ASTARTE_BSON_TYPE_ARRAY);
    if (ares == ASTARTE_RESULT_OK) {
        ares = byte_array_append(bson, name, strlen(name) + 1);
    }
    if (ares == ASTARTE_RESULT_OK) {
        ares = byte_array_append(bson, document, size);
    }

    astarte_bson_serializer_destroy(&array_ser);

    return ares;
}

/************************************************
 * Static functions definitions         *
 ***********************************************/

static void uint32_to_bytes(uint32_t input, uint8_t out[static sizeof(uint32_t)])
{
    uint32_t tmp = sys_cpu_to_le32(input);
    memcpy(out, &tmp, sizeof(tmp));
}

static void int32_to_bytes(int32_t input, uint8_t out[static sizeof(int32_t)])
{
    uint32_t raw_bits = 0;
    memcpy(&raw_bits, &input, sizeof(int32_t));
    uint32_t tmp = sys_cpu_to_le32(raw_bits);
    memcpy(out, &tmp, sizeof(tmp));
}

static void uint64_to_bytes(uint64_t input, uint8_t out[static sizeof(uint64_t)])
{
    uint64_t tmp = sys_cpu_to_le64(input);
    memcpy(out, &tmp, sizeof(tmp));
}

static void int64_to_bytes(int64_t input, uint8_t out[static sizeof(int64_t)])
{
    int64_t raw_bits = 0;
    memcpy(&raw_bits, &input, sizeof(int64_t));
    int64_t tmp = sys_cpu_to_le64(raw_bits);
    memcpy(out, &tmp, sizeof(tmp));
}

static void double_to_bytes(double input, uint8_t out[static sizeof(double)])
{
    uint64_t raw_bits = 0;
    memcpy(&raw_bits, &input, sizeof(double));
    uint64_t tmp = sys_cpu_to_le64(raw_bits);
    memcpy(out, &tmp, sizeof(tmp));
}

static astarte_result_t byte_array_init(astarte_bson_serializer_t *bson, void *bytes, size_t size)
{
    bson->capacity = size;
    bson->size = size;
    bson->buf = malloc(size);

    if (!bson->buf) {
        ASTARTE_LOG_ERR("Cannot allocate memory for BSON payload (size: %zu)!", size);
        return ASTARTE_RESULT_OUT_OF_MEMORY;
    }

    memcpy(bson->buf, bytes, size);
    return ASTARTE_RESULT_OK;
}

static void byte_array_destroy(astarte_bson_serializer_t *bson)
{
    bson->capacity = 0;
    bson->size = 0;
    free(bson->buf);
    bson->buf = NULL;
}

static astarte_result_t byte_array_grow(astarte_bson_serializer_t *bson, size_t needed_size)
{
    if (bson->size + needed_size > bson->capacity) {
        size_t new_capacity = MAX(bson->capacity * 2, 64);
        if (new_capacity < bson->capacity + needed_size) {
            new_capacity = bson->capacity + needed_size;
        }
        void *new_buf = realloc(bson->buf, new_capacity);
        if (!new_buf) {
            ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
            return ASTARTE_RESULT_OUT_OF_MEMORY;
        }
        bson->capacity = new_capacity;
        bson->buf = new_buf;
    }
    return ASTARTE_RESULT_OK;
}

static astarte_result_t byte_array_append_byte(astarte_bson_serializer_t *bson, uint8_t byte)
{
    astarte_result_t res = byte_array_grow(bson, sizeof(uint8_t));
    if (res != ASTARTE_RESULT_OK) {
        return res;
    }
    bson->buf[bson->size] = byte;
    bson->size++;
    return ASTARTE_RESULT_OK;
}

static astarte_result_t byte_array_append(
    astarte_bson_serializer_t *bson, const void *bytes, size_t count)
{
    astarte_result_t res = byte_array_grow(bson, count);
    if (res != ASTARTE_RESULT_OK) {
        return res;
    }
    memcpy(bson->buf + bson->size, bytes, count);
    bson->size += count;
    return ASTARTE_RESULT_OK;
}

static void byte_array_replace(
    astarte_bson_serializer_t *bson, unsigned int pos, const uint8_t *bytes, size_t count)
{
    memcpy(bson->buf + pos, bytes, count);
}
