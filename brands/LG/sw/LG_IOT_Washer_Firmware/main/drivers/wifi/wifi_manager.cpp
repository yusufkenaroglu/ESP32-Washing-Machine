/*
 * wifi_manager.c
 * WiFi management, HTTP server, and OTA updates
 *
 * Copyright 2025 Yusuf Emre Kenaroglu
 * SPDX-License-Identifier: Apache-2.0
 */
#include "wifi_manager_integration.h"
#include "wifi_manager.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "nvs_flash.h"
#include "nvs.h"

// Allow wifi manager to notify UI when provisioning pages are accessed
#include "ui_controller.h"
#include <stdlib.h>
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_err.h"

/* This file now exclusively uses the vendored esp32-wifi-manager
 * component. The integration header pulls in the external public
 * API and provides the correct include path mapping. */

#include "freertos/queue.h"

#include "cJSON.h"

static const char *TAG = "wifi_mgr";

/* Internal event queue to decouple vendor callbacks from heavier processing.
 * Callbacks enqueue minimal events (non-blocking); a dedicated task does
 * the heavier UI updates and string/mutex work. */
typedef struct {
    int code;
} wifi_internal_event_t;

static QueueHandle_t s_wifi_evt_queue = nullptr;
static TaskHandle_t s_wifi_evt_task = nullptr;

static void wifi_event_processor_task(void *pv);

/* Internal event codes (wrapper-local) */
#define EVT_AP_STA_CONNECTED  100

static esp_event_handler_instance_t s_wrapper_wifi_evt_inst = nullptr;

static void wrapper_wifi_event_handler(void *handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)handler_arg;
    (void)event_data;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_internal_event_t evt = { .code = EVT_AP_STA_CONNECTED };
        if (s_wifi_evt_queue) {
            if (xQueueSend(s_wifi_evt_queue, &evt, 0) != pdTRUE) {
                ESP_LOGW(TAG, "Event queue full, dropping AP_STACONNECTED event");
            }
        }
    }
}

/*===========================================================================
 * State Variables
 *===========================================================================*/

#define UNUSED(x) (void)(x)
static httpd_handle_t s_http_server = nullptr;
static wifi_info_t s_wifi_info = {};

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1

#define FIRMWARE_VERSION    "1.0.0"

/* Forward declarations for vendor callback functions (C linkage) */
#ifdef __cplusplus
extern "C" {
#endif
void wm_cb_sta_got_ip(void *param);
void wm_cb_sta_disconnected(void *param);
void wm_cb_order_connect_sta(void *param);
#ifdef __cplusplus
}
#endif

/*===========================================================================
 * Event Handlers
 *===========================================================================*/

/* Most Wi‑Fi handling is delegated to the vendored esp32-wifi-manager
 * component and its callbacks. In a few cases where the vendor does not
 * expose a specific notification (e.g. station connected to our SoftAP),
 * this module registers a lightweight `esp_event` handler strictly to
 * enqueue an internal event; the vendor component still performs the
 * core Wi‑Fi and provisioning work. Prefer vendor callbacks where
 * available — if the vendor exposes AP‑STA notifications in the future,
 * this handler can be removed. */

/*===========================================================================
 * WiFi Implementation
 *===========================================================================*/

