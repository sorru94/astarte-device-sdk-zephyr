/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL); // NOLINT

#include <astarte_device_sdk/unit_testable_component.h>

int main(void)
{
    while (1) {
        int value = unit_testable_component_get_value(0);
        LOG_INF("Hello world! %s, %d\n", CONFIG_BOARD, value); // NOLINT
        k_msleep(CONFIG_SLEEP_MS); // sleep for 1 second
    }
    return 0;
}