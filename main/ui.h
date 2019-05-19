#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	UI_LED_OFFLINE,
	UI_LED_IDLE,
	UI_LED_OTA,
} ui_led_mode;

void ui_init();
void ui_set_led_mode(ui_led_mode mode);

#ifdef __cplusplus
};
#endif
