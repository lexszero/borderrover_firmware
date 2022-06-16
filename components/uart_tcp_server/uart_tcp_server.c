#include "uart_tcp_server.h"

#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_vfs_dev.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))

static int uart_tcp_handler(uart_tcp_server_t *srv, int sock) {
	const char *TAG = srv->name;
	char uart_path[16];
	int uart;
	char buf[128];
	int ret;

	ESP_LOGI(TAG, "sock fd = %d", sock);

	snprintf(uart_path, sizeof(uart_path), "/dev/uart/%d", srv->uart_port);
	if ((uart = open(uart_path, O_RDWR | O_NONBLOCK)) == -1) {
		ESP_LOGE(TAG, "Cannot open UART");
		vTaskDelay(5000 / portTICK_PERIOD_MS);
		return -1;
	}

	esp_vfs_dev_uart_use_driver(srv->uart_port);

	ESP_LOGI(TAG, "uart fd = %d", uart);

	while (1) {
		fd_set rfds;
		struct timeval tv = {
			.tv_sec = 5,
			.tv_usec = 0,
		};

		FD_ZERO(&rfds);
		FD_SET(sock, &rfds);
		FD_SET(uart, &rfds);

		
		ret = select(MAX(sock, uart) + 1, &rfds, NULL, NULL, &tv);

		if (ret < 0) {
			ESP_LOGE(TAG, "Select failed: errno %d", errno);
			goto cleanup;
		} else if (ret == 0) {
			ESP_LOGI(TAG, "Timeout has been reached and nothing has been received");
		} else {
			if (FD_ISSET(sock, &rfds)) {
				ret = recv(sock, buf, sizeof(buf) - 1, 0);
				if (ret > 0) {
					buf[ret] = '\0';
					ESP_LOGD(TAG, "sock rx: %s", buf);
					ret = write(uart, buf, ret);
					if (ret < 0) {
						ret = -errno;
						ESP_LOGE(TAG, "uart tx error: %d", ret);
						goto cleanup;
					} else {
						ESP_LOGD(TAG, "uart tx %d bytes", ret);
					}
				} else {
					ret = -errno;
					ESP_LOGE(TAG, "sock rx error: %d", ret);
					goto cleanup;
				}
			}
			if (FD_ISSET(uart, &rfds)) {
				ret = read(uart, buf, sizeof(buf) - 1);
				if (ret > 0) {
					buf[ret] = '\0';
					ESP_LOGD(TAG, "uart rx: %s", buf);
					ret = send(sock, buf, ret, 0);
					if (ret < 0) {
						ret = -errno;
						ESP_LOGE(TAG, "sock tx error: %d", ret);
						goto cleanup;
					} else {
						ESP_LOGD(TAG, "sock tx %d bytes", ret);
					}
				} else {
					ESP_LOGE(TAG, "uart rx error: %d", errno);
					ret = -errno;
					goto cleanup;
				}
			}
		}
	}
cleanup:
	close(uart);
	return ret;
}

static void uart_tcp_server_task(void *pvParameters)
{
	uart_tcp_server_t *server = (uart_tcp_server_t *)pvParameters;
	const char *TAG = server->name;

	char addr_str[128];
	int ret;

	struct sockaddr_in destAddr;
	destAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	destAddr.sin_family = AF_INET;
	destAddr.sin_port = htons(server->bind_port);
	inet_ntoa_r(destAddr.sin_addr, addr_str, sizeof(addr_str) - 1);

	int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	if (listen_sock < 0) {
		ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
		return;
	}
	ESP_LOGI(TAG, "Socket created");

	int err = bind(listen_sock, (struct sockaddr *)&destAddr, sizeof(destAddr));
	if (err != 0) {
		ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
		return;
	}
	ESP_LOGI(TAG, "Socket bound to port %d", server->bind_port);

	err = listen(listen_sock, 1);
	if (err != 0) {
		ESP_LOGE(TAG, "Error occured during listen: errno %d", errno);
		return;
	}
	ESP_LOGI(TAG, "Socket listening");

	while (1) {
		struct sockaddr_in6 sourceAddr; // Large enough for both IPv4 or IPv6
		uint addrLen = sizeof(sourceAddr);
		int sock = accept(listen_sock, (struct sockaddr *)&sourceAddr, &addrLen);
		if (sock < 0) {
			ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
			break;
		}
		server->client_socket = sock;

		// Get the sender's ip address as string
		inet_ntoa_r(((struct sockaddr_in *)&sourceAddr)->sin_addr.s_addr,
				server->client_addr, sizeof(server->client_addr) - 1);

		ESP_LOGI(TAG, "Connection from %s", server->client_addr);
		ret = uart_tcp_handler(server, sock);
		if (ret < 0) {
			ESP_LOGE(TAG, "Handler returned error: %d", ret);
		}
		if (ret == 0) {
			ESP_LOGI(TAG, "Connection closed");
		}

		server->client_socket = 0;

		if (sock != -1) {
			ESP_LOGE(TAG, "Shutting down socket and restarting...");
			shutdown(sock, 0);
			close(sock);
		}
	}
	vTaskDelete(NULL);
}

uart_tcp_server_t *uart_tcp_server_new(const char *name, uint16_t bind_port,
		uart_port_t uart_port) {
	uart_tcp_server_t *srv = calloc(1, sizeof(uart_tcp_server_t));
	srv->name = name;
	srv->bind_port = bind_port;
	srv->uart_port = uart_port;
	xTaskCreate(uart_tcp_server_task, name, 8192, srv, 5, &srv->task);
	return srv;
}
