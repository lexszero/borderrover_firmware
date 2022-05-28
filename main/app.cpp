#include "common.h"
#include <string.h>
#include "ui.h"
#include "app.hpp"
#include "wifi.h"
#include "ota_server.h"
#include "esp_console.h"

#define TAG "app"

Application *app = NULL;

app_cmd_t app_cmd_args;

int app_cmd_handler(int argc, char **argv) {
	if (!app)
		return 1;

	int nerrors = arg_parse(argc, argv, (void **)&app_cmd_args);
	if (nerrors != 0) {
		arg_print_errors(stderr, app_cmd_args.end, argv[0]);
		return 1;
	}
	app->handleCmd(&app_cmd_args);
	return 0;
}

Application::Application() :
	body()
{}

/*
void Application::tcpBridgeStart() {
	ESP_LOGI(TAG, "Starting TCP bridge mode");
	wifi_wait_for_ip();
	tcp_bridge.srv1 = uart_tcp_server_new("uart_tcp_srv_1", 6661, UART_NUM_1);
	tcp_bridge.srv2 = uart_tcp_server_new("uart_tcp_srv_2", 6662, UART_NUM_2);
	control_mode = CONTROL_MODE_TCP_BRIDGE;
};

void Application::btRemoteStart() {
	ESP_LOGI(TAG, "Starting BT remote control mode");
	esp_err_t err;
	err = esp_r1_init();
	ESP_ERROR_CHECK(err);
	err = esp_r1_enable();
	ESP_ERROR_CHECK(err);

	ctrl = new controller();

	esp_r1_keyboard_register_callback(Application::btRemoteKeyCb);
	esp_r1_pointer_register_callback(Application::btRemotePointerCb);
	esp_r1_device_event_register_callback(Application::btRemoteDeviceEventCb);

	control_mode = CONTROL_MODE_BT_REMOTE;
};

void Application::btRemoteKeyCb(esp_r1_keyboard_data_t *data) {
	ESP_LOGD(TAG, "Key %s - %d", esp_r1_event_name_get_by_id(data->id), data->state);
	if (!app)
		return;
	auto& mc = app->ctrl->mc;
	switch (data->id) {
		case R1_AXIS_X:
			switch (data->state) {
				case R1_AXIS_PLUS:
					mc.turn_right();
					break;
				case R1_AXIS_CENTER:
					mc.reset_turn();
					break;
				case R1_AXIS_MINUS:
					mc.turn_left();
					break;
			}
			break;

		case R1_AXIS_Y:
			switch (data->state) {
				case R1_AXIS_PLUS:
					mc.go(true);
					break;
				case R1_AXIS_CENTER:
					mc.reset_accel();
					break;
				case R1_AXIS_MINUS:
					mc.go(false);
					break;
			}
			break;
		
		case R1_BUTTON7:
			mc.set_brake(data->state == R1_KEY_PRESSED);
			break;

		case R1_BUTTON8:
			mc.set_accelerate(data->state == R1_KEY_PRESSED);

		default:
			// ignore
			break;
	}
}

void Application::btRemotePointerCb(esp_r1_pointer_data_t *data) {
	ESP_LOGI(TAG, "Pointer X %d, Y %d", data->x_axis, data->y_axis);
}

void Application::btRemoteDeviceEventCb(enum esp_r1_device_event_e event) {
	if (!app)
		return;
	if (event == R1_EVENT_DISCONNECTED)
		app->ctrl->mc.idle();
}
*/

extern "C" void app_start() {
	app = new Application();
}
