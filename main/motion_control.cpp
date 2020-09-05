//#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

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

MotionControl::MotionControl(Vesc& _m_l, Vesc& _m_r) :
	Task::Task(TAG, 8*1024, 15),
	m_l(_m_l),
	m_r(_m_r)
{
	m_l.onValues([&](Vesc& m) {
			events.set(MotorValues_Left);
		});
	m_r.onValues([&](Vesc& m) {
			events.set(MotorValues_Right);
		});

	m_l.setBrakeCurrent(param.brake_current);
	m_r.setBrakeCurrent(param.brake_current);

	Task::start();
}

Vesc& MotionControl::motor_by_id(MotorId id)
{
	switch (id) {
		case Left:
			return m_l;
		case Right:
			return m_r;
	}
	throw std::runtime_error("Bad motor id");
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
		{"timestamp", state.timestamp},
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

void to_json(json& j, const Vesc::vescData& data)
{
	j = json {
		{"timestamp", data.timestamp},
		{"I_motor", data.avgMotorCurrent},
		{"I_input", data.avgInputCurrent},
		{"duty", data.dutyCycleNow},
		{"rpm", data.rpm},
		{"U", data.inpVoltage},
		{"E", data.ampHours},
		{"E_ch", data.ampHoursCharged},
		{"tach", data.tachometer},
		{"tach_abs", data.tachometerAbs}
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

void MotionControl::on_state_update(CallbackFn cb)
{
	state_update_callback = cb;
}

void MotionControl::on_motor_values(CallbackFn cb)
{
	motor_values_callback = cb;
}

const Vesc::vescData& MotionControl::get_motor_values(MotorId id)
{
	return motor_by_id(id).data;
}

void MotionControl::run()
{
	while (1) {
		m_l.getValues();
		m_r.getValues();
		auto ev = events.wait(Event(MotorValues_Left | MotorValues_Right),
				param.dt / portTICK_PERIOD_MS,
				true,
				true);

		if (ev) {
			ESP_LOGD(TAG, "got motor values");
			events.set(MotorValues);
			if (motor_values_callback)
				motor_values_callback();
		}

		state_lock lock(state_mutex);
		time_advance(lock);
		update(std::move(lock));
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

float MotionControl::convert_speed(float v)
{
	return clip(v, -1, 1) * param.speed_max;
}

void MotionControl::go_l(float v)
{
	state.throttle_l = v;
	if (v == 0)
		m_l.setCurrent(0);
	else
		m_l.setRPM(convert_speed(v));
}

void MotionControl::go_r(float v)
{
	state.throttle_r = v;
	if (v == 0)
		m_r.setCurrent(0);
	else
		m_r.setRPM(convert_speed(v));
}

/*
void MotionControl::maintain_speed() {

}
*/

void MotionControl::go(bool reverse)
{
	ESP_LOGI(TAG, "go %s", reverse ? "backward" : "forward");
	auto lock = get_state_lock();

	if (state.braking)
		return;

	int dir = reverse ? 1 : -1;

	float current_speed = ((m_l.data.rpm + m_r.data.rpm) / 2) / param.speed_max;
	if (fabs(current_speed) > 0.1) {
		if (dir == sign(current_speed)) {
			state.speed = current_speed;
			state.moving = true;
		}
		else if (dir == -sign(current_speed)) {
			state.braking = true;
			state.moving = false;
		}
	}
	else {
		state.speed = param.speed_start * dir;
		state.moving = true;
	}
	update(std::move(lock), true);
}

void MotionControl::turn_left()
{
	ESP_LOGI(TAG, "turn left");
	auto lock = get_state_lock();
	if (state.braking)
		return;

	float current_speed = ((m_l.data.rpm + m_r.data.rpm) / 2) / param.speed_max;
	if (fabs(current_speed) > 0.1) {
		state.speed = current_speed;
	}
	state.omega = param.speed_turn;
	state.moving = true;

	update(std::move(lock), true);
}

void MotionControl::turn_right()
{
	ESP_LOGI(TAG, "turn right");
	auto lock = get_state_lock();
	if (state.braking)
		return;

	float current_speed = ((m_l.data.rpm + m_r.data.rpm) / 2) / param.speed_max;
	if (fabs(current_speed) > 0.1) {
		state.speed = current_speed;
	}
	state.omega = -param.speed_turn;
	state.moving = true;

	update(std::move(lock), true);
}

void MotionControl::idle_unlocked()
{
	state.moving = false;
	m_l.kill();
	m_r.kill();
	reset_accel_unlocked();
}

void MotionControl::reset_accel_unlocked()
{
	state.speed = 0;
	state.d_speed = 0;
	state.omega = 0;
	state.d_omega = 0;
	state.accelerating = false;
	state.braking = false;
}

void MotionControl::idle()
{
	ESP_LOGI(TAG, "idle");
	auto lock = get_state_lock();
	idle_unlocked();
	update(std::move(lock), true);
}

void MotionControl::set_brake(bool on)
{
	ESP_LOGI(TAG, "brake %s", on ? "on" : "off");
	auto lock = get_state_lock();

	if (on) {
		state.braking = true;
		state.moving = false;
		//m_l.setBrakeCurrent(20000);
		//m_r.setBrakeCurrent(20000);
	}
	else {
		idle_unlocked();
	}
	update(std::move(lock), true);
}

void MotionControl::set_accelerate(bool on) {
	ESP_LOGI(TAG, "accelerate %s", on ? "on" : "off");
	auto lock = get_state_lock();

	state.accelerating = on;
	update(std::move(lock), true);
}

void MotionControl::time_advance(state_lock& lock) {
	if (!state.moving)
		return;

	state.speed = clip(state.speed + state.d_speed, -1.0, 1.0);
	state.omega = clip(state.omega + state.d_omega, -1.0, 1.0);
}

void MotionControl::state_notify() {
	ESP_LOGD(TAG, "changed");
	state.print();
	events.set(Event::StateUpdate);
	if (state_update_callback)
		state_update_callback();
}

bool MotionControl::update(MotionControl::state_lock&& lock, bool notify) {
	ESP_LOGD(TAG, "update");
	float v_l, v_r;
	
	bool changed = false;
	if (state.braking) {
		if (param.brake_reverse_enabled) {
			float current_speed = ((m_l.data.rpm + m_r.data.rpm) / 2) / param.speed_max;
			if (abs(current_speed) < 0.01) {
				m_l.brake();
				m_r.brake();
			}
			else {
				m_l.setCurrent(-sign(current_speed) * param.brake_reverse_current);
				m_r.setCurrent(-sign(current_speed) * param.brake_reverse_current);
			}
		}
		else {
			m_l.brake();
			m_r.brake();
		}
		return false;
	}
	else {
		if (state.accelerating) {
			state.d_speed = param.acceleration * sign(state.speed);
			state.d_omega = param.acceleration * sign(state.omega);
		}
		else {
			state.d_speed = 0;
			state.d_omega = 0;
		}

		v_l = state.speed;
		v_r = state.speed;

		if (state.omega != 0) {
			float omega_abs = abs(state.omega);
			int omega_sign = sign(state.omega);
			if (omega_sign > 0)
				v_r -= omega_abs;
			else
				v_l -= omega_abs;
		}

		if (v_l != state.throttle_l || v_r != state.throttle_r)
			changed = true;

		if (state.speed == 0) {
			m_l.setCurrent(0);
			m_r.setCurrent(0);
		}

		if (changed || state.moving) {
			go_l(v_l);
			go_r(v_r);
		}
	}

	state.timestamp = xTaskGetTickCount();

	lock.unlock();

	if (changed || notify) {
		state_notify();
	}

	return changed;
}

void MotionControl::reset_accel()
{
	auto lock = get_state_lock();
	reset_accel_unlocked();
}

void MotionControl::reset_turn()
{
	auto lock = get_state_lock();
	reset_accel_unlocked();
}