esp_err_t wifi_manager_init(void)
{
    /* Use vendor wifi manager component which runs its own task and HTTP UI */
    wifi_manager_start();
    ESP_LOGI(TAG, "External esp32-wifi-manager started");
    // Register callbacks from the vendored manager so we can update the UI
    // when provisioning/connect events occur. The callbacks are defined
    // with C linkage below.
    wifi_manager_set_callback(WM_EVENT_STA_GOT_IP, wm_cb_sta_got_ip);
    wifi_manager_set_callback(WM_EVENT_STA_DISCONNECTED, wm_cb_sta_disconnected);
    wifi_manager_set_callback(WM_ORDER_CONNECT_STA, wm_cb_order_connect_sta);
    /* Create internal event queue and processor task to handle vendor events
     * outside of the vendor task/callback context. This keeps callbacks
     * minimal and non-blocking. */
    if (s_wifi_evt_queue == nullptr) {
        s_wifi_evt_queue = xQueueCreate(8, sizeof(wifi_internal_event_t));
    }
    if (s_wifi_evt_task == nullptr && s_wifi_evt_queue) {
        xTaskCreate(wifi_event_processor_task, "wifi_evt_proc", 3072, nullptr, tskIDLE_PRIORITY+3, &s_wifi_evt_task);
    }
    /* Register a lightweight esp_event handler to detect when a station
     * connects to our SoftAP. The handler only enqueues an internal event
     * and does not perform blocking work. */
    esp_err_t rc = esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, &wrapper_wifi_event_handler, nullptr, &s_wrapper_wifi_evt_inst);
    if (rc != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register wrapper wifi event handler: %s", esp_err_to_name(rc));
    }
    return ESP_OK;
}

esp_err_t wifi_start_ap(void)
{
    ESP_LOGI(TAG, "Requesting AP mode: %s", WIFI_AP_SSID);

    /* Configure vendor wifi_manager AP settings and request start. The
     * vendored component handles the actual AP interfaces, DHCP and captive
     * portal. We retain our machine HTTP server separately. */
    strncpy((char*)wifi_settings.ap_ssid, WIFI_AP_SSID, MAX_SSID_SIZE-1);
    wifi_settings.ap_ssid[MAX_SSID_SIZE-1] = '\0';
    /* keep existing password in wifi_settings.ap_pwd */
    wifi_manager_send_message(WM_ORDER_START_AP, nullptr);
    s_wifi_info.status = WIFI_STATUS_AP_MODE;
    strncpy(s_wifi_info.ssid, WIFI_AP_SSID, sizeof(s_wifi_info.ssid));
    strcpy(s_wifi_info.ip, DEFAULT_AP_IP);
    return ESP_OK;
}

esp_err_t wifi_start_ap_open(void)
{
    ESP_LOGI(TAG, "Requesting OPEN AP (provisioning): %s", WIFI_AP_SSID);

    /* Request vendor to start an open AP for provisioning. */
    strncpy((char*)wifi_settings.ap_ssid, WIFI_AP_SSID, MAX_SSID_SIZE-1);
    wifi_settings.ap_ssid[MAX_SSID_SIZE-1] = '\0';
    memset(wifi_settings.ap_pwd, 0, MAX_PASSWORD_SIZE);
    wifi_manager_send_message(WM_ORDER_START_AP, nullptr);
    s_wifi_info.status = WIFI_STATUS_AP_MODE;
    strncpy(s_wifi_info.ssid, WIFI_AP_SSID, sizeof(s_wifi_info.ssid));
    strcpy(s_wifi_info.ip, DEFAULT_AP_IP);
    return ESP_OK;
}

esp_err_t wifi_connect(void)
{
    char ssid[33] = {0};
    char password[65] = {0};
    
    esp_err_t ret = wifi_load_credentials(ssid, password);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "No saved credentials, starting AP mode");
        return wifi_start_ap();
    }
    
    return wifi_connect_to(ssid, password);
}

