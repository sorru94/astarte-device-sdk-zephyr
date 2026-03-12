/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "utilities.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/fatal.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell_uart.h>
#include <zephyr/sys/bitarray.h>
#include <zephyr/sys/timeutil.h>
#include <zephyr/sys/util.h>

#include <astarte_device_sdk/data.h>
#include <astarte_device_sdk/interface.h>
#include <astarte_device_sdk/mapping.h>
#include <astarte_device_sdk/object.h>
#include <astarte_device_sdk/result.h>

/************************************************
 *   Constants, static variables and defines    *
 ***********************************************/

LOG_MODULE_REGISTER(utilities, CONFIG_UTILITIES_LOG_LEVEL);

#define CMP_ARRAY(LEFT, RIGHT, TYPE_SIZE)                                                          \
    if ((LEFT).len != (RIGHT).len) {                                                               \
        return false;                                                                              \
    }                                                                                              \
    return memcmp((LEFT).buf, (RIGHT).buf, (LEFT).len * (TYPE_SIZE)) == 0;

#define CMP_ARRAY_SIZED(LEFT, RIGHT) CMP_ARRAY(LEFT, RIGHT, sizeof((LEFT).buf[0]))

/************************************************
 *         Static functions declaration         *
 ***********************************************/

static bool string_array_are_equal(
    const astarte_data_stringarray_t *left, const astarte_data_stringarray_t *right);
static bool binaryblob_array_are_equal(
    const astarte_data_binaryblobarray_t *left, const astarte_data_binaryblobarray_t *right);

/************************************************
 *         Global functions definition          *
 ***********************************************/

bool astarte_data_are_equal(const astarte_data_t *left, const astarte_data_t *right)
{
    if (left->tag != right->tag) {
        return false;
    }

    switch (left->tag) {
        case ASTARTE_MAPPING_TYPE_BOOLEAN:
            return left->data.boolean == right->data.boolean;
        case ASTARTE_MAPPING_TYPE_DATETIME:
            return left->data.datetime == right->data.datetime;
        case ASTARTE_MAPPING_TYPE_DOUBLE:
            return left->data.dbl == right->data.dbl;
        case ASTARTE_MAPPING_TYPE_INTEGER:
            return left->data.integer == right->data.integer;
        case ASTARTE_MAPPING_TYPE_LONGINTEGER:
            return left->data.longinteger == right->data.longinteger;
        case ASTARTE_MAPPING_TYPE_STRING:
            return strcmp(left->data.string, right->data.string) == 0;
        case ASTARTE_MAPPING_TYPE_BINARYBLOB:
            CMP_ARRAY(left->data.binaryblob, right->data.binaryblob, sizeof(uint8_t))
        case ASTARTE_MAPPING_TYPE_BOOLEANARRAY:
            CMP_ARRAY_SIZED(left->data.boolean_array, right->data.boolean_array)
        case ASTARTE_MAPPING_TYPE_DATETIMEARRAY:
            CMP_ARRAY_SIZED(left->data.datetime_array, right->data.datetime_array)
        case ASTARTE_MAPPING_TYPE_DOUBLEARRAY:
            CMP_ARRAY_SIZED(left->data.double_array, right->data.double_array)
        case ASTARTE_MAPPING_TYPE_INTEGERARRAY:
            CMP_ARRAY_SIZED(left->data.integer_array, right->data.integer_array)
        case ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY:
            CMP_ARRAY_SIZED(left->data.longinteger_array, right->data.longinteger_array)
        case ASTARTE_MAPPING_TYPE_STRINGARRAY:
            return string_array_are_equal(&left->data.string_array, &right->data.string_array);
        case ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY:
            return binaryblob_array_are_equal(
                &left->data.binaryblob_array, &right->data.binaryblob_array);
        default:
            CHECK_HALT(true, "Unsupported mapping type");
    }

    CHECK_HALT(true, "Unreachable, the previous switch should handle all possible cases");
}

bool astarte_objects_are_equal(const astarte_object_entry_t *left_entries,
    size_t left_entries_length, const astarte_object_entry_t *right_entries,
    size_t right_entries_length)
{
    if (left_entries_length != right_entries_length) {
        return false;
    }

    for (size_t i = 0; i < left_entries_length; i++) {
        bool entry_found = false;

        for (size_t j = 0; j < right_entries_length; j++) {
            if (strcmp(left_entries[i].path, right_entries[j].path) == 0) {

                if (!astarte_data_are_equal(&left_entries[i].data, &right_entries[j].data)) {
                    return false;
                }

                entry_found = true;
                break;
            }
        }

        if (!entry_found) {
            return false;
        }
    }

    return true;
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static bool string_array_are_equal(
    const astarte_data_stringarray_t *left, const astarte_data_stringarray_t *right)
{
    if (left->len != right->len) {
        return false;
    }

    for (int i = 0; i < left->len; ++i) {
        if (strcmp(left->buf[i], right->buf[i]) != 0) {
            return false;
        }
    }

    return true;
}

static bool binaryblob_array_are_equal(
    const astarte_data_binaryblobarray_t *left, const astarte_data_binaryblobarray_t *right)
{
    if (left->count != right->count) {
        return false;
    }

    for (int i = 0; i < left->count; ++i) {
        if (left->sizes[i] != right->sizes[i]) {
            return false;
        }
        if (memcmp(left->blobs[i], right->blobs[i], left->sizes[i]) != 0) {
            return false;
        }
    }

    return true;
}
