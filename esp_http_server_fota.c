/*
 * Copyright (c) 2024 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <esp_http_server.h>
#include <esp_system.h>
#include <esp_ota_ops.h>
#include <esp_image_format.h>
#include <esp_log.h>
#include <sys/param.h>

#include "include/esp_http_server_fota.h"
#include "include/esp_http_server_misc.h"
#include "esp_http_upload.h"

static const char *TAG = "FOTA";

esp_err_t esp_httpd_app_info_handler(httpd_req_t *req)
{
    const esp_app_desc_t *app_descr = esp_ota_get_app_description();

    cJSON *js = cJSON_CreateObject();
    cJSON_AddStringToObject(js, "version", app_descr->version);
    cJSON_AddStringToObject(js, "project_name", app_descr->project_name);
    cJSON_AddStringToObject(js, "time", app_descr->time);
    cJSON_AddStringToObject(js, "date", app_descr->date);
    cJSON_AddStringToObject(js, "idf_ver", app_descr->idf_ver);

    return esp_httpd_resp_json(req, js);
}

static void handle_ota_failed_action(esp_ota_actions_t *ota_actions)
{
    if (ota_actions && ota_actions->on_update_failed)
        ota_actions->on_update_failed(ota_actions->arg);
}

static esp_err_t esp_httpd_fota_get_partition_descr(const esp_partition_t *part, esp_app_desc_t *descr)
{
    esp_image_header_t esp_image_header;
    esp_image_segment_header_t seg1_hdr;
    esp_app_desc_t app_desc;
    esp_err_t esp_err;

    /* Try to get description standard way */
    esp_err = esp_ota_get_partition_description(part, &app_desc);
    if (esp_err == ESP_OK) {
        *descr = app_desc;
        return ESP_OK;
    }

    /* Now try to read image headers */
    esp_err = esp_partition_read(part, 0, &esp_image_header, sizeof(esp_image_header_t));
    if (esp_err != ESP_OK) {
        ESP_LOGE(TAG, "esp_partition_read err=0x%x %s", esp_err, esp_err_to_name(esp_err));
        return esp_err;
    }

    if (esp_image_header.magic != ESP_IMAGE_HEADER_MAGIC) {
        ESP_LOGE(TAG, "image header magic !=0x02%x", ESP_IMAGE_HEADER_MAGIC);
        return ESP_ERR_NOT_FOUND;
    }

    esp_err = esp_partition_read(part, sizeof(esp_image_header_t), &seg1_hdr, sizeof(esp_image_segment_header_t));
    if (esp_err != ESP_OK) {
        ESP_LOGE(TAG, "esp_partition_read err=0x%x %s", esp_err, esp_err_to_name(esp_err));
        return esp_err;
    }

    /* App description should be located in the second flash segment */
    size_t desc_offset = sizeof(esp_image_header_t);
    desc_offset += sizeof(esp_image_segment_header_t) + seg1_hdr.data_len;
    desc_offset += sizeof(esp_image_segment_header_t);

    esp_err = esp_partition_read(part, desc_offset, &app_desc, sizeof(esp_app_desc_t));
    if (esp_err != ESP_OK) {
        ESP_LOGE(TAG, "esp_partition_read err=0x%x %s", esp_err, esp_err_to_name(esp_err));
        return esp_err;
    }

    if (app_desc.magic_word == ESP_APP_DESC_MAGIC_WORD) {
        *descr = app_desc;
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t esp_httpd_fota_handler(httpd_req_t *req)
{
    esp_ota_handle_t update_handle;
    const esp_partition_t *update_partition;
    esp_ota_actions_t *ota_actions = req->user_ctx;
    esp_err_t ota_err;

    ESP_LOGI(TAG, "starting FOTA...");
    // do before update action
    if (ota_actions && ota_actions->on_update_init)
        ota_actions->on_update_init(ota_actions->arg);

    char boundary[BOUNDARY_LEN] = { 0 };
    size_t bytes_left = req->content_len;
    int32_t bytes_read = 0;

    ota_err = esp_http_get_boundary(req, boundary);
    if (ota_err != ESP_OK) {
        handle_ota_failed_action(ota_actions);
        return esp_http_upload_json_status(req, ota_err, 0);
    }

    //Read request initial data
    bytes_read = esp_http_upload_check_initial_boundary(req, boundary, bytes_left);
    if (bytes_read > 0) {
        bytes_left -= bytes_read;
    } else {
        handle_ota_failed_action(ota_actions);
        return esp_http_upload_json_status(req, ESP_ERR_INVALID_ARG, 0);
    }

    bytes_read = esp_http_upload_find_multipart_header_end(req, bytes_left);
    if (bytes_read > 0) {
        bytes_left -= bytes_read;
    } else {
        handle_ota_failed_action(ota_actions);
        return esp_http_upload_json_status(req, ESP_ERR_INVALID_ARG, 0);
    }

    //now we have content data until end boundary with additional '--' at the end
    ssize_t boundary_len = strlen(boundary);
    ssize_t final_boundary_len = boundary_len + 2;                // additional "--"
    uint32_t binary_size = bytes_left - (final_boundary_len + 4); // CRLF CRLF
    uint32_t bytes_written = 0;
    uint32_t to_read;
    int32_t recv;

    if (binary_size == 0) {
        ESP_LOGE(TAG, "no file uploaded");
        handle_ota_failed_action(ota_actions);
        return esp_http_upload_json_status(req, ESP_ERR_NOT_FOUND, 0);
    }

    // prepare partition
    update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition) {
        ESP_LOGE(TAG, "update part not found");
        handle_ota_failed_action(ota_actions);
        return esp_http_upload_json_status(req, ESP_FAIL, 0);
    }
    ESP_LOGD(TAG, "write partition %s typ %d sub %d at offset 0x%x", update_partition->label, update_partition->type,
             update_partition->subtype, update_partition->address);

    ota_err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
    if (ota_err != ESP_OK) {
        handle_ota_failed_action(ota_actions);
        ESP_LOGE(TAG, "esp_ota_begin failed: err=%d", ota_err);
        return esp_http_upload_json_status(req, ota_err, 0);
    }

    ESP_LOGI(TAG, "esp_ota_begin OK");
    ESP_LOGI(TAG, "uploading firmware...");

    //prepare buffer, keep in mind to free it before return call
    char *buf = (char *)malloc(UPLOAD_BUF_LEN);
    if (!buf) {
        handle_ota_failed_action(ota_actions);
        return esp_http_upload_json_status(req, ESP_ERR_NO_MEM, 0);
    }

    while (bytes_written < binary_size) {
        if (bytes_left > UPLOAD_BUF_LEN)
            to_read = MIN(bytes_left, UPLOAD_BUF_LEN);
        else
            to_read = bytes_left - (final_boundary_len + 4);

        recv = httpd_req_recv(req, buf, to_read);
        if (recv < 0) {
            // Retry receiving if timeout occurred
            if (recv == HTTPD_SOCK_ERR_TIMEOUT)
                continue;
            free(buf);
            ESP_LOGE(TAG, "httpd_req_recv error: err=0x%x", recv);
            handle_ota_failed_action(ota_actions);
            return esp_http_upload_json_status(req, ESP_FAIL, bytes_written);
        }
        bytes_left -= recv;

        ota_err = esp_ota_write(update_handle, (const void *)buf, recv);
        if (ota_err != ESP_OK) {
            free(buf);
            ESP_LOGE(TAG, "esp_ota_write error: err=0x%x", recv);
            handle_ota_failed_action(ota_actions);
            return esp_http_upload_json_status(req, ESP_FAIL, bytes_written);
        }
        bytes_written += recv;
        ESP_LOGD(TAG, "firmware upload %d/%d bytes", bytes_written, binary_size);
    }
    free(buf);

    bytes_read = esp_http_upload_check_final_boundary(req, boundary, bytes_left);
    if (bytes_read > 0) {
        bytes_left -= bytes_read;
    } else {
        handle_ota_failed_action(ota_actions);
        return esp_http_upload_json_status(req, ESP_FAIL, bytes_written);
    }
    ESP_LOGI(TAG, "%d firmware bytes uploaded OK", bytes_written);

    ota_err = esp_ota_end(update_handle);
    if (ota_err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed! err=0x%x. Image is invalid", ota_err);
        handle_ota_failed_action(ota_actions);
        return esp_http_upload_json_status(req, ota_err, bytes_written);
    }

#ifdef CONFIG_APP_UPDATE_CHECK_PROJECT_NAME
    const esp_app_desc_t *app_descr = esp_ota_get_app_description();
    esp_app_desc_t update_descr;

    ota_err = esp_httpd_fota_get_partition_descr(update_partition, &update_descr);
    if (ota_err == ESP_OK) {
        ESP_LOGW(TAG, "new firmware: %s ver %s %s %s", update_descr.project_name, update_descr.version, update_descr.date,
                 update_descr.time);
    } else {
        ESP_LOGE(TAG, "get_partition_descr failed! err=0x%x %s", ota_err, esp_err_to_name(ota_err));
        handle_ota_failed_action(ota_actions);
        return esp_http_upload_json_status(req, ESP_ERR_IMAGE_INVALID, 0);
    }

    if (strcmp(app_descr->project_name, update_descr.project_name) == 0) {
        ESP_LOGI(TAG, "PROJECT_NAME: check OK");
    } else {
        ESP_LOGE(TAG, "update PROJECT_NAME != %s", app_descr->project_name);
        handle_ota_failed_action(ota_actions);
        return esp_http_upload_json_status(req, ESP_ERR_IMAGE_INVALID, 0);
    }
#endif

    ota_err = esp_ota_set_boot_partition(update_partition);
    if (ota_err != ESP_OK) {
        handle_ota_failed_action(ota_actions);
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed! err=0x%x", ota_err);
        return esp_http_upload_json_status(req, ota_err, bytes_written);
    }

    //do after update complete action
    if (ota_actions && ota_actions->on_update_complete)
        ota_actions->on_update_complete(ota_actions->arg);

    esp_http_upload_json_status(req, ESP_OK, bytes_written);

    if (ota_actions && ota_actions->skip_reboot) {
        ESP_LOGI(TAG, "reboot skipped");
    } else {
        ESP_LOGI(TAG, "esp reboot..");
        vTaskDelay(1000 / portTICK_RATE_MS); //let esp send response before reboot
        esp_restart();
    }
    return ESP_OK;
}
