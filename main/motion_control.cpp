#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <utility>
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
	Task::Task(TAG, 8*1024, 10),
	m_l(_m_l),
	m_r(_m_r)
{
	m_l.getValues();
	m_r.getValues();
	Task::start();
}

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

MotionControl::state_lock MotionControl::get_state_lock()
{
	return MotionControl::state_lock(state_mutex);
}

void MotionControl::on_state_update(StateUpdateCb cb)
{
	state_update_callback = cb;
}

void MotionControl::run()
{
	while (1) {
		time_advance();
		/*
		if (braking) {
			m_l.setBrakeCurrent(param.brake_current);
			m_r.setBrakeCurrent(param.brake_current);
			m_l.setRPM(0);
			m_r.setRPM(0);
		}
		else {
			update();1 сотка = acre
		}
		*/
		vTaskDelay(param.dt / portTICK_PERIOD_MS);
	}
}

float MotionControl::convert_speed(float v)
{
	if (v > 1.0)
		v = 1.0;
	return v * param.speed_max;
}

void MotionControl::go_l(float v)
{
	state.throttle_l = v;
	if (v == 0)
		m_l.setCurrent(0);
	else
		m_l.setDuty(-convert_speed(v));
}

void MotionControl::go_r(float v)
{
	state.throttle_r = v;
	if (v == 0)
		m_r.setCurrent(0);
	else
		m_r.setDuty(-convert_speed(v));
}

void MotionControl::go(bool reverse)
{
	ESP_LOGI(TAG, "go %s", reverse ? "backward" : "forward");
	auto lock = get_state_lock();

	if (state.braking)
		return;

	int sign = reverse ? 1 : -1;

	state.speed = param.speed_start * sign;
	state.moving = true;
	update(std::move(lock));
}

void MotionControl::turn_left()
{
	ESP_LOGI(TAG, "turn left");
	auto lock = get_state_lock();
	/*
	if (braking)
		return;
	omega = -param.speed_turn;
	d_omega = -param.acceleration;
	moving = true;
	update();
	*/
}

void MotionControl::turn_right()
{
	ESP_LOGI(TAG, "turn right");
	auto lock = get_state_lock();
	/*
	if (braking)
		return;
	omega = param.speed_turn;
	d_omega = param.acceleration;
	moving = true;
	update();
	*/
}

void MotionControl::idle_unlocked()
{
	state.moving = false;
	reset_accel_unlocked();
	m_l.setCurrent(0);
	m_r.setCurrent(0);
}

void MotionControl::reset_accel_unlocked()
{
	state.speed = 0;
	state.d_speed = 0;
	state.accelerating = false;
}

void MotionControl::idle() {
	ESP_LOGI(TAG, "idle");
	auto lock = get_state_lock();
	idle_unlocked();
	update(std::move(lock));
}

void MotionControl::set_brake(bool on) {
	ESP_LOGI(TAG, "brake %s", on ? "on" : "off");
	auto lock = get_state_lock();

	if (on) {
		state.braking = true;
		state.moving = false;
		if (param.brake_reverse_enabled) {
			m_l.setCurrent(-sign(state.throttle_l) * param.brake_reverse_current);
			m_l.setCurrent(-sign(state.throttle_r) * param.brake_reverse_current);
		}
		else {
			m_l.setRPM(0);
			m_r.setRPM(0);
		}
		//m_l.setBrakeCurrent(20000);
		//m_r.setBrakeCurrent(20000);
	}
	else {
		state.braking = false;
		idle_unlocked();
		update(std::move(lock));
	}
}

void MotionControl::set_accelerate(bool on) {
	ESP_LOGI(TAG, "accelerate %s", on ? "on" : "off");
	auto lock = get_state_lock();

	state.accelerating = on;
	if (state.accelerating) {
		state.d_speed = param.acceleration * sign(state.speed);
	}
	else {
		state.d_speed = 0;
	}
	update(std::move(lock));
}

void MotionControl::time_advance() {
	auto lock = get_state_lock();

	if (!state.moving)
		return;

	state.speed = clip(state.speed + state.d_speed, -1.0, 1.0);
	update(std::move(lock));
}

bool MotionControl::update(MotionControl::state_lock&& lock) {
	float v_l, v_r;

	v_l = state.speed;
	v_r = state.speed;

	bool changed = false;
	if (v_l != state.throttle_l || v_r != state.throttle_r)
		changed = true;

	if (state.speed == 0) {
		m_l.setCurrent(0);
		m_r.setCurrent(0);
	}

	if (changed && state.moving) {
		go_l(v_l);
		go_r(v_r);
	}

	/*
	if (changed && state_update_callback)
		state_update_callback();
	*/

	lock.unlock();

	if (changed) {
		state.print();
		if (state_update_callback)
			state_update_callback();
	}

	return changed;
}

void MotionControl::reset_accel(bool upd)
{
	auto lock = get_state_lock();
	reset_accel_unlocked();
}

void MotionControl::reset_turn(bool upd)
{
}
