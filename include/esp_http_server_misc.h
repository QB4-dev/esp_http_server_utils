/*
 * Copyright (c) 2024 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef _ESP_HTTP_SERVER_MISC_H_
#define _ESP_HTTP_SERVER_MISC_H_

#include <esp_http_server.h>
#include <cJSON.h>

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_IDF_TARGET_ESP8266

#define DECLARE_EMBED_HANDLER(NAME, URI, CT)                             \
    extern const char embed_##NAME[] asm("_binary_" #NAME "_start");     \
    extern const char size_##NAME[] asm("_binary_" #NAME "_size");       \
    esp_err_t get_##NAME(httpd_req_t *req)                               \
    {                                                                    \
        httpd_resp_set_type(req, CT);                                    \
        return httpd_resp_send(req, embed_##NAME, (size_t)&size_##NAME); \
    }                                                                    \
    static const httpd_uri_t route_get_##NAME = { .uri = (URI), .method = HTTP_GET, .handler = get_##NAME }

#else

#define DECLARE_EMBED_HANDLER(NAME, URI, CT)                         \
    extern const char embed_##NAME[] asm("_binary_" #NAME "_start"); \
    extern const size_t size_##NAME asm(#NAME "_length");            \
    esp_err_t get_##NAME(httpd_req_t *req)                           \
    {                                                                \
        httpd_resp_set_type(req, CT);                                \
        return httpd_resp_send(req, embed_##NAME, size_##NAME);      \
    }                                                                \
    static const httpd_uri_t route_get_##NAME = { .uri = (URI), .method = HTTP_GET, .handler = get_##NAME }
#endif

#define CHECK_ARG(VAL)                  \
    do {                                \
        if (!(VAL))                     \
            return ESP_ERR_INVALID_ARG; \
    } while (0)

esp_err_t esp_httpd_resp_json(httpd_req_t *req, cJSON *js);

#ifdef __cplusplus
}
#endif

#endif // _ESP_HTTP_SERVER_MISC_H_
