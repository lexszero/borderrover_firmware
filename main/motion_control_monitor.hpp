#pragma once

#include "freertos/queue.h"
#include "lwip/api.h"
#include "util_event.hpp"
#include "motion_control.hpp"
#include "websocket_server.h"

class MotionControlMonitor;
/*
class HttpWorker :
	private Task
{
	public:
		HttpWorker(MotionControlMonitor& mc);

	protected:
		void run() override;

	private:
		static constexpr char TAG[] = "http_server";

		MotionControlMonitor& mc;
};
*/

class HttpServer :
	private Task
{
	public:
		HttpServer(MotionControlMonitor& mc, uint16_t port);
		void ws_callback(uint8_t num, WEBSOCKET_TYPE_t type, char* msg, uint64_t len);

	protected:
		void run() override;
	
	private:
		static constexpr char TAG[] = "http_server";

		MotionControlMonitor& mc;
		static constexpr auto CLIENT_QUEUE_SIZE = 10;
		QueueHandle_t client_queue;
		struct netconn *srv_conn;

		void serve(struct netconn *conn);
};

class MotionControlMonitor :
	private Task
{
public:
	MotionControlMonitor(MotionControl& mc, uint16_t port);

	enum Event {
		StateUpdate = (1 << 0),
		MotorValues = (1 << 1),

		ClientConnected = (1 << 8),
		ClientDisconnected = (1 << 9),

		Any = 0xff
	};
	EventGroup<Event> events;
	HttpServer http_server;

protected:
	void run() override;

private:
	static constexpr char TAG[] = "mc_monitor";
	MotionControl& mc;


	void ws_send(const std::string& str);
};
