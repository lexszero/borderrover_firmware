//#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include "app.hpp"
#include <math.h>

#define TAG "motion_control"

float clip(float v, float min, float max) {
	if (v < min)
		v = min;
	if (v > max)
		v = max;
	return v;
}

MotionControl::MotionControl(Vesc&& _m_l, Vesc&& _m_r) :
	m_l(_m_l),
	m_r(_m_r)
{
	auto thr_cfg = esp_pthread_get_default_config();
	thr_cfg.thread_name = TAG;
	thr_cfg.prio = 10;
	esp_pthread_set_cfg(&thr_cfg);
	task = std::thread([this]() { run(); });
}

void MotionControl::printState() {
	const char *s_dir, *s_turn;
	if (speed > 0) {
		s_dir = "forward";
	}
	else if (speed < 0) {
		s_dir = "backward";
	}
	else {
		s_dir = "stop";
	}

	if (omega > 0) {
		s_turn = "right";
	}
	else if (omega < 0) {
		s_turn = "left";
	}
	else {
		s_turn = "-";
	}

	ESP_LOGI(TAG, "Control: speed %f (%s, Δ = %f), omega %f (%s, Δ = %f)", speed, s_dir, d_speed, omega, s_turn, d_omega);
	ESP_LOGI(TAG, "Motor: left %f, right %f ", throttle_l, throttle_r);
}

void MotionControl::run() {
	while (1) {
		timeAdvance();
		update();
		vTaskDelay(param.dt / portTICK_PERIOD_MS);
	}
}

int MotionControl::convertSpeed(float v) {
	if (v > 1.0)
		v = 1.0;
	return v * param.speed_max;
}

void MotionControl::goL(float v) {
	throttle_l = v;
	m_l.setDuty(convertSpeed(v));
}

void MotionControl::goR(float v) {
	throttle_r = v;
	m_r.setDuty(convertSpeed(v));
}

void MotionControl::go(bool reverse) {
	ESP_LOGI(TAG, "go %s", reverse ? "backward" : "forward");
	if (braking)
		return;
	int sign = reverse ? -1 : 1;
	speed = param.speed_forward * sign;
	d_speed = param.acceleration * sign;
	moving = true;
	update();
}

void MotionControl::turnLeft() {
	ESP_LOGI(TAG, "turn left");
	if (braking)
		return;
	omega = -param.speed_turn;
	d_omega = -param.acceleration;
	moving = true;
	update();
}

void MotionControl::turnRight() {
	ESP_LOGI(TAG, "turn right");
	if (braking)
		return;
	omega = param.speed_turn;
	d_omega = param.acceleration;
	moving = true;
	update();
}

void MotionControl::idle() {
	ESP_LOGI(TAG, "idle");
	moving = false;
	resetAccel(false);
	resetTurn(false);
	m_l.setCurrent(0);
	m_r.setCurrent(0);
	update();
}

void MotionControl::setBrake(bool on) {
	ESP_LOGI(TAG, "brake %s", on ? "on" : "off");
	if (on) {
		braking = true;
		moving = false;
		m_l.setRPM(0);
		m_r.setRPM(0);
	}
	else {
		braking = false;
		idle();
	}
}

void MotionControl::timeAdvance() {
	if (!moving)
		return;

	omega = clip(omega + d_omega, -1.0, 1.0);
	speed = clip(speed + d_speed, -1.0, 1.0);
}

void MotionControl::update() {
	float v_l, v_r;

	float omega_abs = fabs(omega);
	float omega_sign = (omega > 0) ? 1 : -1;
	float omega_inverse;
	if (omega_abs < 0.5)
		omega_inverse = 0;
	else
		omega_inverse = (omega_abs - 0.5) * 2;

	ESP_LOGD(TAG, "speed %f, omega abs %f, sign %f, inverse %f",
			speed, omega_abs, omega_sign, omega);

	v_l = speed + ((omega_sign < 0) ? omega_abs : -omega_inverse);
	v_r = speed + ((omega_sign > 0) ? omega_abs : -omega_inverse);

	ESP_LOGD(TAG, "before: left %f, right %f", v_l, v_r);

	float v_l_abs = fabs(v_l);
	float v_l_sign = (v_l > 0) ? 1 : -1;
	float v_r_abs = fabs(v_r);
	float v_r_sign = (v_r > 0) ? 1 : -1;

	if (v_l_abs > 1.0) {
		v_r_abs -= (v_l_abs - 1.0);
		v_l_abs = 1.0;
	}
	if (v_r_abs > 1.0) {
		v_l_abs -= (v_r_abs - 1.0);
		v_r_abs = 1.0;
	}

	v_l = v_l_sign * v_l_abs;
	v_r = v_r_sign * v_r_abs;

	bool changed = false;
	if (v_l != throttle_l || v_r != throttle_r)
		changed = true;

	if (speed == 0 && omega == 0) {
		m_l.setCurrent(0);
		m_r.setCurrent(0);
	}

	if (changed || moving) {
		goL(v_l);
		goR(v_r);
	}

	if (changed)
		printState();
}

void MotionControl::resetAccel(bool upd) {
	speed = 0;
	d_speed = 0;
	if (upd)
		update();
}

void MotionControl::resetTurn(bool upd) {
	omega = 0;
	d_omega = 0;
	if (upd)
		update();
}
