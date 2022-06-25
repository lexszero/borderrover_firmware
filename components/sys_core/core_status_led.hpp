#pragma once

#include "core_gpio.hpp"
#include "esp_timer_cxx.hpp"

#include "util_lockable.hpp"

#include <memory>

namespace Core {

class StatusLed :
	private Lockable<std::shared_mutex>
{
	public:
		enum class Mode : int {
			Solid,
			Blinking,
			Single,
		};

		StatusLed(const char *name, OutputGPIO&& gpio);

		void set(bool state);
		void set_mode(Mode mode);
		void blink_once(uint32_t on_ms, uint32_t repeat = 0);
		void blink(uint32_t on_ms, uint32_t off_ms = 0,
				uint32_t repeat = 0, uint32_t interval_ms = 0);

	private:
		const char *name;
		OutputGPIO gpio;
		idf::esp_timer::ESPTimer timer;
		
		struct State {
			State();

			Mode mode;

			uint32_t on_ms, off_ms;
			uint32_t pulse_repeat, pulse_repeat_interval, pulse_count;
			bool state;
		};

		State current, previous;

		void timer_reset(uint32_t ms);
		void timer_cb(void);
};

extern std::shared_ptr<StatusLed> status_led;

} // namespace Core
