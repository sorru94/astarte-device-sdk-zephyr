/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "idata.h"

#include <stdlib.h>
#include <string.h>

#include <astarte_device_sdk/interface.h>
#include <individual_private.h>
#include <log.h>
#include <object_private.h>

#include "utilities.h"
#include "utils.h"
#include "zephyr/logging/log.h"
#include "zephyr/sys/dlist.h"
#include "zephyr/sys/util.h"

/************************************************
 * Constants, static variables and defines
 ***********************************************/

#define MAIN_THREAD_SLEEP_MS 500

LOG_MODULE_REGISTER(idata, CONFIG_IDATA_LOG_LEVEL); // NOLINT

/************************************************
 * Static functions declaration
 ***********************************************/

static e2e_idata_unit_t *idata_get_fist_interface_unit(
    e2e_idata_t *idata, e2e_idata_unit_t *idata_unit, const char *interface);

/************************************************
 * Global functions definition
 ***********************************************/
void idata_unit_log(e2e_idata_unit_t *idata_unit)
{
    ASTARTE_LOG_DBG("Interface data %s", idata_unit->interface->name);

    if (idata_unit->interface->type == ASTARTE_INTERFACE_TYPE_PROPERTIES) {
        e2e_property_data_t *property_data = &idata_unit->values.property;

        // unsets do not store an individual value
        if (property_data->unset) {
            ASTARTE_LOG_DBG("Unset on %s", property_data->path);
        } else {
            ASTARTE_LOG_DBG("Property on %s", property_data->path);
            utils_log_astarte_individual(property_data->individual);
        }
    } else if (idata_unit->interface->aggregation == ASTARTE_INTERFACE_AGGREGATION_INDIVIDUAL) {
        e2e_individual_data_t *individual_data = &idata_unit->values.individual;
        ASTARTE_LOG_DBG("Individual on %s", individual_data->path);
        utils_log_astarte_individual(individual_data->individual);
    } else if (idata_unit->interface->aggregation == ASTARTE_INTERFACE_AGGREGATION_OBJECT) {
        e2e_object_data_t *object_data = &idata_unit->values.object;
        ASTARTE_LOG_DBG("Object on %s", object_data->path);
        utils_log_astarte_object(object_data->entries.buf, object_data->entries.len);
    } else {
        CHECK_HALT(true, "Unkown interface type");
    }
}

e2e_idata_t idata_init()
{
    e2e_idata_t expected = {
        .list = calloc(1, sizeof(sys_dlist_t)),
    };

    CHECK_HALT(!expected.list, "Could not allocate dlist required memory");

    sys_dlist_init(expected.list);

    return expected;
}

int idata_add_individual(e2e_idata_t *idata, const astarte_interface_t *interface,
    e2e_individual_data_t expected_individual)
{
    // TODO unify checks ??? this is equal to the check in get individual
    CHECK_RET_1(interface->aggregation != ASTARTE_INTERFACE_AGGREGATION_INDIVIDUAL,
        "Incorrect aggregation type in interface passed to expected_add_individual");
    CHECK_RET_1(interface->type == ASTARTE_INTERFACE_TYPE_PROPERTIES,
        "Incorrect interface type in interface passed to expected_add_individual");

    e2e_idata_unit_t *element = calloc(1, sizeof(e2e_idata_unit_t));
    CHECK_HALT(!element, "Could not allocate an e2e_expected_unit_t");

    *element = (e2e_idata_unit_t){
        .interface = interface,
        .values.individual = expected_individual,
    };

    sys_dnode_init(&element->node);
    sys_dlist_append(idata->list, &element->node);

    return 0;
}

int idata_add_property(
    e2e_idata_t *idata, const astarte_interface_t *interface, e2e_property_data_t expected_property)
{
    CHECK_RET_1(interface->aggregation != ASTARTE_INTERFACE_AGGREGATION_INDIVIDUAL,
        "Incorrect aggregation type in interface passed to expected_add_property");
    CHECK_RET_1(interface->type != ASTARTE_INTERFACE_TYPE_PROPERTIES,
        "Incorrect interface type in interface passed to expected_add_property");

    e2e_idata_unit_t *element = calloc(1, sizeof(e2e_idata_unit_t));
    CHECK_HALT(!element, "Could not allocate an e2e_expected_unit_t");

    *element = (e2e_idata_unit_t){
        .interface = interface,
        .values.property = expected_property,
    };

    sys_dnode_init(&element->node);
    sys_dlist_append(idata->list, &element->node);

    return 0;
}

int idata_add_object(
    e2e_idata_t *idata, const astarte_interface_t *interface, e2e_object_data_t expected_object)
{
    CHECK_RET_1(interface->aggregation != ASTARTE_INTERFACE_AGGREGATION_OBJECT,
        "Incorrect aggregation type in interface passed to e2e_interface_t object constructor");

    e2e_idata_unit_t *element = calloc(1, sizeof(e2e_idata_unit_t));
    CHECK_HALT(!element, "Could not allocate an e2e_expected_unit_t");

    *element = (e2e_idata_unit_t){
        .interface = interface,
        .values.object = expected_object,
    };

    sys_dnode_init(&element->node);
    sys_dlist_append(idata->list, &element->node);

    return 0;
}

bool idata_is_empty(e2e_idata_t *idata)
{
    return sys_dlist_is_empty(idata->list);
}

