/* Blink Example
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "common.h"

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
	const uart_config_t uart_config = {
		.baud_rate = 115200,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE
	};
	
	ESP_LOGI(TAG, "Initializing UART %d", port);
	uart_param_config(port, &uart_config);
	uart_set_pin(port, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
	uart_driver_install(port, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
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

	tcpip_adapter_init();
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	wifi_init();
	sys_console_init();
	ui_init();
	ui_set_led_mode(UI_LED_OFFLINE);

	ota_server_init();

	uart_init(UART_NUM_1, GPIO_NUM_19, GPIO_NUM_18);
	uart_init(UART_NUM_2, GPIO_NUM_17, GPIO_NUM_16);

	app_start();

	ui_set_led_mode(UI_LED_IDLE);
}
