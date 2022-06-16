#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/uart.h"

typedef struct uart_tcp_server {
	const char *name;
	TaskHandle_t task;

	// TCP params
	uint16_t bind_port;
	int client_socket;
	char client_addr[64];

	// UART params
	uart_port_t uart_port;
} uart_tcp_server_t;

uart_tcp_server_t *uart_tcp_server_new(const char *name, uint16_t bind_port,
		uart_port_t uart_port);

#ifdef __cplusplus
};
#endif


