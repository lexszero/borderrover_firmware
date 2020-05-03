#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "argtable3/argtable3.h"

typedef struct {
	struct arg_str *dir;
	struct arg_end *end;
} app_cmd_t;

void app_start();

#ifdef __cplusplus
};

#include <thread>
#include <esp_pthread.h>
#include "uart_tcp_server.h"
#include "esp_r1_api.h"
#include "vesc.hpp"
#include "motion_control.hpp"
#include "motion_control_monitor.hpp"

class Application {
public:
	Application();
	void tcpBridgeStart();

	void btRemoteStart();

	void handleCmd(app_cmd_t *args);
private:
	enum control_mode_e {
		CONTROL_MODE_DISABLED,
		CONTROL_MODE_TCP_BRIDGE,
		CONTROL_MODE_BT_REMOTE,
	};

	struct {
		uart_tcp_server_t *srv1;
		uart_tcp_server_t *srv2;
	} tcp_bridge;

	control_mode_e control_mode = CONTROL_MODE_DISABLED;

	class controller {
	public:
		controller();

		VescUartInterface left_iface, right_iface;
		Vesc left, right;
		MotionControl mc;
		MotionControlMonitor monitor;
	};

	controller *ctrl;

	static void btRemoteKeyCb(esp_r1_keyboard_data_t *data);
	static void btRemotePointerCb(esp_r1_pointer_data_t *data);
	static void btRemoteDeviceEventCb(enum esp_r1_device_event_e event);
};
#endif
