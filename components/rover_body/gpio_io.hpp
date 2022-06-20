#pragma once

#include "esp_log.h"
#include "driver/gpio.h"

#include <iostream>
#include <functional>

class InputGPIO
{
	public:
		using CallbackFn = std::function<void(bool)>;

		InputGPIO(const char* name, gpio_num_t gpio, bool active_low = false) :
			name(name),
			gpio(gpio),
			active_low(active_low),
			cb_on_change(nullptr)
		{
			ESP_LOGD(name, "init input %s, gpio %d, active_low %d", name, gpio, active_low);
			gpio_pad_select_gpio(gpio);
			gpio_set_direction(gpio, GPIO_MODE_INPUT);
		}

		void on_change(const CallbackFn&& cb)
		{
			cb_on_change = std::move(cb);
		}

		bool get() const
		{
			return gpio_get_level(gpio) ^ static_cast<int>(active_low);
		}
		
		const char* name;

		friend std::ostream& operator<<(std::ostream& os, const InputGPIO& gpio);

	protected:
		gpio_num_t gpio;
		bool active_low;
		virtual void notify()
		{
			if (cb_on_change)
				cb_on_change(get());
		}

	private:
		CallbackFn cb_on_change;
};

class OutputGPIO : public InputGPIO
{
	public:
		OutputGPIO(const char* name, gpio_num_t gpio, bool active_low = false, bool open_drain = false) :
			InputGPIO(name, gpio, active_low)
		{
			ESP_LOGD(name, "init gpio %d, active_low %d", gpio, active_low);
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

		virtual void set(bool state)
		{
			ESP_LOGI(name, "set %d", state);
			gpio_set_level(gpio, state ^ static_cast<int>(active_low));
			notify();
		}
};
