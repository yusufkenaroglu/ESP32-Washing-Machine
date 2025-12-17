#include "ui_controller.h"

#include "machine_state.h"
#include "constants.h"
#include "drivers/freehome/freehome_manager.h"
#if CONFIG_WIFI_ENABLED
#include "drivers/wifi/wifi_manager.h"
#endif

#include <string.h>
#include <cstdio>

namespace {
struct UiState {
    ui_menu_t menu = UI_MENU_DEFAULT;
    int wash_cursor = 0;
    int machine_cursor = 0;
    bool editing = false;
    int freehome_page = 0;
    int freehome_button = 0; // 0 = Back, 1 = Next
};

UiState g_state;

/*
 * UI design rationale:
 * - `UiState` keeps navigation and editing state local to this module so the
 *   rest of the system can request actions without depending on UI internals.
 * - The UI separates navigation (cursor movement) from editing to reduce
 *   accidental changes: users must explicitly enter editing mode to mutate
 *   option values. This avoids surprising state changes when rotating the
 *   dial while simply browsing menus.
 */

void clamp_cursor(int &cursor, int max_idx) {
    if (cursor < 0) cursor = 0;
    if (cursor >= max_idx) cursor = max_idx - 1;
}

void adjust_option(int delta) {
    switch (g_state.wash_cursor) {
        case UI_WASH_OPTION_TEMPERATURE: {
            int idx = machine_get_temp_idx();
            machine_set_temp_idx(idx + delta);
            break;
        }
        case UI_WASH_OPTION_SPIN: {
            int idx = machine_get_spin_idx();
            machine_set_spin_idx(idx + delta);
            break;
        }
        case UI_WASH_OPTION_SOIL: {
            int idx = machine_get_soil_idx();
            machine_set_soil_idx(idx + delta);
            break;
        }
        case UI_WASH_OPTION_PREWASH: {
            bool val = machine_is_prewash_enabled();
            (void)delta;
            machine_set_prewash_enabled(!val);
            break;
        }
        case UI_WASH_OPTION_EXTRA_RINSE: {
            int count = machine_get_extra_rinse_count();
            int next = count + delta;
            if (next < 0) next = 0;
            if (next > 3) next = 3;
            machine_set_extra_rinse_count(static_cast<uint8_t>(next));
            break;
        }
        case UI_WASH_OPTION_MACHINE_SETTINGS:
        case UI_WASH_OPTION_BACK:
            // Navigation rows have no adjustable values
            break;
        default:
            break;
    }
}
}

void ui_controller_reset(void) {
    g_state = UiState{};
}

void ui_controller_show_logo(void) {
    g_state.menu = UI_MENU_LOGO;
    g_state.editing = false;
}

void ui_controller_handle_start_long_press(void) {
    if (g_state.menu == UI_MENU_DEFAULT) {
        g_state.menu = UI_MENU_WASH_SETTINGS;
        g_state.editing = false;
    } else {
        g_state.menu = UI_MENU_DEFAULT;
        g_state.editing = false;
    }
}

bool ui_controller_handle_start_press(void) {
    switch (g_state.menu) {
        case UI_MENU_WASH_SETTINGS:
            if (g_state.editing) {
                g_state.editing = false;
            } else if (g_state.wash_cursor == UI_WASH_OPTION_MACHINE_SETTINGS) {
                g_state.menu = UI_MENU_MACHINE_SETTINGS;
                g_state.editing = false;
                g_state.machine_cursor = 0;
            } else if (g_state.wash_cursor == UI_WASH_OPTION_BACK) {
                g_state.menu = UI_MENU_DEFAULT;
                g_state.editing = false;
            } else {
                g_state.editing = true;
            }
            return true;
        case UI_MENU_FREEHOME:
            // Confirm selected button
            if (g_state.freehome_button == 0) {
                // Back -> return to machine settings
                g_state.menu = UI_MENU_MACHINE_SETTINGS;
                g_state.freehome_page = 0;
                g_state.freehome_button = 0;
            } else {
                // Next -> advance page
                g_state.freehome_page++;
                // If moving to waiting page, start AP for provisioning
                if (g_state.freehome_page == 3) {
#if CONFIG_WIFI_ENABLED
                    // Start open provisioning AP when entering provisioning/waiting page
                    wifi_start_ap_open();
#endif
                }
            }
            return true;
        case UI_MENU_MACHINE_SETTINGS:
            g_state.editing = false;
            if (g_state.machine_cursor == UI_MACHINE_OPTION_BACK) {
                g_state.menu = UI_MENU_WASH_SETTINGS;
            } else if (g_state.machine_cursor == UI_MACHINE_OPTION_FREEHOME) {
                // Enter FreeHome menu
                g_state.menu = UI_MENU_FREEHOME;
                g_state.freehome_page = 0;
                g_state.freehome_button = 0;
                // If device has stored WiFi credentials but FreeHome is not linked,
                // forget those credentials so the device behaves like a fresh FreeHome
                // setup. This ensures partial/incomplete provisioning does not leave
                // the device in a state where it silently connects to Wi-Fi.
        #if CONFIG_WIFI_ENABLED
                if (!freehome_is_linked()) {
                    char ssid[33] = {0};
                    char pass[65] = {0};
                    if (wifi_load_credentials(ssid, pass) == ESP_OK && ssid[0] != '\0') {
                        ESP_LOGI("ui", "Found saved WiFi (%s) without FreeHome link — clearing", ssid);
                        wifi_forget_credentials();
                    }
                }
        #endif
            }
            return true;
        default:
            return false; // allow normal start/stop handling
    }
}

