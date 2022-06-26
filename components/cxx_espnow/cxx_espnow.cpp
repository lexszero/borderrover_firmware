#include "cxx_espnow.hpp"
#include "util_task.hpp"
#include "util_misc.hpp"

#include "esp_log.h"

#include <utility>

#define TAG "espnow"

namespace esp_now {

using std::chrono::time_point_cast;
std::unique_ptr<ESPNow> espnow;

static constexpr TickType_t MAX_DELAY = 512;

void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
	if (!espnow)
		return;

	ESP_LOGD(TAG, "send_cb, status %d", status);

	if (!espnow->events.send({ESPNow::EventSend(mac_addr, status)}, MAX_DELAY)) {
		ESP_LOGW(TAG, "Failed to enqueue EventSend");
	}
}

void espnow_recv_cb(const uint8_t *mac_addr, const uint8_t *data, int data_len)
{
	if (!espnow)
		return;

	ESP_LOGD(TAG, "recv_cb, data_len %d", data_len);

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
	data(static_cast<uint8_t *>(malloc(_data_len))),
	data_len(_data_len)
{
	if (!data)
		throw std::runtime_error("Failed to allocate memory for message");
	memcpy(data, _data, data_len);
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

	wifi_mode_t mode;
	esp_wifi_get_mode(&mode);
	iface = (mode == WIFI_MODE_STA) ? WIFI_IF_STA : WIFI_IF_AP;

	ret = esp_now_init();
	if (ret != ESP_OK)
		throw std::runtime_error("ESP-Now init failed");

	esp_now_get_version(&version);
	ESP_LOGI(TAG, "Initialized, version %d, interface %s", version, (iface == WIFI_IF_STA) ? "STA" : "AP");

	using namespace std::placeholders;
	ret = esp_now_register_send_cb(espnow_send_cb);
	if (ret != ESP_OK)
		throw std::runtime_error("ESP-Now send cb register failed");
	ret = esp_now_register_recv_cb(espnow_recv_cb);
	if (ret != ESP_OK)
		throw std::runtime_error("ESP-Now recv cb register failed");
	ret = esp_wifi_set_protocol(iface, WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N|WIFI_PROTOCOL_LR);
	if (ret != ESP_OK)
		throw std::runtime_error("ESP-Now set wifi protocol failed");
	ret = esp_wifi_config_espnow_rate(iface, WIFI_PHY_RATE_1M_L);
	if (ret != ESP_OK)
		throw std::runtime_error("ESP-Now set rate failed");

	Task::start();
}

ESPNow::~ESPNow()
{
	esp_now_unregister_recv_cb();
	esp_now_unregister_send_cb();
	esp_now_deinit();
}

void ESPNow::set_led(std::shared_ptr<Core::StatusLed> _led)
{
	led = std::move(_led);
}

void ESPNow::add_peer(
		const PeerAddress& address,
		const std::optional<PeerKey>& key,
		uint8_t channel
		)
{
	const auto& found = peers.find(address);
	if (found != peers.end())
		return;

	esp_now_peer_info_t info;
	memcpy(info.peer_addr, address.bytes(), ESP_NOW_ETH_ALEN);
	if (key) {
		memcpy(info.lmk, key->data(), ESP_NOW_KEY_LEN);
		info.encrypt = true;
	}
	else {
		info.encrypt = false;
	}
	info.ifidx = iface;

	auto [peer, is_new] = peers.try_emplace(address);
	info.priv = &peer;

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

std::shared_future<SendResult> ESPNow::send(const MessageInterface& msg)
{
	const auto& addr = msg.peer();
	auto& peer = peers.at(addr);
	if (peer.send_result) {
		auto future = peer.send_result->future;
		auto status = future.wait_for(1s);
		if (status != std::future_status::ready)
			throw std::runtime_error("esp_now_send: Timed out waiting for previous transmission");
	}

	const auto& data = msg.prepare(send_seq++);
	auto ret = esp_now_send(addr.bytes(), data.data(), data.size());
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
	peer.last_tx_seq = send_seq;
	peer.last_tx_time = time_point_cast<duration>(Clock::now());
	peer.send_result = std::make_unique<Peer::ResultPromise>();
	peer.send_result->future = peer.send_result->promise.get_future().share();
	return peer.send_result->future;
}

void ESPNow::on_recv(MessageType type, ESPNow::MessageHandler&& handler)
{
	handlers[type] = std::move(handler);
}

ESPNow::Peer::Peer() :
	send_result()
{}

void ESPNow::run()
{
	while (1) {
		auto ret = events.receive(MAX_DELAY);
		if (!ret)
			continue;

		Event& evt = *ret;

		switch (evt.id) {
			case EventID::None:
				break;

			case EventID::Send:
				handle_send_cb(evt.info.send);
				break;

			case EventID::Recv:
				handle_recv_cb(evt.info.recv);
				break;
		}
	}
}

void ESPNow::handle_send_cb(ESPNow::EventSend& ev)
{
	if (led)
		led->blink_once(100);
	auto found = peers.find(ev.peer);
	if (found == peers.end()) {
		ESP_LOGW(TAG, "send cb event for unknown peer %s", ::to_string(ev.peer).c_str());
		return;
	}
	auto& peer = found->second;
	ESP_LOGD(TAG, "send %s: status %d",
			to_string(ev.peer).c_str(),
			ev.status);
	peer.last_tx_time = time_point_cast<duration>(Clock::now());
	if (peer.send_result) {
		peer.send_result->promise.set_value(ev.status == ESP_NOW_SEND_SUCCESS);
		peer.send_result.reset();
	}
}

void ESPNow::handle_recv_cb(ESPNow::EventRecv& ev)
{
	if (led)
		led->blink_once(100);
	try {
		auto msg = Message(ev.peer, ev.data, ev.data_len);
		ev.release();
		ESP_LOGD(TAG, "recv: %s", msg.to_string().c_str());
	
		const auto hdr = msg.header();
		const auto& handler = handlers.find(hdr.type);
		if (handler == handlers.end()) {
			ESP_LOGW(TAG, "No handler for message: %s", msg.to_string().c_str());
			return;
		}
	
		handler->second(msg);
	}
	catch (const std::exception& e) {
		ESP_LOGE(TAG, "recv: %s", e.what());
	}
}

}
