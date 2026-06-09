/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>

#include "backoff.h"

#include "astarte_device_sdk/device.h"

#include <errno.h>

ZTEST_SUITE(astarte_device_sdk_backoff, NULL, NULL, NULL, NULL, NULL);

ZTEST(astarte_device_sdk_backoff, test_correct_allocation)
{
    astarte_device_config_t device_config = { 0 };
    astarte_device_handle_t device = NULL;

    // Even with an empty config, the device_new function should allocate some space on the heap.
    // Since CONFIG_HEAP_MEM_POOL_SIZE is set to 0, if this test succeeds it means it allocated only
    // on the custom heap, not on the default one.
    zassert_not_equal(ASTARTE_RESULT_OK, astarte_device_new(&device_config, &device),
        "Astarte device creation failure.");
    zassert_equal(ASTARTE_RESULT_OK, astarte_device_destroy(device),
        "Astarte device destroy should safely handle NULL.");
}
