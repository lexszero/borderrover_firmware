#pragma once

#include <shared_mutex>
#include "freertos/FreeRTOS.h"
#include "task.hpp"
#include "vesc.hpp"

#include "nlohmann/json.hpp"
using nlohmann::json;

class MotionControl : private Task {
public:
	MotionControl(Vesc&& _m_l, Vesc&& _m_r);

	struct Param {
		int dt = 100;
		float speed_start = 0.2,
			  speed_turn = 0.2,
			  acceleration = 0.05;
		float speed_max = 1;

		bool brake_reverse_enabled = false;
		float brake_reverse_current = 10000;
	} param;

	struct State {
	public:
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

	using StateUpdateCb = std::function<void()>;
	const State get_state();
	void on_state_update(StateUpdateCb cb);

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

	void reset_accel(bool upd = true);
	void reset_turn(bool upd = true);

private:
	Vesc m_l;
	Vesc m_r;

	State state;
	mutable std::shared_mutex state_mutex;
	StateUpdateCb state_update_callback;
	using state_lock = std::unique_lock<std::shared_mutex>;
	state_lock get_state_lock();

	void run() override;
	void time_advance();
	bool update(state_lock&& lock);
	void idle_unlocked();
	void reset_accel_unlocked();
	float convert_speed(float v);
	void go_l(float v);
	void go_r(float v);
};

void to_json(json& j, const MotionControl::State& state);
