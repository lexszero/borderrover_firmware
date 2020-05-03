#pragma once

#include <shared_mutex>
#include "freertos/FreeRTOS.h"
#include "util_task.hpp"
#include "util_event.hpp"
#include "vesc.hpp"

#include "nlohmann/json.hpp"
using nlohmann::json;

enum MotorId {
	Left = 0,
	Right = 1,
};

class MotionControl : private Task {
public:
	MotionControl(Vesc& _m_l, Vesc& _m_r);

	struct Param {
		int dt = 50;
		float speed_start = 0.2,
			  speed_turn = 0.2,
			  acceleration = 0.05;
		float speed_max = 1;

		bool brake_reverse_enabled = false;
		float brake_reverse_current = 10000;
	} param;

	struct State {
	public:
		TickType_t timestamp;

		float speed = 0;	// how fast we go, positive is forward
		float omega = 0;	// how hard we turn, positive is right. if |ω| > 0.5 - turn with both sides
	
		float d_speed = 0;	// delta for speed
		float d_omega = 0;	// delta for omega
	
		float throttle_l = 0;
		float throttle_r = 0;
	
		bool moving = false;
		bool braking = false;
		bool accelerating = false;

		void print() const;
		json as_json() const;
	};

	using CallbackFn = std::function<void()>;
	const State get_state();
	void on_state_update(CallbackFn cb);

	const Vesc::vescData& get_motor_values(MotorId id);

	void on_motor_values(CallbackFn cb);

	enum Event {
		MotorValues_Left = (1 << 0),
		MotorValues_Right = (1 << 1),
		MotorValues = MotorValues_Left  | MotorValues_Right,

		StateUpdate = (1 << 8),

		Any = 0xff,
	};
	const EventGroup<Event>& get_events();

	// Go straight
	void go(bool reverse);

	// Turns
	void turn_left();
	void turn_right();

	// Let motors spin freely
	void idle();
	// Set brake on/of
	void set_brake(bool on);
	void set_accelerate(bool on);

	void reset_accel();
	void reset_turn();

private:
	Vesc& m_l;
	Vesc& m_r;
	Vesc& motor_by_id(MotorId id);

	CallbackFn motor_values_callback;

	State state;
	mutable std::shared_mutex state_mutex;
	CallbackFn state_update_callback;
	using state_lock = std::unique_lock<std::shared_mutex>;
	state_lock get_state_lock();

	EventGroup<Event> events;

	void run() override;
	void time_advance(state_lock& lock);
	void state_notify();
	bool update(state_lock&& lock, bool notify = false);
	void idle_unlocked();
	void reset_accel_unlocked();
	float convert_speed(float v);
	void go_l(float v);
	void go_r(float v);
};

void to_json(json& j, const MotionControl::State& state);
void to_json(json& j, const Vesc::vescData& data);
