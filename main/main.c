/* Blink Example
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "common.h"

#include "esp_netif.h"
#include "esp_spi_flash.h"
#include "driver/gpio.h"
#include "nvs_flash.h"

#include "ui.h"
#include "sys_console.h"
#include "driver/uart.h"
#include "wifi.h"
#include "ota_server.h"
#include "app.h"

#define TAG "main"

void sys_console_register_commands() {
	wifi_register_commands();
}

void ota_server_start_callback() {
	ui_set_led_mode(UI_LED_OTA);
}

void uart_init(uart_port_t port, int tx_pin, int rx_pin) {
	uart_set_pin(port, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

void app_main() {
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		// 1.OTA app partition table has a smaller NVS partition size than the non-OTA
		// partition table. This size mismatch may cause NVS initialization to fail.
		// 2.NVS partition contains data in new format and cannot be recognized by this version of code.
		// If this happens, we erase NVS partition and initialize NVS again.
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK( err );

	esp_netif_init();
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	wifi_init();
	sys_console_init();
	ui_init();
	ui_set_led_mode(UI_LED_OFFLINE);

	ota_server_init();

	app_start();

	ui_set_led_mode(UI_LED_IDLE);
}
