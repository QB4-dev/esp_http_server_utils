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
