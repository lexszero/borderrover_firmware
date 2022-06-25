#pragma once

#include "util_task.hpp"
#include "util_queue.hpp"

#include "cxx_espnow_message.hpp"
#include "cxx_espnow_peer.hpp"
#include "util_misc.hpp"

#include "esp_now.h"

#include <future>
#include <functional>
#include <memory>
#include <string>
#include <ostream>
#include <unordered_map>

namespace esp_now {

using SendResult = bool;

class ESPNow :
	private Task
{
public:
	ESPNow();
	~ESPNow();

	void add_peer(const esp_now_peer_info_t& info);
	void send(const PeerAddress& peer, const MessageInterface& msg);

	using MessageHandler = std::function<void(const PeerAddress&, const Message&)>;
	void on_recv(MessageType type, MessageHandler&& handler);

private:
	std::unordered_map<MessageType, MessageHandler> handlers;

	struct Peer {
		Peer();
		bool send_in_progress;
		std::shared_future<SendResult> send_result;
		uint32_t last_seq;
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
		const uint8_t *data;
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
	void handle_msg(const PeerAddress& peer, const Message& msg);
	friend void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status);
	friend void espnow_recv_cb(const uint8_t *mac_addr, const uint8_t *data, int data_len);
};

extern std::unique_ptr<ESPNow> espnow;

};
