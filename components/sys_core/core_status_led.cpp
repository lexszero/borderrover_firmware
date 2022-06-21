#include "util_misc.hpp"

#include "core_status_led.hpp"

namespace Core {

std::unique_ptr<StatusLed> status_led;

StatusLed::State::State() :
	mode(Mode::Solid),
	on_ms(500),
	off_ms(500),
	pulse_repeat(0),
	pulse_repeat_interval(0),
	state(false)
{}

StatusLed::StatusLed(const char *name, OutputGPIO&& gpio) :
	Lockable(name),
	name(name),
	gpio(std::move(gpio)),
	timer([this](void) { timer_cb(); }),
	current(),
	previous()
{
	ESP_LOGI(name, "Created status LED");
}

void StatusLed::set(bool new_state)
{
	current.state = new_state;
	set_mode(Mode::Solid);
}

void StatusLed::set_mode(Mode new_mode)
{
	current.mode = new_mode;
	switch (new_mode) {
		case Mode::Solid:
			gpio.set(current.state);
			break;

		case Mode::Blinking:
			gpio.set(true);
			timer_reset(current.on_ms);
			break;
		
		case Mode::Single:
			gpio.set(!gpio.get());
			timer_reset(current.on_ms);
			break;
	}
}

void StatusLed::blink_once(uint32_t _on_ms, uint32_t _repeat)
{
	ESP_LOGD(name, "Blink once on=%d repeat=%d", _on_ms, _repeat);
	previous = current;
	current.on_ms = _on_ms;
	current.off_ms = _on_ms;
	current.pulse_repeat = _repeat;
	set_mode(Mode::Single);
}

void StatusLed::blink(uint32_t _on_ms, uint32_t _off_ms, uint32_t _repeat, uint32_t _interval_ms)
{
	ESP_LOGD(name, "Blink on=%d off=%d repeat=%d interval=%d",
			_on_ms, _off_ms, _repeat, _interval_ms);
	if (!_off_ms)
		_off_ms = _on_ms;
	current.on_ms = _on_ms;
	current.off_ms = _off_ms;
	current.pulse_repeat = _repeat;
	current.pulse_repeat_interval = _interval_ms;
	set_mode(Mode::Blinking);
}

void StatusLed::timer_reset(uint32_t ms)
{
	try {
		timer.stop();
	}
	catch (...) {};
	timer.start(std::chrono::microseconds(ms*1000));
}

void StatusLed::timer_cb(void)
{
	auto& s = current;
	const auto led_state = gpio.get();
	switch (s.mode) {
		case Mode::Solid:
			break;

		case Mode::Blinking:
			if (led_state) {
				gpio.set(false);
				auto t_off = s.off_ms;
				if (s.pulse_repeat) {
					if (s.pulse_count < s.pulse_repeat) {
						s.pulse_count++;
					}
					else {
						s.pulse_count = 0;
						t_off = s.pulse_repeat_interval;
					}
				}
				timer_reset(t_off);
			}
			else {
				gpio.set(true);
				timer_reset(s.on_ms);
			}
			break;
		
		case Mode::Single:
			gpio.set(!led_state);

			current = previous;
			set_mode(current.mode);
			break;
	}
}


} // namespace Core
