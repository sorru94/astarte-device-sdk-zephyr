/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "astarte_device_sdk/device_id.h"

#include "log.h"
ASTARTE_LOG_MODULE_REGISTER(device_id, CONFIG_ASTARTE_DEVICE_SDK_DEVICE_ID_LOG_LEVEL);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_device_id_generate_random(char out[static ASTARTE_DEVICE_ID_LEN + 1])
{
    struct uuid uuid;
    uuid_generate_v4(&uuid);
    return uuid_to_base64url(&uuid, out);
}

astarte_result_t astarte_device_id_generate_deterministic(
    const uint8_t namespace[static ASTARTE_DEVICE_ID_NAMESPACE_SIZE], const uint8_t *name,
    size_t name_size, char out[static ASTARTE_DEVICE_ID_LEN + 1])
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    struct uuid uuid = { 0 };
    struct uuid uuid_namespace = { 0 };
    memcpy(uuid_namespace.val, namespace, ASTARTE_DEVICE_ID_NAMESPACE_SIZE);

    ares = uuid_generate_v5(&uuid_namespace, name, name_size, &uuid);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("UUID V5 generation failed: %s", astarte_result_to_name(ares));
        return ares;
    }

    return uuid_to_base64url(&uuid, out);
}
