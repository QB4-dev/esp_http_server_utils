/*
 * Copyright (c) 2024 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <esp_http_server.h>
#include <esp_system.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <sys/param.h>

#ifndef CONFIG_IDF_TARGET_ESP8266
#include <esp_mac.h> //ESP-IDF
#endif

#if CONFIG_IDF_TARGET_ESP8266
#define esp_ip_info_t tcpip_adapter_ip_info_t
#else //ESP32xx
#define esp_ip_info_t esp_netif_ip_info_t
#endif

#include "include/esp_http_server_wifi.h"
#include "include/esp_http_server_misc.h"

#define DEFAULT_SCAN_LIST_SIZE 16

static const char *TAG = "WiFi";

static cJSON *ap_record_to_json(wifi_ap_record_t *ap_info)
{
    char mac_buf[24];
    cJSON *js = cJSON_CreateObject();
    sprintf(mac_buf, MACSTR, MAC2STR(ap_info->bssid));

    cJSON_AddStringToObject(js, "bssid", mac_buf);
    cJSON_AddStringToObject(js, "ssid", (const char *)ap_info->ssid);
    cJSON_AddNumberToObject(js, "rssi", (const double)ap_info->rssi);
    cJSON_AddNumberToObject(js, "ch", ap_info->primary);
    cJSON_AddStringToObject(js, "authmode",
                            ap_info->authmode == WIFI_AUTH_OPEN            ? "OPEN" :
                            ap_info->authmode == WIFI_AUTH_WEP             ? "WEP" :
                            ap_info->authmode == WIFI_AUTH_WPA_PSK         ? "WPA_PSK" :
                            ap_info->authmode == WIFI_AUTH_WPA2_PSK        ? "WPA2_PSK" :
                            ap_info->authmode == WIFI_AUTH_WPA_WPA2_PSK    ? "WPA_WPA2_PSK" :
                            ap_info->authmode == WIFI_AUTH_WPA2_ENTERPRISE ? "WPA2_ENTERPRISE" :
                                                                             "??");
    return js;
}

static cJSON *ip_info_to_json(esp_ip_info_t *ip_info)
{
    char buf[16];
    cJSON *js = cJSON_CreateObject();

    sprintf(buf, IPSTR, IP2STR(&ip_info->ip));
    cJSON_AddStringToObject(js, "ip", buf);

    sprintf(buf, IPSTR, IP2STR(&ip_info->netmask));
    cJSON_AddStringToObject(js, "netmask", buf);

    sprintf(buf, IPSTR, IP2STR(&ip_info->gw));
    cJSON_AddStringToObject(js, "gw", buf);
    return js;
}

/* get wifi access point interface info */
static cJSON *get_wifi_ap_info(void)
{
    esp_ip_info_t ip_info = { 0 };
    cJSON *js = cJSON_CreateObject();

#if CONFIG_IDF_TARGET_ESP8266
    if (tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ip_info) == ESP_OK)
        cJSON_AddItemToObject(js, "ip_info", ip_info_to_json(&ip_info));

#else //ESP32xx
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");

    if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK)
        cJSON_AddItemToObject(js, "ip_info", ip_info_to_json(&ip_info));
#endif
    return js;
}

/* get wifi station interface info */
static cJSON *get_wifi_sta_info(void)
{
    esp_ip_info_t ip_info = { 0 };
    wifi_ap_record_t ap_info = { 0 };
    esp_err_t rc;

    cJSON *js = cJSON_CreateObject();

#if CONFIG_IDF_TARGET_ESP8266
    if (tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info) == ESP_OK)
        cJSON_AddItemToObject(js, "ip_info", ip_info_to_json(&ip_info));
#else //ESP32xx
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK)
        cJSON_AddItemToObject(js, "ip_info", ip_info_to_json(&ip_info));
#endif

    rc = esp_wifi_sta_get_ap_info(&ap_info);
    if (rc == ESP_OK)
        cJSON_AddItemToObject(js, "connection", ap_record_to_json(&ap_info));

    cJSON_AddStringToObject(js, "status",
                            (rc == ESP_OK && ip_info.ip.addr)  ? "connected" :
                            (rc == ESP_OK && !ip_info.ip.addr) ? "connecting" :
                            (rc == ESP_ERR_WIFI_CONN)          ? "not initialized" :
                            (rc == ESP_ERR_WIFI_NOT_CONNECT)   ? "disconnected" :
                                                                 "??");
    return js;
}

static cJSON *wifi_info_to_json(void)
{
    cJSON *js = cJSON_CreateObject();
    cJSON_AddItemToObject(js, "ap", get_wifi_ap_info());
    cJSON_AddItemToObject(js, "sta", get_wifi_sta_info());
    return js;
}

