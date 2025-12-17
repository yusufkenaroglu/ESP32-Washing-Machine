/*
 * wifi_manager.h
 * WiFi management, HTTP server, and OTA updates
 *
 * Copyright 2025 Yusuf Emre Kenaroglu
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Configuration
 *===========================================================================*/

#define WIFI_AP_SSID            "LG_Washer_Setup"
#define WIFI_AP_PASS            "12345678"
#define WIFI_AP_CHANNEL         6
#define WIFI_AP_MAX_CONN        4

#define WIFI_STA_MAX_RETRY      5
#define WIFI_SCAN_LIST_SIZE     10

#define HTTP_SERVER_PORT        80

/*===========================================================================
 * WiFi Status
 *===========================================================================*/

typedef enum {
    WIFI_STATUS_DISCONNECTED = 0,
    WIFI_STATUS_CONNECTING,
    WIFI_STATUS_CONNECTED,
    WIFI_STATUS_AP_MODE,
    WIFI_STATUS_ERROR,
} wifi_status_t;

typedef struct {
    wifi_status_t status;
    char ssid[33];
    char ip[16];
    int8_t rssi;
} wifi_info_t;

/*===========================================================================
 * WiFi API
 *===========================================================================*/

/**
 * @brief Initialize WiFi subsystem
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_init(void);

/**
 * @brief Start WiFi in AP mode for configuration
 * @return ESP_OK on success
 */
esp_err_t wifi_start_ap(void);

/**
 * @brief Start WiFi AP in open (no-password) provisioning mode
 * @return ESP_OK on success
 */
esp_err_t wifi_start_ap_open(void);

/**
 * @brief Connect to stored WiFi network
 * @return ESP_OK on success
 */
esp_err_t wifi_connect(void);

/**
 * @brief Connect to specific WiFi network
 * @param ssid Network SSID
 * @param password Network password
 * @return ESP_OK on success
 */
esp_err_t wifi_connect_to(const char *ssid, const char *password);

/**
 * @brief Disconnect from WiFi
 */
void wifi_disconnect(void);

/**
 * @brief Get current WiFi status
 * @param[out] info WiFi information structure
 */
void wifi_get_info(wifi_info_t *info);

/**
 * @brief Scan for available networks
 * @param[out] ssid_list Array of SSIDs found
 * @param[out] rssi_list Array of signal strengths
 * @param max_networks Maximum networks to return
 * @return Number of networks found
 */
int wifi_scan(char ssid_list[][33], int8_t *rssi_list, int max_networks);

/**
 * @brief Save WiFi credentials to NVS
 * @param ssid Network SSID
 * @param password Network password
 * @return ESP_OK on success
 */
esp_err_t wifi_save_credentials(const char *ssid, const char *password);

/**
 * @brief Load WiFi credentials from NVS
 * @param[out] ssid Network SSID buffer
 * @param[out] password Network password buffer
 * @return ESP_OK if credentials exist
 */
esp_err_t wifi_load_credentials(char *ssid, char *password);
/**
 * @brief Erase/forget stored WiFi credentials (clears saved SSID/password)
 * @return ESP_OK on success
 */
esp_err_t wifi_forget_credentials(void);

/*===========================================================================
 * HTTP Server API
 *===========================================================================*/

/**
 * @brief Start HTTP server
 * @return ESP_OK on success
 */
esp_err_t http_server_start(void);

/**
 * @brief Stop HTTP server
 */
void http_server_stop(void);

/**
 * @brief Get HTTP server handle
 * @return Server handle or NULL
 */
httpd_handle_t http_server_get_handle(void);

/*===========================================================================
 * OTA Update API
 *===========================================================================*/

/**
 * @brief Check for OTA updates
 * @param url URL to check
 * @param[out] available True if update available
 * @return ESP_OK on success
 */
esp_err_t ota_check_update(const char *url, bool *available);

/**
 * @brief Perform OTA update from URL
 * @param url Firmware URL
 * @return ESP_OK on success
 */
esp_err_t ota_update_from_url(const char *url);

/**
 * @brief Get current firmware version
 * @return Version string
 */
const char *ota_get_version(void);

#ifdef __cplusplus
}
#endif
