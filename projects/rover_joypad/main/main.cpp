#define CORE_STATUS_LED_GPIO GPIO_NUM_2
#include "adc.hpp"

#include "sys_core.hpp"
#include "core_status_led.hpp"
#include "core_gpio.hpp"
#include "util_task.hpp"
#include "util_time.hpp"
#include "cxx_espnow.hpp"
#include "body_control_message.hpp"
#include "wifi.h"

#include "argtable3/argtable3.h"
#include "esp_console.h"
#include "esp_log.h"

#include <memory>
#include <string>

#define TAG "app"

using namespace std::literals;
using namespace Core;
using namespace esp_now;

namespace {

static const auto PeerRoverBody = PeerAddress(0x3c, 0x71, 0xbf, 0x58, 0xf2, 0x89);
static constexpr auto JOY_THRESHOLD = 20;

}  // namespace

class Joystick
{
public:
	Joystick(adc_unit_t x_unit, adc_channel_t x_channel, adc_unit_t y_unit, adc_channel_t y_channel, int num_samples = 64, int max = 100) :
		x(x_unit, x_channel),
		y(y_unit, y_channel),
		num_samples(num_samples),
		max_range(max)
	{};

	int16_t get_x()
	{
		return x.get(num_samples, max_range);
	}

	int16_t get_y()
	{
		return y.get(num_samples, max_range);
	}

private:
	struct Axis {
		Axis(adc_unit_t unit, adc_channel_t channel) :
			adc(unit, channel, ADC_WIDTH_BIT_12, ADC_ATTEN_DB_11),
			mv_min(0),
			mv_max(0)
		{}

		int16_t get(int num_samples, int range)
		{
			auto mv = adc.get_voltage_averaged(num_samples);
			if (mv < mv_min)
				mv_min = mv;
			if (mv > mv_max)
				mv_max = mv;
			auto mv_span = mv_max - mv_min;
			auto mv_center = mv_min + mv_span/2;
			return (mv - mv_center) * range / (mv_span/2);
		}

		AdcChannel adc;
		int mv_min, mv_max;
	};

	Axis x, y;
	int num_samples, max_range;
};

class Application :
	private Task
{
public:
	Application();
	
private:
	JoypadState state;
	std::shared_ptr<StatusLed> led_red, led_blue;
	std::array<InputGPIO, to_underlying(JoypadButton::_CountGpio)> buttons_gpio;
	Joystick joy_left, joy_right;
	
	time_point last_send_announce;
	time_point last_send_state;
	time_point last_receive;

	void run() override;
	bool poll_inputs();
};

Application::Application() :
	Task(TAG, 16*1024, 18),
	state(),
	led_red(std::make_shared<StatusLed>("led_red",
				OutputGPIO("led_red", GPIO_NUM_17, true, true))),
	led_blue(Core::status_led),
	buttons_gpio {
		InputGPIO {"L_shift",		GPIO_NUM_0,		true, GPIO_PULLUP_ONLY},
		InputGPIO {"L_joystick",	GPIO_NUM_12,	true, GPIO_PULLUP_ONLY},
		InputGPIO {"L_center",		GPIO_NUM_26,	true, GPIO_PULLUP_ONLY},
		InputGPIO {"R_shift",		GPIO_NUM_35,	true, GPIO_PULLUP_ONLY},
		InputGPIO {"R_joystick",	GPIO_NUM_12,	true, GPIO_PULLUP_ONLY},
		InputGPIO {"R_center",		GPIO_NUM_27,	true, GPIO_PULLUP_ONLY},
	},
	joy_left(ADC_UNIT_1, ADC_CHANNEL_0, ADC_UNIT_1, ADC_CHANNEL_3),
	joy_right(ADC_UNIT_1, ADC_CHANNEL_4, ADC_UNIT_1, ADC_CHANNEL_5),
	last_send_announce(),
	last_send_state(),
	last_receive()
{
	espnow->set_led(led_blue);
	espnow->add_peer(PeerBroadcast, std::nullopt, DEFAULT_WIFI_CHANNEL);
	espnow->add_peer(PeerRoverBody, std::nullopt, DEFAULT_WIFI_CHANNEL);
	espnow->on_recv(static_cast<MessageType>(RoverBodyState),
		[this](const Message& msg) {
			auto& info = msg.payload_as<const BodyPackedState>();
			ESP_LOGI(TAG, "BodyState lockout %d, outputs 0x%04x", info.lockout, info.outputs);
			led_red->set(info.lockout);
			led_red->blink_once(50);
		});

	Task::start();
}

void Application::run()
{
	ESP_LOGI(TAG, "started");
	while (1) {
		const auto now = time_now();
		try {
			if (now - last_send_announce > 1s) {
				last_send_announce = now;
				espnow->send(Message(PeerBroadcast, esp_now::MessageId::Announce));
			}
			bool changed = poll_inputs();
			auto since_last = now - last_send_state;
			if ((changed && since_last > 10ms) || (since_last > 200ms)) {
				ESP_LOGI(TAG, "%s", to_string(state).c_str());
				espnow->send(MessageRoverJoypadState(PeerRoverBody, state));
				last_send_state = now;
			}
		}
		catch (const std::exception& e) {
			ESP_LOGE(TAG, "Exception: %s", e.what());
		}
		std::this_thread::sleep_for(10ms);
	}
}

bool Application::poll_inputs()
{
	bool changed = false;
	JoypadState st;
	int i = 0;
	st.btn_mask = 0;
	for (const auto& btn : buttons_gpio) {
		if (btn.get())
			st.btn_mask |= (1 << i);
		i++;
	}
	st.joy_left.x = joy_left.get_x();
	st.joy_left.y = joy_left.get_y();
	st.joy_right.x = joy_right.get_x();
	st.joy_right.y = joy_right.get_y();
	if ((st.btn_mask != state.btn_mask) ||
		(std::abs(st.joy_left.x - state.joy_left.x) > JOY_THRESHOLD) ||
		(std::abs(st.joy_left.y - state.joy_left.y) > JOY_THRESHOLD) ||
		(std::abs(st.joy_right.x - state.joy_right.x) > JOY_THRESHOLD) ||
		(std::abs(st.joy_right.y - state.joy_right.y) > JOY_THRESHOLD))
	{
		changed = true;
	}
	state = st;
	return changed;
}

std::unique_ptr<Application> app;

extern "C" void app_main() {
	Core::init({
			.status_led_gpio = GPIO_NUM_2
			});

	app = std::make_unique<Application>();

	status_led->set(false);
}
