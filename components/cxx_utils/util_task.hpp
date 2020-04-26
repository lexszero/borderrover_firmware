#pragma once

#include <sstream>
#include <thread>
#include <esp_pthread.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

class Task {
	public:
		Task(const char* tag, int stack = 4*1024, int prio = 10) {
			cfg = esp_pthread_get_default_config();
			ESP_LOGI("TASK", "%s: creating with stack = %d, prio = %d", tag, stack, prio);
			cfg.thread_name = tag;
			cfg.stack_size = stack;
			cfg.prio = prio;
		}

		void start() {
			ESP_LOGI("TASK", "%s: starting", pcTaskGetTaskName(nullptr));
			esp_pthread_set_cfg(&cfg);
			task = std::thread([this]() { run(); });
		}

		void print_info() {
			std::stringstream ss;
			ss << "core id: " << xPortGetCoreID()
				<< ", prio: " << uxTaskPriorityGet(nullptr)
				<< ", minimum free stack: " << uxTaskGetStackHighWaterMark(nullptr) << " bytes.";
			ESP_LOGI(pcTaskGetTaskName(nullptr), "%s", ss.str().c_str());
		}
	protected:
		virtual void run() {
			ESP_LOGI(pcTaskGetTaskName(nullptr), "FIXME: no overload, sleeping...");
			while (1) {
				vTaskDelay(1000 / portTICK_PERIOD_MS);
			}
		}
		
		std::thread task;
	
	private:
		esp_pthread_cfg_t cfg;
};
