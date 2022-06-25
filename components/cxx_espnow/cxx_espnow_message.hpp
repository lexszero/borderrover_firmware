#pragma once

#include "cxx_espnow_peer.hpp"
#include "util_misc.hpp"

#include <esp_crc.h>
#include <esp_log.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <sstream>
#include <vector>

namespace esp_now {

using Buffer = std::vector<uint8_t>;

enum MessageType : uint8_t {
	Announce = 0,
	Ping,
	Pong,
};

struct MessageHeader
{
	uint32_t magic;
	uint32_t seq;
	MessageType type;
	uint8_t length;
	uint16_t crc;
} __attribute__((packed));

class MessageInterface
{
public:
	virtual std::string to_string() const = 0;
	virtual const Buffer& prepare(uint32_t seq) const = 0;
	virtual const PeerAddress& peer() const = 0;
	virtual const MessageHeader& header() const = 0;
	virtual const void * payload() const = 0;

};

using MessageCallback = std::function<void(const MessageInterface& msg)>;

class Message :
	public MessageInterface
{
public:
	static constexpr uint32_t MAGIC = 0xBEEFBABE;

	Message(const Message& other) :
		peer_addr(other.peer_addr),
		buffer(other.buffer.begin(), other.buffer.end()),
		hdr(*reinterpret_cast<MessageHeader *>(buffer.data()))
	{}

	Message(const PeerAddress& peer, MessageType type, size_t payload_length = 0) :
		peer_addr(peer),
		buffer(sizeof(MessageHeader) + payload_length),
		hdr(*reinterpret_cast<MessageHeader *>(buffer.data()))
	{
		hdr.magic = MAGIC;
		hdr.type = type;
	}

	Message(const PeerAddress& peer, const uint8_t *data, size_t length) :
		peer_addr(peer),
		buffer(length),
		hdr(*reinterpret_cast<MessageHeader *>(buffer.data()))
	{
		for (auto i = 0; i < length; i++) {
			buffer[i] = data[i];
		}
		auto crc_cal = esp_crc16_le(UINT16_MAX, buffer.data() + sizeof(MessageHeader), hdr.length);

		if (crc_cal != hdr.crc)
			throw std::runtime_error("crc check failed");
	}

	Message(const PeerAddress& peer, MessageType type, const uint8_t *data, size_t length) :
		Message(peer, type, length)
	{
		for (auto i = 0; i < length; i++) {
			buffer[i+sizeof(MessageHeader)] = data[i];
		}
		hdr_update_payload();
	}

	Message& operator=(Message&& other)
	{
		peer_addr = other.peer_addr;
		buffer = other.buffer;
		hdr = *reinterpret_cast<MessageHeader *>(buffer.data());
		return *this;
	}

	virtual std::string to_string() const override
	{
		using namespace std;
		ostringstream ss;
		ss << "Message[peer:" << peer_addr <<
			", magic:" << hex << hdr.magic <<
			", seq:" << hdr.seq <<
			", type:" << hdr.type <<
			", len:" << static_cast<unsigned>(hdr.length) << "]{";
		for (size_t i = sizeof(MessageHeader); i < buffer.size(); i++) {
			ss << setfill('0') << setw(2) << right << hex << static_cast<unsigned>(buffer[i]);
		}
		ss << "}";
		return ss.str();
	}
	virtual const Buffer& prepare(uint32_t seq) const override
	{
		hdr.seq = seq;
		hdr_update_payload();
		return buffer;
	}
	virtual const PeerAddress& peer() const override
	{
		return peer_addr;
	}
	virtual const MessageHeader& header() const override
	{
		hdr_update_payload();
		return hdr;
	}
	virtual const void *payload() const override
	{
		return buffer.data() + sizeof(MessageHeader);
	}

protected:
	PeerAddress peer_addr;
	Buffer buffer;
	MessageHeader& hdr;

	void hdr_update_payload() const
	{
		hdr.length = buffer.size() - sizeof(MessageHeader);
		hdr.crc = esp_crc16_le(UINT16_MAX, buffer.data() + sizeof(MessageHeader), hdr.length);
	}
};

template <uint8_t Type, typename Payload>
class GenericMessage :
	public Message
{
	GenericMessage(Message&& msg) :
		Message(msg)
	{}

	const Payload& content() const
	{
		return *reinterpret_cast<Payload *>(buffer.data() + sizeof(MessageHeader));
	}
};

using MessagePing = GenericMessage<MessageType::Ping, void>;
using MessageAnnounce = GenericMessage<MessageType::Announce, void>;

}

std::ostream& operator<<(std::ostream& os, const esp_now::MessageInterface& m);
