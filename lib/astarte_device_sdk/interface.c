/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "astarte_device_sdk/interface.h"

#include "astarte_device_sdk/result.h"
#include "log.h"

ASTARTE_LOG_MODULE_REGISTER(astarte_interface, CONFIG_ASTARTE_DEVICE_SDK_INTROSPECTION_LOG_LEVEL);

astarte_result_t astarte_interface_validate(const astarte_interface_t *interface)
{
    if (!interface) {
        ASTARTE_LOG_ERR("Received NULL interface reference");
        return ASTARTE_RESULT_INVALID_PARAM;
    }
    if ((interface->major_version == 0) && (interface->minor_version == 0)) {
        ASTARTE_LOG_ERR("Trying to add an interface with both major and minor version equal to 0");
        return ASTARTE_RESULT_INTERFACE_INVALID_VERSION;
    }

    return ASTARTE_RESULT_OK;
}
