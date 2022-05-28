#include <stdexcept>

#include "esp_log.h"
#include "esp_netif.h"
#include "lwip/err.h"
#include "lwip/api.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "websocket_server.h"

#include "motion_control_monitor.hpp"

static HttpServer *srv = nullptr;

void websocket_callback(uint8_t num, WEBSOCKET_TYPE_t type, char* msg, uint64_t len)
{
	if (srv)
		srv->ws_callback(num, type, msg, len);
}

HttpServer::HttpServer(MotionControlMonitor& _mc, uint16_t port) :
	Task("http_server", 8*1024, 5),
	mc(_mc),
	client_queue(xQueueCreate(CLIENT_QUEUE_SIZE, sizeof(struct netconn*)))
{
	srv_conn = netconn_new(NETCONN_TCP);
	netconn_bind(srv_conn, NULL, port);

	srv = this;
	Task::start();
}

void HttpServer::run()
{
	struct netconn *newconn;
	static err_t err;

	netconn_listen(srv_conn);
	ESP_LOGI(TAG, "server listening");
	do {
		err = netconn_accept(srv_conn, &newconn);
		ESP_LOGI(TAG,"new client");
		if(err == ERR_OK) {
			//xQueueSendToBack(client_queue, &newconn, portMAX_DELAY);
			serve(newconn);
		}
	} while(err == ERR_OK);
	netconn_close(srv_conn);
	netconn_delete(srv_conn);
}

void HttpServer::serve(struct netconn *conn)
{
	const static char HTML_HEADER[] = "HTTP/1.1 200 OK\nContent-type: text/html\n\n";
	const static char ERROR_HEADER[] = "HTTP/1.1 404 Not Found\nContent-type: text/html\n\n";
	const static char JS_HEADER[] = "HTTP/1.1 200 OK\nContent-type: text/javascript\n\n";
	const static char CSS_HEADER[] = "HTTP/1.1 200 OK\nContent-type: text/css\n\n";
	//const static char PNG_HEADER[] = "HTTP/1.1 200 OK\nContent-type: image/png\n\n";
	const static char ICO_HEADER[] = "HTTP/1.1 200 OK\nContent-type: image/x-icon\n\n";
	//const static char PDF_HEADER[] = "HTTP/1.1 200 OK\nContent-type: application/pdf\n\n";
	//const static char EVENT_HEADER[] = "HTTP/1.1 200 OK\nContent-Type: text/event-stream\nCache-Control: no-cache\nretry: 3000\n\n";
	struct netbuf* inbuf;
	static char* buf;
	static uint16_t buflen;
	static err_t err;
	netconn_set_recvtimeout(conn, 1000); // allow a connection timeout of 1 second
	ESP_LOGI(TAG, "reading from client...");
	err = netconn_recv(conn, &inbuf);
	ESP_LOGI(TAG, "read from client");
	if (err != ERR_OK) {
		goto cleanup_inbuf;
	}
	netbuf_data(inbuf, (void**)&buf, &buflen);
	if (!buf)
		return;

	if (buf) {
		// default page websocket
		if(strstr(buf,"GET / ")
				&& strstr(buf,"Upgrade: websocket")) {
			ESP_LOGI(TAG,"Requesting websocket on /");
			ws_server_add_client(conn, buf, buflen, const_cast<char *>("/"), websocket_callback);
		}
	}
cleanup_inbuf:
	netbuf_delete(inbuf);
}


void HttpServer::ws_callback(uint8_t num, WEBSOCKET_TYPE_t type, char* msg, uint64_t len)
{
	switch (type) {
		case WEBSOCKET_CONNECT:
			ESP_LOGI(TAG, "client %i connected!", num);
			mc.events.set(MotionControlMonitor::Event::ClientConnected);
			break;
		case WEBSOCKET_DISCONNECT_EXTERNAL:
			ESP_LOGI(TAG, "client %i sent a disconnect message", num);
			mc.events.set(MotionControlMonitor::Event::ClientDisconnected);
			break;
		case WEBSOCKET_DISCONNECT_INTERNAL:
			ESP_LOGI(TAG, "client %i was disconnected", num);
			mc.events.set(MotionControlMonitor::Event::ClientDisconnected);
			break;
		case WEBSOCKET_DISCONNECT_ERROR:
			ESP_LOGI(TAG, "client %i was disconnected due to an error", num);
			mc.events.set(MotionControlMonitor::Event::ClientDisconnected);
			break;
		case WEBSOCKET_TEXT:
			if (len) { // if the message length was greater than zero
				ESP_LOGI(TAG, "client %i sent %llu bytes: %s", num, len, msg);
			}
			break;
		case WEBSOCKET_BIN:
			ESP_LOGI(TAG, "client %i sent binary message of size %i:\n%s", num, (uint32_t)len, msg);
			break;
		case WEBSOCKET_PING:
			ESP_LOGI(TAG, "client %i pinged us with message of size %i:\n%s", num, (uint32_t)len, msg);
			break;
		case WEBSOCKET_PONG:
			ESP_LOGI(TAG, "client %i responded to the ping", num);
			break;
	}
}

MotionControlMonitor::MotionControlMonitor(MotionControl& _mc, uint16_t _port) :
	Task(TAG, 8*1024, 10),
	http_server(*this, _port),
	mc(_mc)
{
	mc.on_state_update([this]() {
				events.set(Event::StateUpdate);
			});

	mc.on_motor_values([this]() {
				events.set(Event::MotorValues);
			});

	ws_server_start();

	Task::start();
}

void MotionControlMonitor::run() {
	const TickType_t timeout = 1000 / portTICK_PERIOD_MS;
	static const std::string keepalive = R"(
	{
		"type": "keepalive"
	}
	)"_json.dump();

	int num_clients = 0;
	try {
		while (1) {
			Event wait_mask = Event(Event::ClientConnected | Event::ClientDisconnected);
			if (num_clients)
				wait_mask = Event(wait_mask | Event::StateUpdate | Event::MotorValues);
			Event ev = events.wait(wait_mask, timeout, true, false);

			if (ev & Event::ClientConnected) {
				num_clients++;
			}
			if (ev & Event::ClientDisconnected) {
				num_clients--;
			}
			if (ev & Event::StateUpdate) {
				json msg = {
					{"type", "mc_state"},
					{"data", mc.get_state()},
				};
				ws_send(msg.dump());
			}
			if (ev & Event::MotorValues) {
				json msg = {
					{"type", "motor_state"},
					{"left", mc.get_motor_values(Left)},
					{"right", mc.get_motor_values(Right)},
				};
				ws_send(msg.dump());
			}
		}
	}
	catch (const std::runtime_error& e) {
		ESP_LOGE(TAG, "Exception: %s", e.what());
	}
}

void MotionControlMonitor::ws_send(const std::string& str)
{
	int clients = ws_server_send_text_all(const_cast<char *>(str.c_str()), str.length());
	if (clients > 0) {
		ESP_LOGD(TAG, "sent to %d clients", clients);
	}
}
