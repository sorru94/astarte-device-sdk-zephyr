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
 * Constants, static variables and defines
 ***********************************************/

#define FREE_ARRAY_FN_DEFINITION(TYPE)                                                             \
    static void free_##TYPE(TYPE array)                                                            \
    {                                                                                              \
        if (array.len == 0) {                                                                      \
            return;                                                                                \
        }                                                                                          \
                                                                                                   \
        free(array.buf);                                                                           \
    }

/************************************************
 * Static functions declaration
 ***********************************************/

static void free_e2e_object_entry_array(e2e_object_entry_array array);

static void free_e2e_mapping_data_array(e2e_mapping_data_array array);

static void free_e2e_property_data_array(e2e_property_data_array array);

/************************************************
 * Global functions definition
 ***********************************************/

// e2e mapping helper functions
ALLOC_ARRAY_FROM_STATIC_FN_DEFINITION(e2e_individual_data_t);

// e2e interfaces helper functions
ALLOC_ARRAY_FROM_STATIC_FN_DEFINITION(e2e_interface_data_t);

// astarte pair allocation helper function
ALLOC_ARRAY_FROM_STATIC_FN_DEFINITION(astarte_object_entry_t);

// e2e properties helper allocation function
ALLOC_ARRAY_FROM_STATIC_FN_DEFINITION(e2e_property_data_t);

void free_e2e_interfaces_array(e2e_interface_data_array interfaces_array)
{
    if (interfaces_array.len == 0) {
        return;
    }

    for (int i = 0; i < interfaces_array.len; ++i) {
        e2e_interface_data_t interface = interfaces_array.buf[i];

        if (interface.interface->type == ASTARTE_INTERFACE_TYPE_PROPERTIES) {
            free_e2e_property_data_array(interface.values.property);
        } else if (interface.interface->aggregation == ASTARTE_INTERFACE_AGGREGATION_INDIVIDUAL) {
            free_e2e_mapping_data_array(interface.values.individual);
        } else if (interface.interface->aggregation == ASTARTE_INTERFACE_AGGREGATION_OBJECT) {
            free_e2e_object_entry_array(interface.values.object.entries);
        } else {
            CHECK_HALT(true, "Unkown interface type")
        }
    }

    free((void *) interfaces_array.buf);
}

e2e_interface_data_t *get_e2e_interface(
    e2e_interface_data_array *interfaces_array, const char *interface_name)
{
    for (int i = 0; i < interfaces_array->len; ++i) {
        e2e_interface_data_t *const interface = interfaces_array->buf + i;

        if (strcmp(interface->interface->name, interface_name) == 0) {
            return interface;
        }
    }

    return NULL;
}

const e2e_individual_data_t *get_e2e_mapping(
    const e2e_mapping_data_array *mapping_array, const char *endpoint)
{
    for (int i = 0; i < mapping_array->len; ++i) {
        const e2e_individual_data_t *const mapping = mapping_array->buf + i;

        if (strcmp(mapping->endpoint, endpoint) == 0) {
            return mapping;
        }
    }

    return NULL;
}

const astarte_object_entry_t *get_pair_mapping(
    const e2e_object_entry_array *value_pair_array, const char *endpoint)
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

e2e_interface_data_t e2e_interface_from_interface_individual(
    const astarte_interface_t *interface, e2e_mapping_data_array mappings)
{
    CHECK_HALT(interface->aggregation != ASTARTE_INTERFACE_AGGREGATION_INDIVIDUAL,
        "Incorrect aggregation type in interface passed to e2e_interface_t individual constructor");

    return (e2e_interface_data_t) {
    .interface = interface,
    .values = {
      .individual = mappings,
    },
  };
}

e2e_interface_data_t e2e_interface_from_interface_object(
    const astarte_interface_t *interface, e2e_object_data_t object_value)
{
    CHECK_HALT(interface->aggregation != ASTARTE_INTERFACE_AGGREGATION_OBJECT,
        "Incorrect aggregation type in interface passed to e2e_interface_t object constructor");

    return (e2e_interface_data_t) {
    .interface = interface,
    .values = {
      .object = object_value,
    },
  };
}

e2e_interface_data_t e2e_interface_from_interface_property(
    const astarte_interface_t *interface, e2e_property_data_array properties)
{
    CHECK_HALT(interface->aggregation != ASTARTE_INTERFACE_AGGREGATION_INDIVIDUAL,
        "Incorrect aggregation type in interface passed to e2e_interface_t property constructor");
    CHECK_HALT(interface->type != ASTARTE_INTERFACE_TYPE_PROPERTIES,
        "Incorrect interface type in interface passed to e2e_interface_t property constructor");

    return (e2e_interface_data_t) {
    .interface = interface,
    .values = {
      .property = properties,
    },
  };
}

/************************************************
 * Static functions definitions
 ***********************************************/

FREE_ARRAY_FN_DEFINITION(e2e_object_entry_array);
FREE_ARRAY_FN_DEFINITION(e2e_mapping_data_array);
FREE_ARRAY_FN_DEFINITION(e2e_property_data_array);
