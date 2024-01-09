/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "astarte_device_sdk/unit_testable_component.h"

#include <zephyr/drivers/timer/system_timer.h>

uint32_t unit_testable_component_get_clock_cycle(void)
{
    return sys_clock_cycle_get_32();
}

int unit_testable_component_get_value(int return_value_if_nonzero)
{
    return (return_value_if_nonzero != 0) ? return_value_if_nonzero
                                          : CONFIG_ASTARTE_DEVICE_SDK_GET_VALUE_DEFAULT;
}
