/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bson_deserializer.h"

#include <string.h>
#include <zephyr/sys/byteorder.h>

#include "bson_types.h"
#include "log.h"

ASTARTE_LOG_MODULE_REGISTER(bson_deserializer, CONFIG_ASTARTE_DEVICE_SDK_BSON_LOG_LEVEL);

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

#define NULL_TERM_SIZE 1

/************************************************
 *         Static functions declaration         *
 ***********************************************/

/**
 * @brief Validates if a memory slice falls completely within the document's list boundaries.
 *
 * @param[in] document BSON document to evaluate.
 * @param[in] ptr The starting point of the memory slice.
 * @param[in] ptr The size in bytes of the memory slice.
 * @return True when the slice is within the document bounds, false otherwise.
 */
static bool is_within_bounds(astarte_bson_document_t document, const uint8_t *ptr, size_t size);

/**
 * @brief Safely calculates the expected size of a BSON element's value.
 *
 * @param[in] document BSON document containing the element.
 * @param[in] type BSON type of the element.
 * @param[in] value Pointer to the start of the element's value.
 * @param[out] size Calculated size of the element's value.
 * @return ASTARTE_RESULT_OK if successful, an error otherwise.
 */
static astarte_result_t get_element_value_size(
    astarte_bson_document_t document, uint8_t type, const uint8_t *value, size_t *size);

/************************************************
 * Global functions definitions         *
 ***********************************************/

bool astarte_bson_deserializer_check_validity(const uint8_t *buffer, size_t buffer_size)
{
    if (!buffer) {
        ASTARTE_LOG_WRN("Buffer is NULL");
        return false;
    }

    astarte_bson_document_t document;

    // Validate buffer size is at least 5 bytes, the size of an empty document.
    if (buffer_size < sizeof(document.size) + NULL_TERM_SIZE) {
        ASTARTE_LOG_WRN("Buffer too small: no BSON document found");
        return false;
    }

    document = astarte_bson_deserializer_init_doc(buffer);

    // Ensure the buffer is larger or equal compared to the decoded document size
    if (buffer_size < document.size) {
        ASTARTE_LOG_WRN("Buffer size (%zu) is smaller than BSON document size (%" PRIu32 ")",
            buffer_size, document.size);
        return false;
    }

    // Check if the document list has been initialized
    if (!document.list) {
        ASTARTE_LOG_WRN("BSON document declared size is invalid");
        return false;
    }

    // Check document is terminated with 0x00
    if ((document.list + (document.size - sizeof(document.size)) - NULL_TERM_SIZE)[0] != 0) {
        ASTARTE_LOG_WRN("BSON document is not terminated by NULL byte.");
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
        ASTARTE_LOG_WRN("BSON data too small");
        return false;
    }

    // Check that the first element of the document has a supported index
    switch (document.list[0]) {
        case ASTARTE_BSON_TYPE_DOUBLE:
        case ASTARTE_BSON_TYPE_STRING:
        case ASTARTE_BSON_TYPE_DOCUMENT:
        case ASTARTE_BSON_TYPE_ARRAY:
        case ASTARTE_BSON_TYPE_BINARY:
        case ASTARTE_BSON_TYPE_BOOLEAN:
        case ASTARTE_BSON_TYPE_DATETIME:
        case ASTARTE_BSON_TYPE_INT32:
        case ASTARTE_BSON_TYPE_INT64:
            break;
        default:
            ASTARTE_LOG_WRN("Unrecognized BSON document first type\n");
            return false;
    }

    return true;
}

astarte_bson_document_t astarte_bson_deserializer_init_doc(const uint8_t *buffer)
{
    astarte_bson_document_t document = { 0 };

    if (!buffer) {
        return document;
    }

    // Get the declared document size
    document.size = sys_get_le32(buffer);

    // Chech that declared document size is at least the size of an empty document
    if (document.size >= sizeof(document.size) + NULL_TERM_SIZE) {
        document.list = buffer + sizeof(document.size);
        document.list_size = document.size - sizeof(document.size) - NULL_TERM_SIZE;
    }

    return document;
}

