/*
 * Copyright (c) 2024 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "include/esp_http_server_misc.h"

esp_err_t esp_httpd_resp_json(httpd_req_t *req, cJSON *js)
{
    CHECK_ARG(js);
    CHECK_ARG(req);

    char *js_txt = cJSON_Print(js);
    cJSON_Delete(js);

    httpd_resp_set_type(req, HTTPD_TYPE_JSON);
    httpd_resp_send(req, js_txt, -1);
    free(js_txt);
    return ESP_OK;
}
