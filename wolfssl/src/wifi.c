/*
 * mbedtls/src/wifi.c
 *
 * Copyright (C) 2024-2025 Javier Blanco-Romero @fj-blanco (UC3M, QURSA project)
 *
 * Wi-Fi management for CoAP client
 */

#include <stdio.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/wifi_utils.h>
#include "wifi.h"

#ifndef WIFI_SSID
#define WIFI_SSID "WIFI_SSID_NOT_SET"
#endif

#ifndef WIFI_PASS
#define WIFI_PASS "WIFI_PASS_NOT_SET"  
#endif

#define WIFI_CONNECTION_TIMEOUT_MS 10000 // 10 seconds

struct wifi_connect_req_params wifi_params = {
    .ssid = WIFI_SSID,
    .ssid_length = sizeof(WIFI_SSID) - 1,
    .psk = WIFI_PASS,
    .psk_length = sizeof(WIFI_PASS) - 1,
    .channel = WIFI_CHANNEL_ANY,
    .security = WIFI_SECURITY_TYPE_PSK,
    .mfp = WIFI_MFP_OPTIONAL,
    .timeout = SYS_FOREVER_MS};

#define WIFI_SHELL_MGMT_EVENTS                                                 \
    (NET_EVENT_WIFI_SCAN_RESULT | NET_EVENT_WIFI_SCAN_DONE |                   \
     NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT)

static union {
    struct {
        uint8_t connecting : 1;
        uint8_t disconnecting : 1;
        uint8_t _unused : 6;
    };
    uint8_t all;
} context;

static uint32_t scan_result;
static bool wifi_connected = false;
static struct net_mgmt_event_callback wifi_event_cb;

static void handle_wifi_scan_result(struct net_mgmt_event_callback *cb) {
    const struct wifi_scan_result *entry =
        (const struct wifi_scan_result *)cb->info;

    scan_result++;

    if (scan_result == 1) {
        printf("\n%-4s | %-32s %-5s | %-4s | %-4s | %-5s\n", "Num", "SSID",
               "(len)", "Chan", "RSSI", "Sec");
    }

    printf("%-4d | %-32s %-5u | %-4u | %-4d | %-5s\n", scan_result, entry->ssid,
           entry->ssid_length, entry->channel, entry->rssi,
           (entry->security == WIFI_SECURITY_TYPE_PSK ? "WPA/WPA2" : "Open"));
}

static void handle_wifi_scan_done(struct net_mgmt_event_callback *cb) {
    const struct wifi_status *status = (const struct wifi_status *)cb->info;

    if (status->status) {
        printf("\nWi-Fi scan request failed (%d)\n", status->status);
    } else {
        printf("----------\n");
        printf("Wi-Fi scan request done\n");
    }

    scan_result = 0;
}

static void handle_wifi_connect_result(struct net_mgmt_event_callback *cb) {
    const struct wifi_status *status = (const struct wifi_status *)cb->info;

    if (status->status) {
        printf("\nWi-Fi connection request failed (%d)\n", status->status);
    } else {
        printf("\nWi-Fi connected\n");
        wifi_connected = true;
    }

    context.connecting = false;
}

static void handle_wifi_disconnect_result(struct net_mgmt_event_callback *cb) {
    const struct wifi_status *status = (const struct wifi_status *)cb->info;

    if (context.disconnecting) {
        printf("\nWi-Fi disconnection request %s (%d)\n",
               status->status ? "failed" : "done", status->status);
        context.disconnecting = false;
    } else {
        printf("\nWi-Fi Disconnected\n");
    }
}

void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
                             uint64_t mgmt_event, struct net_if *iface) {
    switch (mgmt_event) {
    case NET_EVENT_WIFI_SCAN_RESULT:
        handle_wifi_scan_result(cb);
        break;
    case NET_EVENT_WIFI_SCAN_DONE:
        handle_wifi_scan_done(cb);
        break;
    case NET_EVENT_WIFI_CONNECT_RESULT:
        handle_wifi_connect_result(cb);
        break;
    case NET_EVENT_WIFI_DISCONNECT_RESULT:
        handle_wifi_disconnect_result(cb);
        break;
    default:
        break;
    }
}

int wifi_init(struct device *unused) {
    ARG_UNUSED(unused);

    context.all = 0;
    scan_result = 0;

    net_mgmt_init_event_callback(&wifi_event_cb, wifi_mgmt_event_handler,
                                 WIFI_SHELL_MGMT_EVENTS);

    printf("Wi-Fi event callback initialized......\n");
    net_mgmt_add_event_callback(&wifi_event_cb);

    return 0;
}

int shell_cmd_scan() {
    struct net_if *iface = net_if_get_default();

    if (net_mgmt(NET_REQUEST_WIFI_SCAN, iface, NULL, 0)) {
        printf("Wi-Fi scan request failed\n");
    } else {
        printf("Wi-Fi scan requested\n");
    }

    return 0;
}

int wait_for_wifi_connection(void) {
    int timeout_count = 0;
    int max_timeout_count = WIFI_CONNECTION_TIMEOUT_MS / 100;

    while (!wifi_connected) {
        k_sleep(K_MSEC(100)); // Sleep for 100 ms
        timeout_count++;

        if (timeout_count >= max_timeout_count) {
            printf("Wi-Fi connection timeout after %d ms\n",
                   WIFI_CONNECTION_TIMEOUT_MS);
            return -ETIMEDOUT;
        }
    }

    printf("Wi-Fi connected successfully\n");
    return 0;
}

void wifi_disconnect(void) {
    struct net_if *iface = net_if_get_default();

    if (net_mgmt(NET_REQUEST_WIFI_DISCONNECT, iface, NULL, 0)) {
        printf("Wi-Fi Disconnection Request Failed\n");
    } else {
        printf("Wi-Fi Disconnection Requested\n");
    }
}

int connect_to_wifi() {
    printf("Connecting to Wi-Fi network......\n");
    int ret;

    struct net_if *iface = net_if_get_default();

    if (!iface) {
        printf("Failed to get Wi-Fi device\n");
        return -ENODEV;
    }
    ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &wifi_params,
                   sizeof(struct wifi_connect_req_params));

    if (ret < 0) {
        printf("Failed to connect to Wi-Fi network: %d\n", ret);
        return ret;
    }

    printf("Wi-Fi connection requested\n");
    return ret;
}