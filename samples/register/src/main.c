/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include <zephyr/data/json.h>
#include <zephyr/kernel.h>
#include <zephyr/net/http/client.h>
#include <zephyr/net/socket.h>
#include <zephyr/posix/arpa/inet.h>

#if !defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_DISABLE_TLS)
#include "ca_certificates.h"
#include <zephyr/net/tls_credentials.h>
#endif

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL); // NOLINT

#include <astarte_device_sdk/device.h>
#include <astarte_device_sdk/pairing.h>

#include "nvs.h"
#include "wifi.h"

int main(void)
{
    astarte_err_t astarte_err = ASTARTE_OK;
    LOG_INF("MQTT Example\nBoard: %s", CONFIG_BOARD); // NOLINT

    // Initialize WiFi driver
#if !defined(CONFIG_BOARD_NATIVE_SIM)
    LOG_INF("Initializing WiFi driver."); // NOLINT
    wifi_init();
#endif

    // Initialize NVS driver
    if (nvs_init() != 0) {
        LOG_ERR("NVS intialization failed!"); // NOLINT
        return -1;
    }

    k_sleep(K_SECONDS(5)); // sleep for 5 seconds

#if !defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_DISABLE_TLS)
    tls_credential_add(CA_CERTIFICATE_ROOT_TAG, TLS_CREDENTIAL_CA_CERTIFICATE, ca_certificate_root,
        sizeof(ca_certificate_root));
#endif

    bool has_cred_secr = false;
    if (nvs_has_cred_secr(&has_cred_secr) != 0) {
        return -1;
    }

    int32_t timeout_ms = 3 * MSEC_PER_SEC;
    char cred_secr[ASTARTE_PAIRING_CRED_SECR_LEN + 1] = { 0 };
    if (has_cred_secr) {
        if (nvs_get_cred_secr(cred_secr, sizeof(cred_secr)) != 0) {
            return -1;
        }
    } else {
        astarte_err = astarte_pairing_register_device(timeout_ms, cred_secr, sizeof(cred_secr));
        if (astarte_err != ASTARTE_OK) {
            return -1;
        }
        if (nvs_store_cred_secr(cred_secr) != 0) {
            return -1;
        }
    }

    LOG_WRN("Credential secret: '%s'", cred_secr); // NOLINT

    astarte_device_config_t device_config;
    device_config.http_timeout_ms = timeout_ms;
    memcpy(device_config.cred_secr, cred_secr, sizeof(cred_secr));

    astarte_device_t device;
    astarte_err = astarte_device_init(&device_config, &device);
    if (astarte_err != ASTARTE_OK) {
        return -1;
    }

    while (1) {
        LOG_INF("Hello world! %s", CONFIG_BOARD); // NOLINT
        k_msleep(CONFIG_SLEEP_MS); // sleep for 1 second
    }
    return 0;
}
