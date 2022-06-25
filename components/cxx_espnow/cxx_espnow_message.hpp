#pragma once

#include "cxx_espnow_peer.hpp"
#include "util_misc.hpp"

#include <esp_crc.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <sstream>
#include <vector>

namespace esp_now {

using Buffer = std::vector<uint8_t>;

enum MessageType : uint8_t {
	Ping = 0,
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
	virtual const MessageHeader& header() const = 0;
	virtual const void * payload() const = 0;

	friend std::ostream& operator<<(std::ostream& os, const MessageInterface& m);
};

using MessageCallback = std::function<void(const MessageInterface& msg)>;

class Message :
	public MessageInterface
{
public:
	static constexpr uint32_t MAGIC = 0xBEEFBABE;

	Message(const Message& other) :
		buffer(other.buffer.begin(), other.buffer.end()),
		hdr(*reinterpret_cast<MessageHeader *>(buffer.data()))
	{}

	Message(MessageType type, size_t payload_length = 0) :
		buffer(sizeof(MessageHeader) + payload_length),
		hdr(*reinterpret_cast<MessageHeader *>(buffer.data()))
	{
		hdr.magic = MAGIC;
		hdr.type = type;
	}

	Message(const uint8_t *data, size_t length) :
		buffer(length),
		hdr(*reinterpret_cast<MessageHeader *>(buffer.data()))
	{
		for (auto i = 0; i < length; i++) {
			buffer[i] = data[i];
		}
	}

	Message(MessageType type, const uint8_t *data, size_t length) :
		Message(type, length)
	{
		for (auto i = 0; i < length; i++) {
			buffer[i+sizeof(MessageHeader)] = data[i];
		}
		hdr_update_payload();
	}

	Message& operator=(Message&& other)
	{
		buffer = other.buffer;
		hdr = *reinterpret_cast<MessageHeader *>(buffer.data());
		return *this;
	}


	virtual std::string to_string() const override
	{
		std::ostringstream ss;
		ss << "Message[magic:" << hdr.magic << ", seq:" << hdr.seq << ", type:" << hdr.type << ", len:" << hdr.length << "]{";
		for (size_t i = 2; i < buffer.size(); i++) {
			ss << buffer[i];
		}
		ss << "}";
		return ss.str();
	}
	virtual const Buffer& prepare(uint32_t seq) const override {
		hdr.seq = seq;
		hdr_update_payload();
		return buffer;
	}
	virtual const MessageHeader& header() const override {
		hdr_update_payload();
		return hdr;
	}
	virtual const void *payload() const override {
		return buffer.data() + sizeof(MessageHeader);
	}

protected:
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
};

using MessagePing = GenericMessage<MessageType::Ping, void>;

}