void ui_controller_handle_dial_delta(int delta) {
    if (delta == 0) return;
    switch (g_state.menu) {
        case UI_MENU_LOGO:
            // ignore dial in logo
            break;
        case UI_MENU_DEFAULT: {
            int prog = machine_get_program();
            prog += delta;
            if (prog < 0) prog = 0;
            if (prog >= NUM_PROGRAMS) prog = NUM_PROGRAMS - 1;
            machine_set_program(prog);
            break;
        }
        case UI_MENU_WASH_SETTINGS:
            if (g_state.editing) {
                adjust_option(delta > 0 ? 1 : -1);
            } else {
                g_state.wash_cursor += (delta > 0 ? 1 : -1);
                clamp_cursor(g_state.wash_cursor, UI_WASH_OPTION_COUNT);
            }
            break;
        case UI_MENU_FREEHOME:
            // rotate between Back and Next
            if (delta > 0) {
                g_state.freehome_button = 1;
            } else {
                g_state.freehome_button = 0;
            }
            break;
        case UI_MENU_MACHINE_SETTINGS:
            g_state.machine_cursor += (delta > 0 ? 1 : -1);
            clamp_cursor(g_state.machine_cursor, UI_MACHINE_OPTION_COUNT);
            break;
    }
}

ui_render_state_t ui_controller_get_render_state(void) {
    ui_render_state_t out = { g_state.menu, g_state.wash_cursor, g_state.machine_cursor, g_state.editing, g_state.freehome_page, g_state.freehome_button };
    return out;
}

// Allow external modules (display/provisioning) to change the current
// FreeHome wizard page. Kept intentionally simple: the UI task will pick
// up the updated `g_state` on the next frame.
void ui_controller_set_freehome_page(int page) {
    if (page < 0) return;
    g_state.freehome_page = page;
    g_state.freehome_button = 0;
}

const char *ui_wash_option_label(int idx) {
    switch (idx) {
        case UI_WASH_OPTION_TEMPERATURE: return "Temperature";
        case UI_WASH_OPTION_SPIN: return "Spin";
        case UI_WASH_OPTION_SOIL: return "Soil";
        case UI_WASH_OPTION_PREWASH: return "Prewash";
        case UI_WASH_OPTION_EXTRA_RINSE: return "Extra Rinse";
        case UI_WASH_OPTION_MACHINE_SETTINGS: return "Machine Settings";
        case UI_WASH_OPTION_BACK: return "Back";
        default: return "";
    }
}

void ui_wash_option_value_string(int idx, char *buf, size_t len) {
    if (!buf || len == 0) return;
    buf[0] = '\0';
    switch (idx) {
        case UI_WASH_OPTION_TEMPERATURE: {
            int t = machine_get_temp_idx();
            if (t >= 0 && t < 5) {
                strncpy(buf, temperatures[t], len - 1);
                buf[len - 1] = '\0';
            }
            break;
        }
        case UI_WASH_OPTION_SPIN: {
            int s = machine_get_spin_idx();
            if (s >= 0 && s < 5) {
                strncpy(buf, spin_speeds[s], len - 1);
                buf[len - 1] = '\0';
            }
            break;
        }
        case UI_WASH_OPTION_SOIL: {
            int soil = machine_get_soil_idx();
            if (soil >= 0 && soil < 4) {
                strncpy(buf, soil_levels[soil], len - 1);
                buf[len - 1] = '\0';
            }
            break;
        }
        case UI_WASH_OPTION_PREWASH: {
            strncpy(buf, machine_is_prewash_enabled() ? "On" : "Off", len - 1);
            buf[len - 1] = '\0';
            break;
        }
        case UI_WASH_OPTION_EXTRA_RINSE: {
            snprintf(buf, len, "%u", (unsigned)machine_get_extra_rinse_count());
            break;
        }
        case UI_WASH_OPTION_MACHINE_SETTINGS:
        case UI_WASH_OPTION_BACK:
            buf[0] = '\0';
            break;
        default:
            buf[0] = '\0';
            break;
    }
}

const char *ui_machine_option_label(int idx) {
    switch (idx) {
        case UI_MACHINE_OPTION_ABOUT: return "About";
        case UI_MACHINE_OPTION_DISPLAY: return "Display";
        case UI_MACHINE_OPTION_ADVANCED: return "Advanced";
        case UI_MACHINE_OPTION_FREEHOME: return "FreeHome";
        case UI_MACHINE_OPTION_BACK: return "Back";
        default: return "";
    }
}
