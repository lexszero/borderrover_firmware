#include <stdexcept>

#include "esp_log.h"
#include "esp_netif.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#define TAG "mc_monitor"

#include "motion_control_monitor.hpp"

MotionControlMonitor::MotionControlMonitor(MotionControl& _mc, uint16_t _port) :
	Task(TAG, 8*1024, 5),
	mc(_mc),
	port(_port)
{
	mc.on_state_update([this]() {
				events.set(Event::StateUpdate);
			});

	mc.on_motor_values([this]() {
				events.set(Event::MotorValues);
			});

	Task::start();
}

void MotionControlMonitor::run() {
	char addr_str[128];
	struct sockaddr_in6 dest_addr;

	struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
	dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
	dest_addr_ip4->sin_family = AF_INET;
	dest_addr_ip4->sin_port = htons(port);

	int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	if (listen_sock < 0) {
		ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
		vTaskDelete(NULL);
		return;
	}
	int opt = 1;
	setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	ESP_LOGI(TAG, "Socket created");

	int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
	if (err != 0) {
		ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
		goto CLEAN_UP;
	}
	ESP_LOGI(TAG, "Socket bound, port %d", port);

	err = listen(listen_sock, 1);
	if (err != 0) {
		ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
		goto CLEAN_UP;
	}

	while (1) {
		ESP_LOGI(TAG, "Socket listening");

		struct sockaddr_in6 source_addr; // Large enough for both IPv4 or IPv6
		uint addr_len = sizeof(source_addr);
		int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
		if (sock < 0) {
			ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
			break;
		}

		// Convert ip address to string
		if (source_addr.sin6_family == PF_INET) {
			inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);
		}
		ESP_LOGI(TAG, "Socket accepted ip address: %s", addr_str);

		do_send(sock);

		shutdown(sock, 0);
		close(sock);
	}

CLEAN_UP:
	close(listen_sock);
	vTaskDelete(NULL);
}

void MotionControlMonitor::do_send(int sock)
{
	const TickType_t timeout = 1000 / portTICK_PERIOD_MS;
	static const std::string keepalive = R"(
	{
		"type": "keepalive"
	}
	)"_json.dump();

	try {
		while (1) {
			Event ev = events.wait(Event::Any, timeout);

			if (ev & Event::StateUpdate) {
				json msg = {
					{"type", "state"},
					{"data", mc.get_state()}
				};
				send(sock, msg.dump());
			}
			if (ev & Event::MotorValues) {
				send_motor(sock, "left", mc.get_motor_values(Left));
				send_motor(sock, "right", mc.get_motor_values(Right));
			}
			else {
				send(sock, keepalive);
			}
		}
	}
	catch (const std::runtime_error& e) {
		ESP_LOGE(TAG, "Exception: %s", e.what());
	}
}

void MotionControlMonitor::send(int sock, const std::string& str)
{
	const char *buf = str.c_str();
	int to_write = str.length();
	while (to_write > 0) {
		int ret = ::send(sock, buf, to_write, 0);
		if (ret < 0)
			throw std::runtime_error("Error during send()");
		buf += ret;
		to_write -= ret;
	}
	int ret = ::send(sock, "\n", 1, 0);
	if (ret < 0)
		throw std::runtime_error("Error during send()");
}

void MotionControlMonitor::send_motor(int sock, const char *id, const Vesc::vescData& data)
{
	json msg = {
		{"type", "motor"},
		{"id", id},
		{"data", data}
	};
	send(sock, msg.dump());
}
