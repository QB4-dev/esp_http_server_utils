/*
 * Copyright (c) 2022 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <string.h>

#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <esp_http_server.h>
#include <esp_http_server_misc.h>
#include <esp_http_server_fota.h>
#include <esp_http_server_wifi.h>

#if CONFIG_IDF_TARGET_ESP32
#include <esp_mac.h>
#endif

static const char *TAG = "APP";

static httpd_handle_t server = NULL;

#if CONFIG_IDF_TARGET_ESP8266
#define ESP_WIFI_AP_SSID "ESP8266-AP"
#elif CONFIG_IDF_TARGET_ESP32
#define ESP_WIFI_AP_SSID "ESP32-AP"
#endif

DECLARE_EMBED_HANDLER(index_html, "/index.html", "text/html");
DECLARE_EMBED_HANDLER(style_css, "/style.css", "text/css");
DECLARE_EMBED_HANDLER(script_js, "/script.js", "text/javascript");

static const httpd_uri_t route_get_root = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = get_index_html //default
};

static httpd_uri_t wifi_get_handler = {
    .uri = "/wifi",
    .method = HTTP_GET,
    .handler = esp_httpd_wifi_handler //
};
static httpd_uri_t wifi_post_handler = {
    .uri = "/wifi",
    .method = HTTP_POST,
    .handler = esp_httpd_wifi_handler //
};

static httpd_uri_t info_handler = {
    .uri = "/info",
    .method = HTTP_GET,
    .handler = esp_httpd_app_info_handler //
};
static httpd_uri_t fota_handler = {
    .uri = "/update",
    .method = HTTP_POST,
    .handler = esp_httpd_fota_handler //
};

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
        case WIFI_EVENT_AP_STACONNECTED: {
            wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
            ESP_LOGI(TAG, "station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
        } break;
        case WIFI_EVENT_AP_STADISCONNECTED: {
            wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
            ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
        } break;
        case WIFI_EVENT_STA_START: {
            //esp_wifi_connect();
        } break;
        case WIFI_EVENT_STA_CONNECTED: {
            wifi_event_sta_connected_t *info = event_data;

            ESP_LOGI(TAG, "Connected to SSID: %s", info->ssid);
        } break;
        case WIFI_EVENT_STA_DISCONNECTED: {
            wifi_event_sta_disconnected_t *info = event_data;

            ESP_LOGE(TAG, "Station disconnected(reason : %d)", info->reason);
            if (info->reason == WIFI_REASON_ASSOC_LEAVE)
                break; // disconnected by user
            esp_wifi_connect();
        } break;
        default:
            break;
        }
    } else if (event_base == IP_EVENT) {
        switch (event_id) {
        case IP_EVENT_STA_GOT_IP: {
            ip_event_got_ip_t *event = event_data;
            ESP_LOGI(TAG, "got ip:" IPSTR "\n", IP2STR(&event->ip_info.ip));
        } break;
        default:
            break;
        }
    }
}

static esp_err_t wifi_init_softap(void)
{
    wifi_config_t wifi_config = {
	.ap = {
	    .ssid = ESP_WIFI_AP_SSID,
	    .ssid_len = strlen(ESP_WIFI_AP_SSID),
	    .max_connection = 1,
	    .authmode = WIFI_AUTH_OPEN,
	} //
    };

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

#if CONFIG_IDF_TARGET_ESP32
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();
#endif

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s", ESP_WIFI_AP_SSID);
    return ESP_OK;
}

static esp_err_t webserver_init(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;
    config.lru_purge_enable = true;

    ESP_ERROR_CHECK(httpd_start(&server, &config));
    // Static file handlers
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &route_get_root));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &route_get_index_html));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &route_get_script_js));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &route_get_style_css));

    // CGI - like handlers
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &wifi_get_handler));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &wifi_post_handler));

    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &info_handler));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &fota_handler));

    ESP_LOGI(TAG, "server started on port %d, free mem: %" PRIu32 " bytes", config.server_port, esp_get_free_heap_size());
    return ESP_OK;
}

void app_main(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    ESP_ERROR_CHECK(wifi_init_softap());
    ESP_ERROR_CHECK(webserver_init());
}
