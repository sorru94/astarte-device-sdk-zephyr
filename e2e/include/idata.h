/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef IDATA_H
#define IDATA_H

/**
 * @file idata.h
 * @brief Interface data (idata) stored in the e2e test to perform checks
 */

#include <zephyr/sys/dlist.h>

#include <astarte_device_sdk/data.h>
#include <astarte_device_sdk/interface.h>
#include <astarte_device_sdk/object.h>

#include "utilities.h"

// Individual data used to store expected values from astarte
typedef struct
{
    // the buffer will be freed by the idata free function
    const char *path;
    // the buffer will be freed by the idata free function
    astarte_data_t data;
    e2e_timestamp_option_t timestamp;
} e2e_individual_data_t;

// Property data used to store expected values from astarte
// if unset is true the data element will contain an invalid value
typedef struct
{
    // the buffer will be freed by the idata free function
    const char *path;
    // the buffer will be freed by the idata free function
    astarte_data_t data;
    bool unset;
} e2e_property_data_t;

// Object data used to store expected values from astarte
typedef struct
{
    // the buffer will be freed by the idata free function
    const char *path;
    // the buffer will be freed by the idata free function
    e2e_object_entry_array_t entries;
    // the buffer will be freed by the idata free function
    e2e_byte_array object_bytes;
    e2e_timestamp_option_t timestamp;
} e2e_object_data_t;

// Data expected in the e2e tests, this stuct identifies an atomic received piece of data
// In case of an individual or property interface this refers to a single individual value
// In case of an object interface to the whole aggregate
// The union follows the type and aggregation of the astarte interface struct
typedef struct
{
    const astarte_interface_t *interface;
    sys_dnode_t node;
    union
    {
        e2e_individual_data_t individual;
        e2e_property_data_t property;
        e2e_object_data_t object;
    } values;
} e2e_idata_unit_t;

void idata_unit_log(e2e_idata_unit_t *idata_unit);

// FIXME for now the order of insertion matters, it has to be the same as the order in which
// messages are received for now it is fixed by giving the user the ability to iterate through the
// matches still i could choose a better alternative
typedef struct
{
    sys_dlist_t *list;
} e2e_idata_t;

e2e_idata_t idata_init();
int idata_add_individual(e2e_idata_t *idata, const astarte_interface_t *interface,
    e2e_individual_data_t expected_individual);
int idata_add_property(e2e_idata_t *idata, const astarte_interface_t *interface,
    e2e_property_data_t expected_property);
int idata_add_object(
    e2e_idata_t *idata, const astarte_interface_t *interface, e2e_object_data_t expected_object);
// get functions, these fetch the next idata unit matching the passed interface name using the
// passed out_individual as starting point if out individual is null they return the first
// occurrence
int idata_get_individual(
    e2e_idata_t *idata, const char *interface, e2e_individual_data_t **out_individual);
int idata_get_property(
    e2e_idata_t *idata, const char *interface, e2e_property_data_t **out_property);
int idata_get_object(e2e_idata_t *idata, const char *interface, e2e_object_data_t **out_object);
bool idata_is_empty(e2e_idata_t *idata);
e2e_idata_unit_t *idata_iter(e2e_idata_t *idata);
e2e_idata_unit_t *idata_iter_next(e2e_idata_t *idata, e2e_idata_unit_t *current);
int idata_remove(e2e_idata_unit_t *idata_unit);
int idata_remove_individual(e2e_individual_data_t *individual);
int idata_remove_property(e2e_property_data_t *property);
int idata_remove_object(e2e_object_data_t *object);
void idata_free(e2e_idata_t idata);

#endif // IDATA_H
