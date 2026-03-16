/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DATA_H
#define DATA_H

#include <astarte_device_sdk/data.h>
#include <astarte_device_sdk/interface.h>
#include <astarte_device_sdk/object.h>

#include "utilities.h"

void data_init(const astarte_interface_t *interfaces[], size_t interfaces_len);
size_t data_size(const astarte_interface_t *interfaces[], size_t interfaces_len);
const astarte_interface_t *data_get_interface(const char *interface_name, size_t len);
void data_free();

int data_add_individual(const astarte_interface_t *interface, const char *path,
    const astarte_data_t *data, const optional_int64_t *timestamp);
int data_add_object(const astarte_interface_t *interface, const char *path,
    const byte_array_t *value, astarte_object_entry_t *entries, size_t entries_length,
    const optional_int64_t *timestamp);
int data_add_set_property(
    const astarte_interface_t *interface, const char *path, const astarte_data_t *data);
int data_add_unset_property(const astarte_interface_t *interface, const char *path);

int data_expected_individual(
    const char *interface_name, const char *path, const astarte_data_t *data);
int data_expected_object(const char *interface_name, const char *path,
    const astarte_object_entry_t *entries, size_t entries_length);
int data_expected_set_property(
    const char *interface_name, const char *path, const astarte_data_t *data);
int data_expected_unset_property(const char *interface_name, const char *path);

#endif // DATA_H
