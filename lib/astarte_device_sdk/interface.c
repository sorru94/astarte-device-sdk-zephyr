/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "astarte_device_sdk/interface.h"

#include <zephyr/logging/log.h>

#include "astarte_device_sdk/error.h"

// NOLINTNEXTLINE
LOG_MODULE_REGISTER(astarte_interface, CONFIG_ASTARTE_DEVICE_SDK_INTROSPECTION_LOG_LEVEL);

astarte_err_t astarte_interface_validate(const astarte_interface_t *interface)
{
    if (interface->major_version == 0 && interface->minor_version == 0) {
        // NOLINTNEXTLINE
        LOG_ERR("Trying to add an interface with both major and minor version equal to 0");
        return ASTARTE_ERR_INTERFACE_INVALID_VERSION_ZERO;
    }

    return ASTARTE_OK;
}
