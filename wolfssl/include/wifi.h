/*
* mbedtls/include/wifi.h
*
* Copyright (C) 2024-2025 Javier Blanco-Romero @fj-blanco (UC3M, QURSA project)
* Author: Javier Blanco-Romero
*
* Wi-Fi management header for CoAP client
*/

#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>

extern struct wifi_connect_req_params wifi_params;
void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
                             uint64_t mgmt_event, struct net_if *iface);
int wifi_init(struct device *unused);
int shell_cmd_scan(void);
int wait_for_wifi_connection(void);
int connect_to_wifi(void);
void wifi_disconnect(void);