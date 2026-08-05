/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TEST_STORAGE_COMMON_H
#define TEST_STORAGE_COMMON_H

#include <zephyr/ztest.h>

#include "astarte_device_sdk/data.h"
#include "astarte_device_sdk/result.h"

#include "introspection.h"
#include "storage/core.h"

struct astarte_device_sdk_storage_fixture
{
    const struct device *flash_device;
    introspection_t introspection;
    off_t flash_offset;
    uint16_t flash_sector_size;
    uint16_t flash_sector_count;
    struct k_mutex test_mutex;
    astarte_storage_data_t caching_handle;
};

/**
 * @brief Helper to compare two Astarte data variants.
 */
bool astarte_data_is_equal(astarte_data_t first, astarte_data_t second);

#endif /* TEST_STORAGE_COMMON_H */
