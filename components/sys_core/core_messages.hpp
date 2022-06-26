#pragma once

#include "cxx_espnow_message.hpp"

namespace Core {

using esp_now::Message;
using esp_now::GenericMessage;
using esp_now::MessageType;

static constexpr size_t KEY_LENGTH = 32;
static constexpr size_t VALUE_LENGTH = 32;
using CtlKey = char[KEY_LENGTH];
using CtlValue = char[VALUE_LENGTH];

struct CtlKeyValue
{
	CtlKey key;
	CtlValue value;
} __attribute__((__packed__));

template <typename Type>
struct Response
{
	uint8_t request_type;
	uint32_t request_seq;
	Type data;
} __attribute__((__packed__));

enum MessageId : MessageType {
	CtlGetAll = 0x80,
	CtlGet = 0x81,
	CtlSet = 0x82,
	CtlUpdate = 0x83,
};

using MessageGetAll = GenericMessage<MessageId::CtlGetAll, void>;
using MessageGet = GenericMessage<MessageId::CtlGet, CtlKey>;
using MessageSet = GenericMessage<MessageId::CtlSet, CtlKeyValue>;
using MessageUpdate = GenericMessage<MessageId::CtlUpdate, CtlKey>;

};