esp_err_t wifi_connect_to(const char *ssid, const char *password)
{
    if (ssid == nullptr || strlen(ssid) == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Connecting to: %s", ssid);
    
    s_wifi_info.status = WIFI_STATUS_CONNECTING;
    strncpy(s_wifi_info.ssid, ssid, sizeof(s_wifi_info.ssid));

    // Use vendor wifi-manager: copy credentials to its config and request connect
    wifi_config_t *cfg = wifi_manager_get_wifi_sta_config();
    if (cfg == nullptr) {
        ESP_LOGW(TAG, "External wifi_manager config unavailable");
        return ESP_ERR_NO_MEM;
    }
    memset(cfg, 0, sizeof(wifi_config_t));
    strncpy((char*)cfg->sta.ssid, ssid, sizeof(cfg->sta.ssid)-1);
    if (password) strncpy((char*)cfg->sta.password, password, sizeof(cfg->sta.password)-1);

    /* request async connect */
    wifi_manager_connect_async();

    /* wait up to 30s for the vendor manager to report an IP */
    const TickType_t poll_delay = pdMS_TO_TICKS(500);
    const int max_loops = 30000 / 500; // 30s
    int loops = 0;
    char *ipstr = nullptr;
    while (loops++ < max_loops) {
        vTaskDelay(poll_delay);
        ipstr = wifi_manager_get_sta_ip_string();
        if (ipstr && ipstr[0] != '\0') break;
    }
    if (ipstr && ipstr[0] != '\0') {
        strncpy(s_wifi_info.ip, ipstr, sizeof(s_wifi_info.ip)-1);
        s_wifi_info.status = WIFI_STATUS_CONNECTED;
        wifi_save_credentials(ssid, password);
        return ESP_OK;
    }
    s_wifi_info.status = WIFI_STATUS_ERROR;
    return ESP_FAIL;
}

void wifi_disconnect(void)
{
    wifi_manager_disconnect_async();
    s_wifi_info.status = WIFI_STATUS_DISCONNECTED;
}

void wifi_get_info(wifi_info_t *info)
{
    if (info) {
        memcpy(info, &s_wifi_info, sizeof(wifi_info_t));
        // Fetch live info from vendor manager where possible
        char *ip = wifi_manager_get_sta_ip_string();
        if (ip && ip[0] != '\0') {
            strncpy(info->ip, ip, sizeof(info->ip)-1);
            info->status = WIFI_STATUS_CONNECTED;
        } else {
            info->status = s_wifi_info.status;
        }
        // RSSI: vendored manager may expose this separately; omit direct esp_wifi calls.
    }
}

int wifi_scan(char ssid_list[][33], int8_t *rssi_list, int max_networks)
{
    // Prefer the vendor's parsed data where available. The vendor exposes
    // a JSON array; parse it robustly with cJSON to avoid brittle string
    // parsing.
    char *json = wifi_manager_get_ap_list_json();
    if (!json || json[0] == '\0') return 0;

    int found = 0;
    cJSON *root = cJSON_Parse(json);
    if (!root) return 0;

    if (cJSON_IsArray(root)) {
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, root) {
            if (found >= max_networks) break;
            cJSON *ssid = cJSON_GetObjectItemCaseSensitive(item, "ssid");
            cJSON *rssi = cJSON_GetObjectItemCaseSensitive(item, "rssi");

            if (cJSON_IsString(ssid) && (ssid->valuestring != nullptr)) {
                strncpy(ssid_list[found], ssid->valuestring, 32);
                ssid_list[found][32] = '\0';
            } else {
                ssid_list[found][0] = '\0';
            }

            if (cJSON_IsNumber(rssi)) {
                rssi_list[found] = (int8_t)rssi->valueint;
            } else {
                rssi_list[found] = 0;
            }

            found++;
        }
    }

    cJSON_Delete(root);
    return found;
}

esp_err_t wifi_save_credentials(const char *ssid, const char *password)
{
    wifi_config_t *cfg = wifi_manager_get_wifi_sta_config();
    if (!cfg) return ESP_ERR_NO_MEM;
    memset(cfg, 0, sizeof(wifi_config_t));
    strncpy((char*)cfg->sta.ssid, ssid, sizeof(cfg->sta.ssid)-1);
    if (password) strncpy((char*)cfg->sta.password, password, sizeof(cfg->sta.password)-1);
    esp_err_t r = wifi_manager_save_sta_config();
    if (r == ESP_OK) ESP_LOGI(TAG, "Credentials saved (external manager)");
    return r;
}

