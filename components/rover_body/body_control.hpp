#pragma once

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include "gpio_io.hpp"

#include <chrono>
#include <shared_mutex>
#include <string>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"

#include "leds.hpp"
#include "core_control_types.hpp"
#include "esp_timer_cxx.hpp"
#include "util_task.hpp"
#include "util_event.hpp"
#include "util_lockable.hpp"
#include "util_misc.hpp"

#include "nlohmann/json.hpp"
using nlohmann::json;

using namespace std::literals;
using std::chrono::milliseconds;
using Clock = std::chrono::system_clock;
using duration = milliseconds;
using time_point = std::chrono::time_point<Clock, milliseconds>;

enum OutputId
{
	Valve0,
	Valve1,
	Valve2,
	Pump,
	Igniter,
	Aux0,
	Aux1,
	Lockout,
	_TotalCount,
	_PulsedCount = 7
};

OutputId from_string(const std::string& s);

class PulsedOutput :
	private Lockable<std::shared_mutex>
{
	public:
		PulsedOutput() = delete;
		PulsedOutput(OutputGPIO& gpio, int order,
				duration min_pulse_length = 10ms,
				duration max_pulse_length = 10*1000ms,
				duration min_pause_length = 100ms);

		void reset();
		bool get();
		void set(bool state);

		friend std::ostream& operator<<(std::ostream& os, const PulsedOutput& out);

	private:
		using unique_lock = Lockable::unique_lock;
		std::string name;

		OutputGPIO& gpio;

		const duration min_pulse_length, max_pulse_length, min_pause_length;
		time_point t_last_on, t_last_off;

		idf::esp_timer::ESPTimer timer_single;
		idf::esp_timer::ESPTimer timer_periodic;
		bool running_single, running_periodic;

		void reset(const unique_lock& lock);
		void reset_single(const unique_lock& lock);
		void reset_periodic(const unique_lock& lock);
		void apply(const unique_lock& lock);
		void pulse_single(const unique_lock& lock, const duration& length);
		void pulse_periodic(const unique_lock& lock, const duration& length, const duration& period);

		duration relative_pulse_length(uint32_t percent);

	public:
		Core::ControlRange<uint32_t, 100> t_on;
		Core::ControlRange<uint32_t, 100> period;
		Core::ControlPushButton trigger_single;
		Core::ControlSwitch enable;
};

class BodyControl :
	private Task,
	private Lockable<std::shared_mutex>
{
	public:

		BodyControl();

		enum class Mode : int {
			Disabled,
			Direct,
			Pulse,
			Burst,
			Remote
		};

		struct State
		{
			using Outputs = std::array<PulsedOutput, static_cast<size_t>(OutputId::_PulsedCount)>;

			State(BodyControl& bc);

			time_point timestamp;
			Mode mode;

			static std::array<OutputGPIO, static_cast<size_t>(OutputId::_TotalCount)> gpios;

			OutputGPIO& lockout;
			Outputs outs;

			void print() const;
			json as_json() const;

			OutputGPIO& get_gpio(OutputId output);
			PulsedOutput& get_output(OutputId output);
			OutputId get_output_id(const std::string& name);
		};

		using CallbackFn = std::function<void()>;
		void on_state_update(CallbackFn cb);

		enum Event {
			StateUpdate = (1 << 0),

			Any = 0xff,
		};
		const EventGroup<Event>& get_events();

		void reset();
		OutputId get_output_id(const std::string& name);
		void set_output(OutputId output, bool on);
		void get_output(OutputId output) const;
		void pulse_output(OutputId output, uint8_t length_percent);
		void test_outputs();
		const State& get_state() const;
		void print_state() const;

		static BodyControl& instance();

		Core::ControlSwitch lockout;

	private:
		static constexpr auto WAIT_TIMEOUT = 10ms;

		std::unique_ptr<State> state;
		CallbackFn state_update_callback;

		std::unique_ptr<Leds::Output> leds;

		EventGroup<Event> events;

		void run() override;
		void notify();

		void register_console_cmd();
		void handle_console_cmd(int argc, char **argv);

		static std::shared_ptr<BodyControl> singleton_instance;

		friend class BodyControl::State;
};

void to_json(json& j, const BodyControl::State& state);
