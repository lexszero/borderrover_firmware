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
#include "vesc.hpp"
#include "esp_r1_api.h"

class MotionControl {
public:
	MotionControl(Vesc&& _m_l, Vesc&& _m_r);

	struct {
		int dt = 100;
		float speed_forward = 0.2,
			  speed_turn = 0.2,
			  acceleration = 0.1;
		int speed_max = 30000;
	} param;

	void printState();

	// Spin motors
	void goL(float v);
	void goR(float v);

	// Go straight
	void go(bool reverse);

	// Turns
	void turnLeft();
	void turnRight();

	// Let motors spin freely
	void idle();
	// Set brake on/of
	void setBrake(bool on);

	void resetAccel(bool upd = true);
	void resetTurn(bool upd = true);

private:
	Vesc m_l;
	Vesc m_r;
	std::thread task;

	bool moving = false;
	bool braking = false;

	float speed = 0;	// how fast we go, positive is forward
	float omega = 0;	// how hard we turn, positive is right. if |ω| > 0.5 - turn with both sides

	float d_speed = 0;	// delta for speed
	float d_omega = 0;	// delta for omega

	float throttle_l = 0;
	float throttle_r = 0;

	void run();
	void timeAdvance();
	void update();
	int convertSpeed(float v);
};

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

	struct {
		VescInterface* left_iface, *right_iface;
	} esc;

	MotionControl *mc;

	static void btRemoteKeyCb(esp_r1_keyboard_data_t *data);
	static void btRemotePointerCb(esp_r1_pointer_data_t *data);
	static void btRemoteDeviceEventCb(enum esp_r1_device_event_e event);
};
#endif
