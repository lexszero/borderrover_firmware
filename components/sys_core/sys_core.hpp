#pragma once

#include "driver/gpio.h"

namespace Core {

struct SystemConfig {
	gpio_num_t status_led_gpio;
};

static constexpr SystemConfig DefaultSystemConfig{
	.status_led_gpio = GPIO_NUM_2
};

void init(const SystemConfig& conf = DefaultSystemConfig);

}  // namespace

void core_init();
