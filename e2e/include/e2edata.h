/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef E2EDATA_H
#define E2EDATA_H

#include <zephyr/sys/util.h>

#include <astarte_device_sdk/individual.h>

#define BOOLEAN_DATA (true)
#define DATE_TIME_DATA (1710940988984)
#define DOUBLE_DATA (15.42)
#define INTEGER_DATA (42)
#define LONGINTEGER_DATA (8589934592)

static const uint8_t binary_blob_data[8] = { 0x53, 0x47, 0x56, 0x73, 0x62, 0x47, 0x38, 0x3d };
static const uint8_t binblob_1[8] = { 0x53, 0x47, 0x56, 0x73, 0x62, 0x47, 0x38, 0x3d };
static const uint8_t binblob_2[5] = { 0x64, 0x32, 0x39, 0x79, 0x62 };
static const uint8_t *const binary_blob_array_data[2] = { binblob_1, binblob_2 };
static const size_t binary_blob_array_sizes_data[2]
    = { ARRAY_SIZE(binblob_1), ARRAY_SIZE(binblob_2) };
static const bool boolean_array_data[3] = { true, false, true };
static const int64_t date_time_array_data[1] = { 1710940988984 };
static const double double_array_data[2] = { 1542.25, 88852.6 };
static const int32_t integer_array_data[3] = { 4525, 0, 11 };
static const int64_t longinteger_array_data[3] = { 8589930067, 42, 8589934592 };
static const char string_data[] = "Hello world!";
static const char *const string_array_data[2] = { "Hello ", "world!" };

#endif /* E2EDATA_H */
