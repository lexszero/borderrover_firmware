#pragma once

#include <sstream>
#include <thread>
#include <esp_pthread.h>
#include <esp_log.h>
#include <freertos/task.h>

class Task {
	public:
		Task(const char* tag, int stack = 4*1024, int prio = 10)
		{
			cfg = esp_pthread_get_default_config();
			ESP_LOGI("TASK", "%s: creating with stack = %d, prio = %d", tag, stack, prio);
			cfg.thread_name = tag;
			cfg.stack_size = stack;
			cfg.prio = prio;
		}

		void start() {
			ESP_LOGI("TASK", "%s: starting %s", pcTaskGetName(nullptr), cfg.thread_name);
			esp_pthread_set_cfg(&cfg);
			task = std::thread([this]() { run(); });
			running = true;
		}

		void print_info() {
			std::stringstream ss;
			ss << "core id: " << xPortGetCoreID()
				<< ", prio: " << uxTaskPriorityGet(nullptr)
				<< ", minimum free stack: " << uxTaskGetStackHighWaterMark(nullptr) << " bytes.";
			ESP_LOGI(pcTaskGetName(nullptr), "%s", ss.str().c_str());
		}
/*
	TaskHandle_t get_handle() {
		pthread_t thr = (task.native_handle());
		return esp_pthread_get_handle(thr);
	}
*/

	protected:
		virtual void run() {
			ESP_LOGI(pcTaskGetName(nullptr), "FIXME: no overload, sleeping...");
			while (1) {
				vTaskDelay(1000 / portTICK_PERIOD_MS);
			}
		}
		
		std::thread task;
	
	private:
		esp_pthread_cfg_t cfg;
		bool running;
};
