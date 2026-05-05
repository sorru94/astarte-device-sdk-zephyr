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
    int res = uuid_generate_v4(&uuid);
    if (res != 0) {
        ASTARTE_LOG_ERR("UUID V4 generation failed: %d", res);
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }
    return (uuid_to_base64url(&uuid, out) != 0) ? ASTARTE_RESULT_INTERNAL_ERROR : ASTARTE_RESULT_OK;
}

astarte_result_t astarte_device_id_generate_deterministic(
    const uint8_t nsp[static ASTARTE_DEVICE_ID_NAMESPACE_SIZE], const uint8_t *name,
    size_t name_size, char out[static ASTARTE_DEVICE_ID_LEN + 1])
{
    struct uuid uuid = { 0 };
    struct uuid uuid_namespace = { 0 };
    memcpy(uuid_namespace.val, nsp, ASTARTE_DEVICE_ID_NAMESPACE_SIZE);

    int res = uuid_generate_v5(&uuid_namespace, name, name_size, &uuid);
    if (res != 0) {
        ASTARTE_LOG_ERR("UUID V5 generation failed: %d", res);
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }

    return (uuid_to_base64url(&uuid, out) != 0) ? ASTARTE_RESULT_INTERNAL_ERROR : ASTARTE_RESULT_OK;
}
