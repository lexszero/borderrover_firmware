#pragma once

#include <sstream>
#include <thread>
#include <esp_pthread.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

class Task {
	public:
		Task(const char* tag, int prio = 10) {
			auto thr_cfg = esp_pthread_get_default_config();
			thr_cfg.thread_name = tag;
			thr_cfg.prio = prio;
			esp_pthread_set_cfg(&thr_cfg);
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
		virtual void run() = 0;
	
	private:
		std::thread task;
};
