#include "cxx_espnow.hpp"
#include "util_task.hpp"
#include "util_misc.hpp"

#include "esp_log.h"

#include <utility>

#define TAG "espnow"

namespace esp_now {

std::unique_ptr<ESPNow> espnow;

static constexpr TickType_t MAX_DELAY = 512;

void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
	if (!espnow)
		return;

	if (!espnow->events.send({ESPNow::EventSend(mac_addr, status)}, MAX_DELAY)) {
		ESP_LOGW(TAG, "Failed to enqueue EventSend");
	}
}

void espnow_recv_cb(const uint8_t *mac_addr, const uint8_t *data, int data_len)
{
	if (!espnow)
		return;

	if (!espnow->events.send({ESPNow::EventRecv(mac_addr, data, data_len)}, MAX_DELAY)) {
		ESP_LOGW(TAG, "Failed to enqueue EventRecv");
	}
}

ESPNow::EventSend::EventSend(const uint8_t *_mac_addr, esp_now_send_status_t _status) :
	peer(_mac_addr),
	status(_status)
{}

ESPNow::EventRecv::EventRecv(const uint8_t *_mac_addr, const uint8_t *_data, size_t _data_len) :
	peer(_mac_addr),
	data(static_cast<const uint8_t *>(malloc(_data_len))),
	data_len(_data_len)
{
	if (!data)
		throw std::runtime_error("Failed to allocate memory for message");
}

void ESPNow::EventRecv::release()
{
	free(const_cast<void *>(static_cast<const void *>(data)));
	data = nullptr;
}

ESPNow::Event::Event() :
	id(EventID::None),
	info{.__empty = 0}
{}

ESPNow::Event::Event(EventSend&& send) :
	id(EventID::Send),
	info{.send = send}
{}

ESPNow::Event::Event(EventRecv&& recv) :
	id(EventID::Recv),
	info{.recv = recv}
{}

ESPNow::ESPNow() :
	Task(TAG, 4*1024, 15),
	send_seq(0),
	events(16)
{
	uint32_t version;
	esp_err_t ret;

	ret = esp_now_init();
	if (ret != ESP_OK)
		throw std::runtime_error("ESP-Now init failed");

	esp_now_get_version(&version);
	ESP_LOGI(TAG, "Initialized, version %d", version);

	using namespace std::placeholders;
	ret = esp_now_register_send_cb(espnow_send_cb);
	if (ret != ESP_OK)
		throw std::runtime_error("ESP-Now send cb register failed");
	ret = esp_now_register_recv_cb(espnow_recv_cb);
	if (ret != ESP_OK)
		throw std::runtime_error("ESP-Now recv cb register failed");
}

ESPNow::~ESPNow()
{
	esp_now_unregister_recv_cb();
	esp_now_unregister_send_cb();
	esp_now_deinit();
}

void ESPNow::add_peer(const esp_now_peer_info_t& info)
{
	PeerAddress addr(info.peer_addr);
	const auto [peer, is_new] = peers.try_emplace(addr);
	if (!is_new)
		return;

	auto ret = esp_now_add_peer(&info);
	switch (ret) {
		case ESP_ERR_ESPNOW_NOT_INIT:
			throw std::runtime_error("esp_now_add_peer: Not initialized");
		case ESP_ERR_ESPNOW_ARG:
			throw std::runtime_error("esp_now_add_peer: Invalid argument");
		case ESP_ERR_ESPNOW_FULL:
			throw std::runtime_error("esp_now_add_peer: Peer list is full");
		case ESP_ERR_ESPNOW_NO_MEM:
			throw std::runtime_error("esp_now_add_peer: Out of memory");
		case ESP_ERR_ESPNOW_EXIST:
			throw std::runtime_error("esp_now_add_peer: Peer already exists");
	}
}

void ESPNow::send(const PeerAddress& peer_addr, const MessageInterface& msg)
{
	const auto& data = msg.prepare(send_seq++);
	auto ret = esp_now_send(peer_addr.bytes(), data.data(), data.size());
	switch (ret) {
		case ESP_ERR_ESPNOW_NOT_INIT:
			throw std::runtime_error("esp_now_send: Not initialized");
		case ESP_ERR_ESPNOW_ARG:
			throw std::runtime_error("esp_now_send: Invalid argument");
		case ESP_ERR_ESPNOW_INTERNAL:
			throw std::runtime_error("esp_now_send: Internal error");
		case ESP_ERR_ESPNOW_NO_MEM:
			throw std::runtime_error("esp_now_send: Out of memory");
		case ESP_ERR_ESPNOW_NOT_FOUND:
			throw std::runtime_error("esp_now_send: Peer is not found");
		case ESP_ERR_ESPNOW_IF:
			throw std::runtime_error("esp_now_send: current WiFi interface doesn’t match that of peer");
	}
}

void ESPNow::on_recv(MessageType type, ESPNow::MessageHandler&& handler)
{

}

ESPNow::Peer::Peer() :
	send_in_progress(false),
	send_result()
{}

void ESPNow::run()
{
	while (1) {
		auto ret = events.receive(MAX_DELAY);
		if (!ret)
			continue;

		const Event& evt = *ret;

		switch (evt.id) {
			case EventID::None:
				break;

			case EventID::Send:
				ESP_LOGD(TAG, "send %s: status %d",
						to_string(evt.info.send.peer).c_str(),
						evt.info.send.status);
				break;

			case EventID::Recv:
				handle_msg(evt.info.recv.peer, Message(evt.info.recv.data, evt.info.recv.data_len));
				break;
		}
	}
}

void ESPNow::handle_msg(const PeerAddress& peer, const Message& msg)
{
	ESP_LOGD(TAG, "recv %s: %s",
			to_string(peer).c_str(),
			msg.to_string().c_str());

	const auto hdr = msg.header();
	const auto& handler = handlers.find(hdr.type);
	if (handler == handlers.end()) {
		ESP_LOGW(TAG, "No handler for message type %d", hdr.type);
		return;
	}

	handler->second(peer, msg);
}

}
