#include "sys_core.hpp"
#include "body_control.hpp"

#include <memory>

std::unique_ptr<BodyControl> bc;

extern "C" void app_main() {
	Core::init({.status_led_gpio = GPIO_NUM_27});
	bc = std::make_unique<BodyControl>();
}
