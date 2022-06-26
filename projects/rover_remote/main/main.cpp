#define CORE_STATUS_LED_GPIO GPIO_NUM_2
#include "sys_core.hpp"
#include "core_status_led.hpp"
#include "core_gpio.hpp"
#include "util_task.hpp"
#include "util_time.hpp"
#include "cxx_espnow.hpp"
#include "body_control_message.hpp"
#include "wifi.h"

#include "argtable3/argtable3.h"
#include "driver/adc.h"
#include "esp_console.h"
#include "esp_log.h"

#include <memory>
#include <string>

#include "leds.hpp"

#define TAG "app"

using namespace std::literals;
using namespace Core;
using namespace esp_now;

namespace {

static const auto PeerRoverBody = PeerAddress(0x3c, 0x71, 0xbf, 0x58, 0xf2, 0x89);

}  // namespace

class Application :
	private Task
{
public:
	Application();
	
private:
	RemoteState state;
	std::shared_ptr<StatusLed> led_red, led_blue;
	Leds::Output pixels;
	std::array<InputGPIO, RemoteButton::_Count> buttons;
	
	time_point last_send_announce;
	time_point last_send_state;
	time_point last_receive;

	void run() override;
	bool poll_inputs();
};

Application::Application() :
	Task(TAG, 16*1024, 20),
	state(),
	led_red(std::make_shared<StatusLed>("led_red",
				OutputGPIO("led_red", GPIO_NUM_4, true, true))),
	led_blue(std::make_shared<StatusLed>("led_blue",
				OutputGPIO("led_blue", GPIO_NUM_16, true, true))),
	pixels(32*8, GPIO_NUM_13, 0),
	buttons {
		InputGPIO {"joystick",	GPIO_NUM_23,	true, GPIO_PULLUP_ONLY},
		InputGPIO {"btn_left",	GPIO_NUM_32,	false, GPIO_PULLDOWN_ONLY},
		InputGPIO {"btn_right",	GPIO_NUM_33,	false, GPIO_PULLDOWN_ONLY},
		InputGPIO {"btn_up",	GPIO_NUM_25,	false, GPIO_PULLDOWN_ONLY},
		InputGPIO {"btn_down",	GPIO_NUM_26,	false, GPIO_PULLDOWN_ONLY},
		InputGPIO {"sw_red",	GPIO_NUM_27,	false, GPIO_PULLDOWN_ONLY},
		InputGPIO {"sw_blue",	GPIO_NUM_14,	false, GPIO_PULLDOWN_ONLY},
	},
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
			led_red->blink_once(100);
		});


	pixels.leds.setNumSegments(1);
	pixels.segments.emplace_back(pixels, "led", 0, 0, 170);
	pixels.start();

	Task::start();
}

void Application::run()
{
	ESP_LOGI(TAG, "started");
	while (1) {
		const auto now = time_now();
		try {
			if (now - last_send_announce > 5s) {
				last_send_announce = now;
				espnow->send(Message(PeerBroadcast, esp_now::MessageId::Announce));
			}
			bool changed = poll_inputs();
			auto since_last = now - last_send_state;
			if ((changed && since_last > 5ms) || (since_last > 500ms)) {
				ESP_LOGI(TAG, "%s", to_string(state).c_str());
				espnow->send(MessageRoverRemoteState(PeerRoverBody, state));
				last_send_state = now;
			}
		}
		catch (const std::exception& e) {
			ESP_LOGE(TAG, "Exception: %s", e.what());
		}
		std::this_thread::sleep_for(1ms);
	}
}

bool Application::poll_inputs()
{
	bool changed = false;
	RemoteState st;
	int i = 0;
	st.btn_mask = 0;
	for (const auto& btn : buttons) {
		if (btn.get())
			st.btn_mask |= (1 << i);
		i++;
	}
	if (st.btn_mask != state.btn_mask) {
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
