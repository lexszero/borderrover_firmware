#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <math.h>

#include "esp_log.h"

#include "motion_control.hpp"

#define TAG "mc"

float clip(float v, float min, float max) {
	if (v < min)
		v = min;
	if (v > max)
		v = max;
	return v;
}

int sign(float v) {
	if (v < 0)
		return -1;
	if (v > 0)
		return 1;
	else
		return 0;
}

MotionControl::MotionControl(Vesc&& _m_l, Vesc&& _m_r) :
	Task(TAG, 10),
	m_l(_m_l),
	m_r(_m_r)
{}

void MotionControl::State::print() const
{
	static const char *dir[9] = {
		"↖", "↑", "↗",
		"←", "⌷", "→",
		"↙", "↓", "↘",
	};
	const char *dir_arrow = dir[(sign(speed)+1)*3 + sign(omega) + 1];
	const char *dir_motor_l = dir[(sign(throttle_l)+1)*3 + 1];
	const char *dir_motor_r = dir[(sign(throttle_r)+1)*3 + 1];

	ESP_LOGI(TAG, "Control: [%s] [speed %f, Δ = %f] [omega %f, Δ = %f]",
			dir_arrow,
			speed, d_speed,
			omega, d_omega);

	ESP_LOGI(TAG, "Motor: % 5.3f %s=%s % -5.3f",
		throttle_l, dir_motor_l, dir_motor_r, throttle_r);
}

void to_json(json& j, const MotionControl::State& state)
{
	j = json {
		{"speed", state.speed},
		{"d_speed", state.d_speed},
		{"omega", state.omega},
		{"d_omega", state.d_omega},
		{"throttle_l", state.throttle_l},
		{"throttle_r", state.throttle_r},
		{"moving", state.moving},
		{"accelerating", state.accelerating},
		{"braking", state.braking}
	};
}

const MotionControl::State MotionControl::get_state()
{
	std::shared_lock lock(state_mutex);
	return state;
}

void MotionControl::on_state_update(StateUpdateCb cb)
{
	state_update_callback = cb;
}

void MotionControl::run()
{
	while (1) {
		timeAdvance();
		/*
		if (braking) {
			m_l.setBrakeCurrent(param.brake_current);
			m_r.setBrakeCurrent(param.brake_current);
			m_l.setRPM(0);
			m_r.setRPM(0);
		}
		else {
			update();
		}
		*/
		vTaskDelay(param.dt / portTICK_PERIOD_MS);
	}
}

float MotionControl::convertSpeed(float v) {
	if (v > 1.0)
		v = 1.0;
	return v * param.speed_max;
}

void MotionControl::goL(float v) {
	/*
	throttle_l = v;
	if (v == 0)
		m_l.setCurrent(0);
	else
		m_l.setDuty(-convertSpeed(v));
	*/
	//if (moving && v == 0) {
	//	m_l.setRPM(0);
	//}
}

void MotionControl::goR(float v) {
	/*
	throttle_r = v;
	if (v == 0)
		m_r.setCurrent(0);
	else
		m_r.setDuty(-convertSpeed(v));
	*/
	//if (moving && v == 0) {
	//	m_r.setRPM(0);
	//}
}

void MotionControl::go(bool reverse) {
	ESP_LOGI(TAG, "go %s", reverse ? "backward" : "forward");
	/*
	if (braking)
		return;
	sign = reverse ? 1 : -1;
	speed = param.speed_forward * sign;
	moving = true;
	update();
	*/
}

void MotionControl::turnLeft() {
	ESP_LOGI(TAG, "turn left");
	/*
	if (braking)
		return;
	omega = -param.speed_turn;
	d_omega = -param.acceleration;
	moving = true;
	update();
	*/
}

void MotionControl::turnRight() {
	ESP_LOGI(TAG, "turn right");
	/*
	if (braking)
		return;
	omega = param.speed_turn;
	d_omega = param.acceleration;
	moving = true;
	update();
	*/
}

void MotionControl::idle() {
	ESP_LOGI(TAG, "idle");
	/*
	moving = false;
	resetAccel(false);
	resetTurn(false);
	m_l.setCurrent(0);
	m_r.setCurrent(0);
	update();
	*/
}

void MotionControl::setBrake(bool on) {
	ESP_LOGI(TAG, "brake %s", on ? "on" : "off");
	/*
	if (on) {
		braking = true;
		moving = false;
		m_l.setRPM(0);
		m_r.setRPM(0);
		//m_l.setBrakeCurrent(20000);
		//m_r.setBrakeCurrent(20000);
	}
	else {
		braking = false;
		idle();
	}
	*/
}

void MotionControl::setAccelerate(bool on) {
	ESP_LOGI(TAG, "accelerate %s", on ? "on" : "off");
	/*
	accelerating = on;
	*/
}

void MotionControl::timeAdvance() {
	/*
	if (!moving)
		return;

	if (accelerating) {
		d_speed = param.acceleration * sign;
		d_omega = param.acceleration;
	}
	else {
		d_speed = 0;
		d_omega = 0;
	}
	omega = clip(omega + d_omega, -1.0, 1.0);
	speed = clip(speed + d_speed, -1.0, 1.0);
	*/
}

void MotionControl::update() {
	/*
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
	*/
}

void MotionControl::resetAccel(bool upd) {
	/*
	speed = 0;
	d_speed = 0;
	accelerating = false;
	if (upd)
		update();
	*/
}

void MotionControl::resetTurn(bool upd) {
	/*
	omega = 0;
	d_omega = 0;
	if (upd)
		update();
	*/
}