int idata_get_individual(
    e2e_idata_t *idata, const char *interface, e2e_individual_data_t **out_individual)
{
    e2e_idata_unit_t *unit = NULL;

    if (*out_individual != NULL) {
        unit = CONTAINER_OF(*out_individual, e2e_idata_unit_t, values.individual);
        // we expect the passed pointer to be the last match so we skip it (i dont like it)
        unit = idata_iter_next(idata, unit);
    } else {
        unit = idata_iter(idata);
    }

    unit = idata_get_fist_interface_unit(idata, unit, interface);

    if (!unit) {
        *out_individual = NULL;
        return 0;
    }

    CHECK_RET_1(unit->interface->aggregation != ASTARTE_INTERFACE_AGGREGATION_INDIVIDUAL,
        "Incorrect interface aggregation associated with interface name passed");
    CHECK_RET_1(unit->interface->type == ASTARTE_INTERFACE_TYPE_PROPERTIES,
        "Incorrect interface type associated with interface name passed");

    *out_individual = &unit->values.individual;
    return 0;
}

int idata_get_property(
    e2e_idata_t *idata, const char *interface, e2e_property_data_t **out_property)
{
    e2e_idata_unit_t *unit = NULL;

    if (*out_property != NULL) {
        unit = CONTAINER_OF(*out_property, e2e_idata_unit_t, values.property);
        // we expect the passed pointer to be the last match so we skip it (i dont like it)
        unit = idata_iter_next(idata, unit);
    } else {
        unit = idata_iter(idata);
    }

    unit = idata_get_fist_interface_unit(idata, unit, interface);

    if (!unit) {
        *out_property = NULL;
        return 0;
    }

    CHECK_RET_1(unit->interface->aggregation != ASTARTE_INTERFACE_AGGREGATION_INDIVIDUAL,
        "Incorrect interface aggregation associated with interface name passed");
    CHECK_RET_1(unit->interface->type != ASTARTE_INTERFACE_TYPE_PROPERTIES,
        "Incorrect interface type associated with interface name passed");

    *out_property = &unit->values.property;
    return 0;
}

int idata_get_object(e2e_idata_t *idata, const char *interface, e2e_object_data_t **out_object)
{
    e2e_idata_unit_t *unit = NULL;

    if (*out_object != NULL) {
        unit = CONTAINER_OF(*out_object, e2e_idata_unit_t, values.object);
        // we expect the passed pointer to be the last match so we skip it (i dont like it)
        unit = idata_iter_next(idata, unit);
    } else {
        unit = idata_iter(idata);
    }

    unit = idata_get_fist_interface_unit(idata, unit, interface);

    if (!unit) {
        *out_object = NULL;
        return 0;
    }

    CHECK_RET_1(unit->interface->aggregation != ASTARTE_INTERFACE_AGGREGATION_OBJECT,
        "Incorrect interface aggregation associated with interface name passed");

    *out_object = &unit->values.object;
    return 0;
}

e2e_idata_unit_t *idata_iter(e2e_idata_t *idata)
{
    e2e_idata_unit_t *current = NULL;

    return SYS_DLIST_PEEK_HEAD_CONTAINER(idata->list, current, node);
}

e2e_idata_unit_t *idata_iter_next(e2e_idata_t *idata, e2e_idata_unit_t *current)
{
    return SYS_DLIST_PEEK_NEXT_CONTAINER(idata->list, current, node);
}

int idata_remove_individual(e2e_individual_data_t *individual)
{
    e2e_idata_unit_t *unit = CONTAINER_OF(individual, e2e_idata_unit_t, values.individual);

    return idata_remove(unit);
}

int idata_remove_property(e2e_property_data_t *property)
{
    e2e_idata_unit_t *unit = CONTAINER_OF(property, e2e_idata_unit_t, values.property);

    return idata_remove(unit);
}

int idata_remove_object(e2e_object_data_t *object)
{
    e2e_idata_unit_t *unit = CONTAINER_OF(object, e2e_idata_unit_t, values.object);

    return idata_remove(unit);
}

int idata_remove(e2e_idata_unit_t *idata_unit)
{
    if (idata_unit->interface->type == ASTARTE_INTERFACE_TYPE_PROPERTIES) {
        e2e_property_data_t *property_data = &idata_unit->values.property;

        // unsets do not store an individual value
        if (!property_data->unset) {
            astarte_individual_destroy_deserialized(property_data->individual);
        }

        free((char *) property_data->path);
    } else if (idata_unit->interface->aggregation == ASTARTE_INTERFACE_AGGREGATION_INDIVIDUAL) {
        e2e_individual_data_t *individual_data = &idata_unit->values.individual;

        free((char *) individual_data->path);
        astarte_individual_destroy_deserialized(individual_data->individual);
    } else if (idata_unit->interface->aggregation == ASTARTE_INTERFACE_AGGREGATION_OBJECT) {
        e2e_object_data_t *object_data = &idata_unit->values.object;

        free((char *) object_data->path);
        free(object_data->object_bytes.buf);
        astarte_object_entries_destroy_deserialized(
            object_data->entries.buf, object_data->entries.len);
    } else {
        CHECK_HALT(true, "Unkown interface type");
    }

    sys_dlist_remove(&idata_unit->node);
    free(idata_unit);
    return 0;
}

void idata_free(e2e_idata_t idata)
{
    e2e_idata_unit_t *current = NULL;
    e2e_idata_unit_t *next = NULL;

    SYS_DLIST_FOR_EACH_CONTAINER_SAFE(idata.list, current, next, node)
    {
        idata_remove(current);
    }

    free(idata.list);
}

/************************************************
 * Static functions definitions
 ***********************************************/

static e2e_idata_unit_t *idata_get_fist_interface_unit(
    e2e_idata_t *idata, e2e_idata_unit_t *idata_unit, const char *interface)
{
    while (idata_unit != NULL) {
        if (strcmp(idata_unit->interface->name, interface) == 0) {
            return idata_unit;
        }

        idata_unit = idata_iter_next(idata, idata_unit);
    }

    return NULL;
}
