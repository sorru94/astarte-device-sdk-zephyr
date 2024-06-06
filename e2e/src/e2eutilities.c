/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "e2eutilities.h"

#include <stdlib.h>
#include <string.h>

#include <zephyr/fatal.h>
#include <zephyr/logging/log.h>

#include "astarte_device_sdk/interface.h"
#include "e2erunner.h"
#include <astarte_device_sdk/individual.h>
#include <astarte_device_sdk/object.h>

LOG_MODULE_REGISTER(e2e_utilities, CONFIG_E2E_UTILITIES_LOG_LEVEL); // NOLINT

/************************************************
 * Global functions definition
 ***********************************************/

e2e_interface_data_t *get_e2e_interface_data(
    e2e_interface_data_array_t *interfaces_array, const char *interface_name)
{
    for (int i = 0; i < interfaces_array->len; ++i) {
        e2e_interface_data_t *const interface = interfaces_array->buf + i;

        if (strcmp(interface->interface->name, interface_name) == 0) {
            return (e2e_interface_data_t *) interface;
        }
    }

    return NULL;
}

const e2e_individual_data_t *get_e2e_individual_data(
    const e2e_individual_data_array_t *mapping_array, const char *endpoint)
{
    for (int i = 0; i < mapping_array->len; ++i) {
        const e2e_individual_data_t *const mapping = mapping_array->buf + i;

        if (strcmp(mapping->path, endpoint) == 0) {
            return mapping;
        }
    }

    return NULL;
}

const astarte_object_entry_t *get_astarte_object_entry(
    const e2e_object_entry_array_t *value_pair_array, const char *endpoint)
{
    for (int i = 0; i < value_pair_array->len; ++i) {
        const astarte_object_entry_t *const entry = value_pair_array->buf + i;

        if (strcmp(entry->endpoint, endpoint) == 0) {
            return entry;
        }
    }

    return NULL;
}

bool astarte_value_equal(astarte_individual_t *a, astarte_individual_t *b)
{
    if (a->tag != b->tag) {
        return false;
    }

    switch (a->tag) {
        case ASTARTE_MAPPING_TYPE_BOOLEAN:
            return a->data.boolean == b->data.boolean;
        case ASTARTE_MAPPING_TYPE_DOUBLE:
            return a->data.dbl == b->data.dbl;
        case ASTARTE_MAPPING_TYPE_INTEGER:
            return a->data.integer == b->data.integer;
        case ASTARTE_MAPPING_TYPE_LONGINTEGER:
            return a->data.longinteger == b->data.longinteger;
        case ASTARTE_MAPPING_TYPE_STRING:
            return strcmp(a->data.string, b->data.string) == 0;
        case ASTARTE_MAPPING_TYPE_BINARYBLOB:
            if (a->data.binaryblob.len != b->data.binaryblob.len) {
                return false;
            }
            return memcmp(
                a->data.binaryblob.buf, b->data.binaryblob.buf, sizeof(a->data.binaryblob.len));
        case ASTARTE_MAPPING_TYPE_BOOLEANARRAY:
            if (a->data.boolean_array.len != b->data.boolean_array.len) {
                return false;
            }
            return memcmp(a->data.boolean_array.buf, b->data.boolean_array.buf,
                sizeof(a->data.boolean_array.buf[0]) * a->data.boolean_array.len);
        case ASTARTE_MAPPING_TYPE_DATETIMEARRAY:
            if (a->data.datetime_array.len != b->data.datetime_array.len) {
                return false;
            }
            return memcmp(a->data.datetime_array.buf, b->data.datetime_array.buf,
                sizeof(a->data.datetime_array.buf[0]) * a->data.datetime_array.len);
        case ASTARTE_MAPPING_TYPE_DOUBLEARRAY:
            if (a->data.double_array.len != b->data.double_array.len) {
                return false;
            }
            return memcmp(a->data.double_array.buf, b->data.double_array.buf,
                sizeof(a->data.double_array.buf[0]) * a->data.double_array.len);
        case ASTARTE_MAPPING_TYPE_INTEGERARRAY:
            if (a->data.integer_array.len != b->data.integer_array.len) {
                return false;
            }
            return memcmp(a->data.integer_array.buf, b->data.integer_array.buf,
                sizeof(a->data.integer_array.buf[0]) * a->data.integer_array.len);
        case ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY:
            if (a->data.longinteger_array.len != b->data.longinteger_array.len) {
                return false;
            }
            return memcmp(a->data.longinteger_array.buf, b->data.longinteger_array.buf,
                sizeof(a->data.longinteger_array.buf[0]) * a->data.longinteger_array.len);
        case ASTARTE_MAPPING_TYPE_STRINGARRAY:
            if (a->data.string_array.len != b->data.string_array.len) {
                return false;
            }
            for (int i = 0; i < a->data.string_array.len; ++i) {
                if (strcmp(a->data.string_array.buf[i], b->data.string_array.buf[i]) != 0) {
                    return false;
                }
            }
            return true;
        case ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY:
            if (a->data.binaryblob_array.count != b->data.binaryblob_array.count) {
                return false;
            }
            for (int i = 0; i < a->data.binaryblob_array.count; ++i) {
                if (a->data.binaryblob_array.sizes[i] != b->data.binaryblob_array.sizes[i]) {
                    return false;
                }
                if (memcmp(a->data.binaryblob_array.blobs[i], b->data.binaryblob_array.blobs[i],
                        a->data.binaryblob_array.sizes[i])
                    != 0) {
                    return false;
                }
            }
            return true;
        default:
            CHECK_HALT(true, "Unsupported mapping type");
    }

    CHECK_HALT(true, "Unreachable");
}
