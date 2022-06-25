#define CORE_STATUS_LED_GPIO GPIO_NUM_2
#include "sys_core.hpp"
#include "core_status_led.hpp"
#include "core_gpio.hpp"
#include "util_task.hpp"
#include "cxx_espnow.hpp"

#include "argtable3/argtable3.h"
#include "esp_console.h"
#include "esp_log.h"

#include <chrono>
#include <memory>
#include <string>

#include "leds.hpp"

#define TAG "app"

using namespace esp_now;

class Application :
	private Task
{
public:
	Application();
	
private:
	Leds::Output leds;

	void run() override;
};

Application::Application() :
	Task(TAG, 16*1024, 20),
	leds(32*8, GPIO_NUM_13, 0)
{
	espnow->set_led(Core::status_led);
	leds.leds.setNumSegments(1);
	leds.segments.emplace_back(leds, "led", 0, 0, 170);
	leds.start();
	Task::start();
}

void Application::run()
{
	ESP_LOGI(TAG, "started");
	espnow->add_peer(PeerBroadcast, std::nullopt, 5);
	while (1) {
		try {
			espnow->send(Message(PeerBroadcast, MessageType::Announce));
		}
		catch (const std::exception& e) {
			ESP_LOGE(TAG, "Exception: %s", e.what());
		}
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}

std::unique_ptr<Application> app;

extern "C" void app_main() {
	core_init();

	app = std::make_unique<Application>();

	Core::status_led->blink(500);
}
