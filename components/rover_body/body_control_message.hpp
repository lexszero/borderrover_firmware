#pragma once

#include "cxx_espnow_message.hpp"

using esp_now::MessageType;
using esp_now::GenericMessage;

enum MessageId : MessageType {
	RoverRemoteState = 0x90,
};

enum RemoteButton : uint32_t
{
	_First		= 0,
	Joystick	= 0,
	Left		= 1,
	Right		= 2,
	Up			= 3,
	Down		= 4,
	SwitchRed	= 5,
	SwitchBlue	= 6,
	_Count
};

std::ostream& operator<<(std::ostream& os, const RemoteButton& btn);

struct RemoteJoystickState
{
	int8_t x;
	int8_t y;
} __attribute__((__packed__));

struct RemoteState
{
	uint32_t btn_mask;
	RemoteJoystickState joy;

	bool is_pressed(RemoteButton button_id) const
	{
		return btn_mask & (1 << to_underlying(button_id));
	}
} __attribute__((__packed__));

std::ostream& operator<<(std::ostream& os, const RemoteState& st);

struct RemoteEvent
{
	enum Type : uint8_t {
		Button,
		Joystick
	};

	Type type;
	union {
		uint32_t btn_mask;
		RemoteJoystickState joystick;
	};
} __attribute__((__packed__));


using MessageRoverRemoteState = GenericMessage<RoverRemoteState, RemoteState>;
