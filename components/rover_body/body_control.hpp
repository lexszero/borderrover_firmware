#pragma once

//#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include "body_control_message.hpp"
#include "core_control_types.hpp"
#include "core_gpio.hpp"
#include "hover_drive.hpp"

#include "cxx_espnow.hpp"
#include "esp_timer_cxx.hpp"
#include "leds.hpp"
#include "util_task.hpp"
#include "util_event.hpp"
#include "util_lockable.hpp"
#include "util_misc.hpp"
#include "util_time.hpp"

#include <freertos/FreeRTOS.h>
#include <esp_log.h>

#include "nlohmann/json.hpp"

#include <chrono>
#include <shared_mutex>
#include <string>

using nlohmann::json;

using Core::OutputGPIO;

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
			time_point lockout_change_time;
			Mode mode;

			static std::array<OutputGPIO, static_cast<size_t>(OutputId::_TotalCount)> gpios;

			OutputGPIO& lockout;
			Outputs outs;

			void print() const;
			json as_json() const;
			BodyPackedState pack() const;

			OutputGPIO& get_gpio(OutputId output);
			PulsedOutput& get_output(OutputId output);
			OutputId get_output_id(const std::string& name);
		};

		using CallbackFn = std::function<void()>;
		void on_state_update(CallbackFn cb);

		enum Event {
			StateUpdate = (1 << 0),
			RemoteLinkUp = (1 << 1),
			RemoteLinkDown = (1 << 2),

			Any = 0xff,
		};
		const EventGroup<Event>& get_events();

		void reset();
		OutputId get_output_id(const std::string& name);
		bool set_output(OutputId output, bool on);
		void pulse_output(OutputId output, uint8_t length_percent);
		void test_outputs();
		const State& get_state() const;
		void print_state() const;

		static BodyControl& instance();

		Core::ControlSwitch lockout;

	private:
		using unique_lock = Lockable::unique_lock;
		static constexpr auto WAIT_TIMEOUT = 10ms;

		std::unique_ptr<State> state;
		time_point last_state_send_time;
		CallbackFn state_update_callback;

		HoverDrive drive;
		Leds::Output leds;
		Core::StatusLed led_remote, led_action;

		EventGroup<Event> events;

		void run() override;
		void notify();
		bool set_output(const unique_lock& lock, OutputId output, bool on);

		void register_console_cmd();
		void handle_console_cmd(int argc, char **argv);

		template <typename StateType>
		struct ControlDevice
		{
			StateType state;
			time_point last_message_time;
			time_point state_time;
		};
		ControlDevice<RemoteState> remote;
		ControlDevice<JoypadState> joypad;

		void handle_loop();
		void handle_remote_state(const RemoteState& remote);
		void handle_joypad_state(const JoypadState& joypad);

		void remote_button_mapping_direct(const unique_lock& lock,
				const RemoteState& st, RemoteButton button, OutputId output);
		void remote_button_mapping_toggle(const unique_lock& lock,
				const RemoteState& st, RemoteButton button, OutputId output);
		void joypad_button_mapping_direct(const unique_lock& lock,
				const JoypadState& st, JoypadButton button, OutputId output);
		void joypad_button_mapping_toggle(const unique_lock& lock,
				const JoypadState& st, JoypadButton button, OutputId output);

		void joystick_drive(int x, int y);


		static std::shared_ptr<BodyControl> singleton_instance;

		friend class BodyControl::State;
};

void to_json(json& j, const BodyControl::State& state);