astarte_result_t astarte_bson_deserializer_doc_count_elements(
    astarte_bson_document_t document, size_t *count)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    astarte_bson_element_t element = { 0 };
    ares = astarte_bson_deserializer_first_element(document, &element);
    if (ares == ASTARTE_RESULT_NOT_FOUND) {
        *count = 0;
        return ASTARTE_RESULT_OK;
    }
    if (ares != ASTARTE_RESULT_OK) {
        return ares;
    }
    size_t document_length = 0U;

    do {
        document_length++;
        ares = astarte_bson_deserializer_next_element(document, element, &element);
    } while (ares == ASTARTE_RESULT_OK);

    if (ares != ASTARTE_RESULT_NOT_FOUND) {
        return ares;
    }

    *count = document_length;
    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_bson_deserializer_first_element(
    astarte_bson_document_t document, astarte_bson_element_t *element)
{
    if (document.size <= sizeof(document.size) + NULL_TERM_SIZE) {
        return ASTARTE_RESULT_NOT_FOUND;
    }

    if (document.list_size < sizeof(element->type)) {
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }

    element->type = document.list[0];
    element->name = (const char *) (document.list + sizeof(element->type));

    size_t max_name_str_len = document.list_size - sizeof(element->type);
    element->name_len = strnlen(element->name, max_name_str_len);

    // Ensure the name's null terminator and the value fit within bounds
    if (element->name_len + NULL_TERM_SIZE > max_name_str_len) {
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }

    element->value = document.list + sizeof(element->type) + element->name_len + NULL_TERM_SIZE;

    // Validate the value payload fits completely inside the document boundaries
    size_t val_size = 0;
    if (get_element_value_size(document, element->type, element->value, &val_size)
        != ASTARTE_RESULT_OK) {
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }

    if (!is_within_bounds(document, element->value, val_size)) {
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }

    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_bson_deserializer_next_element(astarte_bson_document_t document,
    astarte_bson_element_t curr_element, astarte_bson_element_t *next_element)
{
    size_t element_value_size = 0U;

    if (get_element_value_size(document, curr_element.type, curr_element.value, &element_value_size)
        != ASTARTE_RESULT_OK) {
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }

    // Verify the entirely calculated element body fits within the document
    if (!is_within_bounds(document, curr_element.value, element_value_size)) {
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }

    const uint8_t *next_element_start = curr_element.value + element_value_size;
    const uint8_t *end_of_document = document.list + document.list_size;

    // Check if we hit the end of our bounded buffer
    if (next_element_start >= end_of_document) {
        return ASTARTE_RESULT_NOT_FOUND;
    }

    // Check if we reached the BSON End Of Object (0x00)
    if (next_element_start[0] == 0) {
        return ASTARTE_RESULT_NOT_FOUND;
    }

    next_element->type = next_element_start[0];
    next_element->name = (const char *) (next_element_start + sizeof(next_element->type));

    // Calculate offset for the next element from the start of the list
    size_t offset = (size_t) (next_element_start - document.list);
    if (document.list_size < offset + sizeof(next_element->type)) {
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }

    size_t max_name_str_len = document.list_size - offset - sizeof(next_element->type);
    next_element->name_len = strnlen(next_element->name, max_name_str_len);

    if (next_element->name_len + NULL_TERM_SIZE > max_name_str_len) {
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }

    next_element->value
        = next_element_start + sizeof(next_element->type) + next_element->name_len + NULL_TERM_SIZE;

    // Validate that the new element's value payload fits within boundaries
    size_t next_val_size = 0;
    if (get_element_value_size(document, next_element->type, next_element->value, &next_val_size)
        != ASTARTE_RESULT_OK) {
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }

    if (!is_within_bounds(document, next_element->value, next_val_size)) {
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }

    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_bson_deserializer_element_lookup(
    astarte_bson_document_t document, const char *key, astarte_bson_element_t *element)
{
    astarte_bson_element_t candidate_element = { 0 };
    astarte_result_t ares = astarte_bson_deserializer_first_element(document, &candidate_element);
    if (ares != ASTARTE_RESULT_OK) {
        return ares;
    }

    while (strncmp(key, candidate_element.name, candidate_element.name_len + NULL_TERM_SIZE) != 0) {
        ares = astarte_bson_deserializer_next_element(
            document, candidate_element, &candidate_element);
        if (ares != ASTARTE_RESULT_OK) {
            return ares;
        }
    }

    // When match has been found copy the candidate element in the out struct
    element->type = candidate_element.type;
    element->name = candidate_element.name;
    element->name_len = candidate_element.name_len;
    element->value = candidate_element.value;

    return ares;
}

double astarte_bson_deserializer_element_to_double(astarte_bson_element_t element)
{
    double value_double = 0.0;
    uint64_t value_uint = sys_get_le64(element.value);
    memcpy(&value_double, &value_uint, sizeof(double));
    return value_double;
}

const char *astarte_bson_deserializer_element_to_string(
    astarte_bson_element_t element, uint32_t *len)
{
    if (len) {
        uint32_t parsed_len = sys_get_le32(element.value);
        // Avoid integer underflow if parsed_len is maliciously 0
        *len = (parsed_len >= NULL_TERM_SIZE) ? parsed_len - NULL_TERM_SIZE : 0;
    }
    return (const char *) (element.value + sizeof(uint32_t));
}

astarte_bson_document_t astarte_bson_deserializer_element_to_document(
    astarte_bson_element_t element)
{
    return astarte_bson_deserializer_init_doc(element.value);
}

astarte_bson_document_t astarte_bson_deserializer_element_to_array(astarte_bson_element_t element)
{
    return astarte_bson_deserializer_init_doc(element.value);
}

const uint8_t *astarte_bson_deserializer_element_to_binary(
    astarte_bson_element_t element, uint32_t *len)
{
    if (len) {
        *len = sys_get_le32(element.value);
    }
    return (const uint8_t *) (element.value + sizeof(uint32_t) + sizeof(uint8_t));
}

bool astarte_bson_deserializer_element_to_bool(astarte_bson_element_t element)
{
    return (*element.value) != 0;
}

int64_t astarte_bson_deserializer_element_to_datetime(astarte_bson_element_t element)
{
    int64_t value_int = 0;
    uint64_t value_uint = sys_get_le64(element.value);
    memcpy(&value_int, &value_uint, sizeof(int64_t));
    return value_int;
}

int32_t astarte_bson_deserializer_element_to_int32(astarte_bson_element_t element)
{
    int32_t value_int = 0;
    uint32_t value_uint = sys_get_le32(element.value);
    memcpy(&value_int, &value_uint, sizeof(int32_t));
    return value_int;
}

int64_t astarte_bson_deserializer_element_to_int64(astarte_bson_element_t element)
{
    int64_t value_int = 0;
    uint64_t value_uint = sys_get_le64(element.value);
    memcpy(&value_int, &value_uint, sizeof(int64_t));
    return value_int;
}

/************************************************
 * Static functions definitions         *
 ***********************************************/

static bool is_within_bounds(astarte_bson_document_t document, const uint8_t *ptr, size_t size)
{
    const uint8_t *end = document.list + document.list_size;
    if (ptr < document.list || ptr > end) {
        return false;
    }
    return size <= (size_t) (end - ptr);
}

static astarte_result_t get_element_value_size(
    astarte_bson_document_t document, uint8_t type, const uint8_t *value, size_t *size)
{
    uint32_t parsed_len = 0;

    switch (type) {
        case ASTARTE_BSON_TYPE_DOUBLE:
        case ASTARTE_BSON_TYPE_DATETIME:
        case ASTARTE_BSON_TYPE_INT64:
            *size = sizeof(int64_t);
            break;
        case ASTARTE_BSON_TYPE_INT32:
            *size = sizeof(int32_t);
            break;
        case ASTARTE_BSON_TYPE_BOOLEAN:
            *size = sizeof(int8_t);
            break;
        case ASTARTE_BSON_TYPE_STRING:
            if (!is_within_bounds(document, value, sizeof(uint32_t))) {
                return ASTARTE_RESULT_INTERNAL_ERROR;
            }
            parsed_len = sys_get_le32(value);
            // Integer overflow check before calculating size
            if (parsed_len > UINT32_MAX - sizeof(int32_t)) {
                return ASTARTE_RESULT_INTERNAL_ERROR;
            }
            *size = sizeof(int32_t) + parsed_len;
            break;
        case ASTARTE_BSON_TYPE_ARRAY:
        case ASTARTE_BSON_TYPE_DOCUMENT:
            if (!is_within_bounds(document, value, sizeof(uint32_t))) {
                return ASTARTE_RESULT_INTERNAL_ERROR;
            }
            // Size block contains the full document/array length including the integer size itself
            parsed_len = sys_get_le32(value);
            *size = parsed_len;
            break;
        case ASTARTE_BSON_TYPE_BINARY:
            if (!is_within_bounds(document, value, sizeof(uint32_t))) {
                return ASTARTE_RESULT_INTERNAL_ERROR;
            }
            parsed_len = sys_get_le32(value);
            // Integer overflow check before calculating size (size + generic subtype)
            if (parsed_len > UINT32_MAX - (sizeof(int32_t) + sizeof(int8_t))) {
                return ASTARTE_RESULT_INTERNAL_ERROR;
            }
            *size = sizeof(int32_t) + sizeof(int8_t) + parsed_len;
            break;
        default:
            ASTARTE_LOG_WRN("unrecognized BSON type: %i", (int) type);
            return ASTARTE_RESULT_INTERNAL_ERROR;
    }

    return ASTARTE_RESULT_OK;
}
