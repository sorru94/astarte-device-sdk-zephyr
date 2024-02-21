/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "astarte_device_sdk/bson_deserializer.h"
#include "astarte_device_sdk/bson_types.h"

#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

#include <inttypes.h>
#include <string.h>

LOG_MODULE_REGISTER(bson_deserializer, CONFIG_ASTARTE_DEVICE_SDK_BSON_LOG_LEVEL); // NOLINT

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

#define NULL_TERM_SIZE 1

/************************************************
 *         Static functions declaration         *
 ***********************************************/

/**
 * @brief Cast the first four bytes of a little-endian buffer to a uint32_t in the host byte order.
 *
 * @details This function expects the input buffer to be in little-endian order.
 * @param[in] buff Buffer containing the data to read.
 * @return Resulting uint32_t value.
 */
static uint32_t read_uint32(const void *buff);

/**
 * @brief Cast the first eight bytes of a little-endian buffer to a uint64_t in the host byte order.
 *
 * @details This function expects the input buffer to be in little-endian order.
 * @param[in] buff Buffer containing the data to read.
 * @return Resulting uint64_t value.
 */
static uint64_t read_uint64(const void *buff);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

// NOLINTNEXTLINE(hicpp-function-size)
bool bson_deserializer_check_validity(const void *buffer, size_t buffer_size)
{
    bson_document_t document;

    // Validate buffer size is at least 5, the size of an empty document.
    if (buffer_size < sizeof(document.size) + NULL_TERM_SIZE) {
        LOG_WRN("Buffer too small: no BSON document found"); // NOLINT
        return false;
    }

    document = bson_deserializer_init_doc(buffer);

    // Ensure the buffer is larger or equal compared to the decoded document size
    if (buffer_size < document.size) {
        LOG_WRN("Allocated buffer size (%i) is smaller than BSON document size (%" PRIu32 // NOLINT
                ")", // NOLINT
            buffer_size, document.size); // NOLINT
        return false;
    }

    // Check document is terminated with 0x00
    if (*(const char *) ((uint8_t *) document.list + (document.size - sizeof(document.size)) - 1)
        != 0) {
        LOG_WRN("BSON document is not terminated by null byte."); // NOLINT
        return false;
    }

    // Validation for an empty document is over
    if (document.size == sizeof(document.size) + NULL_TERM_SIZE) {
        return true;
    }

    // Check on the minimum buffer size for a non empty document, composed of at least:
    // - 4 bytes for the document size
    // - 1 byte for the element index
    // - 1 byte for the element name (could be an empty string)
    // - 1 byte for the element content (for example a boolean)
    // - 1 byte for the trailing 0x00
    // NB this check could fail on the NULL value element described in the BSON specifications
    if (document.size < sizeof(document.size) + 3 + NULL_TERM_SIZE) {
        LOG_WRN("BSON data too small"); // NOLINT
        return false;
    }

    // Check that the first element of the document has a supported index
    switch (*(const char *) document.list) {
        case BSON_TYPE_DOUBLE:
        case BSON_TYPE_STRING:
        case BSON_TYPE_DOCUMENT:
        case BSON_TYPE_ARRAY:
        case BSON_TYPE_BINARY:
        case BSON_TYPE_BOOLEAN:
        case BSON_TYPE_DATETIME:
        case BSON_TYPE_INT32:
        case BSON_TYPE_INT64:
            break;
        default:
            LOG_WRN("Unrecognized BSON document first type\n"); // NOLINT
            return false;
    }

    return true;
}

bson_document_t bson_deserializer_init_doc(const void *buffer)
{
    bson_document_t document;
    document.size = read_uint32(buffer);
    document.list = (uint8_t *) buffer + sizeof(document.size);
    document.list_size = document.size - sizeof(document.size) - NULL_TERM_SIZE;
    return document;
}

astarte_err_t bson_deserializer_first_element(bson_document_t document, bson_element_t *element)
{
    // Document should not be empty
    if (document.size <= sizeof(document.size) + NULL_TERM_SIZE) {
        return ASTARTE_ERR_NOT_FOUND;
    }

    element->type = *(uint8_t *) document.list;
    element->name = (const char *) ((uint8_t *) document.list + sizeof(element->type));

    // TODO Replace with strnlen after this
    // [PR#68381](https://github.com/zephyrproject-rtos/zephyr/pull/68381) will be merged
    // size_t max_name_str_len = document.list_size - sizeof(element->type);
    // element->name_len = strnlen(element->name, max_name_str_len);
    element->name_len = strlen(element->name);

    element->value
        = (uint8_t *) document.list + sizeof(element->type) + element->name_len + NULL_TERM_SIZE;
    return ASTARTE_OK;
}

