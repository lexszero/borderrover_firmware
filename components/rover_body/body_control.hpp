#pragma once

#include <chrono>
#include <shared_mutex>
#include <string>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "util_task.hpp"
#include "util_event.hpp"
#include "esp_log.h"
#include "utils.hpp"
#include "util_lockable.hpp"

#include "nlohmann/json.hpp"
using nlohmann::json;

using namespace std::literals;
using std::chrono::system_clock;
using std::chrono::milliseconds;

enum OutputId
{
	Lockout,
	Valve0,
	Valve1,
	Valve2,
	Pump,
	Igniter,
	Aux0,
	Aux1,
	_Count
};

OutputId from_string(const std::string& s);

class InputGPIO
{
	public:
		InputGPIO(const char* name, gpio_num_t gpio, bool active_low = false) :
			name(name),
			gpio(gpio),
			active_low(active_low)
		{
			ESP_LOGD(name, "init input %s, gpio %d, active_low %d", name, gpio, active_low);
			gpio_pad_select_gpio(gpio);
			gpio_set_direction(gpio, GPIO_MODE_INPUT);
		}

		bool get() const
		{
			return gpio_get_level(gpio) ^ static_cast<int>(active_low);
		}
		
		const char* name;

	protected:
		gpio_num_t gpio;
		bool active_low;
};

class OutputGPIO : public InputGPIO
{
	public:
		OutputGPIO(const char* name, gpio_num_t gpio, bool active_low = false, bool open_drain = false) :
			InputGPIO(name, gpio, active_low)
		{
			ESP_LOGD(name, "init output %s, gpio %d, active_low %d", name, gpio, active_low);
			gpio_pad_select_gpio(gpio);
			if (open_drain) {
				gpio_set_direction(gpio, GPIO_MODE_INPUT_OUTPUT_OD);
				gpio_set_pull_mode(gpio, GPIO_PULLUP_ONLY);
				gpio_pullup_en(gpio);
			}
			else {
				gpio_set_direction(gpio, GPIO_MODE_OUTPUT);
			}
			set(false);
		}

		void on()
		{
			set(true);
		}

		void off()
		{
			set(false);
		}

		inline void set(bool state)
		{
			ESP_LOGI(name, ": set output %s=%d", name, state);
			gpio_set_level(gpio, state ^ static_cast<int>(active_low));
		}
};

class PulsedOutput : public OutputGPIO
{
	public:
		PulsedOutput(const char* name, gpio_num_t gpio, bool active_low,
				TickType_t min_pulse_length = 1, TickType_t max_pulse_length = 0) :
			OutputGPIO(name, gpio, active_low),
			min_pulse_length(min_pulse_length),
			max_pulse_length(max_pulse_length),
			t_last_on(0), t_last_off(1),
			t_pulse_end(0)
		{}

		void pulse(TickType_t length);
		void tick();
	
	private:
		const TickType_t min_pulse_length, max_pulse_length;
		TickType_t t_last_on, t_last_off ;
		TickType_t t_pulse_end;
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
			State();

			std::chrono::time_point<system_clock> timestamp;
			Mode mode;

			static OutputGPIO outs[static_cast<size_t>(OutputId::_Count)];

			void print() const;
			json as_json() const;
		};

		using CallbackFn = std::function<void()>;
		void on_state_update(CallbackFn cb);

		enum Event {
			StateUpdate = (1 << 8),

			Any = 0xff,
		};
		const EventGroup<Event>& get_events();

		void reset();
		void set_output(OutputId output, bool on);
		void get_output(OutputId output) const;
		void pulse_output(OutputId output, uint32_t length_ms);
		void test_outputs();
		void print_state() const;

		static BodyControl& instance();

	private:
		using time_point = std::chrono::time_point<system_clock>;
		using duration = std::chrono::duration<system_clock>;

		static constexpr auto PULSE_LEN_MIN = 10ms;
		static constexpr auto PULSE_LEN_MAX = 30*1000ms;
		static constexpr auto WAIT_TIMEOUT = 10ms;

		State state;
		CallbackFn state_update_callback;

		EventGroup<Event> events;

		void run() override;
		void notify();
		void idle_unlocked();

		void register_console_cmd();
		void handle_console_cmd(int argc, char **argv);

		static std::shared_ptr<BodyControl> singleton_instance;
};

void to_json(json& j, const BodyControl::State& state);