esp_err_t wifi_load_credentials(char *ssid, char *password)
{
    if (!wifi_manager_fetch_wifi_sta_config()) return ESP_FAIL;
    wifi_config_t *cfg = wifi_manager_get_wifi_sta_config();
    if (!cfg) return ESP_FAIL;
    strncpy(ssid, (char*)cfg->sta.ssid, 32);
    ssid[32] = '\0';
    strncpy(password, (char*)cfg->sta.password, 64);
    password[64] = '\0';
    return ESP_OK;
}

esp_err_t wifi_forget_credentials(void)
{
    wifi_config_t *cfg = wifi_manager_get_wifi_sta_config();
    if (!cfg) return ESP_ERR_NO_MEM;
    memset(cfg, 0, sizeof(wifi_config_t));
    esp_err_t r = wifi_manager_save_sta_config();
    if (r == ESP_OK) {
        ESP_LOGI(TAG, "Cleared saved WiFi credentials");
    } else {
        ESP_LOGW(TAG, "Failed to clear saved WiFi credentials: %s", esp_err_to_name(r));
    }
    return r;
}


// ---------- wifi_manager event callbacks (registered in wifi_manager_init) ----------
#ifdef __cplusplus
extern "C" {
#endif

void wm_cb_sta_got_ip(void *param)
{
    /* Keep callback minimal: enqueue an internal event for processing. */
    wifi_internal_event_t evt = { .code = WM_EVENT_STA_GOT_IP };
    if (s_wifi_evt_queue) {
        if (xQueueSend(s_wifi_evt_queue, &evt, 0) != pdTRUE) {
            ESP_LOGW(TAG, "Event queue full, dropping GOT_IP event");
        }
    }
}

void wm_cb_sta_disconnected(void *param)
{
    wifi_internal_event_t evt = { .code = WM_EVENT_STA_DISCONNECTED };
    if (s_wifi_evt_queue) {
        if (xQueueSend(s_wifi_evt_queue, &evt, 0) != pdTRUE) {
            ESP_LOGW(TAG, "Event queue full, dropping DISCONNECTED event");
        }
    }
}

void wm_cb_order_connect_sta(void *param)
{
    wifi_internal_event_t evt = { .code = WM_ORDER_CONNECT_STA };
    if (s_wifi_evt_queue) {
        if (xQueueSend(s_wifi_evt_queue, &evt, 0) != pdTRUE) {
            ESP_LOGW(TAG, "Event queue full, dropping ORDER_CONNECT event");
        }
    }
}

#ifdef __cplusplus
}
#endif


/*===========================================================================
 * Internal event processor: handle vendor events in a safe task context
 *===========================================================================*/
static void wifi_event_processor_task(void *pv)
{
    wifi_internal_event_t evt;
    for(;;) {
        if (xQueueReceive(s_wifi_evt_queue, &evt, portMAX_DELAY) == pdTRUE) {
            switch (evt.code) {
                case WM_EVENT_STA_GOT_IP: {
                    ESP_LOGI(TAG, "proc: WM_EVENT_STA_GOT_IP");
                    char *ip = wifi_manager_get_sta_ip_string();
                    if (ip && ip[0] != '\0') {
                        strncpy(s_wifi_info.ip, ip, sizeof(s_wifi_info.ip)-1);
                        s_wifi_info.ip[sizeof(s_wifi_info.ip)-1] = '\0';
                        s_wifi_info.status = WIFI_STATUS_CONNECTED;
                    }
                    ui_controller_set_freehome_page(4);
                } break;
                case EVT_AP_STA_CONNECTED: {
                    ESP_LOGI(TAG, "proc: EVT_AP_STA_CONNECTED (station connected to AP)");
                    /* Advance FreeHome wizard from page 2 -> page 3 */
                    ui_controller_set_freehome_page(3);
                } break;
                case WM_EVENT_STA_DISCONNECTED: {
                    ESP_LOGI(TAG, "proc: WM_EVENT_STA_DISCONNECTED");
                    s_wifi_info.status = WIFI_STATUS_DISCONNECTED;
                    ui_controller_set_freehome_page(2);
                } break;
                case WM_ORDER_CONNECT_STA: {
                    ESP_LOGD(TAG, "proc: WM_ORDER_CONNECT_STA");
                } break;
                default:
                    ESP_LOGW(TAG, "proc: unknown event code %d", evt.code);
                    break;
            }
        }
    }
}


