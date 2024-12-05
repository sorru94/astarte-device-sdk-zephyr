/*
 * Copyright (c) 2016 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>

#include "wifi.h"

int main(void)
{
	printk("Started WiFi test application.\n");

	wifi_init();

	k_sleep(K_SECONDS(5));

	printk("Attempting a WiFi connection.\n");
	const char *ssid = CONFIG_WIFI_SSID;
	enum wifi_security_type sec = WIFI_SECURITY_TYPE_PSK;
	const char *psk = CONFIG_WIFI_PASSWORD;
	wifi_connect(ssid, sec, psk);

	while (true) {
		k_sleep(K_SECONDS(5));
	}

	return 0;
}
