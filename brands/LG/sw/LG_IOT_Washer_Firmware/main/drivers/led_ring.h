#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void program_dial_leds_init(void);
void program_dial_leds_set_selected(int idx);
void program_dial_leds_clear(void);

#ifdef __cplusplus
}
#endif