/*===========================================================================
 * HTTP Server Implementation
 *===========================================================================*/



/*===========================================================================
 * HTTP Server Implementation
 *===========================================================================*/

// Forward declarations for handlers
static esp_err_t http_get_root(httpd_req_t *req);
static esp_err_t http_get_status(httpd_req_t *req);
static esp_err_t http_post_start(httpd_req_t *req);
static esp_err_t http_post_stop(httpd_req_t *req);
static esp_err_t http_post_program(httpd_req_t *req);
static esp_err_t http_get_scan(httpd_req_t *req);
static esp_err_t http_post_wifi(httpd_req_t *req);
static esp_err_t http_get_captive(httpd_req_t *req);

#include "machine_state.h"
#include "machine_state/constants.h"

esp_err_t http_server_start(void)
{
    if (s_http_server != nullptr) {
        return ESP_OK;  // Already running
    }
    
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    // Run HTTP server tasks at highest priority to ensure WiFi/provisioning
    // callbacks are serviced promptly during FreeHome setup.
    config.task_priority = configMAX_PRIORITIES - 1;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 10;
    
    esp_err_t ret = httpd_start(&s_http_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Register URI handlers
    httpd_uri_t uri_root = {};
    uri_root.uri = "/";
    uri_root.method = HTTP_GET;
    uri_root.handler = http_get_root;
    httpd_register_uri_handler(s_http_server, &uri_root);
    
    httpd_uri_t uri_status = {};
    uri_status.uri = "/api/status";
    uri_status.method = HTTP_GET;
    uri_status.handler = http_get_status;
    httpd_register_uri_handler(s_http_server, &uri_status);
    
    httpd_uri_t uri_start = {};
    uri_start.uri = "/api/start";
    uri_start.method = HTTP_POST;
    uri_start.handler = http_post_start;
    httpd_register_uri_handler(s_http_server, &uri_start);
    
    httpd_uri_t uri_stop = {};
    uri_stop.uri = "/api/stop";
    uri_stop.method = HTTP_POST;
    uri_stop.handler = http_post_stop;
    httpd_register_uri_handler(s_http_server, &uri_stop);
    
    httpd_uri_t uri_program = {};
    uri_program.uri = "/api/program";
    uri_program.method = HTTP_POST;
    uri_program.handler = http_post_program;
    httpd_register_uri_handler(s_http_server, &uri_program);
    
    httpd_uri_t uri_scan = {};
    uri_scan.uri = "/api/wifi/scan";
    uri_scan.method = HTTP_GET;
    uri_scan.handler = http_get_scan;
    httpd_register_uri_handler(s_http_server, &uri_scan);
    
    httpd_uri_t uri_wifi = {};
    uri_wifi.uri = "/api/wifi/connect";
    uri_wifi.method = HTTP_POST;
    uri_wifi.handler = http_post_wifi;
    httpd_register_uri_handler(s_http_server, &uri_wifi);

    // Captive portal helpers (Android/iOS captive detection)
    httpd_uri_t uri_captive = {};
    uri_captive.uri = "/generate_204"; // Android connectivity check
    uri_captive.method = HTTP_GET;
    uri_captive.handler = http_get_captive;
    httpd_register_uri_handler(s_http_server, &uri_captive);

    httpd_uri_t uri_hotspot = {};
    uri_hotspot.uri = "/hotspot-detect.html"; // iOS connectivity check
    uri_hotspot.method = HTTP_GET;
    uri_hotspot.handler = http_get_captive;
    httpd_register_uri_handler(s_http_server, &uri_hotspot);
    
    ESP_LOGI(TAG, "HTTP server started on port %d", HTTP_SERVER_PORT);
    return ESP_OK;
}

void http_server_stop(void)
{
    if (s_http_server) {
        httpd_stop(s_http_server);
        s_http_server = nullptr;
        ESP_LOGI(TAG, "HTTP server stopped");
    }
}

httpd_handle_t http_server_get_handle(void)
{
    return s_http_server;
}

// HTTP Handlers

static esp_err_t http_get_root(httpd_req_t *req)
{
    /* Minimal machine control page. Wi‑Fi provisioning and captive portal
     * are handled by the vendored esp32-wifi-manager component. */
    const char *body = "<!doctype html><html><head>"
                       "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                       "<title>LG Washer</title></head><body>"
                       "<h2>LG Washing Machine</h2>"
                       "<div><p>Wi‑Fi provisioning is provided by the device provisioning portal."
                       " Connect to the device AP and open your browser; the captive portal"
                       " will redirect you to the provisioning UI.</p></div>"
                       "<div><p><a href='/api/status'>View machine status</a></p></div>"
                       "</body></html>";
    // Notify UI that a provisioning page was loaded so the device UI can
    // advance from the "Join AP" page to the "Waiting for credentials" page.
    ui_controller_set_freehome_page(3);
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, body, strlen(body));
}

