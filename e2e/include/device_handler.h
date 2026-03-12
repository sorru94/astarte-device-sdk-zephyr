/*
 * (C) Copyright 2025, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DEVICE_HANDLER_H
#define DEVICE_HANDLER_H

#include <astarte_device_sdk/device.h>

#include "utilities.h"

void setup_device();
void free_device();
void wait_for_device_connection();
void disconnect_device();
void wait_for_device_disconnection();
uint64_t perfect_hash_device_interface(const char *interface_name, size_t len);

int device_send_individual(
    const char *interface_name, const char *path, astarte_data_t data, optional_int64_t timestamp);
int device_send_object(const char *interface_name, const char *path,
    astarte_object_entry_t *entries, size_t entries_len, optional_int64_t timestamp);
int device_set_property(const char *interface_name, const char *path, astarte_data_t data);
int device_unset_property(const char *interface_name, const char *path);

#endif /* DEVICE_HANDLER_H */
