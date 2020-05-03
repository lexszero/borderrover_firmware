#pragma once

#include "util_event.hpp"
#include "motion_control.hpp"

class MotionControlMonitor :
	private Task
{
public:
	MotionControlMonitor(MotionControl& mc, uint16_t port);
protected:
	void run() override;
private:
	MotionControl& mc;
	uint16_t port;

	enum Event {
		StateUpdate = (1 << 0),
		MotorValues = (1 << 1),
		Any = 0xff
	};
	EventGroup<Event> events;

	void do_send(int sock);
	void send(int sock, const std::string& str);
	void send_motor(int sock, const char *id, const Vesc::vescData& values);
};
