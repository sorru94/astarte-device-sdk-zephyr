/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "register.h"

#include <zephyr/logging/log.h>
#include <zephyr/sys_clock.h>

#include "astarte_device_sdk/result.h"
#include "nvs.h"

LOG_MODULE_REGISTER(register, CONFIG_REGISTER_LOG_LEVEL); // NOLINT

int register_device(char device_id[static ASTARTE_DEVICE_ID_LEN + 1],
    char cred_secr[static ASTARTE_PAIRING_CRED_SECR_LEN + 1])
{
    // Initialize NVS driver
    if (nvs_init() != 0) {
        LOG_ERR("NVS intialization failed!"); // NOLINT
        return -1;
    }

    bool has_cred_secr = false;
    if (nvs_has_cred_secr(&has_cred_secr) != 0) {
        return -1;
    }

    if (has_cred_secr) {
        LOG_INF("Found credential secred in flash"); // NOLINT
        if (nvs_get_cred_secr(cred_secr, ASTARTE_PAIRING_CRED_SECR_LEN + 1) != 0) {
            return -1;
        }
    } else {
        int32_t timeout_ms = 3 * MSEC_PER_SEC;
        astarte_result_t res = astarte_pairing_register_device(
            timeout_ms, device_id, cred_secr, ASTARTE_PAIRING_CRED_SECR_LEN + 1);
        if (res != ASTARTE_RESULT_OK) {
            return -1;
        }
        if (nvs_store_cred_secr(cred_secr) != 0) {
            return -1;
        }
    }

    // You probably shouldn't log a credential secret in a production device
    LOG_INF("Credential secret: '%s'", cred_secr); // NOLINT

    return 0;
}