static esp_err_t http_get_status(httpd_req_t *req)
{
    char json[256];
    const int prog = machine_get_program();
    const int eta = machine_get_eta();
    const int rpm = static_cast<int>(machine_get_current_rpm());
    snprintf(json, sizeof(json),
        "{\"rpm\":%d,\"eta\":%d,\"active\":%s,\"program\":%d,"
        "\"door_open\":%s,\"power_on\":%s}",
        rpm, eta, machine_is_running() ? "true" : "false", prog,
        machine_is_door_open() ? "true" : "false", machine_is_powered() ? "true" : "false");
    
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json, strlen(json));
}

static esp_err_t http_post_start(httpd_req_t *req)
{
    // Trigger start via external function
    extern void handle_start_stop_button(void);
    if (!machine_is_running() && machine_is_powered() && !machine_is_door_open()) {
        handle_start_stop_button();
        return httpd_resp_send(req, "{\"ok\":true}", 11);
    }
    return httpd_resp_send(req, "{\"ok\":false}", 12);
}

static esp_err_t http_post_stop(httpd_req_t *req)
{
    extern void handle_start_stop_button(void);
    if (machine_is_running()) {
        handle_start_stop_button();
        return httpd_resp_send(req, "{\"ok\":true}", 11);
    }
    return httpd_resp_send(req, "{\"ok\":false}", 12);
}

static esp_err_t http_post_program(httpd_req_t *req)
{
    char buf[32];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
    }
    buf[ret] = '\0';
    
    int prog = 0;
    sscanf(buf, "program=%d", &prog);
    if (prog >= 0 && prog < NUM_PROGRAMS) {
        machine_set_program(prog);
        return httpd_resp_send(req, "{\"ok\":true}", 11);
    }
    return httpd_resp_send(req, "{\"ok\":false}", 12);
}

static esp_err_t http_get_scan(httpd_req_t *req)
{
    char ssids[WIFI_SCAN_LIST_SIZE][33];
    int8_t rssi[WIFI_SCAN_LIST_SIZE];
    
    int count = wifi_scan(ssids, rssi, WIFI_SCAN_LIST_SIZE);
    
    char json[1024] = "[";
    for (int i = 0; i < count; i++) {
        char entry[80];
        snprintf(entry, sizeof(entry), "%s{\"ssid\":\"%s\",\"rssi\":%d}",
                 i > 0 ? "," : "", ssids[i], rssi[i]);
        strcat(json, entry);
    }
    strcat(json, "]");
    
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json, strlen(json));
}

