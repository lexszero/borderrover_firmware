#pragma once

#include "util_task.hpp"
#include "util_lockable.hpp"
#include "util_time.hpp"
#include "util_queue.hpp"
#include "util_misc.hpp"
#include "wifi.h"

#include "core_status_led.hpp"
#include "cxx_espnow_message.hpp"
#include "cxx_espnow_peer.hpp"

#include "driver/gpio.h"
#include "esp_now.h"
#include "esp_wifi.h"

#include <chrono>
#include <future>
#include <functional>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <unordered_map>

namespace esp_now {

using SendResult = bool;

class ESPNow :
	private Task,
	private Lockable<std::shared_mutex>
{
public:
	ESPNow();
	~ESPNow();

	void set_led(std::shared_ptr<Core::StatusLed> led);
	void add_peer(const esp_now_peer_info_t& info);
	void add_peer(
			const PeerAddress& address,
			const std::optional<PeerKey>& key = std::nullopt,
			uint8_t channel = 0
			);
	std::shared_future<SendResult> send(const MessageInterface& msg);

	using MessageHandler = std::function<void(const Message&)>;
	void on_recv(MessageType type, MessageHandler&& handler);

private:
	std::shared_ptr<Core::StatusLed> led;
	wifi_interface_t iface;
	std::unordered_map<MessageType, MessageHandler> handlers;

	struct Peer {
		Peer();

		struct ResultPromise {
			std::promise<SendResult> promise;
			std::shared_future<SendResult> future;
		};
		std::unique_ptr<ResultPromise> send_result;
		uint32_t last_rx_seq;
		time_point last_rx_time;
		uint32_t last_tx_seq;
		time_point last_tx_time;
	};
	std::unordered_map<PeerAddress, Peer, PeerAddressHasher> peers;

	uint32_t send_seq;

	void run() override;

	enum EventID {
		None,
		Send,
		Recv,
	};

	struct EventSend {
		EventSend(const uint8_t *mac_addr, esp_now_send_status_t status);

		const PeerAddress peer;
		esp_now_send_status_t status;
	};

	struct EventRecv {
		EventRecv(const uint8_t *mac_addr, const uint8_t *data, size_t data_len);
		void release();

		const PeerAddress peer;
		uint8_t *data;
		const size_t data_len;
	};

	struct Event {
		Event();
		Event(EventSend&& send);
		Event(EventRecv&& recv);

		EventID id;
		union {
			int __empty;
			EventSend send;
			EventRecv recv;
		} info;
	};

	Queue<Event> events;
	void handle_send_cb(EventSend& ev);
	void handle_recv_cb(EventRecv& ev);
	friend void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status);
	friend void espnow_recv_cb(const uint8_t *mac_addr, const uint8_t *data, int data_len);
};

extern std::unique_ptr<ESPNow> espnow;

};
