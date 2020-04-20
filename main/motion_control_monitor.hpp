#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "motion_control.hpp"

class MotionControlMonitor : private Task {
public:
	MotionControlMonitor(MotionControl& mc, uint16_t port);
protected:
	void run() override;
private:
	MotionControl& mc;
	uint16_t port;

	StaticEventGroup_t event_buf;
	EventGroupHandle_t event;

	enum Event {
		StateUpdate = (1 << 0),
		Any = 0xff
	};

	void do_send(int sock);
	void send(int sock, const std::string& str);
};
