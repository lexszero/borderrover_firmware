#include "sys_core.hpp"
#include "body_control.hpp"

#include <driver/uart.h>

#include <memory>

std::unique_ptr<BodyControl> bc;

extern "C" void app_main() {
	Core::init({.status_led_gpio = GPIO_NUM_2});
	ESP_ERROR_CHECK( uart_set_pin(UART_NUM_1, GPIO_NUM_32, GPIO_NUM_33, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) );
	bc = std::make_unique<BodyControl>();
}
