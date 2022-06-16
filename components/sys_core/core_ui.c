#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "core_ui.h"

#define LED_GPIO GPIO_NUM_2

static const char *TAG = "ui";

static ui_led_mode led_mode;
static bool led_state;
static esp_timer_handle_t led_timer;

static inline void led_off()
{
	ESP_LOGD(TAG, "led off");
	led_state = false;
	gpio_set_level(LED_GPIO, 0);
}

static inline void led_on()
{
	ESP_LOGD(TAG, "led on");
	led_state = true;
	gpio_set_level(LED_GPIO, 1);
}

static void led_toggle()
{
	led_state = !led_state;
	gpio_set_level(LED_GPIO, led_state);
}

static void led_timer_reset(int ms)
{
	esp_timer_stop(led_timer);
	ESP_ERROR_CHECK(esp_timer_start_once(led_timer, ms * 1000));
}

static void led_timer_callback(void *arg)
{
	static int blink = 0;
	switch (led_mode) {
		case UI_LED_OFFLINE:
			led_toggle();
			led_timer_reset(300);
			break;

		case UI_LED_IDLE:
			led_toggle();
			led_timer_reset(1000);
			break;

		case UI_LED_OTA:
			if (blink < 3) {
				if (!led_state) {
					led_on();
				} else {
					blink++;
					led_off();
				}
				led_timer_reset(200);
			} else {
				blink = 0;
				led_off();
				led_timer_reset(500);
			}
			break;
	}
}

void ui_init()
{
	gpio_pad_select_gpio(LED_GPIO);
	gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

	const esp_timer_create_args_t led_timer_args = {
		.callback = &led_timer_callback,
		.name = "led"
	};

	ESP_ERROR_CHECK(esp_timer_create(&led_timer_args, &led_timer));
}

void ui_set_led_mode(ui_led_mode mode) {
	ESP_LOGI(TAG, "set mode %d", mode);
	led_mode = mode;
	led_timer_callback(NULL);
}