astarte_err_t bson_deserializer_next_element(
    bson_document_t document, bson_element_t curr_element, bson_element_t *next_element)
{
    // Get the size of the current element
    size_t element_value_size = 0U;
    switch (curr_element.type) {
        case BSON_TYPE_STRING: {
            element_value_size = sizeof(int32_t) + read_uint32(curr_element.value);
            break;
        }
        case BSON_TYPE_ARRAY:
        case BSON_TYPE_DOCUMENT: {
            element_value_size = read_uint32(curr_element.value);
            break;
        }
        case BSON_TYPE_BINARY: {
            element_value_size = sizeof(int32_t) + sizeof(int8_t) + read_uint32(curr_element.value);
            break;
        }
        case BSON_TYPE_INT32: {
            element_value_size = sizeof(int32_t);
            break;
        }
        case BSON_TYPE_DOUBLE:
        case BSON_TYPE_DATETIME:
        case BSON_TYPE_INT64: {
            element_value_size = sizeof(int64_t);
            break;
        }
        case BSON_TYPE_BOOLEAN: {
            element_value_size = sizeof(int8_t);
            break;
        }
        default: {
            LOG_WRN("unrecognized BSON type: %i", (int) curr_element.type); // NOLINT
            return ASTARTE_ERR;
        }
    }

    const void *next_element_start = (uint8_t *) curr_element.value + element_value_size;

    // Check if we are not looking past the end of the document
    const void *end_of_document = (uint8_t *) document.list + document.list_size;
    if (next_element_start >= end_of_document) {
        return ASTARTE_ERR_NOT_FOUND;
    }

    next_element->type = *(uint8_t *) next_element_start;
    next_element->name
        = (const char *) ((uint8_t *) next_element_start + sizeof(next_element->type));

    // TODO Replace with strnlen after this
    // [PR#68381](https://github.com/zephyrproject-rtos/zephyr/pull/68381) will be merged
    // size_t max_name_str_len = document.list_size
    //        - (size_t) ((uint8_t *) document.list - (uint8_t *) next_element_start)
    //        - sizeof(next_element->type);
    // next_element->name_len = strnlen(next_element->name, max_name_str_len);
    next_element->name_len = strlen(next_element->name);

    next_element->value = (uint8_t *) next_element_start + sizeof(next_element->type)
        + next_element->name_len + NULL_TERM_SIZE;
    return ASTARTE_OK;
}

astarte_err_t bson_deserializer_element_lookup(
    bson_document_t document, const char *key, bson_element_t *element)
{
    bson_element_t candidate_element;
    astarte_err_t err = bson_deserializer_first_element(document, &candidate_element);
    if (err != ASTARTE_OK) {
        return err;
    }

    while (strncmp(key, candidate_element.name, candidate_element.name_len + NULL_TERM_SIZE) != 0) {
        err = bson_deserializer_next_element(document, candidate_element, &candidate_element);
        if (err != ASTARTE_OK) {
            return err;
        }
    }

    // When match has been found copy the candidate element in the out struct
    element->type = candidate_element.type;
    element->name = candidate_element.name;
    element->name_len = candidate_element.name_len;
    element->value = candidate_element.value;

    return err;
}

double bson_deserializer_element_to_double(bson_element_t element)
{
    uint64_t value = read_uint64(element.value);
    return ((double *) &value)[0];
}

const char *bson_deserializer_element_to_string(bson_element_t element, uint32_t *len)
{
    if (len) {
        *len = read_uint32(element.value) - NULL_TERM_SIZE;
    }
    return (const char *) ((uint8_t *) element.value + sizeof(uint32_t));
}

bson_document_t bson_deserializer_element_to_document(bson_element_t element)
{
    return bson_deserializer_init_doc(element.value);
}

bson_document_t bson_deserializer_element_to_array(bson_element_t element)
{
    return bson_deserializer_init_doc(element.value);
}

const uint8_t *bson_deserializer_element_to_binary(bson_element_t element, uint32_t *len)
{
    if (len) {
        *len = read_uint32(element.value);
    }
    return (const uint8_t *) ((uint8_t *) element.value + sizeof(uint32_t) + sizeof(uint8_t));
}

bool bson_deserializer_element_to_bool(bson_element_t element)
{
    return *((bool *) element.value);
}

int64_t bson_deserializer_element_to_datetime(bson_element_t element)
{
    uint64_t value = read_uint64(element.value);
    return ((int64_t *) &value)[0];
}

int32_t bson_deserializer_element_to_int32(bson_element_t element)
{
    uint32_t value = read_uint32(element.value);
    return ((int32_t *) &value)[0];
}

int64_t bson_deserializer_element_to_int64(bson_element_t element)
{
    uint64_t value = read_uint64(element.value);
    return ((int64_t *) &value)[0];
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static uint32_t read_uint32(const void *buff)
{
    const unsigned char *bytes = (const unsigned char *) buff;
    return sys_le32_to_cpu(((uint32_t) bytes[0]) | (((uint32_t) bytes[1]) << 8U)
        | (((uint32_t) bytes[2]) << 16U) | (((uint32_t) bytes[3]) << 24U));
}

static uint64_t read_uint64(const void *buff)
{
    const unsigned char *bytes = (const unsigned char *) buff;
    return sys_le64_to_cpu((uint64_t) bytes[0] | ((uint64_t) bytes[1] << 8U)
        | ((uint64_t) bytes[2] << 16U) | ((uint64_t) bytes[3] << 24U) | ((uint64_t) bytes[4] << 32U)
        | ((uint64_t) bytes[5] << 40U) | ((uint64_t) bytes[6] << 48U)
        | ((uint64_t) bytes[7] << 56U));
}
