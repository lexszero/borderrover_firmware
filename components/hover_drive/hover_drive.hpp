#pragma once

#include <driver/uart.h>
#include "util_task.hpp"

struct HoverControlMsg
{
	uint16_t magic;
	int16_t steer;
	int16_t speed;
	uint16_t checksum;
} __attribute__((__packed__));

class HoverDrive :
	private Task
{
public:
	HoverDrive(uart_port_t uart_port);

	void go(int16_t speed, int16_t steer);

private:
	uart_port_t uart_port;
	QueueHandle_t uart_queue;

	void run() override;
	void reset();
	void receive(size_t len);
	void send(int16_t speed, int16_t steer);
};
