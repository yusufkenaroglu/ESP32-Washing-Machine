/*
 * freehome_manager.cpp
 * FreeHome integration manager (skeleton implementation)
 *
 * This module provides persistence (NVS) for FreeHome pairing and a
 * simple API other modules (UI, wifi_manager) can use. The background
 * connectivity task and real REST/MQTT client are left as stubs to be
 * implemented after UI and provisioning are wired in.
 */

#include "freehome_manager.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "freehome";

#ifdef __cplusplus
extern "C" {
#endif

// NVS namespace and keys
static const char *NVS_NAMESPACE = "freehome";
static const char *KEY_LINKED = "linked";
static const char *KEY_ENABLED = "enabled";
static const char *KEY_DEVICE_ID = "device_id";
static const char *KEY_PAIR_TOKEN = "pair_token";

// In-memory cache of state
static freehome_status_t s_status = FREEHOME_STATUS_DISABLED;
static bool s_linked = false;
static bool s_enabled = false;
static char *s_device_id = nullptr; // heap allocated

// State callback (single for now)
static freehome_state_cb_t s_state_cb = nullptr;

static void notify_state_change(freehome_status_t new_state)
{
    s_status = new_state;
    if (s_state_cb) s_state_cb(new_state);
}

int freehome_init(void)
{
    esp_err_t err;
    nvs_handle_t h;

    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (err == ESP_OK) {
        uint8_t linked = 0;
        uint8_t enabled = 0;
        size_t required = 0;

        nvs_get_u8(h, KEY_LINKED, &linked);
        nvs_get_u8(h, KEY_ENABLED, &enabled);

        // device id
        err = nvs_get_str(h, KEY_DEVICE_ID, nullptr, &required);
        if (err == ESP_OK && required > 0) {
            s_device_id = static_cast<char *>(malloc(required));
            if (s_device_id) {
                nvs_get_str(h, KEY_DEVICE_ID, s_device_id, &required);
            }
        }

        nvs_close(h);

        s_linked = linked ? true : false;
        s_enabled = enabled ? true : false;
        if (s_linked) {
            notify_state_change(s_enabled ? FREEHOME_STATUS_LINKED : FREEHOME_STATUS_DISABLED);
        } else {
            notify_state_change(s_enabled ? FREEHOME_STATUS_ENABLED : FREEHOME_STATUS_DISABLED);
        }
        ESP_LOGI(TAG, "FreeHome init: linked=%d enabled=%d device_id=%s", s_linked, s_enabled, s_device_id ? s_device_id : "(none)");
        return 0;
    }

    // No NVS data yet: default state
    s_linked = false;
    s_enabled = false;
    notify_state_change(FREEHOME_STATUS_DISABLED);
    ESP_LOGI(TAG, "FreeHome init: no saved configuration");
    return 0;
}

int freehome_start_setup(void)
{
    // For now, just transition to provisioning state and return success.
    notify_state_change(FREEHOME_STATUS_PROVISIONING);
    ESP_LOGI(TAG, "FreeHome setup started (stub)");
    return 0;
}

bool freehome_is_linked(void)
{
    return s_linked;
}

bool freehome_is_enabled(void)
{
    return s_enabled;
}

freehome_status_t freehome_get_status(void)
{
    return s_status;
}

const char *freehome_get_device_id(void)
{
    if (!s_device_id) return "";
    return s_device_id;
}

static esp_err_t nvs_put_u8(const char *key, uint8_t val)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_u8(h, key, val);
    if (err == ESP_OK) nvs_commit(h);
    nvs_close(h);
    return err;
}

int freehome_set_enabled(bool enabled)
{
    s_enabled = enabled;
    nvs_put_u8(KEY_ENABLED, enabled ? 1 : 0);
    if (!enabled) {
        notify_state_change(FREEHOME_STATUS_DISABLED);
    } else {
        notify_state_change(s_linked ? FREEHOME_STATUS_LINKED : FREEHOME_STATUS_ENABLED);
    }
    ESP_LOGI(TAG, "FreeHome %s", enabled ? "enabled" : "disabled");
    return 0;
}

int freehome_unlink(void)
{
    // Erase freehome keys
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS open failed for unlink: %s", esp_err_to_name(err));
        return -1;
    }

    nvs_erase_key(h, KEY_LINKED);
    nvs_erase_key(h, KEY_PAIR_TOKEN);
    nvs_erase_key(h, KEY_DEVICE_ID);
    nvs_commit(h);
    nvs_close(h);

    s_linked = false;
    if (s_device_id) { free(s_device_id); s_device_id = nullptr; }
    notify_state_change(FREEHOME_STATUS_DISABLED);
    ESP_LOGI(TAG, "FreeHome unlinked and data erased");
    return 0;
}

bool freehome_register_state_callback(freehome_state_cb_t cb)
{
    s_state_cb = cb;
    return true;
}

// Future: background task, REST/MQTT client, pairing exchange, QR creation

#ifdef __cplusplus
}
#endif