static esp_err_t http_post_wifi(httpd_req_t *req)
{
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
    }
    buf[ret] = '\0';
    
    // Parse simple JSON: {"ssid":"...","password":"..."}
    char ssid[33] = {0}, password[65] = {0};
    char *s = strstr(buf, "\"ssid\":\"");
    char *p = strstr(buf, "\"password\":\"");
    
    if (s) {
        s += 8;
        char *end = strchr(s, '"');
        if (end) {
            int len = end - s;
            if (len > 32) len = 32;
            strncpy(ssid, s, len);
        }
    }
    
    if (p) {
        p += 12;
        char *end = strchr(p, '"');
        if (end) {
            int len = end - p;
            if (len > 64) len = 64;
            strncpy(password, p, len);
        }
    }
    
    if (strlen(ssid) > 0) {
        wifi_save_credentials(ssid, password);

        // Spawn a background task to attempt immediate connection so the HTTP
        // handler doesn't block for the whole connection process.
        typedef struct {
            char ssid[33];
            char password[65];
        } wifi_conn_args_t;

        wifi_conn_args_t *args = (wifi_conn_args_t *)malloc(sizeof(wifi_conn_args_t));
        if (args) {
            memset(args, 0, sizeof(*args));
            strncpy(args->ssid, ssid, sizeof(args->ssid) - 1);
            strncpy(args->password, password, sizeof(args->password) - 1);

            extern void wifi_connect_task(void *pvParameters);
            BaseType_t created = xTaskCreate((TaskFunction_t)wifi_connect_task,
                                             "wifi_connect",
                                             4096,
                                             args,
                                             configMAX_PRIORITIES - 1,
                                             nullptr);
            if (created != pdPASS) {
                ESP_LOGW(TAG, "Could not create wifi_connect task");
                free(args);
            } else {
                // Advance UI to waiting state now that credentials were submitted
                ui_controller_set_freehome_page(3);
            }
        } else {
            ESP_LOGW(TAG, "Out of memory allocating connect args");
        }

        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, "{\"ok\":true,\"msg\":\"connecting\"}", 32);
    }

    return httpd_resp_send(req, "{\"ok\":false}", 12);
}

// Background task invoked to attempt WiFi connection with supplied credentials.
// Runs in its own FreeRTOS task so the HTTP server thread is not blocked.
void wifi_connect_task(void *pvParameters)
{
    typedef struct {
        char ssid[33];
        char password[65];
    } wifi_conn_args_t;

    wifi_conn_args_t *args = (wifi_conn_args_t *)pvParameters;
    if (args == nullptr) {
        vTaskDelete(nullptr);
        return;
    }

    ESP_LOGI(TAG, "Provisioning: attempting connect to '%s'", args->ssid);
    esp_err_t ret = wifi_connect_to(args->ssid, args->password);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Provisioning: connected to '%s'", args->ssid);
    } else {
        ESP_LOGW(TAG, "Provisioning: failed to connect: %s", esp_err_to_name(ret));
        // If connect failed, return to AP mode so user can retry
        wifi_start_ap();
    }

    free(args);
    vTaskDelete(nullptr);
}

static esp_err_t http_get_captive(httpd_req_t *req)
{
    const char *resp = "<!doctype html><html><head>"
                       "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                       "<title>LG Washer Setup</title></head><body>"
                       "<h1>LG Washer</h1>"
                       "<p>Open the configuration page <a href='/'>here</a>.</p>"
                       "</body></html>";
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, resp, strlen(resp));
}

/*===========================================================================
 * OTA Implementation
 *===========================================================================*/

esp_err_t ota_check_update(const char *url, bool *available)
{
    // Simple version check would require fetching version info from server
    // For now, just return false
    *available = false;
    return ESP_OK;
}

esp_err_t ota_update_from_url(const char *url)
{
    ESP_LOGI(TAG, "Starting OTA update from: %s", url);
    
    esp_http_client_config_t http_config = {};
    http_config.url = url;
    http_config.timeout_ms = 30000;
    
    esp_https_ota_config_t ota_config = {};
    ota_config.http_config = &http_config;
    
    esp_err_t ret = esp_https_ota(&ota_config);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA update successful, rebooting...");
        esp_restart();
    } else {
        ESP_LOGE(TAG, "OTA update failed: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

const char *ota_get_version(void)
{
    return FIRMWARE_VERSION;
}
