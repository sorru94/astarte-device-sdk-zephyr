/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "wifi.h"

#include <zephyr/kernel.h>
#include <zephyr/net/wifi_mgmt.h>

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

#define WIFI_SHELL_MGMT_EVENTS (NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT)

static struct net_mgmt_event_callback wifi_shell_mgmt_cb;

/************************************************
 *       Callbacks declaration/definition       *
 ***********************************************/

static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
				    uint32_t mgmt_event, struct net_if *iface)
{
    const struct wifi_status *status = (const struct wifi_status *) cb->info;
	switch (mgmt_event) {
	case NET_EVENT_WIFI_CONNECT_RESULT:
        if (status->status) {
            printk("Connection request failed (%d)\n", status->status);
        } else {
            printk("Connected\n");
        }
		break;
	case NET_EVENT_WIFI_DISCONNECT_RESULT:
        if (status->status) {
            printk("Disconnection error, status: (%d)\n", status->status);
        } else {
            printk("Disconnected");
        }
		break;
	default:
		break;
	}
}

/************************************************
 *         Global functions definitions         *
 ***********************************************/

void wifi_init(void)
{
	net_mgmt_init_event_callback(&wifi_shell_mgmt_cb,
				     wifi_mgmt_event_handler,
				     WIFI_SHELL_MGMT_EVENTS);

	net_mgmt_add_event_callback(&wifi_shell_mgmt_cb);
}

int wifi_connect(const char *ssid, enum wifi_security_type sec, const char *psk)
{
	struct net_if *iface = net_if_get_wifi_sta();
	struct wifi_connect_req_params cnx_params = { 0 };
	int ret;

	cnx_params.band = WIFI_FREQ_BAND_UNKNOWN;
	cnx_params.channel = WIFI_CHANNEL_ANY;
	cnx_params.security = WIFI_SECURITY_TYPE_NONE;
	cnx_params.mfp = WIFI_MFP_OPTIONAL;

	cnx_params.ssid = ssid;
	cnx_params.ssid_length = strlen(cnx_params.ssid);
	cnx_params.security = sec;
	cnx_params.psk = psk;
	cnx_params.psk_length = strlen(cnx_params.psk);

	ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface,
		       &cnx_params, sizeof(struct wifi_connect_req_params));
	if (ret) {
		printk("Connection request failed with error: %d\n", ret);
		return -ENOEXEC;
	}

	printk("Connection requested\n");

	return 0;

}
