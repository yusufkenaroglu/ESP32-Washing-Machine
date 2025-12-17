#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_MENU_LOGO = 0,
    UI_MENU_DEFAULT,
    UI_MENU_WASH_SETTINGS,
    UI_MENU_FREEHOME,
    UI_MENU_MACHINE_SETTINGS,
} ui_menu_t;

typedef struct {
    ui_menu_t menu;
    int wash_cursor;
    int machine_cursor;
    bool editing;
    int freehome_page;    // current page in FreeHome wizard
    int freehome_button;  // 0=Back, 1=Next (selected)
} ui_render_state_t;

typedef enum {
    UI_WASH_OPTION_TEMPERATURE = 0,
    UI_WASH_OPTION_SPIN,
    UI_WASH_OPTION_SOIL,
    UI_WASH_OPTION_PREWASH,
    UI_WASH_OPTION_EXTRA_RINSE,
    UI_WASH_OPTION_MACHINE_SETTINGS,
    UI_WASH_OPTION_BACK,
    UI_WASH_OPTION_COUNT
} ui_wash_option_t;

typedef enum {
    UI_MACHINE_OPTION_ABOUT = 0,
    UI_MACHINE_OPTION_DISPLAY,
    UI_MACHINE_OPTION_ADVANCED,
    UI_MACHINE_OPTION_FREEHOME,
    UI_MACHINE_OPTION_BACK,
    UI_MACHINE_OPTION_COUNT
} ui_machine_option_t;

void ui_controller_reset(void);
void ui_controller_show_logo(void);
void ui_controller_handle_start_long_press(void);
// Returns true if the event was consumed (no start/stop action should run)
bool ui_controller_handle_start_press(void);
void ui_controller_handle_dial_delta(int delta);
ui_render_state_t ui_controller_get_render_state(void);
const char *ui_wash_option_label(int idx);
void ui_wash_option_value_string(int idx, char *buf, size_t len);
const char *ui_machine_option_label(int idx);

// Set the current FreeHome wizard page from external modules (thread-safe enough
// for UI task use). This is used by the display code to advance the wizard when
// provisioning events occur (e.g. a client connects to the device AP).
void ui_controller_set_freehome_page(int page);

#ifdef __cplusplus
}
#endif
