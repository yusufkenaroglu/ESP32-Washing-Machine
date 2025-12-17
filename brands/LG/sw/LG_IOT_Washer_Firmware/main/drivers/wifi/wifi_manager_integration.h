/*
 * wifi_manager_integration.h
 * Adapter / integration helper for external esp32-wifi-manager
 *
 * When `USE_ESP32_WIFI_MANAGER` is defined at build time this header
 * will attempt to include the external component's public header and
 * map this project's high-level calls to the external API. By
 * default this header is a no-op and the project continues to use the
 * existing `wifi_manager.*` implementation.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif
/* Include fundamental C and ESP-IDF headers that the vendored
 * `wifi_manager.h` expects (integer types, FreeRTOS types, esp-netif,
 * wifi and error types). Placing these before the vendor include
 * ensures the vendor header can compile correctly when pulled from
 * its `src/` location. */
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "esp_err.h"
#include "esp_netif.h"
#include "esp_wifi.h"

/* Include the vendored component's public header via a relative path
 * from this file's directory. Using an explicit relative path ensures
 * we include the vendored header located under `components/` rather
 * than accidentally picking the project's local
 * `main/drivers/wifi/wifi_manager.h` which would shadow it. */
#include "../../../components/esp32-wifi-manager/src/wifi_manager.h"

#ifdef __cplusplus
}
#endif
