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

Application::Application() {};

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

	esc.left_iface = new VescUartInterface("esc_left", UART_NUM_1);
	esc.right_iface = new VescUartInterface("esc_right", UART_NUM_2);
	
	mc = new MotionControl(
			Vesc(*esc.left_iface),
			Vesc(*esc.right_iface));

	mc_monitor = new MotionControlMonitor(
			*mc, 8042);

	esp_r1_keyboard_register_callback(Application::btRemoteKeyCb);
	esp_r1_pointer_register_callback(Application::btRemotePointerCb);
	esp_r1_device_event_register_callback(Application::btRemoteDeviceEventCb);

	const esp_console_cmd_t cmd = {
		.command = "go",
		.help = "Drive around",
		.hint = NULL,
		.func = app_cmd_handler,
		.argtable = &app_cmd_args,
	};
	app_cmd_args.dir = arg_str0(NULL, NULL, "<direction>", "Direction to go (fwd, back, left, right)");
	app_cmd_args.end = arg_end(1);
	ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));

	control_mode = CONTROL_MODE_BT_REMOTE;
};

void Application::btRemoteKeyCb(esp_r1_keyboard_data_t *data) {
	ESP_LOGD(TAG, "Key %s - %d", esp_r1_event_name_get_by_id(data->id), data->state);
	if (!app)
		return;
	auto mc = app->mc;
	switch (data->id) {
		case R1_AXIS_X:
			switch (data->state) {
				case R1_AXIS_PLUS:
					mc->turn_right();
					break;
				case R1_AXIS_CENTER:
					mc->reset_turn();
					break;
				case R1_AXIS_MINUS:
					mc->turn_left();
					break;
			}
			break;

		case R1_AXIS_Y:
			switch (data->state) {
				case R1_AXIS_PLUS:
					mc->go(true);
					break;
				case R1_AXIS_CENTER:
					mc->reset_accel();
					break;
				case R1_AXIS_MINUS:
					mc->go(false);
					break;
			}
			break;
		
		case R1_BUTTON7:
			mc->set_brake(data->state == R1_KEY_PRESSED);
			break;

		case R1_BUTTON8:
			mc->set_accelerate(data->state == R1_KEY_PRESSED);

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
		app->mc->idle();
}

void Application::handleCmd(app_cmd_t *args) {
	const char *dir = args->dir->sval[0];
	if (strcmp(dir, "fwd") == 0) {
		app->mc->go(false);
	}
	else if (strcmp(dir, "l") == 0) {
		app->mc->turn_left();
	}
	else if (strcmp(dir, "r") == 0) {
		app->mc->turn_right();
	}
	else if (strcmp(dir, "idle") == 0) {
		app->mc->idle();
	}
	else if (strcmp(dir, "state") == 0) {
		app->mc->get_state().print();
	}
}

extern "C" void app_start() {
	app = new Application();
	app->btRemoteStart();
	//app->tcpBridgeStart();
}