static cJSON *wifi_config_to_json(void)
{
    wifi_config_t wifi_config_ap = { 0 };
    wifi_config_t wifi_config_sta = { 0 };

    cJSON *js_ap = cJSON_CreateObject();
    esp_wifi_get_config(ESP_IF_WIFI_AP, &wifi_config_ap);
    cJSON_AddStringToObject(js_ap, "ssid", (const char *)wifi_config_ap.ap.ssid);

    cJSON *js_sta = cJSON_CreateObject();
    esp_wifi_get_config(ESP_IF_WIFI_STA, &wifi_config_sta);
    cJSON_AddStringToObject(js_sta, "ssid", (const char *)wifi_config_sta.sta.ssid);

    cJSON *js = cJSON_CreateObject();
    cJSON_AddItemToObject(js, "ap", js_ap);
    cJSON_AddItemToObject(js, "sta", js_sta);
    return js;
}

static cJSON *wifi_scan_to_json(void)
{
    uint16_t ap_num = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t ap_list[DEFAULT_SCAN_LIST_SIZE] = { 0 };
    wifi_ap_record_t ap_info = { 0 };
    bool connected;

    connected = (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK);

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_disconnect());
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_scan_start(NULL, true));
    esp_wifi_scan_get_ap_records(&ap_num, ap_list);

    if (connected) {
        /* reconnect device if was connected before */
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_connect());
    }

    cJSON *js = cJSON_CreateArray();
    ESP_LOGI(TAG, "APs scanned = %u", ap_num);
    for (int i = 0; (i < ap_num) && (i < DEFAULT_SCAN_LIST_SIZE); i++) {
        ESP_LOGI(TAG, "\tSSID: %s \tch=%d rssi=%d", ap_list[i].ssid, ap_list[i].primary, ap_list[i].rssi);
        cJSON_AddItemToArray(js, ap_record_to_json(&ap_list[i]));
    }
    return js;
}

static esp_err_t wifi_handle_connect_req(httpd_req_t *req)
{
    char *req_data;
    char value[128];
    int bytes_recv = 0;
    int rc;

    wifi_config_t wifi_config = { 0 };
    wifi_mode_t wifi_mode;

    esp_wifi_get_mode(&wifi_mode);
    if (wifi_mode == WIFI_MODE_AP)
        return ESP_FAIL;

    if (req->content_len) {
        req_data = calloc(1, req->content_len + 1);
        if (!req_data)
            return ESP_ERR_NO_MEM;

        for (int bytes_left = req->content_len; bytes_left > 0;) {
            if ((rc = httpd_req_recv(req, req_data + bytes_recv, bytes_left)) <= 0) {
                if (rc == HTTPD_SOCK_ERR_TIMEOUT)
                    continue;
                else
                    return ESP_FAIL;
            }
            bytes_recv += rc;
            bytes_left -= rc;
        }

        if (httpd_query_key_value(req_data, "ssid", value, sizeof(value)) == ESP_OK)
            strncpy((char *)wifi_config.sta.ssid, value, 32);

        if (httpd_query_key_value(req_data, "passwd", value, sizeof(value)) == ESP_OK)
            strncpy((char *)wifi_config.sta.password, value, 64);
        free(req_data);

        esp_wifi_disconnect();
        esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
        esp_wifi_connect();
    }
    return ESP_OK;
}

esp_err_t esp_httpd_wifi_handler(httpd_req_t *req)
{
    CHECK_ARG(req);

    cJSON *js;
    char *url_query;
    size_t qlen;
    char value[128];

    js = cJSON_CreateObject();

    //parse URL query
    qlen = httpd_req_get_url_query_len(req) + 1;
    if (qlen > 1) {
        url_query = malloc(qlen);
        if (httpd_req_get_url_query_str(req, url_query, qlen) == ESP_OK) {
            if (httpd_query_key_value(url_query, "action", value, sizeof(value)) == ESP_OK) {
                if (!strcmp(value, "get_config")) {
                    cJSON_AddItemToObject(js, "data", wifi_config_to_json());
                } else if (!strcmp(value, "scan")) {
                    cJSON_AddItemToObject(js, "data", wifi_scan_to_json());
                } else if (!strcmp(value, "connect")) {
                    wifi_handle_connect_req(req);
                    cJSON_AddItemToObject(js, "data", wifi_info_to_json());
                } else if (!strcmp(value, "disconnect")) {
                    esp_wifi_disconnect();
                    cJSON_AddItemToObject(js, "data", wifi_info_to_json());
                }
            }
        }
        free(url_query);
    } else {
        cJSON_AddItemToObject(js, "data", wifi_info_to_json());
    }
    return esp_httpd_resp_json(req, js);
}
