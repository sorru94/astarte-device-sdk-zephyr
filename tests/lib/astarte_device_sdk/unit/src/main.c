/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * @file astarte-device-sdk-zephyr/tests/lib/astarte_device_sdk/src/main.c
 *
 * @details This test suite verifies that the methods provided with the unit_testable_component
 * module works correctly.
 *
 * @note This should be run with the latest version of zephyr present on master (or 3.6.0)
 */

#include <limits.h>

#include <zephyr/fff.h>
#include <zephyr/ztest.h>

#include <astarte_device_sdk/unit_testable_component.h>
#include <lib/astarte_device_sdk/unit_testable_component.c>

DEFINE_FFF_GLOBALS;
FAKE_VALUE_FUNC(uint32_t, sys_clock_cycle_get_32);

ZTEST(astarte_device_sdk, test_get_clock_cycle)
{
    sys_clock_cycle_get_32_fake.return_val = 42;
    zassert_equal(
        unit_testable_component_get_clock_cycle(), 42, "get_clock_cycle is not equal to 42");
}

ZTEST_SUITE(astarte_device_sdk, NULL, NULL, NULL, NULL, NULL);
