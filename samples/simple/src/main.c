/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL); // NOLINT

int main(void)
{
    while (1) {
        LOG_INF("Hello world! %s\n", CONFIG_BOARD); // NOLINT
        k_msleep(CONFIG_SLEEP_MS); // sleep for 1 second
    }
    return 0;
}
