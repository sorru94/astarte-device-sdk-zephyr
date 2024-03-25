/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "wifi.h"

#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>

LOG_MODULE_REGISTER(wifi, CONFIG_APP_LOG_LEVEL); // NOLINT

/************************************************
 * Constants and defines
 ***********************************************/

static K_SEM_DEFINE(wifi_connected, 0, 1);
static K_SEM_DEFINE(ipv4_address_obtained, 0, 1);

static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback ipv4_cb;

/************************************************
 * Static functions declaration
 ***********************************************/

static void handle_wifi_connect_result(struct net_mgmt_event_callback *cb);
static void handle_wifi_disconnect_result(struct net_mgmt_event_callback *cb);
static void handle_ipv4_result(struct net_if *iface);

static void wifi_connect(void);
static void wifi_status(void);

/************************************************
 *       Callbacks declaration/definition       *
 ***********************************************/

static void wifi_mgmt_event_handler(
    struct net_mgmt_event_callback *cb, uint32_t mgmt_event, struct net_if *iface)
{
    switch (mgmt_event) {

        case NET_EVENT_WIFI_CONNECT_RESULT:
            handle_wifi_connect_result(cb);
            break;

        case NET_EVENT_WIFI_DISCONNECT_RESULT:
            handle_wifi_disconnect_result(cb);
            break;

        case NET_EVENT_IPV4_ADDR_ADD:
            handle_ipv4_result(iface);
            break;

        default:
            LOG_DBG("Received unhandled wifi mgmt event %d", mgmt_event);
            break;
    }
}

/************************************************
 * Global functions definition
 ***********************************************/

void wifi_init(void)
{
    LOG_INF("Initializing wifi...\n\n");

    net_mgmt_init_event_callback(&wifi_cb, wifi_mgmt_event_handler,
        NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT);

    net_mgmt_init_event_callback(&ipv4_cb, wifi_mgmt_event_handler, NET_EVENT_IPV4_ADDR_ADD);

    net_mgmt_add_event_callback(&wifi_cb);
    net_mgmt_add_event_callback(&ipv4_cb);

    do {
        wifi_connect();
        k_sleep(K_SECONDS(1));
    } while (k_sem_count_get(&wifi_connected) == 0);
    wifi_status();
    while (k_sem_count_get(&ipv4_address_obtained) == 0) {
        k_sleep(K_SECONDS(1));
    }

    LOG_INF("Ready...\n\n");
}

void wifi_poll(void)
{
    // Attempt reconnection if disconnected
    while (k_sem_count_get(&wifi_connected) == 0) {
        wifi_connect();
        k_sleep(K_SECONDS(1));
    }
}
/************************************************
 *         Static functions definitions         *
 ***********************************************/

static void handle_wifi_connect_result(struct net_mgmt_event_callback *cb)
{
    const struct wifi_status *status = (const struct wifi_status *) cb->info;

    if (status->status) {
        LOG_ERR("Connection request failed (%d)\n", status->status);
    } else {
        LOG_INF("Connected\n");
        k_sem_give(&wifi_connected);
    }
}

static void handle_wifi_disconnect_result(struct net_mgmt_event_callback *cb)
{
    const struct wifi_status *status = (const struct wifi_status *) cb->info;

    if (status->status) {
        LOG_ERR("Disconnection request (%d)\n", status->status);
    } else {
        LOG_INF("Disconnected\n");
        k_sem_take(&wifi_connected, K_NO_WAIT);
    }
}

static void handle_ipv4_result(struct net_if *iface)
{
    int i = 0;

    for (i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {

        char buf[NET_IPV4_ADDR_LEN];

        if (iface->config.ip.ipv4->unicast[i].addr_type != NET_ADDR_DHCP) {
            continue;
        }

        LOG_INF("IPv4 address: %s\n",
            net_addr_ntop(
                AF_INET, &iface->config.ip.ipv4->unicast[i].address.in_addr, buf, sizeof(buf)));
        LOG_INF("Subnet: %s\n",
            net_addr_ntop(AF_INET, &iface->config.ip.ipv4->netmask, buf, sizeof(buf)));
        LOG_INF(
            "Router: %s\n", net_addr_ntop(AF_INET, &iface->config.ip.ipv4->gw, buf, sizeof(buf)));
    }

    k_sem_give(&ipv4_address_obtained);
}

static void wifi_connect(void)
{
    struct net_if *iface = net_if_get_default();

    struct wifi_connect_req_params wifi_params = { 0 };

    wifi_params.ssid = CONFIG_WIFI_SSID;
    wifi_params.psk = CONFIG_WIFI_PASSWORD;
    wifi_params.ssid_length = strlen(CONFIG_WIFI_SSID);
    wifi_params.psk_length = strlen(CONFIG_WIFI_PASSWORD);
    wifi_params.channel = WIFI_CHANNEL_ANY;
    wifi_params.security = WIFI_SECURITY_TYPE_PSK;
    wifi_params.band = WIFI_FREQ_BAND_2_4_GHZ;
    wifi_params.mfp = WIFI_MFP_OPTIONAL;

    LOG_INF("Connecting to SSID: %s\n", wifi_params.ssid);

    if (net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &wifi_params,
            sizeof(struct wifi_connect_req_params))) {
        LOG_INF("WiFi Connection Request Failed\n");
    }
}

static void wifi_status(void)
{
    struct net_if *iface = net_if_get_default();

    struct wifi_iface_status status = { 0 };

    if (net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, iface, &status, sizeof(struct wifi_iface_status))) {
        LOG_INF("WiFi Status Request Failed\n");
    }

    LOG_INF("\n");

    if (status.state >= WIFI_STATE_ASSOCIATED) {
        LOG_INF("SSID: %-32s\n", status.ssid);
        LOG_INF("Band: %s\n", wifi_band_txt(status.band));
        LOG_INF("Channel: %d\n", status.channel);
        LOG_INF("Security: %s\n", wifi_security_txt(status.security));
        LOG_INF("RSSI: %d\n", status.rssi);
    }
}
